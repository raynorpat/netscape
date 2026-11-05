"""Verify the message/status mappings directly against the original Gold cells."""
from pathlib import Path
import struct


def bitmap(path):
    data = path.read_bytes()
    assert data[:2] == b'BM', path
    origin = struct.unpack_from('<I', data, 10)[0]
    header = struct.unpack_from('<I', data, 14)[0]
    width, height, planes, bits, compression = struct.unpack_from('<iiHHI', data, 18)
    assert width > 0 and height and planes == 1 and bits in (4, 8, 24, 32) and compression == 0, path
    palette = [tuple(data[n:n + 3]) for n in range(14 + header, origin, 4)] if bits < 24 else []
    stride = (width * bits + 31) // 32 * 4
    rows = []
    for y in range(abs(height)):
        row = origin + (y if height < 0 else abs(height) - y - 1) * stride
        pixels = []
        for x in range(width):
            if bits == 4:
                color = palette[(data[row + x // 2] >> (0 if x % 2 else 4)) & 15]
            elif bits == 8:
                color = palette[data[row + x]]
            else:
                start = row + x * (bits // 8); color = tuple(data[start:start + 3])
            pixels.append((192, 192, 192) if color == (255, 0, 255) else color)
        rows.append(pixels)
    return width, abs(height), rows


def check():
    assets = Path(__file__).resolve().parents[2] / 'cmd/winfe/res/classic'
    for name, source, x, width, height in (
        ('message-read', '703', 0, 16, 16), ('message-unread', '703', 0, 16, 16),
        ('status-mail-new', '1204', 0, 24, 12), ('status-mail', '1204', 24, 24, 12),
        ('status-mail-unknown', '1204', 48, 24, 12), ('mail', '1204', 24, 24, 12),
        ('news-host', '702', 0, 16, 16), ('news-group', '723', 0, 16, 16),
    ):
        _, _, original = bitmap(assets / 'source-gold/BITMAP' / (source + '.bmp'))
        actual_width, actual_height, actual = bitmap(assets / (name + '.bmp'))
        assert (actual_width, actual_height) == (width, height), name + ' has the wrong original cell size'
        expected = [row[x:x + width] for row in original[:height]]
        assert actual == expected, name + ' differs from the original Gold resource cell'
    print('Original Gold message rows and all three status-mail cells verified')


if __name__ == '__main__':
    check()
