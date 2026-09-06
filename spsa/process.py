"""Owned subprocess trees; Windows jobs close even when the coordinator dies."""
import ctypes
from ctypes import wintypes
import os
import signal
import subprocess
import time


if os.name == "nt":
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    ntdll = ctypes.WinDLL("ntdll")

    class IO_COUNTERS(ctypes.Structure):
        _fields_ = [(n, ctypes.c_ulonglong) for n in (
            "ReadOperationCount", "WriteOperationCount", "OtherOperationCount",
            "ReadTransferCount", "WriteTransferCount", "OtherTransferCount")]

    class BASIC_LIMIT(ctypes.Structure):
        _fields_ = [("PerProcessUserTimeLimit", ctypes.c_longlong),
                    ("PerJobUserTimeLimit", ctypes.c_longlong), ("LimitFlags", wintypes.DWORD),
                    ("MinimumWorkingSetSize", ctypes.c_size_t), ("MaximumWorkingSetSize", ctypes.c_size_t),
                    ("ActiveProcessLimit", wintypes.DWORD), ("Affinity", ctypes.c_size_t),
                    ("PriorityClass", wintypes.DWORD), ("SchedulingClass", wintypes.DWORD)]

    class EXTENDED_LIMIT(ctypes.Structure):
        _fields_ = [("BasicLimitInformation", BASIC_LIMIT), ("IoInfo", IO_COUNTERS),
                    ("ProcessMemoryLimit", ctypes.c_size_t), ("JobMemoryLimit", ctypes.c_size_t),
                    ("PeakProcessMemoryUsed", ctypes.c_size_t), ("PeakJobMemoryUsed", ctypes.c_size_t)]

    class ACCOUNTING(ctypes.Structure):
        _fields_ = [(n, ctypes.c_longlong) for n in ("TotalUserTime", "TotalKernelTime", "ThisPeriodTotalUserTime", "ThisPeriodTotalKernelTime")] + [
            (n, wintypes.DWORD) for n in ("TotalPageFaultCount", "TotalProcesses", "ActiveProcesses", "TotalTerminatedProcesses")]

    kernel.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
    kernel.CreateJobObjectW.restype = wintypes.HANDLE
    kernel.SetInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
    kernel.SetInformationJobObject.restype = wintypes.BOOL
    kernel.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
    kernel.AssignProcessToJobObject.restype = wintypes.BOOL
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL
    kernel.TerminateJobObject.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel.TerminateJobObject.restype = wintypes.BOOL
    kernel.QueryInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD, ctypes.c_void_p]
    kernel.QueryInformationJobObject.restype = wintypes.BOOL
    ntdll.NtResumeProcess.argtypes = [wintypes.HANDLE]
    ntdll.NtResumeProcess.restype = ctypes.c_long


class OwnedProcess:
    def __init__(self, args, **kwargs):
        self.job = None
        self.proc = None
        try:
            if os.name == "nt":
                self.job = kernel.CreateJobObjectW(None, None)
                if not self.job:
                    raise ctypes.WinError(ctypes.get_last_error())
                limits = EXTENDED_LIMIT()
                limits.BasicLimitInformation.LimitFlags = 0x2000  # KILL_ON_JOB_CLOSE
                if not kernel.SetInformationJobObject(self.job, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
                    raise ctypes.WinError(ctypes.get_last_error())
                # Suspend before assigning the job: no child can escape between
                # Popen and assignment. Popen closes the primary thread handle,
                # so resume via the process handle after assignment succeeds.
                self.proc = subprocess.Popen(args, creationflags=0x00000004 | 0x08000000, **kwargs)
                if not kernel.AssignProcessToJobObject(self.job, int(self.proc._handle)):
                    raise ctypes.WinError(ctypes.get_last_error())
                if ntdll.NtResumeProcess(int(self.proc._handle)) != 0:
                    raise OSError("Could not resume job-owned process")
            else:
                self.proc = subprocess.Popen(args, start_new_session=True, **kwargs)
        except BaseException:
            self.close()
            raise

    def close(self):
        if self.job:
            # Wait for every descendant, not just cutechess, before returning
            # artifact files to the caller. Closing a job starts asynchronous
            # termination; its children may still have log files open briefly.
            kernel.TerminateJobObject(self.job, 1)
            deadline = time.monotonic() + 5
            accounting = ACCOUNTING()
            while time.monotonic() < deadline:
                if not kernel.QueryInformationJobObject(self.job, 1, ctypes.byref(accounting), ctypes.sizeof(accounting), None):
                    break
                if accounting.ActiveProcesses == 0:
                    break
                time.sleep(.01)
            kernel.CloseHandle(self.job)
            self.job = None
        if self.proc:
            if os.name != "nt":
                try:
                    os.killpg(self.proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            if self.proc.poll() is None:
                self.proc.kill()
            self.proc.wait()
            for stream in (self.proc.stdin, self.proc.stdout, self.proc.stderr):
                if stream:
                    stream.close()

    def __enter__(self):
        return self.proc

    def __exit__(self, *exc):
        self.close()
