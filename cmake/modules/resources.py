"""Build-tree generators for Linux localized/embedded resources and tag tables."""
from pathlib import Path
import argparse
import os
import shlex
import re
import subprocess


def text(path):
    return path.read_bytes().replace(b'\r\n', b'\n').decode('latin1')


def c_strings(data):
    return '\n'.join(' "' + ''.join('\\%03o' % b for b in data[i:i+32]) + '"' for i in range(0,len(data),32)) or ' ""'


def rotate(data, all_bytes=False):
    # tr69 rotates every byte; the shell tr recipe rotates only 001..272 octal.
    return bytes((b+69)&255 if all_bytes or 1<=b<=186 else b for b in data)


def substitute(value):
    for key, replacement in [('@NAME@','Netscape'),('@CLASS@','Netscape'),('@PROGNAME@','netscape')]:
        value=value.replace(key,replacement)
    return value


def omit_row(value, marker):
    # Reproduce the two-stage awk range/skipline rules in l10n/us/xp/Makefile.
    result=[]
    in_range=skip=False
    for line in value.splitlines(keepends=True):
        if marker in line or in_range:
            in_range='</TR>' not in line
            skip=True
            continue
        if skip:
            fields=line.split()
            if fields and '</TR>' in fields[0]: skip=False
            continue
        result.append(line)
    return ''.join(result)


def localized(source, output):
    output.mkdir(parents=True,exist_ok=True)
    base=substitute(text(source/'about-all.html'))
    base=''.join(line for line in base.splitlines(keepends=True) if 'Mercutio' not in line and 'ViviStar' not in line)
    special={}
    for suffix in ['', '-java']:
        about=base if suffix else omit_row(base,'about:javalogo')
        special['about'+suffix]=about
        splash=omit_row(about,'about:qtlogo')
        # Match the legacy sed expression removing HTML anchor tags.
        special['splash'+suffix]=''.join(re.sub(r'(.*)<A [^>]*>(.*)',r'\1\2',line).replace('</A>','') for line in splash.splitlines(keepends=True))
    for path in sorted(source.glob('*.html'))+sorted(source.glob('*.msg')):
        if path.stem in special or path.stem=='about-all': continue
        value=substitute(text(path))
        (output/path.name).write_bytes(value.encode('latin1'))
        (output/(path.stem+'.h')).write_text(c_strings(rotate(value.encode('latin1'),path.suffix=='.html'))+'\n')
    for name,value in special.items():
        (output/(name+'.html')).write_bytes(value.encode('latin1'))
        (output/(name+'.h')).write_text(c_strings(rotate(value.encode('latin1')))+'\n')


def parser(source, output, gperf, wrapper):
    output.mkdir(parents=True,exist_ok=True)
    tags=text(source/'pa_tags.h')
    names=dict(re.findall(r'^#define\s+PT_([A-Z_0-9]+)\s+"([^"]*)"',tags,re.M))
    ids=re.findall(r'^#define\s+P_([A-Z_0-9]+)\s+(-?\d+)',tags,re.M)
    entries=[(names[key],'P_'+key,int(number)) for key,number in ids if key in names]
    tokens={int(number):names.get(key,'') for key,number in ids if key not in ['UNKNOWN','MAX']}
    data='struct pa_GeneratedTag { const char *name; int id; };\n%%\n'+''.join('%s, %s\n'%(name,key) for name,key,_ in entries)
    result=subprocess.run([gperf,'--ignore-case','--readonly-tables','--struct-type','--compare-lengths','--includes','--language=ANSI-C','--lookup-function-name=pa_LookupGeneratedTag','--hash-function-name=pa_Hash','--word-array-name=pa_TagWords','-k1,$,2,3'],input=data.encode(),stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True)
    (output/'pa_hash_table.h').write_bytes(result.stdout)
    (output/'pa_hash.c').write_bytes(wrapper.read_bytes())
    (output/'pa_hash.rmap').write_text(''.join('/*%d*/ "%s",\n'%(i,tokens.get(i,'')) for i in range(max(tokens)+1)))


def security(source, output, perl):
    nav=source/'security/lib/nav'
    output.mkdir(parents=True,exist_ok=True)
    for name in ['secprefs.html','convhtml.sh','convhtml','convwrc','convquot','mkxpstrs.pl']:
        data=text(nav/name)
        if name=='convhtml.sh':
            data=data.replace('TMP=/tmp/convhtml.$$','TMP=convhtml.tmp').replace('perl5 mkxpstrs.pl',shlex.quote(perl)+' mkxpstrs.pl')
        (output/name).write_bytes(data.encode('latin1'))
    subprocess.run(['sh','-e','convhtml.sh'],cwd=output,env=dict(os.environ,AKBAR='1'),check=True)
    original=text(source/'include/allxpstr.h')
    start=original.index('ResDef(SA_STRINGS_BEGIN,')
    end=re.search(r'^ResDef\(SA_VARNAMES_END[^\n]*\n',original,re.M)
    if not end:raise ValueError('Missing SA_VARNAMES_END marker')
    combined=original[:start]+text(output/'allxpstr.ins')+original[end.end():]
    include=output.parent/'include';include.mkdir(parents=True,exist_ok=True)
    (include/'allxpstr.h').write_bytes(combined.encode('latin1'))


def main():
    p=argparse.ArgumentParser()
    sub=p.add_subparsers(dest='kind',required=True)
    q=sub.add_parser('localized');q.add_argument('source',type=Path);q.add_argument('output',type=Path)
    q=sub.add_parser('binary');q.add_argument('source',type=Path);q.add_argument('output',type=Path);q.add_argument('symbol')
    q=sub.add_parser('parser');q.add_argument('source',type=Path);q.add_argument('output',type=Path);q.add_argument('gperf');q.add_argument('wrapper',type=Path)
    q=sub.add_parser('security');q.add_argument('source',type=Path);q.add_argument('output',type=Path);q.add_argument('perl')
    a=p.parse_args()
    if a.kind=='localized':localized(a.source,a.output)
    elif a.kind=='security':security(a.source,a.output,a.perl)
    elif a.kind=='parser':parser(a.source,a.output,a.gperf,a.wrapper)
    else:
        a.output.parent.mkdir(parents=True,exist_ok=True)
        a.output.write_text('static unsigned char '+a.symbol+'[] =\n'+c_strings(a.source.read_bytes())+';\n')

if __name__=='__main__': main()
