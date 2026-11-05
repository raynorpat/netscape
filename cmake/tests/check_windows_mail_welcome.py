"""Check the original welcome via native MSG/MIME/LO and preserve existing mail."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

PAYLOAD_SHA256 = "d776835cbba16e0af5bbde1f4e2e1878a58ab813e65486c60a24c25c3dca35dc"


def run(executable, output, profile, name, target=None):
    report = output / (name + ".txt")
    env = os.environ.copy()
    env["NETSCAPE_PROFILE_DIR"] = str(profile)
    command = [str(executable), "--mail-welcome-smoke", str(report), str(output / (name + ".bmp"))]
    if target is not None:
        command.append(str(target))
    result = subprocess.run(command, env=env, cwd=output,
                            capture_output=True, timeout=18)
    (output / (name + ".stderr")).write_bytes(result.stderr)
    if result.returncode:
        raise RuntimeError(f"{name} native fixture exited {result.returncode}")
    fields = dict(line.split("=", 1) for line in report.read_text(errors="replace").splitlines() if "=" in line)
    assert fields["about_outstanding"] == "0", fields
    if target is None:
        assert fields["captured"] == "1", fields
    return fields


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="profile ", dir=output) as directory:
        profile = Path(directory)
        fresh = run(args.executable.resolve(), output, profile, "fresh")
        assert fresh["count"] == "1", fresh
        assert fresh["subject"] == "Welcome!" and fresh["sender"] == "Mozilla", fresh
        assert "1995" in fresh["date"], fresh
        assert fresh["logo_drawn"] == "1", fresh
        assert fresh["auto_folder"] == fresh["auto_message"] == fresh["auto_reader"] == "1", fresh
        assert set(fresh["folders"].split("|")) == {"Inbox", "Sent"}, fresh
        for text in ("Features in Brief", "Learn More about Netscape Mail", "Supplemental Resources"):
            assert text in fresh["reader"], fresh
        inbox = profile / "Mail" / "Inbox"
        original = inbox.read_bytes()
        payload = original.split(b"\n", 1)[1]
        assert hashlib.sha256(payload).hexdigest() == PAYLOAD_SHA256
        reopened = run(args.executable.resolve(), output, profile, "reopened")
        assert reopened["count"] == "1" and reopened["subject"] == "Welcome!", reopened
        assert inbox.read_bytes() == original, "Reopening changed the welcome payload"
        inbox.write_bytes(b"")
        (inbox.parent / "Inbox.snm").unlink(missing_ok=True)
        empty = run(args.executable.resolve(), output, profile, "empty")
        assert empty["count"] == "0" and inbox.read_bytes() == b"", empty
        custom = (b"From user@example.invalid Tue Jan 02 12:00:00 1996\n"
                  b"From: User <user@example.invalid>\nSubject: My saved mail\n"
                  b"Date: Tue, 02 Jan 1996 12:00:00 +0000\nMessage-ID: <saved@example.invalid>\n\nKeep this mail.\n")
        inbox.write_bytes(custom)
        (inbox.parent / "Inbox.snm").unlink(missing_ok=True)
        saved = run(args.executable.resolve(), output, profile, "saved")
        assert saved["count"] == "1" and not saved["subject"], saved
        assert inbox.read_bytes() == custom, "Existing user mail was changed"
    with tempfile.TemporaryDirectory(prefix="write ", dir=output) as directory:
        profile = Path(directory)
        mail = profile / "Mail"
        target = mail / "Inbox"
        mail.write_bytes(b"Keep the blocking file")
        failed = run(args.executable.resolve(), output, profile, "blocked", target)
        assert failed["created"] == "0" and mail.read_bytes() == b"Keep the blocking file", failed
        mail.unlink()
        mail.mkdir()
        # Exhaust all exclusive staging names; collisions belong to somebody else.
        for number in range(256):
            (mail / f".w{number:02x}").write_bytes(b"Owned by another writer")
        collision = run(args.executable.resolve(), output, profile, "collision", target)
        assert collision["created"] == "0" and not target.exists(), collision
        for path in mail.iterdir():
            assert path.read_bytes() == b"Owned by another writer"
            path.unlink()
        retry = run(args.executable.resolve(), output, profile, "retry", target)
        assert retry["created"] == "1", retry
        assert hashlib.sha256(target.read_bytes().split(b"\n", 1)[1]).hexdigest() == PAYLOAD_SHA256
        for number, existing in enumerate((b"", b"User mail must survive")):
            target.write_bytes(existing)
            preserved = run(args.executable.resolve(), output, profile, f"preserved-{number}", target)
            assert preserved["created"] == "1" and target.read_bytes() == existing, preserved
        assert list(mail.iterdir()) == [target], "Leaked first-run staging files"
        target.unlink()
        target.mkdir()
        refused = run(args.executable.resolve(), output, profile, "directory", target)
        assert refused["created"] == "0" and target.is_dir(), refused
        assert list(mail.iterdir()) == [target], "Failed rename leaked a staging file"
    print("Windows first-run welcome: original payload, automatic reader, restart and existing mail preserved")


if __name__ == "__main__":
    main()
