# caScript conversion matrix

`script_conversions.ui` fits all checks into one window using a QTabWidget.
No IOC is required. Select a tab, confirm input edits with Enter, then press
its Run button. Results appear as `JavaScript type: value` or a caught error.
All scripts use Manual triggers and DataDriven execution. Each conversion
has its own catch, so an invalid target does not hide the other results.

## Scalar tabs

Seven tabs supply String, Short (`genSoftPV::Int`), Long, Char, Enum, Float
and Double inputs. Each calls `convert()` for all seven target types: 49
source/target combinations. Enum conversion concerns its numeric value,
not an enum label. Result channels are String channels for displaying both
values and errors; they do not test writing to each EPICS destination type.

| Source / input | Expected results |
|---|---|
| String `42.5`, Double `42.5` | Short, Long, Char, Enum: `number: 43`; Float and Double: `number: 42.5`; String: `string: 42.5` |
| Other scalar tabs, initial `42` | All numeric targets: `number: 42`; String: `string: 42` |
| String `abc` | String preserves `abc`; all six numeric targets show RangeError |
| Double `-1.5` | Short and Long: `number: -2`; Char and Enum reject the negative result |
| Double `255.5` | Char rejects rounded 256 |
| Double `32768` | Short rejects overflow; Long remains valid |
| Double `65536` | Enum and Short reject overflow; Long remains valid |
| Double `2147483648` | Long rejects overflow |
| Double `1e39` | Float and integer targets reject overflow; Double remains valid |

Integer limits: Short −32768…32767, Long −2147483648…2147483647,
Char 0…255, Enum 0…65535. Halfway cases round away from zero.
Use the Double tab for boundary inputs so the source type does not truncate
or reject the value before caScript receives it. A Float input has already
undergone source-side Float precision loss; Double conversion cannot restore it.
The source snapshot is logged after conversions to check it remains unchanged.

## Bytes / text

The fixed sources are seven-byte UTF-8 and six-byte Latin-1 Char waveforms.
Explicit numeric bytes avoid any source-side String encoding conversion.
The tab displays raw bytes, `fromUtf8()` and `fromLatin1()` for each source.

| Source | Expected bytes | Correct decoder |
|---|---|---|
| UTF-8 | `47 72 c3 bc 65 7a 69` | `fromUtf8()` → `Grüezi` |
| Latin-1 | `47 72 fc 65 7a 69` | `fromLatin1()` → `Grüezi` |

Decoder results are deliberately written directly to Internal String outputs.
This also exposes the encoding limits of the output/display path:

- Latin-1 / fromUtf8 produces a replacement character. Expect a warning
  `value cannot be represented by the internal Latin-1 string channel`.
  That output keeps its previous value (`Not run` on a fresh load).
- Other decoded strings can be distorted again by Latin-1 publication followed
  by UTF-8 display decoding. A visually correct string alone does not prove
  the selected input decoder was correct.
- The raw hex rows are the reference for the received bytes. The decoder
  expectations in the table concern the JavaScript strings before publication.

This behavior was observed in the manual Viewer run. The warning is intentional
for this diagnostic case; the panel does not escape or hide it.

## Arrays / matrix

Fixed Double waveform `[1,2,3]`, Double matrix `[[1,2],[3,4]]` and String
waveform `["one","two"]` are displayed alongside the rejection count.
Each must report `7/7 rejected`: `convert()` is scalar-only. Processing
individual elements requires script logic. This panel does not edit waveforms.

The XML and embedded scripts have been checked separately. The Bytes and Arrays / matrix tabs have been checked manually. The array
values and all three 7/7 rejection counts matched the expectations. The seven scalar tabs were also manually tested and confirmed working.
The individual boundary cases are covered by the separate script check;
no separate manual result is recorded for each boundary value.
