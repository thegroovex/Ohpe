import json, sys, collections
def load(p):
    d = collections.defaultdict(dict)
    for l in open(p):
        try: j = json.loads(l)
        except Exception: continue
        d[j['file']][j['block']] = j
    return d
files = sys.argv[1:]
data = [load(f) for f in files]
common = set.intersection(*[set(d) for d in data])
print("presets in common:", len(common))
for f, d in zip(files, data):
    aff = [p for p in common if any(v['free'] > 0 or v['holds'] > 0 for v in d[p].values())]
    per = {}
    for b in (64, 128, 256, 512, 0):
        per[b] = sum(1 for p in common if b in d[p] and (d[p][b]['free'] > 0 or d[p][b]['holds'] > 0))
    ev = sum(1 for p in common if any(v['event'] > 0 for v in d[p].values()))
    tot = sum(v['free'] for p in common for v in d[p].values())
    print(f"{f}: affected {len(aff)}/{len(common)} = {100*len(aff)/max(1,len(common)):.1f}%  per block {per}  free clicks total {tot}  presets w/ event clicks {ev}")
if len(data) == 2:
    a, b = data
    worse = [(p, sum(v['free'] for v in b[p].values()) - sum(v['free'] for v in a[p].values())) for p in common]
    worse.sort(key=lambda x: -x[1])
    print("most worse:", worse[:25])
    better = sorted(worse, key=lambda x: x[1])[:10]
    print("most better:", better)
