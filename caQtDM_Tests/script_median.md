# Median and threshold count

No IOC required: four genSoftPV widgets provide the samples, threshold and two
results. **DataDriven / Manual**; the button starts the script.

| Role | Internal channel | Initial value |
|---|---|---|
| `samples` | `scriptSamples` | Double waveform [1,9,3,7,5], NELM/NORD 5. |
| `threshold` | `scriptThreshold` | Double 5. |
| `median` | `scriptMedian` | Double 0. |
| `aboveThreshold` | `scriptAboveThreshold` | Long 0. |

Edit the table values and NORD, confirm each change with Enter, then click
**Skript ausführen** (execute script). If needed, first increase NORD to 5
to make all cells editable.

| Used values | NORD | Threshold | Median | Count strictly above |
|---|---:|---:|---:|---:|
| [1,9,3,7,5] | 5 | 5 | 5 | 2 |
| [8,2,6,4] | 4 | 5 | 5 | 2 |
| [5] | 1 | 5 | 5 | 0 |
| [1,9,3,7,5] | 5 | 7 | 5 | 1 |

After a successful run, set NORD to 0. Expect `Die Waveform ist leer.`
(the waveform is empty) and unchanged results.

**Known limitation:** editing an Internal waveform requires a build with the
caWaveTable write-lock fix. Without that fix, editing the table can deadlock
when the Internal plugin publishes an update synchronously.
