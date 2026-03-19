# Playchess Syzygy bestmove latency repro

This patch targets a Playchess timeout pattern seen when a Syzygy root hit returns a bestmove in a very low-time search.

## Real Windows-oriented repro

Use a native Windows build and real 6-man KRBvKRB Syzygy files for the authoritative repro:

```powershell
python tests/uci_latency_harness.py --engine .\src\<WORDFISH_BINARY>.exe --syzygy-path <real_6man_syzygy_path> --iterations 1000 --warmup 5 --wtime 200 500 1000 --fen "8/8/4k3/8/8/1r1K1B2/1b6/5R2 w - - 0 86" "8/8/6B1/7R/8/4b1k1/r7/4K3 w - - 84 128" --assert-max-ms 20 --json artifacts\playchess_syzygy_latency.json
```

## Local smoke coverage

Container or Linux runs are only smoke coverage. They are useful for checking that the engine still answers quickly with local Syzygy files, but they do not replace the real Windows + Playchess repro.

Example smoke command:

```bash
python3 tests/uci_latency_harness.py \
  --engine src/<WORDFISH_BINARY_NAME> \
  --syzygy-path <local_test_syzygy_path> \
  --iterations 20 \
  --warmup 5 \
  --wtime 200 500 1000 \
  --fen '4k3/PP6/8/8/8/8/8/4K3 w - - 0 1' \
  --assert-max-ms 50 \
  --json artifacts/uci_latency_4man_after.json
```

The real Playchess issue requires the actual 6-man KRBvKRB Syzygy set.
