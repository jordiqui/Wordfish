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


class ExperienceFileTests(unittest.TestCase):
    KEY = 0x123456789ABCDEF0
    MOVE = (12 << 6) + 28  # e2e4 in Stockfish's internal representation
    SCORE = 37
    DEPTH = 16
    COUNT = 3

    @classmethod
    def setUpClass(cls):
        cls.binary = cls._find_binary()
        if cls.binary is None:
            raise unittest.SkipTest("Wordfish binary not available")

    @staticmethod
    def _find_binary():
        candidates = []

        env_path = os.environ.get("STOCKFISH_BINARY")
        if env_path:
            candidates.append(pathlib.Path(env_path))

        root = pathlib.Path(__file__).resolve().parent.parent
        search_dirs = [root, root / "build", root / "src"]
        for directory in search_dirs:
            if directory.is_dir():
                candidates.extend(sorted(directory.glob("wordfish*")))
                candidates.extend(sorted(directory.glob("stockfish*")))

        for candidate in candidates:
            if candidate and candidate.is_file() and os.access(candidate, os.X_OK):
                return str(candidate)
        return None

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


if __name__ == "__main__":
    unittest.main()
