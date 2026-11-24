# caScript test and diagnostic panels

| Panel | Purpose | Notes |
|---|---|---|
| `script_scalar_outputs.ui` | EPICS score × Internal factor; metadata. | [Expected values and inputs](script_scalar_outputs.md) |
| `script_mandelbrot.ui` | Render an Apfelmännchen through an internal Char waveform and caCamera. | [Launch and expected image](#mandelbrot) |
| `script_median.ui` | Median and count above a threshold. | [Test data and waveform limitation](script_median.md) |
| `script_conversions.ui` | Seven-by-seven scalar conversion matrix, decoding and array rejection in tabs. | [Inputs and expected results](script_conversions.md) |
| `script_console_conversions.ui` | Scalar conversions and bounded console flooding. | [Expected results and timeout checks](script_console_conversions.md) |
| `script_pacman_control.ui` | Direction selection using ghost distances. | [Macros and controls](script_pacman_control.md) |
| `script_string_encoding.ui` | ASCII, UTF-8 and Latin-1 display; no caScript. | [Bytes and expected displays](script_string_encoding.md) |
| `cartesian_fast_calc.ui` | Existing Cartesian buffer path; no caScript. | [IOC data and expected behavior](cartesian_fast_calc.md) |

Each guide describes the panel's inputs, controls, expected results and
limitations.


## Mandelbrot

From the repository root, open the panel in the desktop viewer.

```sh
caQtDM -noMsg "$PWD/caQtDM_Tests/script_mandelbrot.ui"
```

Click **Render Mandelbrot**. The camera shows the dark Mandelbrot set
against a red-to-yellow exterior. caScript reports
`internal://mandelbrotPixels = 49152 samples`. Click again to reproduce the
same frame. The panel defines its own internal width, height, and pixel
channels and requires no IOC.
