# Cup Season IJCRL 2025 Match Review

This note summarizes three recent tournament losses and highlights concrete areas where Wordfish-2.80 can be improved.

## Bullet regression: Wordfish-2.81 at 10+0.1

Wordfish-2.81 was invited to the **Cup Season IJCRL 2025** bullet event (10+0.1, Syzygy 5-men). Despite the nominally equal
hardware and identical opening suite, the engine finished last with a -116 Elo gap to the leader:

```
Rank Name                 Elo    +    - games score oppo. draws
   1 Killfish PB 200525    36   18   17   800   57%    -5   46%
   2 HypnoS 1.01           32   18   17   800   56%    -4   45%
   3 revolution-2.81       30   17   17   800   56%    -4   48%
   4 stockfish 17.1        19   17   17   800   54%    -2   49%
   5 Brainlearn31          13   17   17   800   52%    -2   49%
   6 RapTora 6.0            6   17   17   800   51%    -1   47%
   7 ShashChess39.1        -8   17   17   800   48%     1   48%
   8 ZurgaaPOLY 18.1AI    -13   17   17   800   47%     2   47%
   9 wordfish-2.81       -116   18   18   800   30%    14   42%
```

### Observed weaknesses

The PGN extracts highlight three persistent failure modes:

1. **Slow starts from preloaded FENs.** The event used random mid-game FENs from `UHO_Lichess_4852_v1.epd`, often with locked
   pawn structures. Wordfish repeatedly spent 0.4–0.7 seconds on the first move of the game to load the correct NNUE (see
   rounds 1, 2, 3, 5, 8 and 9). Opponents playing pure Stockfish code replied in 0.1–0.2 seconds and seized initiative. The
   triple-network handshake (big / small / falcon) appears to resample evaluation caches at move one, costing ≈40% of the
   allotted bullet time.
2. **Over-aggressive pawn storms without coordination.** In several black games (e.g. vs. ZurgaaPOLY round 1, HypnoS rounds 9
   and 216, RapTora round 8) Wordfish pushed the f- and g-pawns before completing development, opening files that the opponent
   exploited immediately. The engine evaluated these structures as winning (`-1.5` to `-4.0`) until tactics refuted them,
   suggesting the king-safety heuristics tuned for longer time controls overestimate defensive resources when depth is shallow.
3. **Search instability under time pressure.** Once the opponent generated forcing lines, the evaluation swung from clearly
   winning scores (−5.0 or lower) to immediate mate. The replay logs show that reductions and pruning skip verification in the
   final plies, so the engine never revisits risky moves after the first failing response. This behaviour is consistent with LMR
   parameters calibrated for ≥3 second move times, not bullet.

### Immediate mitigations

* **Profile network warm-up.** Instrument `evaluate.cpp` to log the time spent on `nets.big`, `nets.small` and `nets.falcon`
  acquisition during the first five plies when the engine receives a fresh FEN. Reproduce the 10+0.1 settings with the same
  Cute Chess command line used in the event to confirm whether the three-network cascade costs >150 ms per move.
* **Bullet-oriented option preset.** Ship a helper shell script (e.g. `scripts/presets/bullet-10_0.1.sh`) that disables Falcon
  and Experience features, sets `Minimum Thinking Time` to 10 ms, and caps `Move Overhead` aggressively. This gives operators a
  reproducible configuration instead of manual tinkering.
* **Tighten king-safety features for low depth.** Introduce a depth-aware scaling factor so that pawn storms around our own king
  are discouraged when search depth < 14. The PGNs show repeated cases where depth-9 or depth-10 searches endorsed unsound pawn
  pushes.

### Follow-up tasks

1. **Time-management regression tests.** Extend `scripts/benchmark_blitz.py` (or create an equivalent) to record nodes per
   second and average move time for 50 random UHO positions at 10+0.1. Compare the results with Stockfish 17.1 to establish the
   baseline loss caused by heavier evaluation.
2. **Selective-search audit.** Run self-play matches at 10+0.1 toggling `Late Move Pruning` and `Futility Pruning` knobs. The
   goal is to find a configuration that restores verification search depth in tactical positions without breaking LTC strength.
