# Cup Season IJCRL 2025 Match Review

This note summarizes three recent tournament losses and highlights concrete areas where Wordfish-2.80 can be improved.

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
