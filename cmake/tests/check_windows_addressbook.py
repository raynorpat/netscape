"""Verify native address-book persistence and original nickname expansion."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

def run(executable, output, profile, reopen=False, read_error=False):
    name = "read-error" if read_error else "reopened" if reopen else "addressbook"
    env = os.environ.copy()
    env["NETSCAPE_PROFILE_DIR"] = str(profile)
    report = output / (name + ".txt")
    result = subprocess.run([str(executable), "--addressbook-read-error-smoke" if read_error else "--addressbook-reopen-smoke" if reopen else "--addressbook-smoke",
                             str(report), str(output / (name + ".bmp"))], env=env, cwd=output,
                            capture_output=True, timeout=25)
    (output / (name + ".stderr")).write_bytes(result.stderr)
    fields = dict(line.split("=", 1) for line in report.read_text(errors="replace").splitlines() if "=" in line)
    assert fields["address_context"] == "1", fields
    assert result.returncode == 0, fields
    if read_error:
        assert fields["read_save_blocked"] == "1", fields
        return fields
    for name in ("hidden_lookup", "seed_preserved", "contact_edited", "contact_deleted", "properties_cancel",
                 "invalid_rejected", "list_expanded", "save_retry", "persisted", "singleton", "native_tree",
                 "window_command", "captured", "closed_lookup", "core_edit_cancel"):
        assert fields[name] == "1", (name, fields)
    return fields

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="address profile ", dir=output) as directory:
        profile = Path(directory)
        (profile / "address.htm").write_text('<!DOCTYPE NETSCAPE-Addressbook-file-1>\n<TITLE>Addresses</TITLE>\n<DL><p>\n'
            '<DT><A HREF="mailto:seed@example.invalid" NICKNAME="seed">Existing &amp; Contact</A>\n</DL><p>\n')
        (profile / "bookmark.htm").write_text('<!DOCTYPE NETSCAPE-Bookmark-file-1>\n<DL><p>\n<DT><A HREF="about:blank">Preserve my bookmarks</A>\n</DL><p>\n')
        bookmarks = (profile / "bookmark.htm").read_bytes()
        run(args.executable.resolve(), output, profile)
        saved = (profile / "address.htm").read_bytes()
        (output / "addressbook-saved.htm").write_bytes(saved)
        assert saved.startswith(b"<!DOCTYPE NETSCAPE-Addressbook-file-1>"), saved[:80]
        assert b"discard@example.invalid" not in saved and b"Cancelled name" not in saved
        assert b"&amp;" in saved and b"&lt;native&gt;" in saved, saved
        run(args.executable.resolve(), output, profile, True)
        assert (profile / "address.htm").read_bytes() == saved, "Read-only reopen rewrote the address book"
        assert (profile / "bookmark.htm").read_bytes() == bookmarks, "Address book changed bookmarks"
    with tempfile.TemporaryDirectory(prefix="read failure ", dir=output) as directory:
        profile = Path(directory)
        blocked = profile / "address.htm"
        blocked.mkdir()
        (blocked / "keep.txt").write_bytes(b"Existing data")
        run(args.executable.resolve(), output, profile, read_error=True)
        assert (blocked / "keep.txt").read_bytes() == b"Existing data"
    print("Native address book passed")

if __name__ == "__main__":
    main()
