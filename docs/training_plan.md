# Training and Evaluation Adjustments

## King Safety Focus
- Increase self-play sampling of positions with aggressive pawn storms around the king.
- Track open-file exposure, rook lifts, and dark-square weaknesses in evaluation logs.
- Add targeted puzzles highlighting premature pawn pushes near the king to the regression suite.

## Development Prioritization
- Prioritize game fragments where both rooks remain disconnected beyond move 12.
- Emphasize feature extraction that rewards timely rook coordination before flank pawn advances.

## Selective Search Verification
- Schedule dedicated testing of verification search thresholds on tactical suites (mate nets, perpetuals).
- Monitor late-move reduction statistics to ensure sharp positions retain sufficient depth.

## Endgame Reinforcement
- Expand simplified-position self-play batches with restricted material (rook endings, bishop vs. pawns).
- Integrate Syzygy tablebase evaluations during training labels to penalize inaccurate pawn pushes.
