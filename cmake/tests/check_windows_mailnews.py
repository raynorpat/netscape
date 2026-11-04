"""Exercise the native Windows mail/news UI against isolated local fixtures.

The executable must implement --mailnews-smoke REPORT mail|news [HOST:PORT].
Reports contain key=value lines captured from the live MSG-backed controls.
The test catches bypassing MIME decoding, failing to enumerate mail folders,
and displaying news without fetching its group, overview, and article.
"""

import argparse
import base64
from contextlib import contextmanager
from email import policy
from email.parser import BytesParser
import os
from pathlib import Path
import socketserver
import subprocess
import sys
import threading


GROUP = "test.native"
MAIL_SUBJECTS = ("Mail native smoke", "Second mail message")
NEWS_SUBJECTS = ("News native smoke", "Second news message")
MAIL_BODIES = ("Mail reader decoded body.", "Second mail decoded body.")
NEWS_BODIES = ("News reader decoded body.", "Second news decoded body.")
DATES = ("Tue, 02 Jan 1996 12:00:00 +0000", "Mon, 01 Jan 1996 12:00:00 +0000")
DELIVERY_SUBJECT = "Native delivery smoke"
DELIVERY_BODY = "Native delivery body"
DELIVERY_RECIPIENT = "recipient@example.test"


def message(number, news=False):
    """Return wire-format MIME, also used verbatim inside the mbox fixture."""
    subjects = NEWS_SUBJECTS if news else MAIL_SUBJECTS
    bodies = NEWS_BODIES if news else MAIL_BODIES
    subject = subjects[number - 1].replace(" ", "_")
    body = bodies[number - 1]
    if number == 1:
        encoding = "quoted-printable"
        body = body.replace("body", "=62ody")
    else:
        encoding = "base64"
        body = base64.b64encode(body.encode("ascii")).decode("ascii")
    headers = [
        "From: Native Fixture <sender@example.invalid>",
        "To: Native Reader <reader@example.invalid>",
        "Subject: =?us-ascii?Q?" + subject + "?=",
        "Date: " + DATES[number - 1],
        "Message-ID: <native-{}-{}@example.invalid>".format(
            "news" if news else "mail", number
        ),
        "MIME-Version: 1.0",
        "Content-Type: text/plain; charset=us-ascii",
        "Content-Transfer-Encoding: " + encoding,
    ]
    if news:
        headers.append("Newsgroups: " + GROUP)
    return ("\r\n".join(headers) + "\r\n\r\n" + body + "\r\n").encode("ascii")


def seed_mail(profile):
    mail = profile / "Mail"
    mail.mkdir(parents=True, exist_ok=True)
    # Remove only summaries for our named fixtures: an earlier run must not
    # substitute cached headers for parsing the newly written messages.
    for name in ("Inbox", "Drafts", "Saved mail"):
        for suffix in (".snm", ".msf"):
            (mail / (name + suffix)).unlink(missing_ok=True)
    with (mail / "Inbox").open("wb") as inbox:
        for number in (1, 2):
            inbox.write(b"From sender@example.invalid Tue Jan 02 12:00:00 1996\n")
            inbox.write(message(number).replace(b"\r\n", b"\n"))
            inbox.write(b"\n")
    (mail / "Drafts").write_bytes(b"")
    (mail / "Saved mail").write_bytes(b"")


class WireHandler(socketserver.StreamRequestHandler):
    def setup(self):
        super().setup()
        self.connection.settimeout(8)

    def reply(self, line):
        self.wfile.write(line.encode("ascii") + b"\r\n")
        self.wfile.flush()

    def multiline(self, data):
        for line in data.splitlines():
            if line.startswith(b"."):
                line = b"." + line
            self.wfile.write(line + b"\r\n")
        self.wfile.write(b".\r\n")
        self.wfile.flush()

    def receive_message(self):
        data = bytearray()
        while len(data) < 1024 * 1024:
            line = self.rfile.readline(65537)
            if not line or len(line) > 65536:
                raise ValueError("Incomplete or oversized delivery fixture line")
            if line in (b".\r\n", b".\n"):
                self.server.messages.append(bytes(data))
                return
            if line.startswith(b".."):
                line = line[1:]
            data.extend(line)
        raise ValueError("Delivery fixture exceeded one megabyte")


