#!/usr/bin/env bash
# Configure a Cute Chess match preset for 10+0.1 bullet games.
# Applies a low-latency configuration for Wordfish by disabling Falcon/Experience
# features, reducing move overhead, and enforcing a 10 ms minimum thinking time.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CUTECHESS_CLI_BIN="${CUTECHESS_CLI:-cutechess-cli}"
ENGINE_NAME="${ENGINE_NAME:-}"
ENGINE_BIN="${ENGINE:-}"

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 /path/to/opponent [games]" >&2
  exit 1
fi

if ! command -v "$CUTECHESS_CLI_BIN" >/dev/null; then
  echo "cutechess-cli is required but was not found in PATH" >&2
  exit 1
fi

if [[ -z "$ENGINE_BIN" ]]; then
  # Prefer a bundled Wordfish binary if one exists under src/.
  ENGINE_BIN="$(find "$ROOT_DIR/src" -maxdepth 1 -type f -perm -111 -name 'wordfish*' | sort | head -n1 || true)"
fi

if [[ -z "$ENGINE_BIN" ]]; then
  echo "Unable to locate the Wordfish binary. Set the ENGINE environment variable." >&2
  exit 1
fi

if [[ -z "$ENGINE_NAME" ]]; then
  ENGINE_NAME="$(basename "$ENGINE_BIN")"
fi

OPPONENT_BIN="$1"
GAMES="${2:-20}"
OPPONENT_NAME="${OPPONENT_NAME:-Opponent}"
CONCURRENCY="${CONCURRENCY:-4}"
OUTPUT_PGN="${OUTPUT_PGN:-$ROOT_DIR/bullet-10_0.1.pgn}"

"$CUTECHESS_CLI_BIN" \
  -engine cmd="$ENGINE_BIN" name="$ENGINE_NAME" \
    option.FalconFile=None \
    option."Experience Enabled"=false \
    option."Experience Book"=false \
    option."Experience Concurrent"=false \
    option."Minimum Thinking Time"=10 \
    option."Move Overhead"=5 \
  -engine cmd="$OPPONENT_BIN" name="$OPPONENT_NAME" \
  -each proto=uci tc=10+0.1 \
  -games "$GAMES" \
  -concurrency "$CONCURRENCY" \
  -pgn "$OUTPUT_PGN"
