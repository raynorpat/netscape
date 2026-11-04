#!/usr/bin/env python3
"""Retrieve the official 1998 WinFE as an unmodified, separate reference tree."""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import tarfile
import urllib.request

URL = "https://releases.mozilla.org/pub/mozilla/source/mozilla-19980408.tar.gz"
SHA256 = "2cfcc52d87834663d4aa0b4959c51f66451df965283b28fbfbad62d46741e013"
REFERENCES = {"LICENSE", "LEGAL", "include/fe_proto.h", "include/xp_core.h"}


def digest(path):
    checksum = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            checksum.update(block)
    return checksum.hexdigest()


def extract(archive, destination):
    files = []
    destination = destination.resolve()
    with tarfile.open(archive, "r:gz") as source:
        for member in source:
            path = PurePosixPath(member.name)
            if not path.parts or path.parts[0] != "ns":
                continue
            relative = PurePosixPath(*path.parts[1:])
            if "CVS" in relative.parts:
                continue
            selected = relative.as_posix() in REFERENCES or relative.parts[:2] in (
                ("cmd", "winfe"), ("cmd", "wincom")
            )
            if not selected or not member.isfile():
                continue
            if ".." in relative.parts or "\\" in relative.as_posix() or ":" in relative.as_posix():
                raise ValueError(f"Unsafe archive member: {member.name}")
            target = destination.joinpath(*relative.parts).resolve()
            if not target.is_relative_to(destination):
                raise ValueError(f"Archive member escapes destination: {member.name}")
            target.parent.mkdir(parents=True, exist_ok=True)
            with source.extractfile(member) as content, target.open("wb") as output:
                shutil.copyfileobj(content, output)
            files.append(relative.as_posix())
    manifest = {"url": URL, "sha256": SHA256, "files": sorted(files)}
    (destination / "inventory.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return len(files)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, default=Path("out/research/mozilla-19980408.tar.gz"))
    parser.add_argument("--destination", type=Path, default=Path("out/research/mozilla-19980408"))
    args = parser.parse_args()
    if not args.archive.exists():
        args.archive.parent.mkdir(parents=True, exist_ok=True)
        partial = args.archive.with_suffix(args.archive.suffix + ".part")
        with urllib.request.urlopen(URL, timeout=60) as source, partial.open("wb") as output:
            shutil.copyfileobj(source, output)
        if digest(partial) != SHA256:
            raise ValueError(f"SHA-256 mismatch for {partial}")
        partial.replace(args.archive)
    if digest(args.archive) != SHA256:
        raise ValueError(f"SHA-256 mismatch for {args.archive}")
    count = extract(args.archive, args.destination)
    print(f"Verified archive; extracted {count} reference files into {args.destination.resolve()}")


if __name__ == "__main__":
    main()
