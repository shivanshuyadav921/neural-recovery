import json
from pathlib import Path

import pytest
import torch

from ml.models.config import ModelConfig
from ml.models.dataset_adapter import NeuroLangDataset
from ml.models.repair_model import RepairModel
from ml.models.vocabulary import Vocabulary
from ml.training.checkpoint import CheckpointManager
from ml.training.dataset_stats import DatasetAnalyzer
from ml.training.metrics import (
    compute_classification_metrics,
    evaluate_predictions,
)
from ml.training.trainer import Trainer


@pytest.fixture
def dataset_dir():
    return Path("ml/dataset/generated")


def test_dataset_analyzer_stats(dataset_dir):
    if not dataset_dir.exists():
        pytest.skip("Generated dataset not found")

    report = DatasetAnalyzer.analyze_dataset_dir(dataset_dir)
    assert report.total_samples > 0
    assert report.is_leakage_free is True
    assert len(report.leakage_errors) == 0
    assert report.train_stats.num_samples > 0
    assert report.validation_stats.num_samples > 0
    assert report.test_stats.num_samples > 0


def test_classification_metrics():
    y_true = [0, 1, 2, 0, 1, 2]
    y_pred = [0, 1, 1, 0, 1, 2]
    classes = ["A", "B", "C"]

    report = compute_classification_metrics(y_true, y_pred, classes)
    assert 0.0 <= report.accuracy <= 1.0
    assert report.accuracy == pytest.approx(5 / 6, abs=1e-3)
    assert "A" in report.per_class
    assert report.per_class["A"]["precision"] == 1.0
    assert report.per_class["A"]["recall"] == 1.0


def test_evaluate_predictions_exact_repair():
    vocab = Vocabulary()
    insert_id = vocab.op_to_id["INSERT_TOKEN"]
    delete_id = vocab.op_to_id["DELETE_TOKEN"]
    semi_id = vocab.token_to_id[";"]

    # Sample 1: Perfect match (INSERT ;)
    # Sample 2: Wrong token (INSERT ) instead of ;)
    # Sample 3: Perfect match (DELETE at pos 2)
    target_ops = [insert_id, insert_id, delete_id]
    pred_ops = [insert_id, insert_id, delete_id]

    target_tokens = [semi_id, semi_id, vocab.pad_id]
    pred_tokens = [semi_id, vocab.token_to_id[")"], vocab.pad_id]

    target_pos = [10, 10, 2]
    pred_pos = [10, 10, 2]

    confidences = [0.95, 0.80, 0.99]
    corruptions = ["missing_semicolon", "missing_semicolon", "unexpected_token"]

    results = evaluate_predictions(
        target_ops=target_ops,
        pred_ops=pred_ops,
        target_tokens=target_tokens,
        pred_tokens=pred_tokens,
        target_positions=target_pos,
        pred_positions=pred_pos,
        confidences=confidences,
        corruptions=corruptions,
        vocabulary=vocab,
    )

    assert results.num_samples == 3
    assert results.operation_report.accuracy == 1.0
    assert results.token_accuracy == 0.5  # 1 out of 2 applicable token predictions correct
    assert results.position_accuracy == 1.0
    assert results.exact_repair_accuracy == pytest.approx(2 / 3, abs=1e-3)
    assert results.invalid_prediction_rate == 0.0


def test_checkpoint_manager_save_and_load(tmp_path):
    config = ModelConfig(embedding_dim=32, num_heads=4, max_sequence_length=32)
    model = RepairModel(config)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)

    mgr = CheckpointManager(tmp_path)
    saved_path = mgr.save_checkpoint(
        model=model,
        optimizer=optimizer,
        epoch=3,
        best_metric=0.85,
        is_best=True,
    )

    assert saved_path.exists()
    assert (tmp_path / "best_checkpoint.pt").exists()
    assert (tmp_path / "latest_checkpoint.pt").exists()

    loaded_model, chk_data = CheckpointManager.load_checkpoint(saved_path)
    assert chk_data["epoch"] == 3
    assert chk_data["best_metric"] == 0.85
    assert loaded_model.config.embedding_dim == 32


def test_trainer_tiny_overfit(dataset_dir, tmp_path):
    if not dataset_dir.exists():
        pytest.skip("Generated dataset not found")

    vocab = Vocabulary()
    config = ModelConfig(
        vocab_size=vocab.size,
        embedding_dim=64,
        num_layers=2,
        num_heads=4,
        feed_forward_dim=128,
        dropout=0.0,
        max_sequence_length=64,
        num_operations=vocab.num_operations,
        num_token_types=vocab.size,
    )

    train_file = dataset_dir / "train.jsonl"
    full_dataset = NeuroLangDataset(train_file, config=config, vocabulary=vocab)

    # Pick 8 samples
    small_samples = full_dataset.samples[:8]
    small_dataset = NeuroLangDataset(small_samples, config=config, vocabulary=vocab)

    model = RepairModel(config, vocabulary=vocab)
    trainer = Trainer(
        model=model,
        train_dataset=small_dataset,
        val_dataset=small_dataset,
        output_dir=tmp_path,
        learning_rate=0.005,
        epochs=30,
        batch_size=8,
        early_stopping_patience=0,
        seed=42,
    )

    train_res = trainer.train()
    # After 30 steps on 8 samples, model should memorize the batch
    eval_res = trainer.evaluate(small_dataset)
    assert eval_res.operation_report.accuracy >= 0.85
    assert eval_res.exact_repair_accuracy >= 0.75
