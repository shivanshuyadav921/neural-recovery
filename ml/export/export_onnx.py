import argparse
import sys
from pathlib import Path

if sys.stdout and hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
if sys.stderr and hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8")

import torch

from ml.export.onnx_exporter import (
    export_model_to_onnx,
    verify_onnx_numerical_parity,
)
from ml.training.checkpoint import CheckpointManager


def main():
    parser = argparse.ArgumentParser(description="Export NeuroParse PyTorch Model to ONNX")
    parser.add_argument(
        "--checkpoint",
        type=str,
        default="artifacts/training/checkpoints/best_checkpoint.pt",
        help="Path to trained PyTorch .pt checkpoint",
    )
    parser.add_argument(
        "--output",
        type=str,
        default="artifacts/onnx/repair_model.onnx",
        help="Target path for the exported .onnx model",
    )
    parser.add_argument(
        "--opset",
        type=int,
        default=18,
        help="ONNX opset version (default: 18)",
    )
    parser.add_argument(
        "--no-verify",
        action="store_true",
        help="Skip numerical equivalence verification with ONNX Runtime",
    )
    parser.add_argument(
        "--atol",
        type=float,
        default=1e-4,
        help="Absolute numerical tolerance threshold",
    )
    parser.add_argument(
        "--rtol",
        type=float,
        default=1e-4,
        help="Relative numerical tolerance threshold",
    )

    args = parser.parse_args()
    chk_path = Path(args.checkpoint)
    out_path = Path(args.output)

    print("=" * 60)
    print(" NeuroParse ONNX Model Export & Verification (Phase 11)")
    print("=" * 60)
    print(f"Source Checkpoint:     {chk_path}")
    print(f"Target ONNX Model:     {out_path}")
    print(f"Selected Opset:        {args.opset}")
    print("-" * 60)

    if not chk_path.exists():
        print(f"[ERROR] Checkpoint file '{chk_path}' not found.")
        sys.exit(1)

    # 1. Export model
    print("Exporting model to ONNX...")
    exported_path = export_model_to_onnx(
        checkpoint_path=chk_path,
        output_onnx_path=out_path,
        opset_version=args.opset,
        dynamic_axes=True,
        verbose=True,
    )

    file_size_kb = exported_path.stat().st_size / 1024
    print(f"Exported ONNX file size: {file_size_kb:.2f} KB ({exported_path.stat().st_size:,} bytes)")

    # 2. Verify Parity
    if not args.no_verify:
        print("\nVerifying numerical equivalence with ONNX Runtime...")
        model, _ = CheckpointManager.load_checkpoint(chk_path, device=torch.device("cpu"))
        report = verify_onnx_numerical_parity(
            model=model,
            onnx_path=exported_path,
            atol=args.atol,
            rtol=args.rtol,
        )

        print("-" * 60)
        print(f"Numerical Equivalence:  {'[PASSED]' if report.is_equivalent else '[FAILED]'}")
        print(f"Cases Tested:           {report.num_cases_tested}")
        print(f"Max Op Abs Difference:  {report.max_op_abs_diff:.7f}")
        print(f"Max Tok Abs Difference: {report.max_tok_abs_diff:.7f}")
        print(f"Max Pos Abs Difference: {report.max_pos_abs_diff:.7f}")
        print(f"All Predictions Match:  {report.all_predictions_match}")
        print("-" * 60)
        print("Per-Case Breakdown:")
        for res in report.case_results:
            print(
                f"  {res.case_name:16s} (B={res.batch_size}, L={res.seq_len:3d}) | "
                f"Op Diff: {res.op_max_abs_diff:.6f}, Tok Diff: {res.tok_max_abs_diff:.6f}, Pos Diff: {res.pos_max_abs_diff:.6f} | "
                f"Match: {res.predictions_match}"
            )
        print("=" * 60)

        if not report.is_equivalent:
            print("[ERROR] ONNX model failed numerical parity validation.")
            sys.exit(1)

    print(f"\n[SUCCESS] ONNX model ready at '{out_path}'.")


if __name__ == "__main__":
    main()
