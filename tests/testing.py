import subprocess
import unittest
from typing import List
import os
import collections
import time
import sys
import traceback
import fnmatch
from functools import wraps
from contextlib import redirect_stdout
import io
import tarfile
import pathlib
import concurrent.futures
import tempfile
import shutil
import requests

CYAN_COLOR = "\033[36m"
GRAY_COLOR = "\033[2m"
RED_COLOR = "\033[31m"
GREEN_COLOR = "\033[32m"
RESET_COLOR = "\033[0m"
WHITE_BOLD = "\033[1m"

MAX_TIMEOUT = 60 * 5

PATH = pathlib.Path(__file__).parent.resolve()


class Valgrind:
    @staticmethod
    def get_valgrind_command():
        return [
            "valgrind",
            "--error-exitcode=42",
            "--errors-for-leak-kinds=all",
            "--leak-check=full",
        ]

    @staticmethod
    def get_valgrind_thread_command():
        return ["valgrind", "--error-exitcode=42", "--fair-sched=try"]


class TSAN:
    @staticmethod
    def set_tsan_option():
        with open(f"tsan.supp", "w") as f:
            f.write(
                """
race:Stockfish::TTEntry::read
race:Stockfish::TTEntry::save
race:Stockfish::TranspositionTable::probe
race:Stockfish::TranspositionTable::hashfull
"""
            )

        os.environ["TSAN_OPTIONS"] = "suppressions=./tsan.supp"

    @staticmethod
    def unset_tsan_option():
        os.environ.pop("TSAN_OPTIONS", None)
        os.remove(f"tsan.supp")


class EPD:
    @staticmethod
    def create_bench_epd():
        with open(f"{os.path.join(PATH,'bench_tmp.epd')}", "w") as f:
            f.write(
                """
Rn6/1rbq1bk1/2p2n1p/2Bp1p2/3Pp1pP/1N2P1P1/2Q1NPB1/6K1 w - - 2 26
rnbqkb1r/ppp1pp2/5n1p/3p2p1/P2PP3/5P2/1PP3PP/RNBQKBNR w KQkq - 0 3
3qnrk1/4bp1p/1p2p1pP/p2bN3/1P1P1B2/P2BQ3/5PP1/4R1K1 w - - 9 28
r4rk1/1b2ppbp/pq4pn/2pp1PB1/1p2P3/1P1P1NN1/1PP3PP/R2Q1RK1 w - - 0 13
"""
            )

    @staticmethod
    def delete_bench_epd():
        os.remove(f"{os.path.join(PATH,'bench_tmp.epd')}")


class Syzygy:
    @staticmethod
    def get_syzygy_path():
        return os.path.abspath("syzygy")

    @staticmethod
    def download_syzygy():
        if not os.path.isdir(os.path.join(PATH, "syzygy")):
            url = "https://api.github.com/repos/niklasf/python-chess/tarball/9b9aa13f9f36d08aadfabff872882f4ab1494e95"
            file = "niklasf-python-chess-9b9aa13"

            with tempfile.TemporaryDirectory() as tmpdirname:
                tarball_path = os.path.join(tmpdirname, f"{file}.tar.gz")

                response = requests.get(url, stream=True)
                with open(tarball_path, "wb") as f:
                    for chunk in response.iter_content(chunk_size=8192):
                        f.write(chunk)

                with tarfile.open(tarball_path, "r:gz") as tar:
                    tar.extractall(tmpdirname)

                shutil.move(
                    os.path.join(tmpdirname, file), os.path.join(PATH, "syzygy")
                )


class OrderedClassMembers(type):
    @classmethod
    def __prepare__(self, name, bases):
        return collections.OrderedDict()

    def __new__(self, name, bases, classdict):
        classdict["__ordered__"] = [
            key for key in classdict.keys() if key not in ("__module__", "__qualname__")
        ]
        return type.__new__(self, name, bases, classdict)


