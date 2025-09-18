# Changelog

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
