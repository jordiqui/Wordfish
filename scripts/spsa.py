#!/usr/bin/env python3
"""Simple SPSA tuner for the Revolution engine."""
import argparse
import os
import random
import subprocess

def run_bench(engine, name, value):
    engine_dir = os.path.dirname(engine)
    proc = subprocess.Popen(
        [engine], cwd=engine_dir, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True
    )
    cmd = f"uci\nsetoption name EvalFile value {engine_dir}/nn-1c0000000000.nnue\n"
    cmd += f"setoption name {name} value {int(value)}\nbench\nquit\n"
    out, _ = proc.communicate(cmd)
    for line in out.splitlines():
        if line.startswith("Nodes searched:"):
            return int(line.split(":")[1])
    raise RuntimeError("Unexpected bench output")

def main():
    p = argparse.ArgumentParser(description="SPSA tuning for Revolution")
    p.add_argument("--param", nargs=4, metavar=("NAME", "START", "MIN", "MAX"), action='append', required=True)
    p.add_argument("--engine", default="src/revolution")
    p.add_argument("--iterations", type=int, default=10)
    args = p.parse_args()

    theta = {n: float(s) for n, s, mn, mx in args.param}
    bounds = {n: (float(mn), float(mx)) for n, s, mn, mx in args.param}

    a, c, A = 0.5, 0.1, 10
    alpha, gamma = 0.602, 0.101

    for k in range(1, args.iterations + 1):
        ak = a / ((k + A) ** alpha)
        ck = c / (k ** gamma)
        grad = {}
        for name in theta:
            delta = random.choice([-1, 1])
            high = max(bounds[name][0], min(bounds[name][1], theta[name] + ck * delta))
            low = max(bounds[name][0], min(bounds[name][1], theta[name] - ck * delta))
            y_high = run_bench(args.engine, name, high)
            y_low = run_bench(args.engine, name, low)
            grad[name] = (y_high - y_low) / (2 * ck * delta)
        for name in theta:
            theta[name] = max(bounds[name][0], min(bounds[name][1], theta[name] - ak * grad[name]))
        print(f"Iter {k}: " + ", ".join(f"{n}={theta[n]:.2f}" for n in theta))

if __name__ == "__main__":
    main()
