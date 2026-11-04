"""Generation and bootstrap rules for the original pre-JNI Sun Java tree."""
import json
from pathlib import Path
import re
import subprocess
import shlex
import sys
import zipfile

kind, *args = sys.argv[1:]
if kind == 'opcodes':
    source, dest = map(Path, args)
    rows = [line.split() for line in source.read_text().splitlines() if re.match('[a-z]', line)]
    if len(rows) > 256:
        raise SystemExit('More than 256 Java opcodes')
    dest.mkdir(parents=True, exist_ok=True)
    (dest / 'opcodes.h').write_text('typedef enum {\n' + ''.join('  opc_%s = %d,\n' % (r[0], i) for i, r in enumerate(rows)) + '  opc_first_unused_index = %d\n} opcode_type;\n' % len(rows))
    (dest / 'opcodes.init').write_text('char *opnames[256] = {\n' + ''.join('   "%s",\n' % r[0] for r in rows) + '   "??",\n' * (256-len(rows)) + '};\n')
    (dest / 'opcodes.length').write_text('short opcode_length[256] = {\n' + ''.join('   %s,\t\t/* %s */\n' % (r[1], r[0]) for r in rows) + '   -1,\n' * (256-len(rows)) + '};\n')
    (dest / 'opcodes.in_out').write_text('char *opcode_in_out[][2] = {\n' + ''.join('   {"%s", "%s"}, \t\t/* %s */\n' % ('' if r[2]=='-' else r[2], '' if r[3]=='-' else r[3], r[0]) for r in rows) + '};\n')
elif kind == 'constants':
    generator, output = args
    result = subprocess.run([generator], check=True, stdout=subprocess.PIPE).stdout
    Path(output).parent.mkdir(parents=True, exist_ok=True)
    Path(output).write_bytes(b'package sun.tools.java;\npublic interface RuntimeConstants {\n' + result + b'\n}\n')
elif kind == 'natives':
    javah, root, manifest, classpath = args
    for entry in json.loads(Path(manifest).read_text()):
        output = Path(root) / entry['directory'] / entry['output']
        output.mkdir(parents=True, exist_ok=True)
        subprocess.run([javah, '-classpath', classpath, *entry['args'], '-d', str(output), *entry['classes']], check=True)
elif kind == 'classes':
    root, archive, manifest, *compiler = args
    if not compiler:
        raise SystemExit('Set NETSCAPE_JAVA_BOOTSTRAP_COMMAND to a compatible vintage javac command; modern javac is not a replacement for this runtime.')
    root, archive = Path(root), Path(archive)
    packages = json.loads(Path(manifest).read_text())
    for package in dict.fromkeys(packages['runtime'] + packages['compiler']):
        sources = sorted((root / package).glob('*.java'))
        if sources:
            subprocess.run([*compiler, '-d', str(root), *map(str, sources)], cwd=root, check=True)
    archive.parent.mkdir(parents=True, exist_ok=True)
    for name, package_list in [(archive, packages['runtime']), (archive.parent / 'javac.zip', packages['compiler'])]:
        with zipfile.ZipFile(name, 'w', zipfile.ZIP_DEFLATED) as z:
            for package in dict.fromkeys(package_list):
                for c in sorted((root / package).glob('*.class')):
                    z.write(c, c.relative_to(root).as_posix())
elif kind == 'launcher':
    vm, archive, name, output = args
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text('#!/bin/sh\nexec ' + shlex.quote(vm) + ' -classpath ' + shlex.quote(archive) + ' -ms8m sun.tools.' + name + '.Main "$@"\n')
    output.chmod(0o755)
else:
    raise SystemExit('Unknown generation rule: ' + kind)
