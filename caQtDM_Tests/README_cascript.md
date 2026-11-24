# caScript test and diagnostic panels

| Panel | Purpose | Notes |
|---|---|---|
| `script_scalar_outputs.ui` | EPICS score × Internal factor; metadata. | [Expected values and inputs](script_scalar_outputs.md) |
| `script_mandelbrot.ui` | Explore the Apfelmännchen with live rendering, palettes, zoom, and pan. | [Launch and expected image](#mandelbrot) |
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

The first image renders automatically. Change **Maximale Iterationen** or
choose **Klassisch**, **Feuer**, or **Graustufen** to redraw it. Use **Zoom**
to magnify the fractal and enter new **Realteil** or **Imaginärteil** values
to move the center. For example, use 64× and enter `-0.743644` and
`0.131826` to explore a boundary. Press Enter to apply each coordinate.

**Ansicht zurücksetzen** restores the original center and 1× zoom. caScript
computes each 256 × 192 frame and publishes it as an internal RGB Char
waveform using a packed JavaScript `ArrayBuffer`. Palette changes reuse the
calculated escape intensities. caCamera holds the previous frame while a new
one is calculated.
The panel requires no IOC.
