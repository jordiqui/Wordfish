#!/usr/bin/env python3
import argparse
import json
import statistics
import subprocess
import time
from pathlib import Path


def send(proc, line):
    proc.stdin.write((line + "\n").encode("utf-8"))
    proc.stdin.flush()


def read_until(proc, predicate, timeout):
    deadline = time.monotonic() + timeout
    lines = []
    while time.monotonic() < deadline:
        line = proc.stdout.readline().decode("utf-8", errors="replace").rstrip("\r\n")
        if not line:
            continue
        lines.append(line)
        if predicate(line):
            return lines
    raise TimeoutError(f"Timed out waiting for engine output: {lines[-10:]}")


def launch_engine(engine_path):
    return subprocess.Popen(
        [str(engine_path)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=0,
    )


def handshake(proc, syzygy_path):
    send(proc, "uci")
    read_until(proc, lambda line: line == "uciok", 10.0)
    send(proc, f"setoption name SyzygyPath value {syzygy_path}")
    send(proc, "isready")
    read_until(proc, lambda line: line == "readyok", 10.0)


def run_case(proc, fen, wtime_ms):
    send(proc, f"position fen {fen}")
    send(proc, f"go wtime {wtime_ms} btime {wtime_ms}")
    started = time.perf_counter_ns()
    lines = read_until(proc, lambda line: line.startswith("bestmove "), 10.0)
    elapsed_ms = (time.perf_counter_ns() - started) / 1_000_000.0
    bestmove = next(line for line in reversed(lines) if line.startswith("bestmove "))
    return {"fen": fen, "wtime": wtime_ms, "latency_ms": elapsed_ms, "bestmove": bestmove}


def summarize(samples):
    latencies = [sample["latency_ms"] for sample in samples]
    return {
        "count": len(latencies),
        "min_ms": min(latencies),
        "max_ms": max(latencies),
        "mean_ms": statistics.fmean(latencies),
        "median_ms": statistics.median(latencies),
    }


def main():
    parser = argparse.ArgumentParser(description="Measure UCI bestmove latency for Syzygy root hits.")
    parser.add_argument("--engine", required=True)
    parser.add_argument("--syzygy-path", required=True)
    parser.add_argument("--iterations", type=int, default=20)
    parser.add_argument("--warmup", type=int, default=5)
    parser.add_argument("--wtime", nargs="+", type=int, required=True)
    parser.add_argument("--fen", nargs="+", required=True)
    parser.add_argument("--assert-max-ms", type=float, default=None)
    parser.add_argument("--json", required=True)
    args = parser.parse_args()

    engine_path = Path(args.engine)
    syzygy_path = Path(args.syzygy_path)
    output_path = Path(args.json)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    proc = launch_engine(engine_path)
    try:
        handshake(proc, syzygy_path)

        for _ in range(args.warmup):
            for fen in args.fen:
                for wtime_ms in args.wtime:
                    run_case(proc, fen, wtime_ms)

        samples = []
        for _ in range(args.iterations):
            for fen in args.fen:
                for wtime_ms in args.wtime:
                    sample = run_case(proc, fen, wtime_ms)
                    if args.assert_max_ms is not None and sample["latency_ms"] > args.assert_max_ms:
                        raise AssertionError(
                            f"Latency {sample['latency_ms']:.3f}ms exceeded {args.assert_max_ms}ms for {fen}"
                        )
                    samples.append(sample)
    finally:
        try:
            send(proc, "quit")
        except Exception:
            pass
        proc.kill()
        proc.wait()

    result = {
        "engine": str(engine_path),
        "syzygy_path": str(syzygy_path),
        "iterations": args.iterations,
        "warmup": args.warmup,
        "wtime": args.wtime,
        "fens": args.fen,
        "summary": summarize(samples),
        "samples": samples,
    }

    output_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result["summary"], indent=2))


if __name__ == "__main__":
    main()
