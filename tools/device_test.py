#!/usr/bin/env python3
"""Local USB test runner for opt-in Tab5 development firmware."""
from __future__ import annotations
import argparse
import importlib.util
from pathlib import Path
import re
import struct
import sys
import subprocess
import time
import zlib

class Device:
    def __init__(self, port: str, output: Path):
        import serial
        holders = subprocess.run(['lsof', '-t', port], capture_output=True, text=True)
        if holders.returncode not in (0, 1) or holders.stdout.strip():
            raise RuntimeError('Stop the serial monitor before testing')
        self.serial = serial.Serial(port=None, baudrate=115200, timeout=.2, write_timeout=2, exclusive=True)
        self.serial.dtr = False
        self.serial.rts = False
        self.serial.port = port
        self.serial.open()
        self.log = (output / 'serial.log').open('wb')
        self.output = ''
        self.partial = b''
        self.transferring = False
        self.command_pending = False

    def line(self, timeout=5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            data = self.serial.readline()
            if data:
                self.log.write(data)
                self.log.flush()
                self.partial += data
                if not self.partial.endswith(b'\n'):
                    continue
                line = self.partial.decode(errors='replace').strip()
                self.partial = b''
                if any(s in line for s in ('Guru Meditation', 'panic', 'watchdog')):
                    raise RuntimeError('Device fault: ' + line)
                if line.startswith('TABOS TEST ERROR'):
                    raise RuntimeError(line)
                if line.startswith('TABOS TEST '):
                    return line
        raise TimeoutError('No test-control response; enable TABOS_ENABLE_DEVICE_TEST_CONTROL')

    def request(self, command, prefix='TABOS TEST OK', timeout=5):
        self.serial.write(('TABOS TEST ' + command + '\n').encode())
        self.serial.flush()
        response = self.line(timeout)
        if not response.startswith(prefix):
            raise RuntimeError('Unexpected response: ' + response)
        return response

    def status(self):
        return int(self.request('STATUS', 'TABOS TEST STATUS ').split()[-1])

    def mute_output(self, muted=True):
        """Select hardware codec mute at the shell without changing PCM work."""
        self.request('MUTE 1' if muted else 'MUTE 0')

    def read(self):
        while True:
            # Exit/reporting may hold the console lock while graphics/audio close.
            # Wait for this response rather than sending a second command.
            line = self.request('READ', 'TABOS TEST DATA', timeout=15)
            chunk = bytes.fromhex(line.removeprefix('TABOS TEST DATA').strip()).decode(errors='replace')
            self.output += chunk
            if not chunk:
                return self.output

    def key(self, key, modifiers=0, down=True):
        self.request(f'KEY {key} {modifiers} {int(down)}')
        time.sleep(.04)

    def tap(self, key, modifiers=0):
        self.key(key, modifiers)
        self.key(key, modifiers, False)

    def start(self, command):
        if self.status() != 1:
            raise RuntimeError('Device is busy; refusing to replace an active application')
        self.request('BEGIN')
        self.output = ''
        for start in range(0, len(command), 9):
            self.request('TEXT ' + command[start:start+9].encode('ascii').hex())
            time.sleep(.04)
        self.command_pending = True
        self.tap(40)

    def wait_shell(self, timeout=120):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.read()
            # Serial input acknowledgement precedes runtime launch. A transient
            # process count of one is not completion; require the new prompt.
            prompt = re.search(r'(?:^|[\r\n])(?:[A-Za-z]:/[^\r\n]*)?> $', self.output)
            if self.status() == 1 and (not self.command_pending or prompt is not None):
                self.command_pending = False
                return self.read()
            time.sleep(.25)
        raise TimeoutError('Application did not return to the shell')

    def stats(self):
        return {k: int(v) for k, v in re.findall(
            r'(\w+)=(\d+)', self.request('STATS', 'TABOS TEST STATS '))}

    def drain_screenshot(self, timeout=90):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                if self.line(min(5, deadline - time.monotonic())) == 'TABOS TEST OK':
                    return
            except (RuntimeError, TimeoutError):
                continue
        raise TimeoutError('Could not finish interrupted screenshot transfer')

    def screenshot(self, path):
        for attempt in range(2):
            try:
                self._screenshot(path)
                return
            except (ValueError, RuntimeError, TimeoutError) as error:
                transferring = self.transferring
                if transferring:
                    self.drain_screenshot()
                    self.transferring = False
                # A benign firmware log may split a row. Retry only a completed
                # transfer; faults and command rejection still fail the test.
                if attempt == 1 or not transferring or str(error).startswith('Device fault:'):
                    raise

    def _screenshot(self, path):
        self.request('SHOT', 'TABOS TEST IMAGE 640 360', timeout=10)
        self.transferring = True
        raw = bytearray()
        for _ in range(360):
            line = self.line(10)
            pixels = decode_row(line)
            raw.append(0)  # PNG row filter
            for pixel, in struct.iter_unpack('>H', pixels):
                r, g, b = pixel >> 11, (pixel >> 5) & 63, pixel & 31
                raw.extend(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
        if self.line() != 'TABOS TEST OK':
            raise RuntimeError('Incomplete screenshot')
        self.transferring = False
        def chunk(kind, data):
            return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
        path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 640, 360, 8, 2, 0, 0, 0)) +
                         chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))

    def close(self):
        self.serial.close()
        self.log.close()

