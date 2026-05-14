import gzip
import os
import pathlib
import struct
import sys
import tempfile
import types
import unittest

TESTS_DIR = pathlib.Path(__file__).resolve().parent
if str(TESTS_DIR) not in sys.path:
    sys.path.append(str(TESTS_DIR))

if "requests" not in sys.modules:
    requests_stub = types.ModuleType("requests")

    def _requests_unavailable(*_args, **_kwargs):
        raise RuntimeError("requests is not available in the test environment")

    requests_stub.get = _requests_unavailable
    sys.modules["requests"] = requests_stub

from testing import Stockfish


def find_engine_binary():
    candidates = []

    env_path = os.environ.get("STOCKFISH_BINARY")
    if env_path:
        candidates.append(pathlib.Path(env_path))

    root = pathlib.Path(__file__).resolve().parent.parent
    search_dirs = [root, root / "build", root / "src"]
    for directory in search_dirs:
        if directory.is_dir():
            candidates.extend(sorted(directory.glob("[Ww]ordfish*")))
            candidates.extend(sorted(directory.glob("[Ss]tockfish*")))

    for candidate in candidates:
        if candidate and candidate.is_file() and os.access(candidate, os.X_OK):
            return str(candidate)
    return None


class EngineTestCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = find_engine_binary()
        if cls.binary is None:
            raise unittest.SkipTest("Wordfish binary not available")

    def setUp(self):
        self.engine = Stockfish([], self.binary)
        self.engine.send_command("uci")
        self.engine.expect("uciok")
        self.engine.clear_output()

    def tearDown(self):
        if getattr(self, "engine", None):
            try:
                self.engine.quit()
            finally:
                self.engine.close()


class ExperienceFileTests(EngineTestCase):
    KEY = 0x123456789ABCDEF0
    MOVE = (12 << 6) + 28  # e2e4 in Stockfish's internal representation
    SCORE = 37
    DEPTH = 16
    COUNT = 3

    @classmethod
    def _experience_payload(cls):
        header = b"SugaR Experience version 2"
        record = struct.pack("<Q I i i H 2x", cls.KEY, cls.MOVE, cls.SCORE, cls.DEPTH, cls.COUNT)
        return header + record

    def _write_experience_file(self, suffix, writer):
        with tempfile.NamedTemporaryFile("wb", suffix=suffix, delete=False) as handle:
            writer(handle)
            return handle.name

    def _load_experience(self, path):
        self.engine.setoption("Experience File", path)
        self.engine.setoption("Experience Enabled", "true")
        self.engine.send_command("isready")

        lines = []

        def parser(output):
            lines.append(output)
            return output == "readyok"

        self.engine.check_output(parser)
        self.engine.clear_output()
        return [line for line in lines if line.startswith("info string ")]

    def _dump_entries(self):
        self.engine.send_command("experience dump")

        dump_lines = []

        def parser(output):
            dump_lines.append(output)
            return output == "info string experience dump end"

        self.engine.check_output(parser)
        self.engine.clear_output()

        self.assertIn("info string experience dump begin", dump_lines)
        self.assertIn("info string experience dump end", dump_lines)

        entries = []
        for line in dump_lines:
            if line.startswith("info string experience entry "):
                tokens = line.split()
                key, move, score, depth, count = map(int, tokens[4:9])
                entries.append((key, move, score, depth, count))

        return entries

    def test_uncompressed_exp_file(self):
        payload = self._experience_payload()

        exp_path = self._write_experience_file(".exp", lambda handle: handle.write(payload))
        self.addCleanup(lambda: os.remove(exp_path))

        info_lines = self._load_experience(exp_path)
        self.assertTrue(any("Total moves: 1" in line for line in info_lines))
        self.assertTrue(any("Total positions: 1" in line for line in info_lines))
        self.assertTrue(any("Duplicate moves: 0" in line for line in info_lines))

        entries = self._dump_entries()
        self.assertEqual(entries, [(self.KEY, self.MOVE, self.SCORE, self.DEPTH, self.COUNT)])

    def test_compressed_ccz_file(self):
        payload = self._experience_payload()

        def write_ccz(handle):
            with gzip.GzipFile(fileobj=handle, mode="wb") as gz:
                gz.write(payload)

        ccz_path = self._write_experience_file(".ccz", write_ccz)
        self.addCleanup(lambda: os.remove(ccz_path))

        info_lines = self._load_experience(ccz_path)
        self.assertTrue(any("Total moves: 1" in line for line in info_lines))
        self.assertTrue(any("Total positions: 1" in line for line in info_lines))
        self.assertTrue(any("Duplicate moves: 0" in line for line in info_lines))

        entries = self._dump_entries()
        self.assertEqual(entries, [(self.KEY, self.MOVE, self.SCORE, self.DEPTH, self.COUNT)])

    def test_duplicate_entries_accumulate_visit_count(self):
        header = "# Wordfish experience format v1\n".encode()
        first  = f"{self.KEY} {self.MOVE} {self.SCORE} {self.SCORE} {self.DEPTH} 2\n".encode()
        second = f"{self.KEY} {self.MOVE} {self.SCORE} {self.SCORE} {self.DEPTH} 5\n".encode()

        exp_path = self._write_experience_file(".exp", lambda handle: handle.write(header + first + second))
        self.addCleanup(lambda: os.remove(exp_path))

        self._load_experience(exp_path)

        entries = self._dump_entries()
        self.assertEqual(entries, [(self.KEY, self.MOVE, self.SCORE, self.DEPTH, 7)])

    def test_text_experience_without_visits_defaults_to_single_visit(self):
        header = "# Wordfish experience format v1\n".encode()
        entry  = f"{self.KEY} {self.MOVE} {self.SCORE} {self.SCORE} {self.DEPTH}\n".encode()

        exp_path = self._write_experience_file(".exp", lambda handle: handle.write(header + entry))
        self.addCleanup(lambda: os.remove(exp_path))

        self._load_experience(exp_path)

        entries = self._dump_entries()
        self.assertEqual(entries, [(self.KEY, self.MOVE, self.SCORE, self.DEPTH, 1)])


