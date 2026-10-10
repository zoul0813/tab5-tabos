"""Capture portable benchmark phase timings and Tab5 display statistics."""
import json
import re


def configure(parser):
    parser.add_argument('--screenshots', action='store_true')


def validate(parser, args):
    return None


def run(device, args, configuration):
    try:
        device.start('gbench')
        if args.screenshots:
            device.screenshot(args.output / 'frame.png')
        output = device.wait_shell()
        phases = re.findall(r'(Present only|Clear \+ present|Scene): (\d+) frames in (\d+) ms', output)
        if len(phases) != 3 or any(int(frames) == 0 for _, frames, _ in phases):
            raise RuntimeError('Missing benchmark phases: ' + output)
        result = {'phases': [{'name': name, 'frames': int(frames), 'elapsed_ms': int(ms)}
                             for name, frames, ms in phases], 'stats': device.stats()}
        (args.output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        (args.output / 'console.txt').write_text(output)
    finally:
        if device.status() != 1:
            device.wait_shell(timeout=30)
