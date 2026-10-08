import random
from pathlib import Path
from typing import Optional

import torch
import torch.optim as optim

from ml.models.config import ModelConfig
from ml.models.dataset_adapter import NeuroLangDataset
from ml.models.repair_model import RepairModel
from ml.models.vocabulary import Vocabulary


def run_sanity_check(dataset_path: Optional[Path] = None) -> bool:
    print("=" * 60)
    print(" [MODEL SANITY CHECK] - Neural Repair Model Verification")
    print("=" * 60)

    # Set seeds for deterministic verification
    torch.manual_seed(42)
    random.seed(42)

    vocab = Vocabulary()
    config = ModelConfig(
        vocab_size=vocab.size,
        embedding_dim=64,
        num_layers=2,
        num_heads=4,
        feed_forward_dim=128,
        dropout=0.0,  # 0.0 dropout for deterministic overfit check
        max_sequence_length=64,
        num_operations=vocab.num_operations,
        num_token_types=vocab.size,
    )

    if dataset_path is None:
        dataset_path = Path("ml/dataset/generated/train.jsonl")

    if not dataset_path.exists():
        raise FileNotFoundError(f"Dataset path '{dataset_path}' not found.")

    dataset = NeuroLangDataset(dataset_path, config=config, vocabulary=vocab)
    print(f"Loaded {len(dataset)} sample(s) from '{dataset_path}'.")

    # Take a small batch of 8 samples
    small_subset = [dataset[i] for i in range(min(8, len(dataset)))]
    batch = NeuroLangDataset.collate_fn(small_subset)

    # Model initialization
    model = RepairModel(config, vocabulary=vocab)
    model.train()

    size_info = model.get_model_size()
    print(f"Model Architecture: Transformer ({config.num_layers} layers, {config.num_heads} heads, d_model={config.embedding_dim})")
    print(f"Total Parameters: {size_info['total_parameters']:,}")
    print(f"Trainable Parameters: {size_info['trainable_parameters']:,}")
    print(f"Approximate FP32 Memory: {size_info['fp32_memory_mb']} MB")

    optimizer = optim.Adam(model.parameters(), lr=0.005)

    # Run 25 steps to verify convergence on this tiny batch
    initial_loss = None
    final_loss = None

    for step in range(25):
        optimizer.zero_grad()
        output = model(
            input_ids=batch["input_ids"],
            attention_mask=batch["attention_mask"],
            segment_ids=batch["segment_ids"],
            error_positions=batch["error_positions"],
            target_operations=batch["target_operations"],
            target_tokens=batch["target_tokens"],
            target_positions=batch["target_positions"],
        )

        loss = output.loss
        assert loss is not None, "Model failed to return loss when targets provided"
        loss_val = loss.item()

        if initial_loss is None:
            initial_loss = loss_val

        assert not torch.isnan(loss), f"NaN loss encountered at step {step}"
        assert not torch.isinf(loss), f"Inf loss encountered at step {step}"

        loss.backward()

        # Check gradient existence
        has_grads = any(p.grad is not None and p.grad.norm().item() > 0 for p in model.parameters())
        assert has_grads, "No gradients propagated to model parameters"

        optimizer.step()
        final_loss = loss_val

        if step % 5 == 0 or step == 24:
            print(f"  Step {step:02d}: Loss = {loss_val:.4f} (Op: {output.loss_dict['operation_loss']:.4f}, Tok: {output.loss_dict.get('token_loss', 0.0):.4f})")

    print("-" * 60)
    print(f"Initial Loss: {initial_loss:.4f} -> Final Loss: {final_loss:.4f}")
    assert final_loss < initial_loss, f"Sanity check failed: loss did not decrease ({initial_loss} -> {final_loss})"

    print("\n[MODEL SANITY CHECK] SUCCESS: Forward pass, backward pass, and parameter updates verified.")
    print("=" * 60)
    return True


if __name__ == "__main__":
    run_sanity_check()
