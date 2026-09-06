import ctypes
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

from spsa.process import OwnedProcess


@unittest.skipUnless(os.name == "nt", "Windows Job Object checks")
class ProcessTests(unittest.TestCase):
    def test_parent_death_kills_owned_child_tree(self):
        # A separate coordinator is killed without executing Python cleanup.
        # Both its job-owned child and grandchild must exit.
        with tempfile.TemporaryDirectory() as temp:
            pidfile = Path(temp) / "pids.txt"
            child = "import subprocess,sys,time; p=subprocess.Popen([sys.executable,'-c','import time; time.sleep(120)']); print(p.pid,flush=True); time.sleep(120)"
            coordinator = "import sys,subprocess,time; from pathlib import Path; from spsa.process import OwnedProcess; p=OwnedProcess([sys.executable,'-c',sys.argv[2]],stdout=subprocess.PIPE,text=True); grandchild=p.proc.stdout.readline().strip(); Path(sys.argv[1]).write_text(str(p.proc.pid)+' '+grandchild); time.sleep(120)"
            parent = subprocess.Popen([sys.executable, "-c", coordinator, str(pidfile), child])
            handles = []
            kernel = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
            kernel.OpenProcess.restype = ctypes.c_void_p
            kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
            kernel.WaitForSingleObject.restype = ctypes.c_ulong
            kernel.CloseHandle.argtypes = [ctypes.c_void_p]
            try:
                deadline = time.monotonic() + 10
                while not pidfile.exists() and time.monotonic() < deadline:
                    time.sleep(.05)
                self.assertTrue(pidfile.exists())
                for pid in map(int, pidfile.read_text().split()):
                    handle = kernel.OpenProcess(0x00100000, False, pid)
                    self.assertTrue(handle)
                    handles.append(handle)
                parent.kill(); parent.wait(timeout=10)
                for handle in handles:
                    self.assertEqual(kernel.WaitForSingleObject(handle, 5000), 0, "Child survived coordinator death")
            finally:
                if parent.poll() is None: parent.kill()
                parent.wait()
                for handle in handles: kernel.CloseHandle(handle)
