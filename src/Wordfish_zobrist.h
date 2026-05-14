/*
  Wordfish experience hashing utilities
  Copyright (C) 2025

  This file defines helper routines used by the self-learning subsystem to
  generate stable pseudo-random numbers for hashing. The implementation is
  intentionally lightweight so it can be used in constexpr contexts when
  possible, but also keeps the interface header-only as in the upstream
  project.

  Note: this auxiliary combiner is only for Experience bookkeeping and does
  not replace Stockfish core Position/TT/pawn/material Zobrist keys.
*/

#ifndef WORDFISH_ZOBRIST_H_INCLUDED
#define WORDFISH_ZOBRIST_H_INCLUDED

#include <cstdint>

namespace Stockfish::Experience::Zobrist {

// splitmix64 is a small, fast pseudo random number generator that we use to
// decorrelate inputs into seemingly random 64 bit values. The constants come
// from the original splitmix64 reference implementation by Sebastiano Vigna.
inline std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

// Utility that combines two 64 bit values in a symmetric way. This is inspired
// by boost::hash_combine but specialised for 64 bit inputs.
inline std::uint64_t combine(std::uint64_t seed, std::uint64_t value) {
    seed ^= splitmix64(value + 0x9E3779B97F4A7C15ULL + (seed << 6) + (seed >> 2));
    return seed;
}

// Helper to mix a single 64 bit value.
inline std::uint64_t mix(std::uint64_t value) { return splitmix64(value); }

}  // namespace Stockfish::Experience::Zobrist

#endif  // #ifndef WORDFISH_ZOBRIST_H_INCLUDED
