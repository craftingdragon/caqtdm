# caScript test and diagnostic panels

| Panel | Purpose | Notes |
|---|---|---|
| `script_scalar_outputs.ui` | EPICS score × Internal factor; metadata. | [Expected values and inputs](script_scalar_outputs.md) |
| `script_median.ui` | Median and count above a threshold. | [Test data and waveform limitation](script_median.md) |
| `script_console_conversions.ui` | Scalar conversions and bounded console flooding. | [Expected results and timeout checks](script_console_conversions.md) |
| `script_pacman_control.ui` | Direction selection using ghost distances. | [Macros and controls](script_pacman_control.md) |
| `script_string_encoding.ui` | ASCII, UTF-8 and Latin-1 display; no caScript. | [Bytes and expected displays](script_string_encoding.md) |
| `cartesian_fast_calc.ui` | Existing Cartesian buffer path; no caScript. | [IOC data and expected behavior](cartesian_fast_calc.md) |

Each guide describes the panel's inputs, controls, expected results and
limitations.
