#!/bin/bash
# Run perft validation on the Revolution engine.
# Builds the engine if needed and executes the existing test suite.

set -e
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="$ROOT_DIR/src/revolution"
TEST_DIR="$ROOT_DIR/tests"

if [ ! -x "$ENGINE" ]; then
  echo "Building engine..."
  (cd "$ROOT_DIR/src" && make -j2 build ARCH=x86-64 >/dev/null)
fi

# tests/perft.sh expects a binary called 'stockfish'
ln -sf "$ENGINE" "$TEST_DIR/stockfish"

(cd "$TEST_DIR" && ./perft.sh)
