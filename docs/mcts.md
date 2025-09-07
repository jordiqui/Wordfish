# Monte Carlo Tree Search Options (Experimental)

Wordfish includes experimental support for Monte Carlo Tree Search (MCTS) to
explore certain position types inspired by Shashin's classification. The
feature is based on original work by Stephan Nicolet and aims to leverage
Lc0-style search in Petrosian high, high middle and middle positions.

## UCI Options

- **MCTS by Shashin**: Boolean (default `false`). Enables MCTS for the
  position categories described above.
- **MCTSThreads**: Integer (default `0`, range `0`–`512`). Number of helper
  threads dedicated to MCTS; the main thread continues alpha-beta search.
- **MCTS Multi Strategy**: Integer (default `20`, range `0`–`100`). Tree
  policy parameter used in multi-MCTS mode.
- **MCTS Multi MinVisits**: Integer (default `5`, range `0`–`1000`). Minimum
  visits threshold for the upper confidence bound in multi-MCTS mode.
- **MCTS Explore**: Boolean (default `false`). Also apply MCTS to highly Tal
  type positions and Capablanca positions.

## Credits

The Monte Carlo Tree Search algorithm was originally introduced by Rémi Coulom
in *Efficient Selectivity and Backup Operators in Monte-Carlo Tree Search*
(2006). For an overview and further references see the
[Chess Programming Wiki](https://www.chessprogramming.org/Monte-Carlo_Tree_Search).

