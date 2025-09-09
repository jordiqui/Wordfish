Wordfish v2.10 dev-090925 avx
- Initial fork from Stockfish.
- Updated engine name and build system.
- Added experience book system with persistent `.exp` file (legacy `.bin` files are converted automatically) and new UCI options.
- Iterative deepening now begins at depth 2 for faster, deeper search.
- Strengthened king safety evaluation and search extensions for exposed kings.
- Introduced dynamic activity evaluation to reward mobility and pressure on the enemy king.
- Updated version strings and AUTHORS for first public release.
