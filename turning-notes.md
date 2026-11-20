# Turning and threading notes

## Thread depth profile

Chip area of a V-thread grows with depth². Equal X steps load the last
passes most. With the current "squared" profile, the middle passes carry
the most load.

Load per pass, 8 passes. Each value is the pass chip area divided by the
average (1.00 = average):

| Profile | p1   | p2   | p3   | p4   | p5   | p6   | p7   | p8   |
|---------|------|------|------|------|------|------|------|------|
| linear  | 0.12 | 0.38 | 0.62 | 0.88 | 1.12 | 1.38 | 1.62 | 1.88 |
| squared | 0.44 | 1.09 | 1.44 | 1.53 | 1.41 | 1.12 | 0.72 | 0.25 |
| Sandvik | 0.34 | 0.80 | 1.14 | 1.14 | 1.14 | 1.14 | 1.14 | 1.14 |

Sandvik: `d1 = D·√(0.3/(N−1))`, `dn = D·√((n−1)/(N−1))` for n ≥ 2.
N=1 cuts full depth. Spring passes stay as they are now.

Decisions:
- Replace `bool squaredDepth` in `PassPlan` with
  `enum class DepthProfile { Linear, Squared, Sandvik }`.
- `passDepthFraction` switches on the enum. Each profile is a pure
  function, unit-tested directly. The enum serializes to `ModePass` JSON
  and to a future setting.
- Sandvik is the Thread default. Squared stays as an enum value.
- Keep pass count as the input.
- Include the 0.3 first-pass factor. No minimum-step clamp: the pass
  count is the user's choice.
- Problem being solved: spindle stall and stepper lost steps on deep
  M12 passes (force, not chatter).

Other modes:
- Turn: rectangular chip, linear is already constant area.
- Slot: linear is correct for a square tool. A V or dovetail form tool
  would benefit from Sandvik, through the future setting.
- G-code: no pass plan. A future G76 depth degression `d_n ∝ n^(1/R)`
  can use the same profile functions (R=2 is constant area).

## Candidates from Sandvik practice

1. Modified flank infeed on one side (Thread). Feed along one flank at
   3–5° less than the half angle. Lower force than radial. The current
   `flank` option only alternates.
   Decisions:
   - `flank` becomes a 3-state enum: off, alternating, one side.
   - One side is set automatically: the leading edge cuts. The side
     follows from hand, spindle reverse and internal.
   - No new field. The button cycles its text: `Flank: off | \/ | /`.
     Nextion needs `\\` for the backslash.
2. Finish allowance in Turn. Rough passes, then one finish pass with a
   small allowance and a finer feed. New parameters: `finish`,
   `finish-pitch`.
3. Thread run-out (Thread). X retracts while Z stays synchronized over
   about 1 pitch, for threads near a shoulder. Current retract behavior
   not checked.
4. Peck in Part: already present.

## Implementation phases

1. Depth profile: `DepthProfile` enum, Sandvik function, Thread default,
   unit tests. Firmware only, no UI change.
2. Flank mode: 3-state enum with automatic side, one-side offset in
   `passFlankOffsetDu`, multi-value param kind if missing, stored
   settings migration, display button cycle, web client, unit tests.
3. Thread run-out: discuss more before any change.
4. Turn finish allowance: `finish`, `finish-pitch`. Check display space
   for Turn first.
