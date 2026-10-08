import argparse
from pathlib import Path

from ml.models.config import ModelConfig
from ml.models.repair_model import RepairModel
from ml.models.vocabulary import Vocabulary


def inspect_model(config_path: str = "ml/models/config.json"):
    path = Path(config_path)
    if path.exists():
        config = ModelConfig.load(path)
    else:
        config = ModelConfig()

    vocab = Vocabulary()
    model = RepairModel(config, vocabulary=vocab)
    size_info = model.get_model_size()

    print("=" * 60)
    print(" NeuroParse Neural Repair Model Architecture")
    print("=" * 60)
    print(f"Vocabulary Size:        {config.vocab_size} tokens")
    print(f"Supported Operations:   {config.num_operations} operations")
    print(f"Embedding Dimension:    {config.embedding_dim}")
    print(f"Transformer Layers:     {config.num_layers}")
    print(f"Attention Heads:        {config.num_heads}")
    print(f"Feed Forward Dim:       {config.feed_forward_dim}")
    print(f"Max Sequence Length:    {config.max_sequence_length}")
    print(f"Dropout Rate:           {config.dropout}")
    print("-" * 60)
    print(f"Total Parameters:       {size_info['total_parameters']:,}")
    print(f"Trainable Parameters:   {size_info['trainable_parameters']:,}")
    print(f"Approx FP32 Memory:     {size_info['fp32_memory_mb']} MB")
    print(f"Approx FP16 Memory:     {size_info['fp16_memory_mb']} MB")
    print("=" * 60)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Inspect NeuroParse Neural Repair Model")
    parser.add_argument("--config", default="ml/models/config.json", help="Path to config.json")
    args = parser.parse_args()
    inspect_model(args.config)
