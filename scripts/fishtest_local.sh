#!/bin/bash
# Launch a local fishtest worker for tuning Revolution.
# Requires a fishtest repository cloned locally.

set -e
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="$ROOT_DIR/src/revolution"
FISHTEST_DIR="${FISHTEST_DIR:-$HOME/fishtest}"

if [ ! -x "$ENGINE" ]; then
  echo "Engine binary not found at $ENGINE"
  exit 1
fi

if [ ! -d "$FISHTEST_DIR/worker" ]; then
  echo "Fishtest worker not found in $FISHTEST_DIR"
  exit 1
fi

python3 "$FISHTEST_DIR/worker/worker.py" --concurrency "$(nproc)" --stockfish "$ENGINE" "$@"
