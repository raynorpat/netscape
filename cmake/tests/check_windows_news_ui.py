"""Exercise the classic News reference UI against a loopback NNTP server."""
import argparse
from pathlib import Path
import os
import subprocess
import sys
import tempfile

from check_windows_mailnews import WireHandler, fixture_server, message, DATES
from check_windows_mail_ui import Bitmap, read_report, rectangle, require_fields

GROUPS = ("news.announce.newusers", "news.announce.questions", "news.answers")
TOOLBAR = "To:News|To:Mail|Re:News|Re:Mail|Re:All|Forward|Previous|Next|Thread|Group|Print|Stop"


class NewsReferenceHandler(WireHandler):
    def handle(self):
        group = GROUPS[0]
        try:
            self.reply("200 Native News reference fixture")
            for unused in range(160):
                raw = self.rfile.readline(4097)
                if not raw or len(raw) > 4096:
                    return
                text = raw.decode("ascii", errors="replace").strip()
                self.server.commands.append(text)
                command, unused, argument = text.partition(" ")
                command = command.upper()
                if command == "QUIT":
                    self.reply("205 Goodbye")
                    return
                if command == "MODE":
                    self.reply("200 Reader mode")
                elif command == "LIST":
                    self.reply("215 Newsgroups follow")
                    self.multiline("\r\n".join(g + " 2 1 y" for g in GROUPS).encode("ascii"))
                elif command == "GROUP" and argument in GROUPS:
                    group = argument
                    self.reply("211 2 1 2 " + group)
                elif command == "XOVER":
                    self.reply("224 Overview follows")
                    rows = []
                    for number in (1, 2):
                        rows.append(("{}\t{}\tNative Fixture <sender@example.invalid>\t{}\t"
                                     "<native-news-{}@example.invalid>\t\t300\t1").format(
                                         number, ("News native smoke", "Second news message")[number - 1],
                                         DATES[number - 1], number))
                    self.multiline("\r\n".join(rows).encode("ascii"))
                elif command in ("ARTICLE", "HEAD", "BODY"):
                    number = 2 if "2" in argument else 1
                    self.reply("{} {} <native-news-{}@example.invalid> Article follows".format(
                        {"ARTICLE": 220, "HEAD": 221, "BODY": 222}[command], number, number))
                    article = message(number, True).replace(b"test.native", group.encode("ascii"))
                    if command == "HEAD":
                        article = article.split(b"\r\n\r\n", 1)[0]
                    elif command == "BODY":
                        article = article.split(b"\r\n\r\n", 1)[1]
                    self.multiline(article)
                else:
                    self.reply("500 Unsupported fixture command")
        except OSError as error:
            self.server.errors.append(str(error))


def run(executable, output):
    output.mkdir(parents=True, exist_ok=True)
    # Every invocation must observe NNTP discovery, including Release then Debug.
    profile = Path(tempfile.mkdtemp(prefix="news profile with spaces 100% ", dir=output))
    report = output / "news-ui.txt"
    snapshot = output / "news-ui.bmp"
    report.unlink(missing_ok=True)
    snapshot.unlink(missing_ok=True)
    env = os.environ.copy()
    env["NETSCAPE_PROFILE_DIR"] = str(profile)
    with fixture_server(output, NewsReferenceHandler, "news-reference") as server:
        endpoint = "127.0.0.1:{}".format(server.server_address[1])
        result = subprocess.run([str(executable), "--news-ui-smoke", str(report), str(snapshot), endpoint],
                                env=env, cwd=output, capture_output=True, timeout=45)
        (output / "news-ui-stdout.txt").write_bytes(result.stdout)
        (output / "news-ui-stderr.txt").write_bytes(result.stderr)
        if result.returncode:
            raise RuntimeError("News UI exited with status {} ({})".format(result.returncode, report))
        commands = [c.partition(" ")[0] for c in server.commands]
        for required in ("LIST", "GROUP", "XOVER", "ARTICLE"):
            if required not in commands:
                raise RuntimeError("News UI omitted actual NNTP " + required)
        if "POST" in commands:
            raise RuntimeError("News reference fixture must never post")
    text, fields = read_report(report)
    require_fields(fields, {
        "toolbar_labels": TOOLBAR, "menu_labels": "File|Edit|View|Message|Go|Options|Window|Help",
        "folder_columns": "News Server|Status|Unread", "article_columns": "Sender|Status|Subject",
        "directory_counts": "1", "browser_post": "1", "leaf_guard": "1",
        "empty_commands": "1", "tree_keyboard": "1", "tree_mouse": "1",
        "subscription_keyboard": "1", "subscription_mouse": "1", "view_retained": "1",
        "post_command": "1", "mail_command": "1", "reply_post": "1", "reply_mail": "1", "reply_both": "1",
        "mark_thread": "1", "mark_group": "1", "narrow_layout": "1", "splitter_clamps": "1",
        "logo_visible": "1", "reader_body": "1", "snapshot": "1", "location_visible": "0", "directory_visible": "0",
    }, text)
    if not all(g in fields.get("tree", "") for g in GROUPS):
        raise RuntimeError("News tree omits a loopback group")
    folders, messages, reader = [rectangle(fields, n) for n in ("folders", "messages", "reader")]
    if not (folders[2] >= messages[2] * .42 and folders[3] > reader[3] * .55
            and reader[0] == 0 and reader[2] == messages[2] and reader[1] >= folders[3]):
        raise RuntimeError("News reference pane proportions differ: " + text)
    bitmap = Bitmap(snapshot)
    logo = bitmap.region(rectangle(fields, "logo_bounds"))
    if logo["colorful"] < 15 or logo["dark"] < 15:
        raise RuntimeError("News capture lacks the visible Netscape toolbar logo")
    region = bitmap.region((reader[0] + 5, reader[1] + 5, reader[2] - 22, reader[3] - 22))
    gray = 0
    for y in range(reader[1] + 5, reader[3] - 22):
        row = bitmap.height - 1 - y if bitmap.bottom_up else y
        for x in range(reader[0] + 5, reader[2] - 22):
            offset = bitmap.offset + row * bitmap.stride + x * bitmap.pixel_bytes
            gray += bitmap.data[offset:offset + 3] == bytes((192, 192, 192))
    if gray < region["pixels"] * .85:
        raise RuntimeError("Empty News reader is not classic gray")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        run(args.executable.resolve(), args.output.resolve())
    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
        print("News reference UI failed: " + str(error), file=sys.stderr)
        return 1
    print("News reference UI passed: native chrome, tree, panes and loopback NNTP")
    return 0


if __name__ == "__main__":
    sys.exit(main())
