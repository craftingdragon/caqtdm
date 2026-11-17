# caScript Pacman control

Test source: [epics_pacman](https://github.com/craftingdragon/epics_pacman).
Open the panel with `-macro "P=PACMAN"`; adjust `P` for a different
IOC prefix.
All required board, position, ghost, power-pellet and fright-timer channels
must be available.
**DataDriven / Periodic**, interval 100 ms.

## Procedure and expected behavior

1. On opening the panel, `Enabled` must be 0 and the requested direction -1.
   No direction 0–3 may be sent before enabling control. Use the
   **Enable control** toggle to set `Enabled` to 1.
2. Observe movement in the game panel. At tile centers, the script explores
   less-visited paths. Near SCATTER/CHASE ghosts, it favors escape distance
   and checks corridors for traps. It seeks available power pellets and hunts
   FRIGHT ghosts while `GAME_FRIGHT_EXPIRATION_TIMER` is 1. When that timer
   becomes 0, it treats FRIGHT ghosts as threats again. SPAWN and WAIT are ignored.
3. Compare the debug display, `Requested direction` and, after acceptance,
   `Actual direction`: 0 right, 1 left, 2 up, 3 down.
4. Switch the toggle off: the requested direction must become -1 (no requested
   turn). This also clears its visit history, but does not stop IOC-controlled
   movement. While disabled, the script keeps its direction output at -1.

Data path: caScript → existing Internal direction channel → caCalc (`A`) →
`emitSignal(double)` → caTextEntry `setValue(double)` → EPICS.
Both the direction-defining `genSoftPV.value` and `caCalc.initialValue` are -1.
The enabled flag uses `genSoftPV.value = "0"`. Both Internal sources explicitly
use Constant mode with persistence disabled, so no counter or saved initial
value starts the game. The bridge may send -1 during initialization.
caScript itself creates no PV and writes only to the Internal target.

## Power pellets and hunting

| Input | IOC channel | Board tile (x, y) |
|---|---|---|
| `power0` | `$(P):VIS_3_2` | (1, 3) |
| `power1` | `$(P):VIS_8_2` | (26, 3) |
| `power2` | `$(P):VIS_19_6` | (1, 23) |
| `power3` | `$(P):VIS_10_7` | (26, 23) |

A VIS value of 2 means the power pellet is available; 0 means consumed.
These channel suffixes are pellet identifiers, not board coordinates.
The `frightTimer` input reads `$(P):GAME_FRIGHT_EXPIRATION_TIMER`: it is a
boolean phase indicator, not a remaining-seconds value. During the final warning
phase the script stops hunting, even if a ghost still reports FRIGHT.
Pellet and hunting rewards never remove the immediate-danger penalties.

## Limitations

The script remembers visited tiles across runs and searches up to eight tiles
ahead for less-visited routes. Within six tiles of a dangerous ghost, it weighs
escape distance and corridor safety more heavily, penalizing dead ends.
Near threats, it also searches escape routes up to eight tiles beyond junctions,
rejecting paths where its estimated ghost-arrival margin is exhausted. A longer
estimated escape takes priority over rewards and the preference against reversing.
A safe forward/side route is preferred when it offers the same escape depth;
reversing is allowed when it provides a better escape or the only way out. Repeated identical turn requests at the
same tile are suppressed for one second while waiting for the IOC to react.
History resets when disabled, when the board layout changes, or on panel reload.

The `requested` input reads the existing `internal://scriptPacmanDirection`
output. This stored Internal value lets the script replace an outdated pending
turn with the currently selected direction, including straight ahead. The visit
history remains in the widget's persistent JavaScript state; no extra state PV
is created.

The escape estimate uses equal tile speeds and all dangerous ghost positions;
it does not simulate their individual targeting, reversal rules or actual timing.
It does not plan escape routes through portals. It monitors the four power pellets, but has no regular-pellet map.
Distance 868 is the fallback when the breadth-first search reaches no dangerous
ghost. Loops and collisions remain
possible; the corridor score is a heuristic, not a collision guarantee.
