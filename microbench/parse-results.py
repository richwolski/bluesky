#!/usr/bin/python2

import os, re, sys

def load_results(prefix):
    settings_file = open(prefix + ".settings")
    results_file = open(prefix + ".results")

    settings = {}
    for l in settings_file:
        l = l.strip()
        try:
            (k, v) = l.split('=', 1)
            settings[k] = v
        except: pass

    results = []
    result_group = {}
    for l in results_file:
        l = l.strip()
        if len(l) == 0 and len(result_group) > 0:
            results.append(result_group)
            result_group = {}
        m = re.match(r"^(\w+): \[(.*)\]$", l)
        if m:
            vals = [float(x.strip()) for x in m.group(2).split(',')]
            result_group[m.group(1)] = vals
    if len(result_group) > 0:
        results.append(result_group)

    return (settings, results)

def extract(data, params={}, ty='read', index=0):
    if ty == 'mixed':
        ty = ('read', 'write')
    else:
        ty = (ty,)

    results = {}
    for (s, r) in data:
        match = True
        for (k, v) in params.items():
            if s[k] != v: match = False
        if not match: continue

        ops = int(s['BENCH_OPS'])

        vs = 0.0
        for t in ty:
            vals = [x[t][index] for x in r]
            vals = vals[5:]
            vs += sum(vals) / (len(vals) or 1)
        results[ops] = vs

    return results

data = []
if __name__ == '__main__':
    dirname = '20110310'
    for f in os.listdir(dirname):
        if f.endswith('.settings'):
            data.append(load_results(dirname + '/' + f[:-len('.settings')]))

ratios = {'read': '0.0', 'write': '1.0', 'mixed': '0.5'}

blocksizes = set(int(s[0]['BENCH_BLOCKSIZE'])
                   or int(s[0]['BENCH_FILESIZE'])
                 for s in data)
print blocksizes
sizes = set(int(s[0]['BENCH_FILESIZE']) * int(s[0]['BENCH_FILECOUNT']) / 1024**2
                for s in data)
print sizes

for size in sorted(sizes):
    for blocksize in sorted(blocksizes):
        for ratio in ratios:
            params = {'BLUESKY_TARGET': 's3:mvrable-bluesky-west',
                      'BENCH_WRITERATIO': ratios[ratio],
                      'BENCH_FILECOUNT': str(size),
                      'BENCH_BLOCKSIZE': str(blocksize)}
            basesize = 1 << 20
            if blocksize < basesize:
                params['BENCH_BLOCKSIZE'] = str(blocksize)
                params['BENCH_FILESIZE'] = str(basesize)
            elif blocksize >= basesize:
                params['BENCH_BLOCKSIZE'] = str(0)
                params['BENCH_FILESIZE'] = str(blocksize)

            d0 = extract(data, params, ty=ratio, index=0)
            d1 = extract(data, params, ty=ratio, index=1)

            fp = open('%s-%d-%s-%dk.data' % (params['BLUESKY_TARGET'], size, ratio, blocksize / 1024), 'w')
            for k in sorted(d0.keys()):
                fp.write("%d\t%f\t%f\n" % (k, d0[k], d1[k]))
            fp.close()