class TimeoutException(Exception):
    def __init__(self, message: str, timeout: int):
        self.message = message
        self.timeout = timeout


def timeout_decorator(timeout: float):
    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            with concurrent.futures.ThreadPoolExecutor() as executor:
                future = executor.submit(func, *args, **kwargs)
                try:
                    result = future.result(timeout=timeout)
                except concurrent.futures.TimeoutError:
                    raise TimeoutException(
                        f"Function {func.__name__} timed out after {timeout} seconds",
                        timeout,
                    )
            return result

        return wrapper

    return decorator


class MiniTestFramework:
    def __init__(self):
        self.passed_test_suites = 0
        self.failed_test_suites = 0
        self.passed_tests = 0
        self.failed_tests = 0
        self.stop_on_failure = True

    def has_failed(self) -> bool:
        return self.failed_test_suites > 0

    def run(self, classes: List[type]) -> bool:
        self.start_time = time.time()

        for test_class in classes:
            with tempfile.TemporaryDirectory() as tmpdirname:
                original_cwd = os.getcwd()
                os.chdir(tmpdirname)

                try:
                    if self.__run(test_class):
                        self.failed_test_suites += 1
                    else:
                        self.passed_test_suites += 1
                except Exception as e:
                    self.failed_test_suites += 1
                    print(f"\n{RED_COLOR}Error: {e}{RESET_COLOR}")
                finally:
                    os.chdir(original_cwd)

        self.__print_summary(round(time.time() - self.start_time, 2))
        return self.has_failed()

    def __run(self, test_class) -> bool:
        test_instance = test_class()
        test_name = test_instance.__class__.__name__
        test_methods = [m for m in test_instance.__ordered__ if m.startswith("test_")]

        print(f"\nTest Suite: {test_name}")

        if hasattr(test_instance, "beforeAll"):
            test_instance.beforeAll()

        fails = 0

        for method in test_methods:
            fails += self.__run_test_method(test_instance, method)

        if hasattr(test_instance, "afterAll"):
            test_instance.afterAll()

        self.failed_tests += fails

        return fails > 0

    def __run_test_method(self, test_instance, method: str) -> int:
        print(f"    Running {method}... \r", end="", flush=True)

        buffer = io.StringIO()
        fails = 0

        try:
            t0 = time.time()

            with redirect_stdout(buffer):
                if hasattr(test_instance, "beforeEach"):
                    test_instance.beforeEach()

                getattr(test_instance, method)()

                if hasattr(test_instance, "afterEach"):
                    test_instance.afterEach()

            duration = time.time() - t0

            self.print_success(f" {method} ({duration * 1000:.2f}ms)")
            self.passed_tests += 1
        except Exception as e:
            if isinstance(e, TimeoutException):
                self.print_failure(
                    f" {method} (hit execution limit of {e.timeout} seconds)"
                )

            if isinstance(e, AssertionError):
                self.__handle_assertion_error(t0, method)

            if self.stop_on_failure:
                self.__print_buffer_output(buffer)
                raise e

            fails += 1
        finally:
            self.__print_buffer_output(buffer)

        return fails

    def __handle_assertion_error(self, start_time, method: str):
        duration = time.time() - start_time
        self.print_failure(f" {method} ({duration * 1000:.2f}ms)")
        traceback_output = "".join(traceback.format_tb(sys.exc_info()[2]))

        colored_traceback = "\n".join(
            f"  {CYAN_COLOR}{line}{RESET_COLOR}"
            for line in traceback_output.splitlines()
        )

        print(colored_traceback)

    def __print_buffer_output(self, buffer: io.StringIO):
        output = buffer.getvalue()
        if output:
            indented_output = "\n".join(f"    {line}" for line in output.splitlines())
            print(f"    {RED_COLOR}⎯⎯⎯⎯⎯OUTPUT⎯⎯⎯⎯⎯{RESET_COLOR}")
            print(f"{GRAY_COLOR}{indented_output}{RESET_COLOR}")
            print(f"    {RED_COLOR}⎯⎯⎯⎯⎯OUTPUT⎯⎯⎯⎯⎯{RESET_COLOR}")

    def __print_summary(self, duration: float):
        print(f"\n{WHITE_BOLD}Test Summary{RESET_COLOR}\n")
        print(
            f"    Test Suites: {GREEN_COLOR}{self.passed_test_suites} passed{RESET_COLOR}, {RED_COLOR}{self.failed_test_suites} failed{RESET_COLOR}, {self.passed_test_suites + self.failed_test_suites} total"
        )
        print(
            f"    Tests:       {GREEN_COLOR}{self.passed_tests} passed{RESET_COLOR}, {RED_COLOR}{self.failed_tests} failed{RESET_COLOR}, {self.passed_tests + self.failed_tests} total"
        )
        print(f"    Time:        {duration}s\n")

    def print_failure(self, add: str):
        print(f"    {RED_COLOR}✗{RESET_COLOR}{add}", flush=True)

    def print_success(self, add: str):
        print(f"    {GREEN_COLOR}✓{RESET_COLOR}{add}", flush=True)


