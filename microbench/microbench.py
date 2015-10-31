#!/usr/bin/python

import os, subprocess, sys, time

MNTDIR = '/mnt/bluesky'
SERVER = 'c09-45.sysnet.ucsd.edu'

class TestClient:
    def setup_client(self, export):
        print "Mounting file system..."
        subprocess.check_call(['mount', '-t', 'nfs',
                               '-o', 'vers=3,tcp,rw,soft,intr',
                               export, MNTDIR])

    def cleanup_client(self):
        print "Unmounting file system..."
        subprocess.check_call(['umount', '-f', MNTDIR])

    def run(self, proc, args=(), export=(SERVER + ":/export")):
        self.setup_client(export)
        startdir = os.getcwd()
        try:
            os.chdir(MNTDIR)
            results = proc(*args)
        finally:
            os.chdir(startdir)
            self.cleanup_client()
        return results

class TestCommands:
    def serial_stat(self):
        files = ["file-%d" % (i,) for i in range(8)]

        times = []
        for f in files:
            start = time.time()
            os.stat(f)
            times.append(time.time() - start)

        return times

    def serial_read(self):
        files = ["file-%d" % (i,) for i in range(8)]

        times = []
        for f in files:
            start = time.time()
            open(f).read()
            times.append(time.time() - start)

        return times

    def serial_write(self):
        files = ["write-%d" % (i,) for i in range(8)]

        times = []
        buf = "A" * 32768
        for f in files:
            start = time.time()
            fp = open(f, 'w')
            fp.write(buf)
            fp.close()
            times.append(time.time() - start)

        return times

    def large_write(self):
        buf = "A" * 1048576

        start = time.time()
        fp = open("largefile", 'w')
        for i in range(128): fp.write(buf)
        fp.close()

        return [time.time() - start]

    def large_read(self):
        buf = None

        start = time.time()
        fp = open("largefile")
        while buf != "":
            buf = fp.read(1048576)
        fp.close()

        return [time.time() - start]

    def shell(self):
        subprocess.call(['/bin/sh'])

if __name__ == '__main__':
    cmd = sys.argv[1]
    args = sys.argv[2:]
    if not cmd.startswith("_"):
        fun = getattr(TestCommands(), cmd)
        client = TestClient()
        result = client.run(fun, args)
        print "Results:", result
