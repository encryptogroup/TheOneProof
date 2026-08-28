import glob
import json
import sys

if "r" in sys.argv:
    PATH_SELECTOR = "_reproduced"
    print("using freshly benchmarked data")
else:
    PATH_SELECTOR = ""
    print("using data from repository")
if "silent" in sys.argv:
    SILENT = True
else:
    SILENT = False

lanfiles_fliop = glob.glob(f'LAN{PATH_SELECTOR}/*/fliop-*.txt')
lanfiles_semi = glob.glob(f'LAN{PATH_SELECTOR}/*/semi-*.txt')
wanfiles_fliop = glob.glob(f'WAN{PATH_SELECTOR}/*/fliop-*.txt')
wanfiles_semi = glob.glob(f'WAN{PATH_SELECTOR}/*/semi-*.txt')
max_ram = 0
max_ram_one_mil = 0

results = dict()

for f in lanfiles_fliop + lanfiles_semi + wanfiles_fliop + wanfiles_semi:
    with open(f, 'r') as ff:
        for line in ff.readlines():
            j = json.loads(line)
            val = j['stats']['peak_virtual_memory'] / 1024 / 1024 # in GiB
            max_ram = max(max_ram, val)
            if 'gates_per_level' in j['details'] and 'depth' in 'depth' in j['details']:
                mults = j['details']['gates_per_level'] * j['details']['depth']
                if mults >= 1000000 and mults <= 1000100: # because we sometimes have a bit more
                    max_ram_one_mil = max(max_ram_one_mil, val)
            parts = f.split("/")
            if parts[2] in results.keys():
                if parts[1] == "p0":
                    if "dealer" not in results[parts[2]].keys():
                        results[parts[2]]["dealer"] = val
                    else:
                        results[parts[2]]["dealer"] = max(results[parts[2]]["dealer"], val)
                else:
                    if "party" not in results[parts[2]].keys():
                        results[parts[2]]["party"] = val
                    else:
                        results[parts[2]]["party"] = max(results[parts[2]]["party"], val)
            else:
                if parts[1] == "p0":
                    results[parts[2]] = dict()
                    results[parts[2]]["dealer"] = val
                else:
                    results[parts[2]] = dict()
                    results[parts[2]]["party"] = val

for bench in results.keys():
    if not SILENT:
        print(f'{bench:<50}, dealer: {results[bench]["dealer"]:.2f} GiB, each party: {results[bench]["party"]:.2f} GiB max')

print(f'max per party: {max_ram:.2f} GiB, for 10^6 mults: {max_ram_one_mil:.2f} GiB')