def decode_row(line):
    if line.startswith('TABOS TEST ROW '):
        pixels = bytes.fromhex(line.removeprefix('TABOS TEST ROW '))
    elif line.startswith('TABOS TEST RLE '):
        encoded = bytes.fromhex(line.removeprefix('TABOS TEST RLE '))
        if len(encoded) % 4 != 0:
            raise RuntimeError('Invalid screenshot run')
        pixels = bytearray()
        for count, pixel in struct.iter_unpack('>HH', encoded):
            if count == 0 or len(pixels) + count * 2 > 1280:
                raise RuntimeError('Invalid screenshot run length')
            pixels.extend(struct.pack('>H', pixel) * count)
    else:
        raise RuntimeError('Missing screenshot row')
    if len(pixels) != 1280:
        raise RuntimeError('Invalid screenshot width')
    return pixels

def load_workload(app):
    if re.fullmatch(r'[a-z][a-z0-9_-]*', app) is None:
        raise ValueError('Invalid app name: ' + app)
    path = Path(__file__).resolve().parents[1] / 'apps' / app / 'tests' / 'device.py'
    if not path.is_file():
        raise ValueError('No device tests for app: ' + app)
    spec = importlib.util.spec_from_file_location('tabos_device_workload_' + app, path)
    workload = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(workload)
    return workload

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('app', help='App with tests in apps/<app>/tests/device.py')
    parser.add_argument('--port', default='/dev/cu.usbmodem101')
    parser.add_argument('--output', type=Path, required=True)
    # Load the selected workload before parsing its app-specific options or
    # opening the serial port. Names cannot select paths outside apps/.
    selector = argparse.ArgumentParser(add_help=False)
    selector.add_argument('app', nargs='?')
    selector.add_argument('--port')
    selector.add_argument('--output')
    selection, _ = selector.parse_known_args()
    if selection.app is None:
        parser.parse_args()
    try:
        workload = load_workload(selection.app)
    except ValueError as error:
        parser.error(str(error))
    workload.configure(parser)
    args = parser.parse_args()
    workload.validate(parser, args)
    args.output.mkdir(parents=True, exist_ok=False)
    device = Device(args.port, args.output)
    try:
        device.request('BEGIN')
        configuration = device.stats()
        if configuration.get('cpu_mhz', 0) <= 0 or configuration.get('cpu_mhz') != configuration.get('configured_cpu_mhz'):
            raise RuntimeError('Actual CPU clock does not match configured frequency: ' + str(configuration))
        workload.run(device, args, configuration)
    finally:
        try:
            device.request('END')
        except Exception as error:
            print('Device cleanup failed: ' + str(error), file=sys.stderr)
        finally:
            device.close()

if __name__ == '__main__':
    main()
