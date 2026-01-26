# Changelog

## [Unreleased]
### Added
- Nothing yet.

### Changed
- Nothing yet.

## [4.0.1-260126]
### Changed
- Rebranded the engine to "Wordfish-4.0.1-260126" and extended architecture-aware executable and UCI suffixes so GUIs like Fritz 20 and CuteChess show `Wordfish-4.0.1-260126-sse41popcnt`, `Wordfish-4.0.1-260126-avx2`, `Wordfish-4.0.1-260126-bmi2`, `Wordfish-4.0.1-260126-FMA3`, or `Wordfish-4.0.1-260126-avx512` depending on the compiled target.

## [3.70-070126]
### Changed
- Rebranded the engine to "Wordfish-3.70-070126" and extended architecture-aware executable and UCI suffixes so GUIs like Fritz 20 and CuteChess show `Wordfish-3.70-070126-sse41popcnt`, `Wordfish-3.70-070126-avx2`, `Wordfish-3.70-070126-bmi2`, `Wordfish-3.70-070126-FMA3`, or `Wordfish-3.70-070126-avx512` depending on the compiled target.

## [3.60-181225]
### Added
- Documented new training emphasis on king safety, rook coordination, and endgame supervision.

### Changed
- Tightened king-safety heuristics for open files, rook lifts, and dark-square weaknesses.
- Reduced aggressive pruning in sharp positions and added verification search for large evaluation swings.
- Rewarded connected rooks while penalizing premature flank pawn storms in the handcrafted evaluation.
- Updated the main NNUE network to `nn-2962dca31855.nnue` from Stockfish dev 20251130.
- Rebranded the engine to "Wordfish-3.60-181225" and aligned executable and UCI identifiers to include architecture-aware suffixes (e.g., `-sse41popcnt` or `-avx2`) so GUI search listings reflect the compiled target.

## [3.50]
### Changed
- Rebranded the engine to "Wordfish-3.50", ensuring the executable, UCI identification, and GUI analysis headers display the new name consistently.

## [3.20 101125]
### Changed
- Rebranded the engine to "Wordfish-3.20-101125", ensuring the executable, UCI identification, and GUI analysis headers display the new name consistently.

## [3.10 011125]
### Changed
- Rebranded the engine to "Wordfish-3.10-011125", ensuring the executable, UCI identification, and GUI analysis headers display the new name consistently.

## [3.0 281025]
### Changed
- Updated engine id name to "wordfish-3.0-281025" and aligned the generated executable name with the new branding.

## [2.90 191025]
### Changed
- Updated engine id name to "Wordfish 2.90 191025".

## [2.42-190825]
### Changed
- Updated engine id name to "Wordfish 2.42-190825".

## [2.40 120925 avx]
### Changed
- Updated engine id name to "Wordfish v. 2.40 120925 avx".
- Integrated third neural network `nn-baff1ede1f90.nnue`.

## [2.30 110925]
### Changed
- Updated engine id name to "Wordfish v. 2.30 110925".
- Increased evaluation weight for successful sacrificial attacks.

## [2.0.1 avx 070925]
### Changed
- Updated engine id name to "Wordfish v. 2.0.1 avx 070925".

## [2.0 dev-060925]
### Changed
- Renamed engine to Wordfish 2.0 dev-060925 avx.


## [1.0.1] - 2025-09-01
### Added
- UCI option `Minimum Thinking Time` to enforce a minimum search duration per move.
- UCI option `Slow Mover` to adjust engine time usage.
- Engine now appends the build date after its name in UCI identification.
### Changed
- Simplified rule-50 key adjustment by removing the unused template parameter.

## [1.0.0-dev 2708225]
### Changed
- Simplified LMR logic to streamline search and improve speed.
- Updated default engine name to "wordfish device v.1.0.0" with build identifier 2708225.

## [1.0] - 2025-08-27
### Added
- Initial public release of Wordfish v1.0.
- Iterative deepening now starts at depth 2 for faster and deeper analysis.
- Updated engine name and author information.
