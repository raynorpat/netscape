"""Crop original Gold 3.02 Windows resources (development tool; Pillow required).

Normal builds use checked-in assets. --check verifies reproducibility without
changing files. Source hashes retain the extraction provenance.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'cmd/winfe/res/classic'
SOURCE = OUTPUT / 'source-gold'
KEY = (192, 192, 192)
STRIPS = {
    '2007': (20, 18, ['back', 'forward', 'home', 'edit', 'reload', 'images', 'open', 'print', 'find', 'stop']),
    '750': (20, 20, ['getmail', 'delete', 'compose', 'reply', 'replyall', 'forwardmsg', 'previous', 'next', None, None]),
    '751': (20, 20, ['post', None, None, 'post-reply', 'post-both', None, None, None, 'mark-thread', 'mark-group', None, None]),
    '755': (20, 20, ['send', 'quote', 'attach', 'address', None]),
    '1204': (24, 12, ['status-mail-new', 'status-mail', 'status-mail-unknown']),
}
SINGLE = {'folder': '701', 'folder-open': '702', 'news-host': '717', 'news-group': '718',
          'news': '718', 'subscribed': '719', 'unsubscribed': '720',
          'message-flag': '721', 'priority-header': '725', 'status-apps': '460',
          'message-read': '703', 'message-unread': '703'}
WINDOWS = {'browser': '2', 'mail': '221', 'news': '224', 'compose': '225', 'bookmarks': '231'}


def icon(group):
    data = (SOURCE / 'GROUP_ICON' / group).read_bytes()
    count = struct.unpack_from('<H', data, 4)[0]
    result = bytearray(data[:6]); payloads = []; offset = 6 + count * 16
    for i in range(count):
        width, height, colors, reserved, planes, bits, size, number = struct.unpack_from('<BBBBHHIH', data, 6 + i * 14)
        # The extraction tool wraps each unchanged RT_ICON payload in an ICO.
        wrapped = (SOURCE / 'ICON' / (str(number) + '.ico')).read_bytes()
        length, origin = struct.unpack_from('<II', wrapped, 14)
        payload = wrapped[origin:origin + length]
        assert len(payload) == size
        result.extend(struct.pack('<BBBBHHII', width, height, colors, reserved, planes, bits, size, offset))
        payloads.append(payload); offset += size
    return bytes(result) + b''.join(payloads)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    for name, digest in json.loads((SOURCE / 'sha256.json').read_text()).items():
        assert hashlib.sha256((SOURCE / name).read_bytes()).hexdigest() == digest, name
    images = {}
    for number, (width, height, names) in STRIPS.items():
        strip = Image.open(SOURCE / 'BITMAP' / (number + '.bmp')).convert('RGB')
        assert strip.size == (width * len(names), height)
        for i, name in enumerate(names):
            if name:
                images[name] = strip.crop((i * width, 0, (i + 1) * width, height))
    for name, number in SINGLE.items():
        images[name] = Image.open(SOURCE / 'BITMAP' / (number + '.bmp')).convert('RGB')
    images['mail'] = images['status-mail'].copy()
    for name, number, size in [('logo', '1100', 48), ('logo-small', '1000', 30)]:
        images[name] = Image.open(SOURCE / 'BITMAP' / (number + '.bmp')).convert('RGB').crop((0, 0, size, size))
    outputs = {}
    for name, image in images.items():
        # Magenta is the original list/status transparency marker; toolbar
        # strips already use #C0C0C0. Only the mask color is normalized.
        image.putdata([KEY if p == (255, 0, 255) else p for p in image.getdata()])
        for suffix, bitmap in [('', image), ('-disabled', image.copy())]:
            if suffix:
                bitmap.putdata([KEY if p == KEY else (128, 128, 128) for p in bitmap.getdata()])
            buffer = io.BytesIO(); bitmap.save(buffer, format='BMP')
            outputs[OUTPUT / (name + suffix + '.bmp')] = buffer.getvalue()
    for name, group in WINDOWS.items():
        outputs[ROOT / 'cmd/winfe/res/gold' / (name + '.ico')] = icon(group)
    mismatches = []
    for path, data in outputs.items():
        if args.check:
            if not path.exists() or path.read_bytes() != data:
                mismatches.append(str(path.relative_to(ROOT)))
        else:
            path.write_bytes(data)
    if mismatches:
        raise SystemExit('Generated Gold resources differ: ' + ', '.join(mismatches))
    print(('Verified' if args.check else 'Generated'), len(outputs), 'original Gold Windows assets')


if __name__ == '__main__':
    main()
