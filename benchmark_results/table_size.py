import sys
from contextlib import ExitStack

from benchmark_parser import aggregate, format

PARTIES = 3
SIZE = [1000, 10000, 100000, 1000000, 10000000]
DEPTH = 30
COMPR = 20
THREADS = 8

M_LABELS = {
    1000: "1K",
    10000: "10K",
    100000: "100K",
    1000000: "1M",
    10000000: "10M"
}

if len(sys.argv) > 1 and sys.argv[1] == "latex":
    LATEX = True
else:
    LATEX = False

# Creates a table row for the given benchmark results (passive baseline)
def get_row_semi(lan_semi, wan_semi):
    off_com_semi, on_com_semi, rounds_semi, off_lan_semi, on_lan_semi, off_wan_semi, on_wan_semi = aggregate(lan_semi, wan_semi)
    if LATEX:
        return f'${format(off_com_semi+on_com_semi, 2, 6)}$ & ${format(rounds_semi, 0, 2)}$ & ${format(off_lan_semi+on_lan_semi, 2, 5)}$ & ${format(off_wan_semi+on_wan_semi, 2, 5)}$'
    else:
        return f' {format(off_com_semi+on_com_semi, 2, 6)} |     {format(rounds_semi, 0, 2)} |   {format(off_lan_semi+on_lan_semi, 2, 5)} |  {format(off_wan_semi+on_wan_semi, 2, 5)}'

# Creates a table row for the given benchmark results (actively secure protocol)
def get_row_active(lan_fliop, wan_fliop):
    off_com_fliop, on_com_fliop, rounds_fliop, off_lan_fliop, on_lan_fliop, off_wan_fliop, on_wan_fliop = aggregate(lan_fliop, wan_fliop)
    if LATEX:
        return f'${format(off_com_fliop+on_com_fliop, 2, 6)}$ & ${format(rounds_fliop, 0, 2)}$ & ${format(off_lan_fliop+on_lan_fliop, 2, 5)}$  & ${format(off_wan_fliop+on_wan_fliop, 2, 5)}$ '
    else:
        return f' {format(off_com_fliop+on_com_fliop, 2, 6)} |     {format(rounds_fliop, 0, 2)} |   {format(off_lan_fliop+on_lan_fliop, 2, 5)} |  {format(off_wan_fliop+on_wan_fliop, 2, 5)}'

# Creates a table row for the given benchmark results (overhead fliop version vs passive baseline)
def get_row_overhead(lan_semi, lan_fliop, wan_semi, wan_fliop):
    off_com_semi, on_com_semi, rounds_semi, off_lan_semi, on_lan_semi, off_wan_semi, on_wan_semi = aggregate(lan_semi, wan_semi)
    off_com_fliop, on_com_fliop, rounds_fliop, off_lan_fliop, on_lan_fliop, off_wan_fliop, on_wan_fliop = aggregate(lan_fliop, wan_fliop)
    if LATEX:
        return f'$+{format(100*(off_com_fliop+on_com_fliop)/(off_com_semi+on_com_semi)-100, 1, 5)}\\%$ & $+{format(100*rounds_fliop/rounds_semi-100, 1, 4)}\\%$ & $+{format(100*(off_lan_fliop+on_lan_fliop)/(off_lan_semi+on_lan_semi)-100, 1, 5)}\\%$ & $+{format(100*(off_wan_fliop+on_wan_fliop)/(off_wan_semi+on_wan_semi)-100, 1, 4)}\\%$'
    else:
        return f'+{format(100*(off_com_fliop+on_com_fliop)/(off_com_semi+on_com_semi)-100, 1, 5)}% | +{format(100*rounds_fliop/rounds_semi-100, 1, 4)}% | +{format(100*(off_lan_fliop+on_lan_fliop)/(off_lan_semi+on_lan_semi)-100, 1, 5)}% | +{format(100*(off_wan_fliop+on_wan_fliop)/(off_wan_semi+on_wan_semi)-100, 1, 4)}%'

if __name__ == "__main__":
    if not LATEX:
        print("   m |          |  comm.  | rounds | tm. LAN | tm. WAN ")
    for m in SIZE:
        with ExitStack() as stack:
            files_LAN_semi = [stack.enter_context(open(f"LAN/p{p}/semi-m{m}-n{PARTIES}-d{DEPTH}-pking-t{THREADS}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            files_WAN_semi = [stack.enter_context(open(f"WAN/p{p}/semi-m{m}-n{PARTIES}-d{DEPTH}-pking-t{THREADS}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            assert all(len(f) == len(files_LAN_semi[0]) for f in files_LAN_semi)
            assert all(len(f) == len(files_WAN_semi[0]) for f in files_WAN_semi)

            if LATEX:
                print(f'$\\Block{{3-1}}{{{M_LABELS[m]}}}$ & passive & {get_row_semi(files_LAN_semi, files_WAN_semi)} \\\\')
            else:
                print(f'{M_LABELS[m]:>4} | passive  | {get_row_semi(files_LAN_semi, files_WAN_semi)}')

            files_LAN_fliop = [stack.enter_context(open(f"LAN/p{p}/fliop-m{m}-n{PARTIES}-c{COMPR}-d{DEPTH}-pking-1-t{THREADS}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            files_WAN_fliop = [stack.enter_context(open(f"WAN/p{p}/fliop-m{m}-n{PARTIES}-c{COMPR}-d{DEPTH}-pking-1-t{THREADS}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            assert all(len(f) == len(files_LAN_fliop[0]) for f in files_LAN_fliop)
            assert all(len(f) == len(files_WAN_fliop[0]) for f in files_WAN_fliop)

            if LATEX:
                print(f'& active & {get_row_active(files_LAN_fliop, files_WAN_fliop)} \\\\')
            else:
                print(f'     | active   | {get_row_active(files_LAN_fliop, files_WAN_fliop)}')

            if LATEX:
                print(f'& overhead & {get_row_overhead(files_LAN_semi, files_LAN_fliop, files_WAN_semi, files_WAN_fliop)} \\\\ \\midrule')
            else:
                print(f'     | overhead | {get_row_overhead(files_LAN_semi, files_LAN_fliop, files_WAN_semi, files_WAN_fliop)}')