class PolyglotBookTests(EngineTestCase):
    STARTPOS_MOVE = (12 << 6) + 28  # e2e4 in Polyglot encoding

    def _read_polyglot_key(self):
        key_line = None

        def parser(output):
            nonlocal key_line
            if output.startswith("info string polyglot key "):
                key_line = output
                return True

        self.engine.send_command("position startpos")
        self.engine.send_command("book key")
        self.engine.check_output(parser)
        self.engine.clear_output()

        if key_line is None:
            self.fail("Engine did not report a polyglot key")

        return int(key_line.rsplit(" ", 1)[-1])

    def _write_book(self, key):
        with tempfile.NamedTemporaryFile("wb", suffix=".bin", delete=False) as handle:
            entry = struct.pack(">QHHI", key, self.STARTPOS_MOVE, 1, 0)
            handle.write(entry)
            return handle.name

    def _load_book(self, path):
        self.engine.setoption("Book1 File", path)
        self.engine.send_command("isready")

        lines = []

        def parser(output):
            lines.append(output)
            return output == "readyok"

        self.engine.check_output(parser)
        self.engine.clear_output()
        return lines

    def test_polyglot_key_ignores_non_capturable_ep_square(self):
        fen_without_ep = "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"
        fen_with_ep = "4k3/8/8/8/8/8/4P3/4K3 w - e6 0 1"

        def read_for_fen(fen):
            key_line = None

            def parser(output):
                nonlocal key_line
                if output.startswith("info string polyglot key "):
                    key_line = output
                    return True

            self.engine.send_command(f"position fen {fen}")
            self.engine.send_command("book key")
            self.engine.check_output(parser)
            self.engine.clear_output()

            if key_line is None:
                self.fail("Engine did not report a polyglot key")

            return int(key_line.rsplit(" ", 1)[-1])

        self.assertEqual(read_for_fen(fen_without_ep), read_for_fen(fen_with_ep))

    def test_polyglot_book_generated_in_test(self):
        key       = self._read_polyglot_key()
        book_path = self._write_book(key)
        self.addCleanup(lambda: os.remove(book_path))

        info_lines = self._load_book(book_path)
        self.assertTrue(any(f"Book loaded: {book_path}" in line for line in info_lines))

        self.engine.send_command("position startpos")
        self.engine.send_command("book")
        self.engine.starts_with("bestmove e2e4")

    def test_polyglot_book_generated_by_engine(self):
        self.engine.send_command("position startpos")

        with tempfile.NamedTemporaryFile("wb", suffix=".bin", delete=False) as handle:
            book_path = handle.name

        self.addCleanup(lambda: os.path.exists(book_path) and os.remove(book_path))

        os.remove(book_path)
        self.engine.send_command(f"book generate {book_path} e2e4")
        self.engine.starts_with(f"info string Generated polyglot book at {book_path}")

        info_lines = self._load_book(book_path)
        self.assertTrue(any(f"Book loaded: {book_path}" in line for line in info_lines))

        self.engine.send_command("position startpos")
        self.engine.send_command("book")
        self.engine.starts_with("bestmove e2e4")


if __name__ == "__main__":
    unittest.main()
