"""Finite, non-network SDK validation on a physical Tab5."""
import json
import re


def configure(parser):
    parser.add_argument('--screenshots', action='store_true')


def validate(parser, args):
    return None


def run(device, args, configuration):
    reports = []
    outputs = []
    try:
        for option, label in (('--filesystem', 'Filesystem'), ('--input', 'Input')):
            device.start('tester ' + option)
            output = device.wait_shell()
            outputs.append(output)
            match = re.search(label + r' assertions: (\d+); failures: (\d+)', output)
            if match is None or int(match[1]) == 0 or int(match[2]) != 0:
                raise RuntimeError('Missing or failed SDK report: ' + output)
            reports.append({'module': label, 'assertions': int(match[1]), 'failures': int(match[2])})
        if args.screenshots:
            device.screenshot(args.output / 'shell.png')
        (args.output / 'result.json').write_text(json.dumps({'tests': reports, 'stats': device.stats()}, indent=2) + '\n')
        (args.output / 'console.txt').write_text('\n'.join(outputs))
    finally:
        if device.status() != 1:
            device.wait_shell(timeout=30)