class Stockfish:
    def __init__(
        self,
        prefix: List[str],
        path: str,
        args: List[str] = [],
        cli: bool = False,
    ):
        self.path = path
        self.process = None
        self.args = args
        self.cli = cli
        self.prefix = prefix
        self.output = []

        self.start()

    def _check_process_alive(self):
        if not self.process or self.process.poll() is not None:
            print("\n".join(self.output))
            raise RuntimeError("Stockfish process has terminated")

    def start(self):
        if self.cli:
            self.process = subprocess.run(
                self.prefix + [self.path] + self.args,
                capture_output=True,
                text=True,
            )

            if self.process.returncode != 0:
                print(self.process.stdout)
                print(self.process.stderr)
                print(f"Process failed with return code {self.process.returncode}")

            return

        self.process = subprocess.Popen(
            self.prefix + [self.path] + self.args,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            bufsize=1,
        )

    def setoption(self, name: str, value: str):
        self.send_command(f"setoption name {name} value {value}")

    def send_command(self, command: str):
        if not self.process:
            raise RuntimeError("Stockfish process is not started")

        self._check_process_alive()

        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    @timeout_decorator(MAX_TIMEOUT)
    def equals(self, expected_output: str):
        for line in self.readline():
            if line == expected_output:
                return

    @timeout_decorator(MAX_TIMEOUT)
    def expect(self, expected_output: str):
        for line in self.readline():
            if fnmatch.fnmatch(line, expected_output):
                return

    @timeout_decorator(MAX_TIMEOUT)
    def contains(self, expected_output: str):
        for line in self.readline():
            if expected_output in line:
                return

    @timeout_decorator(MAX_TIMEOUT)
    def starts_with(self, expected_output: str):
        for line in self.readline():
            if line.startswith(expected_output):
                return

    @timeout_decorator(MAX_TIMEOUT)
    def check_output(self, callback):
        if not callback:
            raise ValueError("Callback function is required")

        for line in self.readline():
            if callback(line) == True:
                return

    def readline(self):
        if not self.process:
            raise RuntimeError("Stockfish process is not started")

        while True:
            self._check_process_alive()
            line = self.process.stdout.readline().strip()
            self.output.append(line)

            yield line

    def clear_output(self):
        self.output = []

    def get_output(self) -> List[str]:
        return self.output

    def quit(self):
        self.send_command("quit")

    def close(self):
        if self.process:
            self.process.stdin.close()
            self.process.stdout.close()
            return self.process.wait()

        return 0


