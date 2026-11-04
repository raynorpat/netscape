"""Test resource payloads and compile the actual generated tag lookup code."""
from pathlib import Path
import importlib.util
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
sys.dont_write_bytecode = True
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('resources',ROOT/'cmake/modules/resources.py')
resources=importlib.util.module_from_spec(spec);spec.loader.exec_module(resources)

def payload(path):
    return bytes(int(x,8) for x in re.findall(r'\\([0-7]{3})',path.read_text()))

class Resources(unittest.TestCase):
    def test_binary_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            for source in (ROOT/'lib/xp').iterdir():
                if source.suffix not in ['.gif','.jpg','.au']:continue
                output=Path(directory)/(source.stem+'.h')
                subprocess.run([sys.executable,str(ROOT/'cmake/modules/resources.py'),'binary',str(source),str(output),source.stem+'_'+source.suffix[1:]],check=True)
                self.assertEqual(payload(output),source.read_bytes())

    def test_localized_encoding(self):
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'in';output=Path(directory)/'out';source.mkdir()
            (source/'about-all.html').write_text('<HEAD>@NAME@</HEAD>\n<TR>about:javalogo\n</TR>\n<TR>Java details\n</TR>\n<TR>about:qtlogo\n</TR>\n<TR>QT details\n</TR>\n<A HREF="url">link</A>\n')
            (source/'sample.html').write_bytes(b'@CLASS@\n\xf1\n')
            (source/'mail.msg').write_text('From: @NAME@\n@PROGNAME@\n')
            resources.localized(source,output)
            self.assertEqual(bytes((b-69)&255 for b in payload(output/'sample.h')),b'Netscape\n\xf1\n')
            self.assertEqual(bytes((b-69)&255 for b in payload(output/'mail.h')),b'From: Netscape\nnetscape\n')
            self.assertNotIn('javalogo',(output/'about.html').read_text())
            self.assertIn('javalogo',(output/'about-java.html').read_text())
            self.assertNotIn('qtlogo',(output/'splash-java.html').read_text())
            self.assertNotIn('<A ',(output/'splash.html').read_text())

    def test_generated_parser(self):
        gperf=os.environ.get('GPERF') or shutil.which('gperf')
        if not gperf: self.fail('gperf is required for parser generation tests')
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)
            resources.parser(ROOT/'lib/libparse',output,gperf,ROOT/'cmake/templates/pa_hash.c.in')
            shutil.copy(ROOT/'lib/libparse/pa_tags.h',output/'pa_tags.h')
            (output/'pa_parse.h').write_text('#include <stddef.h>\n#include <string.h>\n#include "pa_tags.h"\ntypedef int intn;\nstruct pa_TagTable { char *name; int id; };\nextern struct pa_TagTable *pa_LookupTag(char *, unsigned int);\n')
            tags=(output/'pa_tags.h').read_text()
            names=dict(re.findall(r'^#define\s+PT_([A-Z_0-9]+)\s+"([^"]*)"',tags,re.M))
            ids=re.findall(r'^#define\s+P_([A-Z_0-9]+)\s+(-?\d+)',tags,re.M)
            cases=[(names[key],'P_'+key) for key,number in ids if key in names]
            assertions=''.join('assert(pa_tokenize_tag("%s")==%s);\n'%(variant,key) for name,key in cases for variant in (name,name.upper(),name.title()))
            (output/'test.c').write_text('#include <assert.h>\n#include "pa_tags.h"\nint pa_tokenize_tag(char *);\nint main(void){\n'+assertions+'assert(pa_tokenize_tag(0)==P_UNKNOWN);\nassert(pa_tokenize_tag("not_a_tag")==P_UNKNOWN);\nassert(pa_tokenize_tag("")==P_UNKNOWN);\n#ifdef DEBUG_jwz\nassert(pa_tokenize_tag("style")==P_TITLE);\n#else\nassert(pa_tokenize_tag("style")==P_UNKNOWN);\n#endif\nreturn 0;}\n')
            for flags in ([],['-DDEBUG_jwz']):
                subprocess.run([*shlex.split(os.environ.get('CC','cc')),'-std=gnu90',*flags,str(output/'pa_hash.c'),str(output/'test.c'),'-o',str(output/'test')],check=True)
                subprocess.run([str(output/'test')],check=True)
            self.assertEqual(len((output/'pa_hash.rmap').read_text().splitlines()),92)

if __name__=='__main__':unittest.main()
