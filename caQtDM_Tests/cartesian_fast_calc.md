# Fast calc waveform in a Cartesian plot

Diagnostic for the existing numeric monitor/buffer path; contains no caScript.

Start the test IOC from the repository root:

```sh
softIoc -d caQtDM_Tests/IOC/cartesian_fast_calc.db
```

Then open `cartesian_fast_calc.ui`. Channel names are fixed:
`epics3://IPAFAST:counter` and `epics3://IPAFAST:wave`.
The calc counter is processed nominally every 0.1 seconds; its forward link
fills a compress circular buffer with 32 values.

**Expected:** the numeric display counts upwards; once filled, the plot shows
32 increasing values against their sample index. The curve is blue with points.
Select the direct update path in the `UpdateType` menu and check that updates
continue.

The nominal 10 Hz is not a separately measured display rate.
This panel exercises waveform display, not the caWaveTable write path.
