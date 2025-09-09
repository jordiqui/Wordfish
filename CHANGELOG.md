# Changelog

## [2.10 dev-090925 avx]
### Changed
- Updated engine id name to "Wordfish v2.10 dev-090925 avx".
- Strengthened king safety evaluation and extended search on exposed enemy kings.
- Introduced dynamic activity evaluation emphasizing mobility and initiative.

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
