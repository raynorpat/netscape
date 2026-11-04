"""Reproduce legacy text generators without modifying the source tree."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('kind', choices=['mime', 'license', 'ekit', 'resources'])
p.add_argument('source', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
root = a.source
xfe = root / 'cmd/xfe'
a.output.parent.mkdir(parents=True, exist_ok=True)

def shell(script, arguments=(), data=None, cwd=None):
    # Windows checkouts may contain CRLF; execute an LF copy in the build tree.
    with tempfile.TemporaryDirectory(dir=a.output.parent) as temporary:
        normalized = Path(temporary) / script.name
        normalized.write_bytes(script.read_bytes().replace(b'\r\n', b'\n'))
        return subprocess.run(['sh', str(normalized.resolve()), *arguments],
                              input=data, cwd=cwd, stdout=subprocess.PIPE,
                              check=True).stdout

def ad2c(data):
    return shell(xfe / 'ad2c', data=data.replace(b'\r', b''))

def rotated(data):
    data = data.replace(b'\r\n', b'\n')
    data = bytes(b + 69 if 1 <= b <= 186 else b for b in data)
    return b'\n'.join(b'"' + b''.join(('\\%03o' % b).encode() for b in data[i:i+16]) + b'"'
                      for i in range(0, len(data), 16)) or b'""'

if a.kind == 'mime':
    def content(suffix):
        return ad2c(b''.join(line for line in (root / ('lib/libnet/mime.types' + suffix)).read_bytes().splitlines(keepends=True)
                            if not line.startswith(b'#')))
    result = b'/* Generated file - do not edit! */\n' + content('') + b'\n#ifdef XP_UNIX\n' + content('-unix') + b'\n#else\n' + content('-nonunix') + b'\n#endif\n'
elif a.kind == 'ekit':
    result = b'char *fe_ekitDefaultDatabase =\n' + rotated((xfe / 'e_kit.ad').read_bytes()) + b';\n'
elif a.kind == 'license':
    version = re.search(rb'^#define VERSION_NUMBER\s+(\S+)', (xfe / 'versionn.h').read_bytes(), re.M)[1]
    name = 'LICENSE-export-beta' if b'b' in version else 'LICENSE-alpha' if b'a' in version else 'LICENSE-export'
    result = b'const char fe_LicenseData[] =\n' + rotated((root / 'l10n/us/xp' / name).read_bytes()) + b';\n'
else:
    # The original script reads resources, optional locale map, and strs from cwd.
    adfile = a.output.with_suffix('.ad')
    shell(xfe / 'make-resources', [str(adfile.resolve()), 'Netscape', 'Netscape', '', 'export', ''], cwd=xfe)
    result = b'char *fe_fallbackResources[] = {\n' + ad2c(adfile.read_bytes()) + b'0};\n'
a.output.write_bytes(result)
