import os
import subprocess
import sys
import time
import unittest
from pipes import capture

class Pipes(unittest.TestCase):
    def launch(self, code):
        return subprocess.Popen([sys.executable, '-c', code], stdout=subprocess.PIPE)

    def test_exited_launcher_with_inherited_writer_has_deadline(self):
        # The orphan exits itself; no process is signalled by this regression.
        child = self.launch('import os,time; p=os.fork(); time.sleep(.6) if p==0 else None; os._exit(0)')
        self.assertEqual(child.wait(1), 0)
        start = time.monotonic()
        output, eof = capture(child.stdout, start + .15)
        self.assertEqual(output, b'')
        self.assertFalse(eof)
        self.assertLess(time.monotonic() - start, .5)

    def test_real_eof(self):
        child = self.launch('print("bash")')
        output, eof = capture(child.stdout, time.monotonic() + 1)
        self.assertTrue(eof)
        self.assertEqual(output, b'bash\n')
        self.assertEqual(child.wait(1), 0)

    def test_byte_budget(self):
        child = self.launch('print("x" * 100)')
        with self.assertRaisesRegex(ValueError, 'byte budget'):
            capture(child.stdout, time.monotonic() + 1, limit=10)
        self.assertEqual(child.wait(1), 0)

if __name__ == '__main__':
    unittest.main()
