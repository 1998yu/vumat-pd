"""Expand the read-only gzip CSV archive into the ignored generated directory."""
from pathlib import Path
import gzip
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parent


def unpack():
    dest = ROOT / 'generated' / 'archive_results'
    for source in (ROOT / 'raw').rglob('*'):
        if not source.is_file():
            continue
        target = dest / source.relative_to(ROOT / 'raw')
        target.parent.mkdir(parents=True, exist_ok=True)
        if source.suffix == '.gz':
            target = target.with_suffix('')
            target.write_bytes(gzip.decompress(source.read_bytes()))
        else:
            shutil.copyfile(source, target)
    expected = json.loads((ROOT / 'expected' / 'analysis.json').read_text())
    for relative, digest in expected['hashes'].items():
        actual = hashlib.sha256((dest / relative).read_bytes()).hexdigest()
        if actual != digest:
            raise RuntimeError(f'Archive hash mismatch: {relative}')
    print(f"Verified {len(expected['hashes'])} original CSV hashes.")
    return dest


if __name__ == '__main__':
    print(unpack())
