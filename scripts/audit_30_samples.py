import json
import sys
import torch
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from ml.training.checkpoint import CheckpointManager
from ml.models.vocabulary import Vocabulary
from ml.compiler_interface import run_compiler
from ml.evaluation.benchmark_metrics import check_exact_repair, compare_ast_structures

checkpoint_path = Path('artifacts/training/checkpoints/best_checkpoint.pt')
model, _ = CheckpointManager.load_checkpoint(checkpoint_path, device=torch.device('cpu'))
model.eval()
vocab = Vocabulary()

test_path = Path('ml/dataset/generated/test.jsonl')
with open(test_path, 'r', encoding='utf-8') as f:
    samples = [json.loads(line) for line in f if line.strip()]

results = []
for i, s in enumerate(samples):
    corr_type = s.get('corruption_type')
    corrupted_src = s.get('corrupted_source')
    orig_src = s.get('original_source', '')
    gt_repair = s.get('repair', {})
    ctx = s.get('context', {})
    
    tokens_before = [vocab.encode_token(t) for t in ctx.get('tokens_before', [])]
    current_tok = [vocab.encode_token(ctx.get('current_token', {}))]
    tokens_after = [vocab.encode_token(t) for t in ctx.get('tokens_after', [])]
    
    err_pos = len(tokens_before)
    seq = tokens_before + current_tok + tokens_after
    seq_len = min(64, len(seq))
    seq = seq[:seq_len]
    pad_len = 64 - len(seq)
    
    input_ids = torch.tensor([seq + [vocab.pad_id]*pad_len], dtype=torch.long)
    attn_mask = torch.tensor([[1]*len(seq) + [0]*pad_len], dtype=torch.long)
    seg = [1]*len(tokens_before) + [2]*len(current_tok) + [3]*len(tokens_after)
    seg = seg[:64] + [0]*(64 - len(seq))
    seg_ids = torch.tensor([seg], dtype=torch.long)
    err_pos_tensor = torch.tensor([min(err_pos, 63)], dtype=torch.long)
    
    with torch.no_grad():
        out = model(input_ids, attn_mask, seg_ids, err_pos_tensor)
        op_idx = torch.argmax(out.operation_logits, dim=-1).item()
        tok_idx = torch.argmax(out.token_logits, dim=-1).item()
        pos_idx = torch.argmax(out.position_logits, dim=-1).item()
        
        op_name = ['INSERT_TOKEN', 'DELETE_TOKEN', 'REPLACE_TOKEN', 'NO_REPAIR'][op_idx]
        pred_token = vocab.id_to_token.get(tok_idx, '<UNK>')
    
    gt_op = gt_repair.get('operation')
    gt_tok = gt_repair.get('token')
    gt_pos = gt_repair.get('position')
    
    exact, op_m, tok_m, pos_m = check_exact_repair(op_name, pred_token, pos_idx, gt_op, gt_tok, gt_pos)
    
    # Run compiler in neural mode
    comp_ast_neural = run_compiler('--recovery=neural', '--dump-ast', input_text=corrupted_src)
    comp_exp_neural = run_compiler('--recovery=neural', '--explain-recovery', input_text=corrupted_src)
    
    # Run compiler in heuristic mode
    comp_ast_heur = run_compiler('--recovery=heuristic', '--dump-ast', input_text=corrupted_src)
    comp_exp_heur = run_compiler('--recovery=heuristic', '--explain-recovery', input_text=corrupted_src)

    # Run compiler in panic mode
    comp_ast_panic = run_compiler('--recovery=panic', '--dump-ast', input_text=corrupted_src)
    
    # Run compiler in none mode
    comp_ast_none = run_compiler('--recovery=none', '--dump-ast', input_text=corrupted_src)
    
    # Compare with original AST
    orig_ast_res = run_compiler('--dump-ast', input_text=orig_src)
    orig_ast = orig_ast_res.stdout.strip()
    
    ast_valid_neural = compare_ast_structures(comp_ast_neural.stdout.strip(), orig_ast)
    ast_valid_heur = compare_ast_structures(comp_ast_heur.stdout.strip(), orig_ast)
    ast_valid_panic = compare_ast_structures(comp_ast_panic.stdout.strip(), orig_ast)
    
    results.append({
        'i': i + 1,
        'corr_type': corr_type,
        'gt': f'{gt_op} {gt_tok} @{gt_pos}',
        'model_pred': f'{op_name} {pred_token} @{pos_idx}',
        'exact': exact,
        'op_m': op_m,
        'tok_m': tok_m,
        'pos_m': pos_m,
        'neural_has_ast': ('Program' in comp_ast_neural.stdout),
        'neural_ast_valid': ast_valid_neural,
        'heur_has_ast': ('Program' in comp_ast_heur.stdout),
        'heur_ast_valid': ast_valid_heur,
        'panic_has_ast': ('Program' in comp_ast_panic.stdout),
        'panic_ast_valid': ast_valid_panic,
        'none_has_ast': ('Program' in comp_ast_none.stdout),
    })

exact_count = sum(1 for r in results if r['exact'])
neural_ast_count = sum(1 for r in results if r['neural_has_ast'])
heur_ast_count = sum(1 for r in results if r['heur_has_ast'])
panic_ast_count = sum(1 for r in results if r['panic_has_ast'])
none_ast_count = sum(1 for r in results if r['none_has_ast'])

print(f"=== HELD-OUT TEST EVALUATION (N={len(results)}) ===")
print(f"PyTorch Model Exact Matches:      {exact_count} / {len(results)} ({exact_count/len(results)*100:.1f}%)")
print(f"Neural Mode Parse Success (AST): {neural_ast_count} / {len(results)} ({neural_ast_count/len(results)*100:.1f}%)")
print(f"Heuristic Mode Parse Success:     {heur_ast_count} / {len(results)} ({heur_ast_count/len(results)*100:.1f}%)")
print(f"Panic Mode Parse Success:         {panic_ast_count} / {len(results)} ({panic_ast_count/len(results)*100:.1f}%)")
print(f"None Mode Parse Success:          {none_ast_count} / {len(results)} ({none_ast_count/len(results)*100:.1f}%)")
print("=" * 120)

for r in results:
    print(f"#{r['i']:02d} | {r['corr_type']:22s} | GT: {r['gt']:25s} | Pred: {r['model_pred']:25s} | Exact: {str(r['exact']):5s} | Neural AST: {str(r['neural_has_ast']):5s} (Valid: {str(r['neural_ast_valid']):5s}) | Heur AST: {str(r['heur_has_ast']):5s} (Valid: {str(r['heur_ast_valid']):5s})")
