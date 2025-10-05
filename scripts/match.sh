#!/bin/bash
# Run automated matches between Wordfish and another UCI engine using cutechess-cli.
# Usage: scripts/match.sh /path/to/opponent [games] [timecontrol]

set -e
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE_NAME="Wordfish-2.80-061025"
ENGINE_DEFAULT="$ROOT_DIR/src/$ENGINE_NAME"
ENGINE="${ENGINE:-$ENGINE_DEFAULT}"
OPPONENT="${1:?Opponent engine path required}"
GAMES="${2:-10}"
TC="${3:-40/0.4+0.4}"

if ! command -v cutechess-cli >/dev/null; then
  echo "cutechess-cli is required but not installed"
  exit 1
fi

cutechess-cli \
  -engine cmd="$ENGINE" name="$ENGINE_NAME" \
  -engine cmd="$OPPONENT" name=Opponent \
  -each proto=uci tc=$TC \
  -games $GAMES -concurrency 2 \
  -pgn "$ROOT_DIR/match.pgn"