class NewsHandler(WireHandler):
    """The 3.02 mknews.c MODE READER/LIST/GROUP/XOVER/ARTICLE dialogue."""

    def article_number(self, argument):
        if not argument:
            return 1
        for number in (1, 2):
            if argument == str(number) or argument == (
                "<native-news-{}@example.invalid>".format(number)
            ):
                return number
        return None

    def handle(self):
        try:
            self.reply("200 Native loopback fixture ready")
            for unused in range(100):
                line = self.rfile.readline(4097)
                if not line:
                    return
                if len(line) > 4096:
                    self.reply("500 Command too long")
                    return
                text = line.decode("ascii", errors="replace").strip()
                self.server.commands.append(text)
                command, unused, argument = text.partition(" ")
                command = command.upper()
                argument = argument.strip()
                if command == "QUIT":
                    self.reply("205 Closing connection")
                    return
                if command == "MODE" and argument.upper() == "READER":
                    self.reply("200 Reader mode")
                elif command == "CAPABILITIES":
                    self.reply("101 Capability list")
                    self.multiline(b"VERSION 2\r\nREADER\r\nOVER\r\nLIST ACTIVE NEWSGROUPS")
                elif command in ("LIST", "NEWGROUPS"):
                    self.reply("215 Newsgroups follow" if command == "LIST" else "231 New groups follow")
                    if argument.upper().startswith("NEWSGROUPS"):
                        self.multiline(b"test.native Native Windows fixture")
                    elif argument.upper().startswith("OVERVIEW.FMT"):
                        self.multiline(b"Subject:\r\nFrom:\r\nDate:\r\nMessage-ID:\r\nReferences:\r\n:bytes\r\n:lines")
                    else:
                        self.multiline(b"" if self.server.empty_list else b"test.native 2 1 y")
                elif command in ("GROUP", "LISTGROUP"):
                    if argument and argument.split()[0] != GROUP:
                        self.reply("411 No such group")
                    else:
                        self.reply("211 2 1 2 " + GROUP)
                        if command == "LISTGROUP":
                            self.multiline(b"1\r\n2")
                elif command in ("XOVER", "OVER"):
                    self.reply("224 Overview follows")
                    rows = []
                    for number in (1, 2):
                        rows.append((
                            "{}\t=?us-ascii?Q?{}?=\tNative Fixture <sender@example.invalid>\t{}\t"
                            "<native-news-{}@example.invalid>\t\t{}\t1"
                        ).format(number, NEWS_SUBJECTS[number - 1].replace(" ", "_"),
                                 DATES[number - 1], number, len(message(number, True))))
                    self.multiline("\r\n".join(rows).encode("ascii"))
                elif command == "XHDR":
                    self.reply("221 Header follows")
                    header = argument.split()[0].lower() if argument else "subject"
                    values = []
                    for number in (1, 2):
                        raw_headers = message(number, True).split(b"\r\n\r\n", 1)[0]
                        for field in raw_headers.split(b"\r\n"):
                            name, separator, value = field.partition(b":")
                            if separator and name.decode("ascii").lower() == header:
                                values.append(str(number).encode("ascii") + b" " + value.strip())
                    self.multiline(b"\r\n".join(values))
                elif command in ("HEAD", "ARTICLE", "BODY", "STAT"):
                    number = self.article_number(argument)
                    if number is None:
                        self.reply("430 No such article")
                        continue
                    code = {"HEAD": 221, "ARTICLE": 220, "BODY": 222, "STAT": 223}[command]
                    self.reply("{} {} <native-news-{}@example.invalid> Article follows".format(code, number, number))
                    article = message(number, True)
                    if command == "HEAD":
                        self.multiline(article.split(b"\r\n\r\n", 1)[0])
                    elif command == "BODY":
                        self.multiline(article.split(b"\r\n\r\n", 1)[1])
                    elif command == "ARTICLE":
                        self.multiline(article)
                elif command == "DATE":
                    self.reply("111 19960102120000")
                elif command == "POST":
                    self.reply("340 Send article, ending with a dot")
                    self.receive_message()
                    self.reply("240 Article received by loopback fixture")
                else:
                    self.reply("500 Unsupported fixture command")
        except (OSError, ValueError) as error:
            self.server.errors.append(str(error))


class SMTPHandler(WireHandler):
    """Receive a message locally; this fixture never relays or resolves mail."""

    def handle(self):
        sender = False
        recipient = False
        try:
            self.reply("220 localhost Native SMTP fixture")
            for unused in range(100):
                line = self.rfile.readline(4097)
                if not line:
                    return
                if len(line) > 4096:
                    raise ValueError("SMTP fixture command exceeded 4096 bytes")
                text = line.decode("ascii", errors="replace").strip()
                self.server.commands.append(text)
                upper = text.upper()
                if upper == "QUIT":
                    self.reply("221 Closing connection")
                    return
                if upper.startswith(("HELO ", "EHLO ")):
                    self.reply("250 localhost")
                elif upper.startswith("MAIL FROM:"):
                    sender = True
                    recipient = False
                    self.reply("250 Sender accepted")
                elif upper.startswith("RCPT TO:"):
                    if sender and DELIVERY_RECIPIENT.lower() in text.lower():
                        recipient = True
                        self.reply("250 Recipient accepted")
                    else:
                        self.reply("550 Unexpected fixture recipient")
                elif upper == "DATA" and sender and recipient:
                    self.reply("354 End data with a dot")
                    self.receive_message()
                    self.reply("250 Message received by loopback fixture")
                elif upper == "RSET":
                    sender = recipient = False
                    self.reply("250 Reset")
                elif upper == "NOOP":
                    self.reply("250 OK")
                else:
                    self.reply("503 Unexpected fixture command")
        except (OSError, ValueError) as error:
            self.server.errors.append(str(error))