3. **NNUE cache residency experiment.** Modify the network loader to keep Falcon disabled by default in bullet. If this change
   recovers ≥40 Elo at fast TC, we can revisit enabling Falcon only when `time > 12000 ms` per move.
4. **Opening policy refresh.** Generate a compact book targeting the UHO_Lichess_4852_v1 suite, focusing on solid development
   moves for both colours. Feeding better first moves into the search reduces the need to burn time recalculating well-known
   continuations.


## Round 3 vs. ShashChess 39.1 (White, loss)
* **Opening structure:** After 1.exd4 cxd4 the engine steered into a Benoni-like setup with a vulnerable king. Moves such as 4.Ne1?! and 9.Rc1?! left the queenside undeveloped and ceded dark-square control. Training data that rewards rapid piece activity in Benoni structures could help avoid passivity.
* **Overextension on the queenside:** The sequence 14.Nxa5? 15.a4? traded activity for material but opened files for Black's heavy pieces. Evaluation should penalize exposed queenside kings after committing the a-pawn, especially when opposite-side castling is absent.
* **King safety heuristics:** The plan with 18.f4 and 21.a5 further weakened white's king. Wordfish continued pushing pawns despite Black's looming rook lift (…Ra6–Rh6). Emphasize features measuring rook lifts and open files towards own king to trigger defensive play.
* **Tactical awareness:** On move 31…Ng5, Black launched a decisive mating net. The search did not foresee the forced mating sequence despite relatively quiet preceding moves. Investigate extensions or selective pruning settings in positions with mating threats against a cornered king.

## Round 2 vs. Revolution 2.81 (Black, loss)
* **Opening inaccuracies:** The early …Rg8–Rh8 plan against the h-pawn thrust left the king on e8 without central control. Consider expanding the opening book or policy network for anti-Trompowsky/Colle positions to avoid awkward rook shuffles.
* **Exchange sacrifice evaluation:** After 20…Nxe4 21.Qxb7 the engine entered complications where Black's king remained in the center. The follow-up 23…Nxd4 24.Nxd4 Rh1+ forced the king walk but failed to convert initiative. Improve evaluation of positions where material equality returns but king exposure persists.
* **Endgame technique:** The transition after 31.Qe2+ Kd5 32.Qxe7 left a worse pawn structure. Later, the engine declined defensive resources like 38…Kg6 (instead of 38…g4?) leading to a lost pawn endgame. Reinforce tablebase-guided training and add penalties for pushing pawns that create permanent weaknesses in opposite-color bishop endgames.

## Round 1 vs. ZurgaaPOLY 18.1AI (White, loss)
* **Handling minor-piece pressure:** Moves 13.b4?! and 14.Rf1? allowed …Bf2! and an enduring pin on e1. Enhance feature weights for long-term piece activity so the search prioritizes completing development (e.g., 13.Qd2, 14.Qf3) instead of pawn grabs.
* **Counterplay recognition:** The engine missed chances to liquidate the dark-squared bishop with 19.Bd4 or 20.Bxd4, relieving pressure. Incorporate a tactical motif detector for trapped bishops and pinned pieces to guide exchanges earlier.
* **Endgame conversion:** After 50.Rxb5 Kh7 51.b5 the passed a- and b-pawns looked promising, but inaccurate handling (54.Rb6?, 55.Rb8?) let Black generate counterplay with passed g- and h-pawns. Integrate reinforcement training with tablebase-guided rollouts to learn precise conversion when ahead in material but facing connected passers.

## Cross-Game Themes
1. **King safety heuristics:** All three games featured optimistic pawn advances around Wordfish's own king. Tune evaluation terms for open files, rook lifts, and dark-square weaknesses.
2. **Development prioritization:** Early pawn thrusts without completing development led to cramped positions. Update the training regimen or handcrafted bonuses to favor connecting rooks before flank pawn storms.
3. **Selective search adjustments:** Wordfish missed forced sequences (mate nets, perpetual defenses). Review pruning thresholds (late move reductions, null-move depth) for sharp positions and consider adding verification search when the evaluation swings sharply.
4. **Endgame reinforcement:** Both Black games transitioned into lost endgames after inaccurate pawn pushes. Expanding self-play in simplified positions and integrating Syzygy supervision should reduce such errors.

These adjustments target recurring weaknesses and should raise robustness against dynamic engines in future Cup Season rounds.
