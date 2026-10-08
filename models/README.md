# Models directory

Checkpoints and ONNX files are produced by training/export, not committed as fake binaries.

If this directory has no `.onnx` file, native neural recovery must report Unavailable and use deterministic fallback.
