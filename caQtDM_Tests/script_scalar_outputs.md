# EPICS score × Internal factor

| Alias | Channel | Use |
|---|---|---|
| `score` | `epics3://PACMAN:PACMAN_USER_SCORE` | Score. |
| `factor` | `internal://scriptFactor` | Editable Double factor, initially 2. |
| `freq` | `SWISSFEL-STATUS:Bunch-1-Exp-Freq-RB` | Additional metadata output. |
| `result` (output) | `internal://scriptScaledScore` | Existing Double target, initially 0. |

The current configuration uses **DataDriven / AnyInput**. All three inputs must
be available; the frequency channel can also trigger execution. It is not used
in the calculation, but is logged with its connection state, units, precision,
status and severity.

Check: a score of 100 and factor of 2 must produce 200. Change the factor to 3
and confirm with Enter: with the score unchanged, the result must automatically
become 300. Further score changes must update the product. The debug display
shows the last Internal channel written and its value.

The button is also connected to `executeScript()`. Select **Manual** in Designer
for a button-only test; the existing button instructions refer to that earlier
manual test case.

To run with only the Pacman IOC and Internal plugin, remove both `freq` and its
`console.log()` block from a copy of the panel.
