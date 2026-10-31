"""Line endings may vary in JSON; changed values and CSV bytes must not pass."""
import importlib.util
from pathlib import Path
import unittest

source = Path(__file__).resolve().parents[1] / "benchmarks/tensile3d/unpack_archive.py"
spec = importlib.util.spec_from_file_location("archive", source)
archive = importlib.util.module_from_spec(spec)
spec.loader.exec_module(archive)

class IntegrityTest(unittest.TestCase):
    def test_json_line_endings(self):
        lf = b'{"dt": 0.1}\n'
        crlf = b'{"dt": 0.1}\r\n'
        self.assertEqual(archive.archive_digest(lf, "metadata.json"),
                         archive.archive_digest(crlf, "metadata.json"))
        self.assertNotEqual(archive.archive_digest(lf, "metadata.json"),
                            archive.archive_digest(b'{"dt": 0.2}\n', "metadata.json"))
    def test_csv_bytes_remain_exact(self):
        self.assertNotEqual(archive.archive_digest(b"a,b\n1,2\n", "data.csv"),
                            archive.archive_digest(b"a,b\r\n1,2\r\n", "data.csv"))

if __name__ == "__main__":
    unittest.main()
