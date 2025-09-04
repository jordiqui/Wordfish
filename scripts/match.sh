#!/bin/bash
# Run automated matches between Wordfish and another UCI engine using cutechess-cli.
# Usage: scripts/match.sh /path/to/opponent [games] [timecontrol]

set -e
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="${ENGINE:-$ROOT_DIR/src/wordfish}"
OPPONENT="${1:?Opponent engine path required}"
GAMES="${2:-10}"
TC="${3:-40/0.4+0.4}"

if ! command -v cutechess-cli >/dev/null; then
  echo "cutechess-cli is required but not installed"
  exit 1
fi

cutechess-cli \
  -engine cmd="$ENGINE" name=Wordfish \
  -engine cmd="$OPPONENT" name=Opponent \
  -each proto=uci tc=$TC \
  -games $GAMES -concurrency 2 \
  -pgn "$ROOT_DIR/match.pgn"
