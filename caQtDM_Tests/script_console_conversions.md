# Scalar conversions and console flood

No IOC required. genSoftPV defines a Double input (`scriptConsoleSource`,
initially 42.5) and seven result channels. All three scripts use
**DataDriven / Manual** and separate buttons connected to `executeScript()`.
Confirm input edits with Enter.

Use a build containing the console-message limit before running the endless
case. Older builds can become unresponsive from the unbounded log queue.

The fixed allowance is **100 console messages per script execution and widget**,
shared by `console.log`, `console.warn` and `console.error`. It resets at the
start of each execution; it is not a 100 ms time window. The first call beyond
the allowance emits one suppression warning. Remaining console calls are
discarded for that run, without stopping the script or invalidating outputs.
Runtime warnings and timeout errors remain visible independently of this limit.

1. Click **Run conversions**. At input 42.5, expect:

   | Conversion | Result |
   |---|---|
   | Short, Long, Char, Enum | 43 |
   | Float, Double | 42.5 |
   | String | `42.5` |

   The original input remains 42.5. Numeric result PVs are deliberately Double:
   the script performs each conversion; the test does not change PV types.

2. Set the input to 32768, confirm, and run conversions. Expect a Short-range
   error and unchanged results from the preceding successful run. Set the input
   back to 42.5 for the remaining steps.

3. Click **1000 logs + conversions**. The loop calls `convert(EpicsType.Double)`
   and alternates `console.log`, `console.warn` and `console.error`.
   Expect 100 script-console messages in total and one suppression warning.
   A subsequent click starts a fresh message allowance. A runtime warning is
   also possible if the run takes more than 100 ms on the test machine.

4. Click **Endless logs + conversions**. Each iteration converts the input to
   String and logs it. Expect 100 console messages, one suppression warning,
   the 100 ms runtime warning, and interruption at the 1000 ms limit. While it
   runs, click the finite button: its separate script should still execute and
   the viewer should remain responsive.

5. Start the endless script five times, waiting for each timeout. Expect the
   distinct disabled message after the fifth consecutive timeout. Further
   clicks on that button do nothing; the other scripts remain usable. Reload
   the panel to enable the endless script again.

The flood scripts have no outputs and leave the displayed conversion results
unchanged. Check both the message window and terminal for diagnostics.