class TestNNUEThreadSafety(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.binary = cls._find_binary()
        if cls.binary is None:
            raise unittest.SkipTest("Stockfish binary not available")

        cls.default_eval_file = cls._probe_option_default("EvalFile")

    @staticmethod
    def _find_binary():
        candidates = []

        env_path = os.environ.get("STOCKFISH_BINARY")
        if env_path:
            candidates.append(pathlib.Path(env_path))

        candidates.extend(
            [
                PATH.parent / "stockfish",
                PATH.parent / "build" / "stockfish",
                PATH.parent / "src" / "stockfish",
            ]
        )

        for candidate in candidates:
            if candidate and os.path.isfile(candidate) and os.access(candidate, os.X_OK):
                return str(candidate)

        return None

    @classmethod
    def _probe_option_default(cls, name: str):
        runner = Stockfish([], cls.binary, args=["uci"], cli=True)

        if not runner.process or runner.process.stdout is None:
            return None

        target = f"option name {name} "
        for line in runner.process.stdout.splitlines():
            if target in line and " default " in line:
                return line.split("default", 1)[1].strip()

        return None

    def setUp(self):
        self.engine = Stockfish([], self.binary)
        self.engine.send_command("uci")
        self.engine.expect("uciok")
        self.engine.clear_output()

        self.engine.setoption("Experience Enabled", "false")
        self.engine.send_command("isready")
        self.engine.expect("readyok")
        self.engine.clear_output()

    def tearDown(self):
        if getattr(self, "engine", None):
            try:
                self.engine.quit()
            finally:
                self.engine.close()

    def _run_search(self, threads: int, moves=None, depth: int = 3):
        moves = moves or []

        self.engine.setoption("Threads", str(threads))
        self.engine.send_command("isready")
        self.engine.expect("readyok")
        self.engine.clear_output()

        self.engine.send_command("ucinewgame")
        position_cmd = "position startpos"
        if moves:
            position_cmd += " moves " + " ".join(moves)
        self.engine.send_command(position_cmd)

        self.engine.send_command(f"go depth {depth}")

        result = {}

        def parser(line: str):
            tokens = line.split()
            if len(tokens) >= 2 and tokens[0] == "info" and "score" in tokens:
                try:
                    score_index = tokens.index("score")
                    score_type = tokens[score_index + 1]
                    score_value = tokens[score_index + 2]
                    if score_type in {"cp", "mate"}:
                        result["score"] = (score_type, int(score_value))
                except (ValueError, IndexError):
                    pass

            if tokens and tokens[0] == "bestmove":
                result["bestmove"] = tokens[1] if len(tokens) > 1 else None
                return True

            return False

        self.engine.check_output(parser)
        self.engine.clear_output()

        self.assertIn("bestmove", result)
        return result["bestmove"], result.get("score")

    def test_consistent_scores_across_threads(self):
        moves = ["e2e4", "e7e5", "g1f3", "b8c6"]

        single_thread = self._run_search(1, moves=moves, depth=4)
        multi_thread = self._run_search(4, moves=moves, depth=4)

        self.assertEqual(single_thread[0], multi_thread[0])

        if single_thread[1] and multi_thread[1]:
            self.assertEqual(single_thread[1][0], multi_thread[1][0])
            self.assertLessEqual(abs(single_thread[1][1] - multi_thread[1][1]), 20)

    def test_network_reload_preserves_evaluation(self):
        if not self.default_eval_file:
            self.skipTest("Default EvalFile option not available")

        baseline = self._run_search(2, moves=["d2d4", "d7d5"], depth=3)

        self.engine.setoption("EvalFile", self.default_eval_file)
        self.engine.send_command("isready")
        self.engine.expect("readyok")
        self.engine.clear_output()

        after_reload = self._run_search(2, moves=["d2d4", "d7d5"], depth=3)

        self.assertIsNotNone(after_reload[0])

        if baseline[1] and after_reload[1]:
            self.assertEqual(baseline[1][0], after_reload[1][0])
            self.assertLessEqual(abs(baseline[1][1] - after_reload[1][1]), 20)
