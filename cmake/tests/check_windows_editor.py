"""Round-trip a local HTML document through the original EDT engine and WinFE."""
import argparse
import base64
from html.parser import HTMLParser
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


class Document(HTMLParser):
    def __init__(self):
        super().__init__()
        self.tags = []
        self.text = []

    def handle_starttag(self, tag, attrs):
        self.tags.append((tag, dict(attrs)))

    def handle_data(self, data):
        self.text.append(data)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    # Fresh paths avoid overwrite prompts and exercise escaped file URLs.
    run = Path(tempfile.mkdtemp(prefix='Gold % ', dir=output))
    source = run / 'source.html'
    source.write_text('<html><head><title>Gold round trip</title></head><body>'
                      '<h1>Original Gold heading</h1><p>Preserve <b>bold</b> text.</p>'
                      '<p><a href="relative-target.html">Relative link</a></p>'
                      '<p><img src="pixel.gif" alt="Fixture image"></p></body></html>', encoding='ascii')
    (run / 'pixel.gif').write_bytes(base64.b64decode('R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7'))
    report = run / 'editor.txt'
    env = os.environ.copy()
    env['NETSCAPE_PROFILE_DIR'] = str(run / 'profile')
    result = subprocess.run([args.executable, '--editor-smoke', str(report), source.as_uri()],
                            env=env, capture_output=True, text=True, timeout=95)
    (run / 'process.txt').write_text(result.stdout + result.stderr, encoding='utf-8')
    values = dict(line.split('=', 1) for line in report.read_text().splitlines() if '=' in line) if report.exists() else {}
    required = ['passed', 'loaded', 'typed', 'selection', 'format', 'undo', 'redo', 'clipboard',
                'saved', 'reopened', 'browsed', 'failed_save_retained', 'capture', 'reference_size',
                'title', 'final_flush_rejected', 'mouse_selection', 'close_cancel', 'new_document',
                'navigation_cancel']
    assert result.returncode == 0, (result.returncode, values, result.stderr)
    # Compare every compiled menu item, including nested editor commands and
    # separators, with the independently decoded original Gold resources.
    subprocess.run([sys.executable,
                    str(Path(__file__).resolve().parents[1] / 'tools' / 'generate_windows_menus.py'),
                    '--edition', 'gold', '--compare', str(report) + '.menus.tsv'], check=True)
    assert all(values.get(key) == '1' for key in required), values
    assert values.get('editor_ui') == 'pass', values
    html = Document()
    html.feed(Path(str(report) + '.saved.html').read_text(encoding='latin1'))
    text = ''.join(html.text)
    assert 'Original Gold heading' in text and text.count('Native Gold editing') == 2, text
    assert any(tag == 'h1' for tag, _ in html.tags), html.tags
    assert any(tag == 'b' for tag, _ in html.tags), html.tags
    assert any(tag == 'a' and attrs.get('href', '').endswith('relative-target.html') for tag, attrs in html.tags), html.tags
    assert any(tag == 'img' and attrs.get('src', '').endswith('pixel.gif') for tag, attrs in html.tags), html.tags
    bitmap = Path(str(report) + '.bmp').read_bytes()
    assert bitmap[:2] == b'BM' and struct.unpack_from('<ii', bitmap, 18) == (1105, -765)
    assert len(set(bitmap[54:])) > 16, 'Empty GDI capture'
    print(f'Gold engine editing, clipboard, save/reopen and native chrome verified: {report}')


if __name__ == '__main__':
    main()
