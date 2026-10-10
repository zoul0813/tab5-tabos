"""Transport/result regression checks without a connected device."""
import importlib.util
from pathlib import Path
import struct
import io
import unittest
import tempfile
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('device_test', Path(__file__).resolve().parents[2] / 'tools/device_test.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class DeviceTest(unittest.TestCase):
    def test_raw_and_compressed_rows(self):
        raw = struct.pack('>H', 0x1234) * 640
        self.assertEqual(module.decode_row('TABOS TEST ROW ' + raw.hex()), raw)
        self.assertEqual(module.decode_row('TABOS TEST RLE 02801234'), raw)
        for malformed in ['TABOS TEST ROW 1234', 'TABOS TEST RLE 00001234',
                          'TABOS TEST RLE 02811234', 'TABOS TEST RLE 01001234', 'TABOS TEST RLE 00']:
            with self.assertRaises(RuntimeError):
                module.decode_row(malformed)

    def test_busy_device_does_not_receive_commands(self):
        class Busy(module.Device):
            def status(self):
                return 2
            def request(self, *args):
                raise AssertionError('Busy device received a command')
        with self.assertRaises(RuntimeError):
            Busy.__new__(Busy).start('example')

    def test_interrupted_screenshot_drains_before_cleanup(self):
        class Interrupted(module.Device):
            def _screenshot(self, path):
                raise ValueError('Interrupted row')
            def line(self, timeout):
                return self.responses.pop(0)
        device = Interrupted.__new__(Interrupted)
        device.transferring = True
        device.responses = ['TABOS TEST RLE 02800000', 'TABOS TEST OK']
        with self.assertRaises(ValueError):
            device.screenshot(Path('/unused.png'))
        self.assertEqual(device.responses, [])

    def test_partial_serial_lines_and_embedded_faults(self):
        class Serial:
            def __init__(self, chunks):
                self.chunks = iter(chunks)
            def readline(self):
                return next(self.chunks)
        device = module.Device.__new__(module.Device)
        device.serial = Serial([b'firmware log\n', b'TABOS TEST O', b'K\n'])
        device.log = io.BytesIO()
        device.partial = b''
        self.assertEqual(device.line(), 'TABOS TEST OK')
        device.serial = Serial([b'TABOS TEST RLE abc Task watchdog got triggered\n'])
        with self.assertRaisesRegex(RuntimeError, 'Device fault'):
            device.line()

    def test_clock_mismatch_prevents_application_launch(self):
        class WrongClock:
            closed = False
            def request(self, command, *args):
                if command == 'STATS':
                    return 'TABOS TEST STATS cpu_mhz=90 configured_cpu_mhz=400'
                return 'TABOS TEST OK'
            def stats(self):
                return {'cpu_mhz': 90, 'configured_cpu_mhz': 400}
            def start(self, command):
                raise AssertionError('Clock mismatch launched an application')
            def close(self):
                self.closed = True
        device = WrongClock()
        with tempfile.TemporaryDirectory() as folder:
            with patch.object(module, 'Device', return_value=device), patch('sys.argv',
                    ['device_test.py', 'tester', '--output', str(Path(folder) / 'run')]):
                with self.assertRaisesRegex(RuntimeError, 'Actual CPU clock'):
                    module.main()
        self.assertTrue(device.closed)

    def test_workload_selection_rejects_unknown_and_traversal(self):
        for name in ('../tester', 'tester/tests/device', '/tester', 'no-such-app'):
            with self.assertRaises(ValueError):
                module.load_workload(name)
        self.assertTrue(callable(module.load_workload('tester').run))

    def test_failed_workload_ends_session_and_closes_transport(self):
        from types import SimpleNamespace
        device = unittest.mock.Mock()
        device.stats.return_value = {'cpu_mhz': 360, 'configured_cpu_mhz': 360}
        workload = SimpleNamespace(configure=lambda parser: None, validate=lambda parser, args: None,
                                   run=unittest.mock.Mock(side_effect=RuntimeError('workload failed')))
        with tempfile.TemporaryDirectory() as folder:
            with patch.object(module, 'Device', return_value=device), \
                    patch.object(module, 'load_workload', return_value=workload), \
                    patch('sys.argv', ['device_test.py', 'example', '--output', str(Path(folder) / 'run')]):
                with self.assertRaisesRegex(RuntimeError, 'workload failed'):
                    module.main()
        self.assertEqual([call.args for call in device.request.call_args_list], [('BEGIN',), ('END',)])
        device.close.assert_called_once()

    def test_general_workloads_use_real_commands_and_parse_results(self):
        from types import SimpleNamespace
        import json
        class Fake:
            def __init__(self, outputs):
                self.outputs = iter(outputs)
                self.commands = []
            def start(self, command):
                self.commands.append(command)
            def wait_shell(self, **kwargs):
                return next(self.outputs)
            def status(self):
                return 1
            def stats(self):
                return {'frames': 240}
        for app, outputs, expected in (
            ('tester', ['Filesystem assertions: 24; failures: 0', 'Input assertions: 18; failures: 0', 'Compute assertions: 12; failures: 0'],
             ['tester --filesystem', 'tester --input', 'tester --compute']),
            ('graphics_benchmark', ['Present only: 60 frames in 1000 ms\nClear + present: 60 frames in 1000 ms\nScene: 120 frames in 2000 ms'], ['gbench']),
        ):
            with tempfile.TemporaryDirectory() as folder:
                fake = Fake(outputs)
                args = SimpleNamespace(output=Path(folder), screenshots=False)
                module.load_workload(app).run(fake, args, None)
                self.assertEqual(fake.commands, expected)
                self.assertEqual(json.loads((Path(folder) / 'result.json').read_text())['stats']['frames'], 240)
        with tempfile.TemporaryDirectory() as folder:
            fake = Fake(['Filesystem assertions: 24; failures: 1'])
            with self.assertRaises(RuntimeError):
                module.load_workload('tester').run(fake, SimpleNamespace(output=Path(folder), screenshots=False), None)

    def test_shell_wait_does_not_confuse_pending_launch_with_completion(self):
        class Pending(module.Device):
            def __init__(self):
                self.command_pending = True
                self.output = ''
                self.outputs = iter(['gbench\n', 'gbench\nScene: done\nT:/> ', 'gbench\nScene: done\nT:/> '])
                self.reads = 0
            def read(self):
                self.reads += 1
                self.output = next(self.outputs)
                return self.output
            def status(self):
                return 1
        device = Pending()
        with patch.object(module.time, 'sleep'):
            self.assertIn('Scene: done', device.wait_shell())
        self.assertEqual(device.reads, 3)
        self.assertFalse(device.command_pending)

if __name__ == '__main__':
    unittest.main()
