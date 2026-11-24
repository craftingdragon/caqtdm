# caScript test and diagnostic panels

| Panel | Purpose | Notes |
|---|---|---|
| [`script_debug.ui`](script_debug.ui) | Compare all four caScript display modes without an IOC. | [Scenarios and expected results](#debug-display) |
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

## Debug display

From the repository root, open the panel with `caQtDM "$PWD/caQtDM_Tests/script_debug.ui"`.
Select a scenario to run the four scripts or click **Run Again** to repeat it.

| Scenario | What to look for |
|---|---|
| Success | LastOutput shows 42, Status shows Succeeded, and Diagnostics shows an info message. |
| Warning | LastOutput changes to 84, Status shows Succeeded, and Diagnostics shows a warning. |
| Script Error | Status shows Failed, Diagnostics reports the exception, and LastOutput retains the last successful write. |
| Rejected Write | Status shows Succeeded, Diagnostics explains why the write was rejected, and LastOutput retains the last successful write. |
| Timeout | Status shows Running and then Timed out. After five consecutive timeouts it shows Disabled. Reload the panel to start again. |

Hidden stays blank in the viewer. In Qt Designer, select any of the four blue
caScript widgets to inspect its `displayMode` property.

## Mandelbrot

From the repository root, open the panel in the desktop viewer.

```sh
caQtDM -noMsg "$PWD/caQtDM_Tests/script_mandelbrot.ui"
```

The first image renders automatically. Change **Maximale Iterationen** or
choose **Klassisch**, **Feuer**, or **Graustufen** to redraw it. Select
**256 × 192**, **640 × 480**, or **1280 × 960** under **Auflösung** to change
the render size. The larger sizes take longer to calculate. Click a feature
in the image to center the view on it, then use **Zoom** to magnify it. You
can also enter **Realteil** and **Imaginärteil** to set the center precisely.
For example, use 64× and enter `-0.743644` and
`0.131826` to explore a boundary. Press Enter to apply each coordinate.

**Ansicht zurücksetzen** restores the original center and 1× zoom. caScript
computes each frame at the selected size and publishes it as an internal RGB Char
waveform using a packed JavaScript `ArrayBuffer`. Palette changes reuse the
calculated escape intensities. caCamera holds the previous frame while a new
one is calculated.
The panel requires no IOC.
