# EPICS 3 string encoding

Diagnostic panel with three caLineEdits; contains no caScript.

Use the existing `IOC/caQtDM_otherPV.template` with `LBL=IPAENC`.
The newer parser used for this test requires quoting the INP link
`$(LBL):TEST:AO` in a temporary copy. Leave the original template unchanged.

| PV | Sent bytes (hex) | Expected display in the tested path |
|---|---|---|
| `IPAENC:TEST:MBBO.DESC` | `41 42 43` | `ABC` |
| `IPAENC:TEST1:MBBO.DESC` | `47 72 C3 BC 65 7A 69` | `Grüezi` |
| `IPAENC:COM:2.DESC` | `47 72 FC 65 7A 69` | `Gr�ezi` |

Write the bytes explicitly as ASCII, UTF-8 and Latin-1, respectively.
Terminal encoding must not accidentally make the two umlaut cases identical.
Compare the three panel displays.

The replacement character for FC reflects the GUI decoding path. Compare
received bytes separately before drawing conclusions about transport loss.

The caScript methods `rawBytes`, `fromLatin1()` and `fromUtf8()` are not
tested by this panel.
