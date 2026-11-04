"""Verify native vector pagination using the real LO/GDI document renderer."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--executable',required=True)
    parser.add_argument('--output',required=True)
    args=parser.parse_args()
    output=Path(args.output).resolve();output.mkdir(parents=True,exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix='Print % ',dir=output))
    source=run/'pages.html'
    source.write_text('<html><head><title>Native print fixture</title></head><body>'+
                      '<form><input name="field" value="Printed field value">'
                      '<input type="password" value="Private fixture secret">'
                      '<select multiple size="3"><option selected>Printed list choice</option>'
                      '<option>Other printed choice</option></select></form>'+
                      ''.join('<p>Printed line %d: original layout vector text.</p>'%n for n in range(180))+
                      '</body></html>',encoding='ascii')
    report=run/'printing.txt'
    env=os.environ.copy();env['NETSCAPE_PROFILE_DIR']=str(run/'profile')
    result=subprocess.run([args.executable,'--printing-smoke',str(report),source.as_uri()],
                          env=env,capture_output=True,text=True,timeout=50)
    (run/'process.txt').write_text(result.stdout+result.stderr,encoding='utf8')
    values=dict(line.split('=',1) for line in report.read_text().splitlines() if '=' in line) if report.exists() else {}
    required=('pagination','vector_content','invalid_geometry','render_failure','page_setup_cancel','view_preserved','editor_preserved')
    assert result.returncode==0 and all(values.get(k)=='1' for k in required),(result.returncode,values,result.stderr[:1000])
    emf=Path(str(report)+'.first.emf').read_bytes()
    assert 'Printed field value'.encode('utf-16le') in emf or b'Printed field value' in emf, 'Printed form value missing'
    assert 'Printed list choice'.encode('utf-16le') in emf or b'Printed list choice' in emf, 'Printed list selection missing'
    assert 'Private fixture secret'.encode('utf-16le') not in emf and b'Private fixture secret' not in emf, 'Password was printed in plain text'
    print('Native vector printing verified:',report)


if __name__=='__main__':
    main()
