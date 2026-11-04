"""Extract a generated TGZ into a path with spaces and check relocation."""
from pathlib import Path
import argparse
import subprocess
import sys
import tarfile
import tempfile

p=argparse.ArgumentParser()
p.add_argument('archive',type=Path)
p.add_argument('--java',action='store_true')
p.add_argument('--security',action='store_true')
a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='netscape-package-') as directory:
    output=Path(directory)/'relocated install';output.mkdir()
    with tarfile.open(a.archive) as archive:
        for member in archive.getmembers():
            target=(output/member.name).resolve()
            assert output.resolve() in target.parents or target==output.resolve(),member.name
            assert not member.issym() and not member.islnk(),member.name
        if hasattr(tarfile,'data_filter'):archive.extractall(output,filter='data')
        else:archive.extractall(output)
    launchers=list(output.glob('**/bin/netscape'))
    assert len(launchers)==1,launchers
    command=[sys.executable,str(Path(__file__).with_name('check_install.py')),str(launchers[0].parent.parent)]
    if a.java:command.append('--java')
    if a.security:command.append('--security')
    subprocess.run(command,check=True)
print('Relocated package passed:',a.archive)
