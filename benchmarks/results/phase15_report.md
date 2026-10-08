# NeuroParse Phase 15 Experimental Benchmark Results

**Date/Time:** 2026-10-08T16:14:19Z
**Dataset:** C:\Users\shiva\OneDrive\Desktop\neurology\ml\dataset\generated\test.jsonl (30 held-out samples)
**Hardware Platform:** AMD64 (Intel64 Family 6 Model 140 Stepping 2, GenuineIntel)
**Operating System:** Windows-11-10.0.26200-SP0
**Compiler Build:** 0.1.0

### Table 1 — Overall Recovery Comparison
| System / Strategy | Samples | Parse Success | Recovery Success | AST Valid | Remaining Errors | Fallback Rate |
|---|---|---|---|---|---|---|
| **None** | 30 | 0 (0.0%) | 0 (0.0%) | 1 (3.3%) | 35 | 0.0% |
| **Panic** | 30 | 18 (60.0%) | 26 (86.7%) | 9 (30.0%) | 13 | 0.0% |
| **Heuristic** | 30 | 21 (70.0%) | 30 (100.0%) | 19 (63.3%) | 14 | 0.0% |
| **Neural** | 30 | 29 (96.7%) | 30 (100.0%) | 16 (53.3%) | 2 | 0.0% |
| **Auto** | 30 | 29 (96.7%) | 30 (100.0%) | 16 (53.3%) | 2 | 0.0% |

### Table 2 — Per-Corruption-Type Recovery & Exact Repair
| Corruption Type | Samples | Baseline (Heuristic) | NeuroParse (Neural) | Exact Repair (Neural) |
|---|---|---|---|---|
| `missing_semicolon` | 6 | 100.0% | **100.0%** | 100.0% |
| `missing_closing_paren` | 4 | 100.0% | **100.0%** | 100.0% |
| `missing_closing_brace` | 4 | 100.0% | **100.0%** | 0.0% |
| `missing_comma` | 2 | 100.0% | **100.0%** | 100.0% |
| `replace_delimiter` | 8 | 100.0% | **100.0%** | 0.0% |
| `unexpected_token` | 6 | 100.0% | **100.0%** | 0.0% |

### Table 3 — Compiler Execution & Recovery Latency
| System | Compiler Mean (µs) | Median (µs) | P95 (µs) | Recovery Mean (µs) | Overhead vs Baseline |
|---|---|---|---|---|---|
| **None** | 20090.94 | 17830.1 | 46401.0 | 0.0 | +9.8% |
| **Panic** | 17715.29 | 16990.2 | 24058.0 | 0.0 | -3.2% |
| **Heuristic** | 18296.91 | 17195.7 | 24676.1 | 0.0 | 0.0% (ref) |
| **Neural** | 18767.56 | 18173.75 | 28315.4 | 0.0 | +2.6% |
| **Auto** | 17569.53 | 16802.4 | 24469.7 | 0.0 | -4.0% |

### Table 4 — Neural Recovery Pipeline Statistics
| System | Total Proposals | Accepted | Rejected | Applied | Successful | Success given Accepted |
|---|---|---|---|---|---|---|
| **Neural** | 30 | 30 (100.0%) | 0 | 30 | 29 | 96.7% |
| **Auto** | 30 | 30 (100.0%) | 0 | 30 | 29 | 96.7% |

### Neural Inference Engine Latency (ONNX Runtime CPU)
- **Cold Start Latency (Session Init + 1st inference):** 68778.8 µs
- **Warm Inference Mean:** 919.21 µs
- **Warm Inference Median:** 883.25 µs
- **Warm Inference P95:** 1197.0 µs
- **Confidence Distribution:** {'low_conf (<0.60)': 0, 'med_conf (0.60-0.85)': 9, 'high_conf (>=0.85)': 21, 'mean_confidence': 0.8817}

### Dataset Leakage Audit
- **Train Originals:** 4 programs (110 samples)
- **Validation Originals:** 1 programs (36 samples)
- **Test Originals:** 1 programs (30 samples)
- **Train-Test Overlap:** []
- **Leakage-Free Verified:** **YES (0 overlap)**
