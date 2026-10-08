import random
from collections import defaultdict
from typing import Dict, List, Tuple

from ml.dataset.schema import DatasetSample


class DatasetSplitter:
    def __init__(
        self,
        train_ratio: float = 0.8,
        validation_ratio: float = 0.1,
        test_ratio: float = 0.1,
        seed: int = 42,
    ):
        total = train_ratio + validation_ratio + test_ratio
        if abs(total - 1.0) > 1e-5:
            raise ValueError(f"Split ratios must sum to 1.0 (got {total})")

        self.train_ratio = train_ratio
        self.validation_ratio = validation_ratio
        self.test_ratio = test_ratio
        self.seed = seed

    def split(
        self, samples: List[DatasetSample]
    ) -> Tuple[List[DatasetSample], List[DatasetSample], List[DatasetSample]]:
        if not samples:
            return [], [], []

        # Group samples by source_file to prevent data leakage
        file_groups: Dict[str, List[DatasetSample]] = defaultdict(list)
        for s in samples:
            file_groups[s.source_file].append(s)

        files = sorted(list(file_groups.keys()))
        rng = random.Random(self.seed)
        rng.shuffle(files)

        n_files = len(files)
        if n_files <= 1:
            # If only 1 file is available, place in train
            return samples, [], []

        if n_files == 2:
            # 2 files -> 1 train, 1 test
            train_files = files[:1]
            test_files = files[1:]
            val_files = []
        else:
            n_train = max(1, int(round(n_files * self.train_ratio)))
            n_val = int(round(n_files * self.validation_ratio))
            if n_train + n_val >= n_files:
                n_train = n_files - 2
                n_val = 1

            train_files = files[:n_train]
            val_files = files[n_train : n_train + n_val]
            test_files = files[n_train + n_val :]

            # Guarantee non-empty test if >= 3 files
            if not test_files and len(train_files) > 1:
                test_files = [train_files.pop()]

        train_samples = []
        for f in train_files:
            train_samples.extend(file_groups[f])

        val_samples = []
        for f in val_files:
            val_samples.extend(file_groups[f])

        test_samples = []
        for f in test_files:
            test_samples.extend(file_groups[f])

        return train_samples, val_samples, test_samples