@contextmanager
def fixture_server(output, handler, prefix, empty_list=False):
    server = socketserver.ThreadingTCPServer(("127.0.0.1", 0), handler)
    server.daemon_threads = True
    server.commands = []
    server.errors = []
    server.messages = []
    server.empty_list = empty_list
    worker = threading.Thread(target=server.serve_forever,
                              kwargs={"poll_interval": 0.05}, daemon=True)
    worker.start()
    try:
        yield server
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=2)
        (output / (prefix + "-commands.txt")).write_text(
            "\n".join(server.commands) + "\n", encoding="utf-8")
        (output / (prefix + "-errors.txt")).write_text(
            "\n".join(server.errors), encoding="utf-8")
        for index, data in enumerate(server.messages, 1):
            (output / (prefix + "-message-{}.eml".format(index))).write_bytes(data)


def news_server(output):
    return fixture_server(output, NewsHandler, "mailnews-nntp")


def run_process(executable, output, mode, profile, server=None, delivery=False,
                expected_returncode=0):
    profile.mkdir(parents=True, exist_ok=True)
    prefix = ("composer-delivery-" if delivery else "mailnews-") + mode
    report = output / (prefix + "-smoke.txt")
    report.unlink(missing_ok=True)
    environment = os.environ.copy()
    environment["NETSCAPE_PROFILE_DIR"] = str(profile)
    command = [str(executable), "--composer-delivery-smoke" if delivery else "--mailnews-smoke",
               str(report), mode]
    if server:
        command.append("127.0.0.1:{}".format(server.server_address[1]))
    try:
        process = subprocess.run(command, env=environment, cwd=str(output),
                                 capture_output=True, text=True, encoding="utf-8",
                                 errors="replace", timeout=35, check=False)
    except subprocess.TimeoutExpired as error:
        for name, value in (("stdout", error.stdout), ("stderr", error.stderr)):
            if isinstance(value, bytes):
                value = value.decode("utf-8", errors="replace")
            (output / (prefix + "-" + name + ".txt")).write_text(
                value or "", encoding="utf-8")
        raise RuntimeError(prefix + " UI exceeded the 35-second timeout") from error
    for name, value in (("stdout", process.stdout), ("stderr", process.stderr)):
        (output / (prefix + "-" + name + ".txt")).write_text(
            value, encoding="utf-8")
    if process.returncode != expected_returncode:
        raise RuntimeError("{} UI exited with status {}, expected {}".format(
            mode, process.returncode, expected_returncode))
    text = report.read_text(encoding="utf-8", errors="replace")
    fields = dict(line.split("=", 1) for line in text.splitlines() if "=" in line)
    return text, fields


def run_smoke(executable, output, mode, profile, server=None):
    text, fields = run_process(executable, output, mode, profile, server)
    for name, expected in (("passed", "1"), ("mode", mode), ("backend", "MSG"),
                           ("window_survived", "1"), ("composer", "1")):
        if fields.get(name) != expected:
            raise RuntimeError("{} report has unexpected {}:\n{}".format(mode, name, text))
    folders = [name.strip() for name in fields.get("folders", "").split("|")]
    expected_folders = ("Inbox", "Drafts", "Saved mail") if mode == "mail" else (GROUP,)
    if any(name not in folders for name in expected_folders):
        raise RuntimeError(mode + " folder controls are missing fixture folders:\n" + text)
    if int(fields.get("folder_count", "0")) < len(expected_folders):
        raise RuntimeError(mode + " folder control count is too small:\n" + text)
    subjects = fields.get("subjects", "").split("|")
    expected_subjects = MAIL_SUBJECTS if mode == "mail" else NEWS_SUBJECTS
    if int(fields.get("message_count", "0")) != 2 or any(
            subject not in subjects for subject in expected_subjects):
        raise RuntimeError(mode + " message controls did not decode both MIME subjects:\n" + text)
    bodies = MAIL_BODIES if mode == "mail" else NEWS_BODIES
    if not any(body in fields.get("reader_text", "") for body in bodies):
        raise RuntimeError(mode + " reader did not render a decoded MIME body:\n" + text)


