"""Check native Mail and Message Composition against the reference layouts.

The executable implements --mail-ui-smoke REPORT SNAPSHOT.bmp and
--compose-ui-smoke REPORT SNAPSHOT.bmp QUOTE_URL. Fixtures use an isolated local profile;
no public network or mail delivery is needed. Reports use client coordinates,
and the primary BMP captures the client area at its original size.
"""

import argparse
import base64
from contextlib import contextmanager
from email import policy
from email.parser import BytesParser
import http.server
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import threading
from urllib.parse import urlsplit


MAIL_TOOLBAR = "Get Mail|Delete|To:Mail|Re:Mail|Re:All|Forward|Previous|Next|Print|Stop"
MAIL_MENUS = "File|Edit|View|Message|Go|Options|Window|Help"
COMPOSE_TOOLBAR = "Send|Quote|Attach|Address|Stop"
COMPOSE_MENUS = "File|Edit|View|Options|Window"
SUBJECTS = ("Hello!", "Second message")
BODIES = ("Hi User,\n\nJust a test.\n", "Second message body.\n" + "".join(
    "Reader scroll line {:02d}: this message continues below the visible pane.\n".format(line)
    for line in range(1, 81)))
QUOTE_BODY = b"<HTML><BODY><P>Quoted reference text</P></BODY></HTML>\n"


class QuoteHandler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def setup(self):
        super().setup()
        self.connection.settimeout(3)

    def do_GET(self):
        if urlsplit(self.path).path != "/quote-source":
            self.send_error(404)
            return
        with self.server.request_lock:
            request = {"path": "/quote-source", "number": len(self.server.requests) + 1,
                       "body_sent": False}
            self.server.requests.append(request)
        try:
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=us-ascii")
            self.send_header("Content-Length", str(len(QUOTE_BODY)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.flush()
            # Keep the translation context alive long enough for native Stop.
            # A threaded server lets the retry finish while this request waits.
            if request["number"] == 1 and self.server.stopping.wait(1.5):
                return
            self.wfile.write(QUOTE_BODY)
            self.wfile.flush()
            request["body_sent"] = True
        except OSError:
            # Interrupting the first quote intentionally closes its socket.
            pass

    def log_message(self, format, *values):
        pass


@contextmanager
def quote_server(output):
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), QuoteHandler)
    server.daemon_threads = True
    server.block_on_close = False
    server.requests = []
    server.request_lock = threading.Lock()
    server.stopping = threading.Event()
    worker = threading.Thread(target=server.serve_forever,
                              kwargs={"poll_interval": 0.05}, daemon=True)
    worker.start()
    try:
        yield server
    finally:
        server.stopping.set()
        server.shutdown()
        server.server_close()
        worker.join(timeout=2)
        (output / "compose-ui-quote-requests.json").write_text(
            json.dumps(server.requests, indent=2) + "\n", encoding="utf-8")


def seed_mail(profile):
    """Use real MIME messages, including encoded headers and both body encodings."""
    mail = profile / "Mail"
    mail.mkdir(parents=True, exist_ok=True)
    for name in ("Inbox", "Drafts", "Saved mail"):
        for suffix in (".snm", ".msf"):
            (mail / (name + suffix)).unlink(missing_ok=True)
    messages = []
    for number in range(2):
        headers = [
            "From: " + ("=?us-ascii?Q?Your_Name?= <user@example.invalid>"
                        if number == 0 else "Second Sender <second@example.invalid>"),
            "To: User <reader@example.invalid>",
            "Subject: " + ("=?us-ascii?Q?Hello!?=" if number == 0 else SUBJECTS[1]),
            "Date: " + ("Tue, 02 Jan 1996 12:00:00 +0000" if number == 0
                         else "Mon, 01 Jan 1996 12:00:00 +0000"),
            "Message-ID: <reference-mail-{}@example.invalid>".format(number + 1),
            "MIME-Version: 1.0",
            "Content-Type: text/plain; charset=us-ascii",
            "Content-Transfer-Encoding: " + ("quoted-printable" if number == 0 else "base64"),
        ]
        body = (BODIES[0].replace("test", "=74est") if number == 0 else
                base64.b64encode(BODIES[1].encode("ascii")).decode("ascii") + "\n")
        messages.append(("\n".join(headers) + "\n\n" + body).encode("ascii"))
    with (mail / "Inbox").open("wb") as inbox:
        for message in messages:
            inbox.write(b"From user@example.invalid Tue Jan 02 12:00:00 1996\n")
            inbox.write(message)
            inbox.write(b"\n")
    (mail / "Drafts").write_bytes(b"")
    (mail / "Saved mail").write_bytes(b"")


