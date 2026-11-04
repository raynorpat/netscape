"""Exercise preferences, independent templates and the native offline wizard."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='Gold workflows ', dir=output))
    source = run / 'source.html'
    source.write_text('<html><head><title>Gold workflow fixture</title></head>'
                      '<body><h1>Original Gold heading</h1></body></html>', encoding='ascii')
    report = run / 'editor.txt'
    env = os.environ.copy()
    env['NETSCAPE_PROFILE_DIR'] = str(run / 'profile')
    result = subprocess.run([args.executable, '--editor-workflows-smoke', str(report), source.as_uri()],
                            env=env, capture_output=True, text=True, timeout=120)
    (run / 'process.txt').write_text(result.stdout + result.stderr, encoding='utf-8')
    values = dict(line.split('=', 1) for line in report.read_text().splitlines() if '=' in line) if report.exists() else {}
    required = ['workflow_browser', 'workflow_preferences', 'workflow_cancel', 'workflow_validation',
                'workflow_autosave', 'workflow_autosave_write', 'workflow_template', 'workflow_template_unchanged',
                'workflow_wizard', 'workflow_quote', 'workflow_restore', 'workflow_original_distinct', 'workflow_save_identity']
    assert result.returncode == 0, (result.returncode, values, result.stderr)
    assert all(values.get(key) == '1' for key in required), values
    assert values.get('editor_workflows') == 'pass', values
    template = Path(str(report) + '.template.html')
    assert template.read_text(encoding='ascii') == '<html><head><title>Template original</title></head><body><h1>Template source untouched</h1></body></html>'
    copied = Path(str(report) + '.template-copy.html').read_text(encoding='latin1')
    assert 'Template source untouched' in copied and 'Independent copy' in copied and 'Autosave fixture write' in copied, copied
    wizard = Path(str(report) + '.wizard.html').read_text(encoding='latin1')
    assert 'Offline wizard fixture' in wizard and 'Wizard body fixture' in wizard, wizard
    assert 'gold-editor.ini' not in ''.join(p.name for p in (run / 'profile').glob('*')), 'Smoke changed profile'
    print(f'Gold preferences, independent template and offline wizard verified: {report}')


if __name__ == '__main__':
    main()