def run_empty_news(executable, output):
    # An empty LIST must fail once with MK_EMPTY_NEWS_LIST, rather than using
    # a stale byte count from MODE READER and re-entering LIST indefinitely.
    with fixture_server(output, NewsHandler, "mailnews-empty-nntp", empty_list=True) as server:
        profile = output / "empty news profile with spaces 100%"
        text, fields = run_process(executable, output, "news", profile, server,
                                   expected_returncode=4)
        for name, expected in (("passed", "0"), ("mode", "news"), ("backend", "MSG"),
                               ("status", "-227"), ("message_count", "0"),
                               ("window_survived", "1")):
            if fields.get(name) != expected:
                raise RuntimeError("Empty news report has unexpected {}:\n{}".format(name, text))
        commands = [command.split()[0].upper() for command in server.commands if command]
        if commands.count("LIST") != 1:
            raise RuntimeError("Empty news client sent {} LIST commands, expected one".format(
                commands.count("LIST")))
        if any(command in commands for command in ("GROUP", "LISTGROUP", "XOVER", "XHDR", "HEAD", "ARTICLE")):
            raise RuntimeError("Empty news client attempted to retrieve a nonexistent group or article")


def run_delivery(executable, output, mode):
    protocol = "smtp" if mode == "mail" else "nntp"
    handler = SMTPHandler if mode == "mail" else NewsHandler
    with fixture_server(output, handler, "composer-delivery-" + protocol) as server:
        profile = output / ("delivery " + mode + " profile with spaces 100%")
        text, fields = run_process(executable, output, mode, profile, server, delivery=True)
        if fields.get("delivered") != "1" or fields.get("protocol") != protocol:
            raise RuntimeError(mode + " composer did not report delivery:\n" + text)
        if len(server.messages) != 1:
            raise RuntimeError("{} fixture received {} messages, expected one".format(
                protocol, len(server.messages)))
        parsed = BytesParser(policy=policy.default).parsebytes(server.messages[0])
        if str(parsed.get("Subject", "")) != DELIVERY_SUBJECT:
            raise RuntimeError(protocol + " delivery Subject differs from the native composer fixture")
        target = str(parsed.get("To" if mode == "mail" else "Newsgroups", ""))
        if (DELIVERY_RECIPIENT if mode == "mail" else GROUP) not in target:
            raise RuntimeError(protocol + " delivery recipient/group is missing")
        bodies = []
        for part in parsed.walk():
            if part.get_content_type() == "text/plain":
                body = part.get_payload(decode=True)
                if body:
                    bodies.append(body.decode(part.get_content_charset() or "ascii", errors="replace"))
        if not any(DELIVERY_BODY in body for body in bodies):
            raise RuntimeError(protocol + " server did not receive the composed body")
        commands = [command.split()[0].upper() for command in server.commands if command]
        required = ("MAIL", "RCPT", "DATA") if mode == "mail" else ("POST",)
        if any(command not in commands for command in required):
            raise RuntimeError(protocol + " server did not receive its delivery dialogue")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", "--exe", type=Path, required=True)
    parser.add_argument("--output", "--output-dir", type=Path, required=True)
    parser.add_argument("--mode", choices=("mail", "news", "all", "empty-news"), default="all")
    parser.add_argument("--delivery-fixtures", action="store_true",
                        help="Also send composed messages only to local SMTP/NNTP fixtures")
    args = parser.parse_args()
    if args.mode == "empty-news" and args.delivery_fixtures:
        parser.error("--delivery-fixtures requires --mode mail, news, or all")
    executable = args.executable.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    try:
        if args.mode == "empty-news":
            run_empty_news(executable, output)
        if args.mode in ("all", "mail"):
            mail_profile = output / "mail profile with spaces 100%"
            seed_mail(mail_profile)
            run_smoke(executable, output, "mail", mail_profile)
        if args.mode in ("all", "news"):
            with news_server(output) as server:
                run_smoke(executable, output, "news", output / "news profile with spaces 100%", server)
                commands = [command.split()[0].upper() for command in server.commands if command]
                for required in ("LIST", "GROUP", "ARTICLE"):
                    if required not in commands:
                        raise RuntimeError("Native news client did not send " + required)
                if not any(command in commands for command in ("XOVER", "HEAD", "XHDR")):
                    raise RuntimeError("Native news client did not fetch message headers")
        if args.delivery_fixtures:
            for mode in (("mail", "news") if args.mode == "all" else (args.mode,)):
                run_delivery(executable, output, mode)
    except (OSError, RuntimeError, ValueError) as error:
        print("Windows mail/news smoke failed: " + str(error), file=sys.stderr)
        return 1
    if args.mode == "empty-news":
        print("Windows empty news smoke passed: one LIST, status -227, no group download")
    else:
        print("Windows {} smoke passed: MSG folders, MIME headers, native reader, composer{}".format(
            args.mode, ", and local delivery" if args.delivery_fixtures else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
