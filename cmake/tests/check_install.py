"""Validate an installed tree and its browser launcher without opening a GUI."""
from pathlib import Path
import argparse
import os
import subprocess
import zipfile

p=argparse.ArgumentParser()
p.add_argument('prefix',type=Path)
p.add_argument('--java',action='store_true')
p.add_argument('--security',action='store_true')
a=p.parse_args()
root=a.prefix.resolve()
for name in ['bin/netscape','bin/netscape-export','libexec/netscape/netscape-export','share/netscape/Netscape.ad','share/netscape/XKeysymDB','share/netscape/about.html','share/netscape/mail.msg','share/applications/netscape.desktop','share/doc/netscape/LICENSE']:
    assert (root/name).is_file(),name
for name in ['netscape','netscape-export']:
    script=root/'bin'/name
    assert os.access(script,os.X_OK)
    contents=script.read_text()
    assert 'CMakeFiles' not in contents and '.cache/netscape-cmake' not in contents
    result=subprocess.run([str(script),'-version'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,check=True)
    assert 'Netscape 3.02' in result.stdout+result.stderr
if a.java:
    for name in ['java','javah','javap']:
        assert (root/'libexec/netscape'/name).is_file()
        assert os.access(root/'bin'/('netscape-'+name),os.X_OK)
    archives=list(root.glob('lib*/**/netscape/java_*'))
    assert archives,'Missing installed runtime class archive'
    for archive in archives: assert zipfile.is_zipfile(archive),archive
if a.security:
    assert zipfile.is_zipfile(root/'share/netscape/policyMoz40P2')
print('Installed tree and browser launchers passed:',root)
