"""Verify native address-book persistence and original nickname expansion."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from email import policy
from email.parser import BytesParser
from email.utils import getaddresses

def run(executable, output, profile, reopen=False, read_error=False, picker=False):
    name = ("picker" if (profile / "address.htm").is_file() else "picker-empty") if picker else "read-error" if read_error else "reopened" if reopen else "addressbook"
    env = os.environ.copy()
    env["NETSCAPE_PROFILE_DIR"] = str(profile)
    report = output / (name + ".txt")
    result = subprocess.run([str(executable), "--address-picker-smoke" if picker else "--addressbook-read-error-smoke" if read_error else "--addressbook-reopen-smoke" if reopen else "--addressbook-smoke",
                             str(report), str(output / (name + ".bmp"))], env=env, cwd=output,
                            capture_output=True, timeout=25)
    (output / (name + ".stderr")).write_bytes(result.stderr)
    fields = dict(line.split("=", 1) for line in report.read_text(errors="replace").splitlines() if "=" in line)
    if picker:
        assert fields["address_enabled"] == "1", fields
        assert result.returncode == 0, fields
        for field in ("picker_cancel", "picker_accepted", "recipients", "bcc_visible", "dirty_state", "repeat_deduplicated",
                     "body_attachments", "draft_saved", "owner_closed", "captured"):
            assert fields[field] == "1", (field, fields)
        draft = (profile / "Mail" / "Drafts").read_bytes().split(b"\n", 1)[1]
        (output / (name + "-draft.eml")).write_bytes(draft)
        message = BytesParser(policy=policy.default).parsebytes(draft)
        expected = {"to": {"existing@example.invalid"}, "cc": {"copy@example.invalid"}, "bcc": {"blind@example.invalid"}}
        if fields["empty_book"] == "0":
            expected["to"].add("alpha@example.invalid")
            expected["cc"].add("beta@example.invalid")
            expected["bcc"].add("seed@example.invalid")
        for header, recipients in expected.items():
            actual = [email.lower() for _, email in getaddresses(message.get_all(header, []))]
            assert len(actual) == len(recipients) and set(actual) == recipients, (header, actual, recipients)
        parts = list(message.walk())
        assert any(part.get_filename() == "picker-attachment.txt" for part in parts)
        assert any(b"Recipient picker body" in (part.get_payload(decode=True) or b"") for part in parts)
        return fields
    assert fields["address_context"] == "1", fields
    assert result.returncode == 0, fields
    if read_error:
        assert fields["read_save_blocked"] == "1", fields
        for name in ("retry_load_menu", "reload_cancel_preserved", "read_recovered"):
            assert fields[name] == "1", (name, fields)
        return fields
    for name in ("hidden_lookup", "seed_preserved", "contact_edited", "contact_deleted", "properties_cancel",
                 "invalid_rejected", "list_expanded", "save_retry", "persisted", "singleton", "native_tree",
                 "window_command", "captured", "closed_lookup", "core_edit_cancel", "undo_properties", "nickname_undo"):
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
        run(args.executable.resolve(), output, profile, picker=True)
        assert (profile / "address.htm").read_bytes() == saved, "Read-only reopen rewrote the address book"
        assert (profile / "bookmark.htm").read_bytes() == bookmarks, "Address book changed bookmarks"
    with tempfile.TemporaryDirectory(prefix="read failure ", dir=output) as directory:
        profile = Path(directory)
        blocked = profile / "address.htm"
        blocked.mkdir()
        (blocked / "keep.txt").write_bytes(b"Existing data")
        run(args.executable.resolve(), output, profile, read_error=True)
        assert (profile / "address.htm.unreadable" / "keep.txt").read_bytes() == b"Existing data"
        assert b"Recovered and saved" in blocked.read_bytes()
    with tempfile.TemporaryDirectory(prefix="empty picker ", dir=output) as directory:
        run(args.executable.resolve(), output, Path(directory), picker=True)
    print("Native address book passed")

if __name__ == "__main__":
    main()