def check_mail_preserved(profile):
    """Navigation may mark messages read; it must retain their actual MIME data."""
    data = (profile / "Mail" / "Inbox").read_bytes()
    chunks = data.split(b"\nFrom ")
    if len(chunks) != 2 or not chunks[0].startswith(b"From "):
        raise RuntimeError("Mail UI changed the two-message Inbox fixture")
    for number, chunk in enumerate(chunks):
        message = BytesParser(policy=policy.default).parsebytes(chunk.split(b"\n", 1)[1])
        if str(message["Subject"]) != SUBJECTS[number]:
            raise RuntimeError("Mail UI changed the fixture Subject")
        body = message.get_payload(decode=True).decode("ascii").replace("\r\n", "\n")
        if body.strip() != BODIES[number].strip():
            raise RuntimeError("Mail UI changed the fixture MIME body")


def read_report(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    fields = dict(line.split("=", 1) for line in text.splitlines() if "=" in line)
    return text, fields


def require_fields(fields, expected, text):
    for name, value in expected.items():
        if fields.get(name) != value:
            raise RuntimeError("UI report has unexpected {} (expected {!r}):\n{}".format(
                name, value, text))


def rectangle(fields, name):
    values = tuple(int(value) for value in fields.get(name, "").split(","))
    if len(values) != 4 or values[2] <= values[0] or values[3] <= values[1]:
        raise RuntimeError("UI report has invalid {} geometry: {}".format(name, values))
    return values


def client_rect(fields, prefix=""):
    values = tuple(int(value) for value in fields.get(prefix + "client", "").split(","))
    if len(values) != 2 or min(values) <= 0:
        raise RuntimeError("UI report lacks {}client size".format(prefix))
    return (0, 0, values[0], values[1])


def contains(parent, child):
    return (parent[0] <= child[0] < child[2] <= parent[2]
            and parent[1] <= child[1] < child[3] <= parent[3])


def check_mail_geometry(fields, prefix=""):
    client = client_rect(fields, prefix)
    folders = rectangle(fields, prefix + "folders")
    messages = rectangle(fields, prefix + "messages")
    reader = rectangle(fields, prefix + "reader")
    if not all(contains(client, rect) for rect in (folders, messages, reader)):
        raise RuntimeError(prefix + "Mail panes extend outside the client area")
    dpi = int(fields.get("ui_dpi", "96"))
    scale = dpi / 96
    tolerance = max(6, round(8 * scale))
    if (abs(folders[1] - messages[1]) > tolerance
            or abs(folders[3] - messages[3]) > tolerance
            or not 0 <= messages[0] - folders[2] <= tolerance * 2
            or not 0 <= reader[1] - max(folders[3], messages[3]) <= tolerance * 2
            or abs(reader[0] - folders[0]) > tolerance
            or abs(reader[2] - messages[2]) > tolerance
            or reader[0] > tolerance or client[2] - reader[2] > tolerance):
        raise RuntimeError(prefix + "Mail lacks adjacent upper lists and a full-width reader")
    if (folders[2] - folders[0] < 70 * scale
            or messages[2] - messages[0] < 100 * scale
            or reader[3] - reader[1] < 80 * scale):
        raise RuntimeError(prefix + "Mail panes are too small to use")
    if not prefix and (abs(folders[2] - folders[0] - 333 * scale) > tolerance
                       or abs(folders[3] - folders[1] - 203 * scale) > tolerance):
        raise RuntimeError("Mail default panes differ from original 3.02")
    if prefix and client[2] > 400 * scale:
        raise RuntimeError("Mail narrow-layout check did not resize the window")
    return client, folders, messages, reader


def check_messaging_reference(report, news=False):
    text, fields = read_report(Path(str(report) + '.geometry'))
    assert rectangle(fields, 'first') == (5, 3, 46, 44), ('Original Mail/News toolbar button', fields)
    assert rectangle(fields, 'second')[0] == (46 if news else 54), 'Original toolbar group spacing'
    assert rectangle(fields, 'last')[0] == (488 if news else 406), 'Original final toolbar button'
    assert int(fields['chrome_height']) == 53, ('Original Mail/News chrome height', fields)
    assert [int(fields['folder_column_' + str(i)]) for i in range(3)] == ([243, 16, 42] if news else [243, 42, 46]), 'Folder header widths differ from original'
    widths = [int(fields['message_column_' + str(i)]) for i in range(5)]
    assert widths[:4] == [150, 16, 16, 150], ('Original fixed Sender/Subject widths', widths)
    assert sum(widths) == int(fields['message_client_width']), 'Date does not fill the remaining header'
    assert fields['date_marker'] == '1', 'Initial Date sort indicator is missing'


def check_mail_report(text, fields):
    require_fields(fields, {
        "passed": "1", "completed": "1", "toolbar_count": "10",
        "toolbar_labels": MAIL_TOOLBAR, "menu_labels": MAIL_MENUS,
        "location_visible": "0", "directory_visible": "0",
        "columns": "Sender|Status|Status|Subject|Date", "selected_subject": SUBJECTS[0],
        "reader_header": "1", "reader_body": "1", "previous_next": "1",
        "reader_scroll": "1",
        "menu_flag_unflag": "1", "menu_select_all": "1", "menu_native_deselect": "1",
        "menu_native_clear": "1", "menu_sort_subject": "1", "menu_sort_date": "1",
        "menu_sort_ascending": "1", "menu_attachments": "1", "menu_headers": "1",
        "menu_mail_images_preserve_headers": "1", "menu_find_again": "1", "menu_copy_trash": "1", "menu_restore": "1",
        "splitter_clamps": "1", "narrow_layout": "1",
    }, text)
    if int(fields.get("folder_icons", "0")) < 1:
        raise RuntimeError("Mail folder controls lack their native artwork")
    if "Inbox" not in fields.get("status_text", ""):
        raise RuntimeError("Mail status does not identify the selected Inbox")
    assert 'bytes wasted' in fields.get('status_text', ''), 'Original mailbox space status was overwritten'
    if "Hello!" not in fields.get("caption", ""):
        raise RuntimeError("Mail caption does not identify the selected message")
    sender = fields.get("selected_sender", "")
    if "Your Name" not in sender:
        raise RuntimeError("Mail did not preserve the selected MIME sender")
    if not all(subject in fields.get("subjects", "").split("|") for subject in SUBJECTS):
        raise RuntimeError("Mail did not populate both real MIME messages")
    reader = fields.get("reader_text", "")
    if ("Hi User," not in reader or "Just a test." not in reader
            or "user@example.invalid" not in reader):
        raise RuntimeError("Mail reader did not decode the selected MIME body")
    check_mail_geometry(fields, "narrow_")
    return check_mail_geometry(fields)


def check_compose_toolbar(fields):
    dpi = int(fields.get("ui_dpi", "96"))
    def scale(value):
        return (value * dpi + 48) // 96
    toolbar = rectangle(fields, "toolbar")
    if dpi == 96:
        assert toolbar == (5, 3, 226, 44), "Original composer toolbar bounds differ"
    buttons = [rectangle(fields, "toolbar_" + str(index)) for index in range(5)]
    assert buttons[0][:2] == (scale(5), scale(3)), buttons[0]
    for index, button in enumerate(buttons):
        assert (button[2] - button[0], button[3] - button[1]) == (scale(41), scale(41)), index
        assert button[1] == scale(3), index
        if index:
            assert button[0] - buttons[index - 1][2] == scale(8 if index in (1, 4) else 0), index
    assert toolbar == (buttons[0][0], buttons[0][1], buttons[-1][2], buttons[-1][3]), toolbar
    width = client_rect(fields)[2]
    logo = rectangle(fields, "toolbar_logo")
    assert logo[:2] == (width - scale(38), scale(1)), logo
    assert (logo[2] - logo[0], logo[3] - logo[1]) == (scale(32), scale(32)), logo
    assert abs(width - logo[2] - scale(6)) <= 1, logo
    assert fields["toolbar_logo_border"] == "1", "Original small logo needs its native frame"


def check_compose_report(text, fields):
    check_compose_toolbar(fields)
    require_fields(fields, {
        "passed": "1", "completed": "1", "toolbar_count": "5",
        "toolbar_labels": COMPOSE_TOOLBAR, "menu_labels": COMPOSE_MENUS,
        "compact_headers": "1", "advanced_headers": "1", "narrow_layout": "1",
        "hidden_headers_retained": "1", "draft_command": "1",
        "attachment_command": "1", "cancel_command": "1",
        "composition_title": "1", "address_disabled": "0",
        "original_save_as": "1", "attachment_toggle": "1", "paste_quotation": "1",
        "wrap_toggle": "1", "deferred_delivery": "1", "immediate_delivery": "1",
        "body_saved": "1", "original_size": "1",
        "quote_backend": "1", "stop_idle": "1",
        "quote_dirty": "1", "quote_stop_active": "1",
        "quote_stop_survived": "1", "quote_retry": "1",
    }, text)
    if int(fields.get("status_panes", "0")) < 2:
        raise RuntimeError("Composer lacks separate status panes")
    client = client_rect(fields)
    editor = rectangle(fields, "editor")
    rows = [rectangle(fields, name) for name in ("mail_to", "cc", "subject", "attachment")]
    if not all(contains(client, rect) for rect in rows + [editor]):
        raise RuntimeError("Composer controls extend outside its client area")
    if any(rows[index][3] > rows[index + 1][1] for index in range(3)):
        raise RuntimeError("Composer header rows overlap")
    if (rows[-1][3] > editor[1] or editor[3] - editor[1] < client[3] * 0.45
            or rows[-1][3] - rows[0][1] > client[3] * 0.35):
        raise RuntimeError("Composer lacks compact headers and a large editor")
    narrow = client_rect(fields, "narrow_")
    narrow_editor = rectangle(fields, "narrow_editor")
    if (narrow[2] > 500 * int(fields.get("ui_dpi", "96")) / 96
            or not contains(narrow, narrow_editor)
            or narrow_editor[2] - narrow_editor[0] < 120
            or narrow_editor[3] - narrow_editor[1] < 80):
        raise RuntimeError("Composer narrow window leaves no usable editor")
    return client, editor


class Bitmap:
    def __init__(self, path):
        self.data = path.read_bytes()
        if len(self.data) < 54 or self.data[:2] != b"BM":
            raise RuntimeError("UI capture is not a Windows BMP")
        size, self.offset = struct.unpack_from("<I4xI", self.data, 2)
        header, self.width, height, planes, bits, compression = struct.unpack_from(
            "<IiiHHI", self.data, 14)
        self.height = abs(height)
        self.bottom_up = height > 0
        self.pixel_bytes = bits // 8
        self.stride = ((self.width * bits + 31) // 32) * 4
        if (size != len(self.data) or header < 40 or self.offset < 14 + header
                or not 300 <= self.width <= 4096 or not 350 <= self.height <= 4096
                or planes != 1 or bits not in (24, 32) or compression != 0
                or self.offset + self.stride * self.height > len(self.data)):
            raise RuntimeError("UI capture has invalid dimensions, pixel format or scanlines")

    def region(self, rect):
        if not contains((0, 0, self.width, self.height), rect):
            raise RuntimeError("Reported UI region lies outside the actual bitmap")
        dark = white = colorful = yellow = count = 0
        colors = set()
        for y in range(rect[1], rect[3]):
            row = self.height - 1 - y if self.bottom_up else y
            for x in range(rect[0], rect[2]):
                offset = self.offset + row * self.stride + x * self.pixel_bytes
                pixel = self.data[offset:offset + 3]
                count += 1
                dark += max(pixel) < 110
                white += min(pixel) > 235
                colorful += max(pixel) - min(pixel) > 35
                yellow += pixel[2] > 150 and pixel[1] > 120 and pixel[0] < 130
                if len(colors) < 64:
                    colors.add(pixel)
        return {"pixels": count, "dark": dark, "white": white,
                "colorful": colorful, "yellow": yellow, "colors": len(colors)}

    def gray_vertical_run(self, rect):
        """Measure an unwanted table outline in this plain MIME header fixture."""
        if not contains((0, 0, self.width, self.height), rect):
            raise RuntimeError("Reader edge check lies outside the actual bitmap")
        longest = 0
        for x in range(rect[0], rect[2]):
            run = 0
            for y in range(rect[1], rect[3]):
                row = self.height - 1 - y if self.bottom_up else y
                offset = self.offset + row * self.stride + x * self.pixel_bytes
                pixel = self.data[offset:offset + 3]
                if 100 <= min(pixel) <= max(pixel) <= 180 and max(pixel) - min(pixel) <= 3:
                    run += 1
                    longest = max(longest, run)
                else:
                    run = 0
        return longest


def check_bitmap(snapshot, mode, fields, geometry):
    bitmap = Bitmap(snapshot)
    client = geometry[0]
    if (bitmap.width, bitmap.height) != (client[2], client[3]):
        raise RuntimeError("Client bitmap dimensions differ from live client geometry")
    # Text/artwork assertions inspect actual scanlines, independently of flags.
    regions = {}
    if mode == "mail":
        _, folders, messages, reader = geometry
        for name, rect in (("folders", folders), ("messages", messages), ("reader", reader)):
            regions[name] = bitmap.region((rect[0] + 2, rect[1] + 2, rect[2] - 2, rect[3] - 2))
        if any(regions[name]["dark"] < 30 for name in regions):
            raise RuntimeError("Mail bitmap lacks visible folder/message/reader text")
        if regions["reader"]["white"] < regions["reader"]["pixels"] // 3:
            raise RuntimeError("Mail bitmap lacks the rendered white message reader")
        scale = int(fields.get("ui_dpi", "96")) / 96
        regions["folder_artwork"] = bitmap.region((
            folders[0] + 2, folders[1] + round(22 * scale),
            min(folders[2] - 2, folders[0] + round(50 * scale)), folders[3] - 2))
        if regions["folder_artwork"]["yellow"] < 4:
            raise RuntimeError("Mail bitmap lacks actual yellow folder artwork")
        # BORDER=0 MIME header tables must have no continuous gray left edge.
        # This fixture has no quoted text that could legitimately draw a rule.
        left_rule = bitmap.gray_vertical_run((
            reader[0] + round(6 * scale), reader[1] + round(5 * scale),
            reader[0] + round(11 * scale),
            min(reader[3] - 2, reader[1] + round(120 * scale))))
        regions["reader"]["left_rule_run"] = left_rule
        if left_rule >= round(30 * scale):
            raise RuntimeError("Mail MIME header has an outline despite BORDER=0")
        toolbar_bottom = folders[1] - 2
    else:
        editor = geometry[1]
        regions["editor"] = bitmap.region((editor[0] + 3, editor[1] + 3,
                                           editor[2] - 3, editor[3] - 3))
        if (regions["editor"]["white"] < regions["editor"]["pixels"] // 2
                or regions["editor"]["dark"] < 30):
            raise RuntimeError("Composer bitmap lacks the populated white text editor")
        toolbar_bottom = rectangle(fields, "mail_to")[1] - 2
    regions["toolbar"] = bitmap.region((2, 2, bitmap.width - 2, toolbar_bottom))
    if regions["toolbar"]["dark"] < 40 or regions["toolbar"]["colors"] < 8:
        raise RuntimeError("UI bitmap lacks rendered toolbar captions and artwork")
    return {"width": bitmap.width, "height": bitmap.height, "regions": regions}


def run_smoke(executable, output, mode, server=None):
    prefix = mode + "-ui"
    report = output / (prefix + "-smoke.txt")
    snapshot = output / (prefix + "-smoke.bmp")
    for path in (report, snapshot):
        path.unlink(missing_ok=True)
    profile = output / (prefix + " profile with spaces 100%")
    profile.mkdir(parents=True, exist_ok=True)
    seed_mail(profile)
    environment = os.environ.copy()
    environment["NETSCAPE_PROFILE_DIR"] = str(profile)
    command = [str(executable), "--" + mode + "-ui-smoke", str(report), str(snapshot)]
    if server is not None:
        command.append("http://127.0.0.1:{}/quote-source".format(server.server_port))
    try:
        process = subprocess.run(command, env=environment, cwd=str(output),
                                 capture_output=True, text=True, encoding="utf-8",
                                 errors="replace", timeout=40, check=False)
    except subprocess.TimeoutExpired as error:
        for name, value in (("stdout", error.stdout), ("stderr", error.stderr)):
            if isinstance(value, bytes):
                value = value.decode("utf-8", errors="replace")
            (output / (prefix + "-" + name + ".txt")).write_text(value or "", encoding="utf-8")
        raise RuntimeError(prefix + " exceeded the 40-second timeout") from error
    for name, value in (("stdout", process.stdout), ("stderr", process.stderr)):
        (output / (prefix + "-" + name + ".txt")).write_text(value, encoding="utf-8")
    if process.returncode:
        raise RuntimeError(prefix + " exited with status {} (report: {})".format(
            process.returncode, report))
    if mode == "compose" and (server is None or len(server.requests) < 2
            or not any(request["number"] >= 2 and request["body_sent"]
                       for request in server.requests)):
        raise RuntimeError("Composer Quote/Stop omitted the actual HTTP retry")
    text, fields = read_report(report)
    if mode == 'mail':
        check_messaging_reference(report)
    geometry = (check_mail_report(text, fields) if mode == "mail" else
                check_compose_report(text, fields))
    capture = check_bitmap(snapshot, mode, fields, geometry)
    if mode == "mail":
        check_mail_preserved(profile)
    (output / (prefix + "-capture.json")).write_text(
        json.dumps(capture, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", "--exe", type=Path, required=True)
    parser.add_argument("--output", "--output-dir", type=Path, required=True)
    parser.add_argument("--mode", choices=("mail", "compose", "all"), default="all")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    try:
        for mode in (("mail", "compose") if args.mode == "all" else (args.mode,)):
            if mode == "compose":
                with quote_server(output) as server:
                    run_smoke(args.executable.resolve(), output, mode, server)
            else:
                run_smoke(args.executable.resolve(), output, mode)
    except (OSError, RuntimeError, ValueError, struct.error) as error:
        print("Windows mail UI smoke failed: " + str(error), file=sys.stderr)
        return 1
    print("Windows mail UI smoke passed: {} native layout, commands and BMP capture".format(args.mode))
    return 0


if __name__ == "__main__":
    sys.exit(main())
