"""Expand the read-only gzip CSV archive into the ignored generated directory."""
from pathlib import Path
import gzip
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parent


def archive_digest(content, relative):
    """JSON permits CRLF/LF checkouts; all other bytes, especially CSV, are exact."""
    if relative.endswith('.json'):
        content = content.replace(b'\r\n', b'\n')
    return hashlib.sha256(content).hexdigest()


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
    expected = json.loads((ROOT / 'expected' / 'archive_hashes.json').read_text())
    for relative, digest in expected.items():
        actual = archive_digest((dest / relative).read_bytes(), relative)
        if actual != digest:
            raise RuntimeError(f'Archive hash mismatch: {relative}')
    print(f"Verified {len(expected)} archived CSV/metadata hashes (including FEM symmetry).")
    return dest


if __name__ == '__main__':
    print(unpack())
