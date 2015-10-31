#!/usr/bin/python

import os, random, sys, time

def read_files(files, rate=1.0):
    while True:
        f = random.sample(files, 1)[0]

        start = time.time()
        fp = open(f, 'r')
        blocks = (16 << 20) / 32768
        offset = random.randrange(blocks) * 32768
        fp.seek(offset)
        fp.read(32768)
        print time.time() - start
        fp.close()

if __name__ == '__main__':
    all_files = []
    for (path, dirs, files) in iter(os.walk(".")):
        for f in files:
            all_files.append(os.path.join(path, f))
    print len(all_files), "files total"
    read_files(all_files)
