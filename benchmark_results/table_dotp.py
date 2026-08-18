from contextlib import ExitStack
import sys

from benchmark_parser import aggregate, format

PARTIES = 3
DEPTH = 1
# 10000 many dim 10, 100, 1000
# 100000 many dim 10, 100
# 1000000 many dim 10
DIM = [(10000,10),(10000,100),(10000,1000),(100000,10),(100000,100),(1000000,10)]
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

def get_row(lan_semi, lan_fliop, wan_semi, wan_fliop):
    off_com_semi, on_com_semi, rounds_semi, off_lan_semi, on_lan_semi, off_wan_semi, on_wan_semi = aggregate(lan_semi, wan_semi, 1)
    off_com_fliop, on_com_fliop, rounds_fliop, off_lan_fliop, on_lan_fliop, off_wan_fliop, on_wan_fliop = aggregate(lan_fliop, wan_fliop, 1)
    if LATEX:
        return f'${format(off_com_fliop+on_com_fliop, 2, 5)}$ ($+{format(100*(off_com_fliop+on_com_fliop)/(off_com_semi+on_com_semi)-100, 1, 4)}\\%$) & ${format(rounds_fliop, 0, 2)}$ & ${format(off_lan_fliop+on_lan_fliop, 2, 4)}$ ($\\times{format((off_lan_fliop+on_lan_fliop)/(off_lan_semi+on_lan_semi), 1, 4)}$) & ${format(off_wan_fliop+on_wan_fliop, 2, 4)}$ ($\\times{format((off_wan_fliop+on_wan_fliop)/(off_wan_semi+on_wan_semi), 1, 3)}$)'
    else:
        return f'{format(off_com_fliop+on_com_fliop, 2, 5)} (+{format(100*(off_com_fliop+on_com_fliop)/(off_com_semi+on_com_semi)-100, 1, 4)}%) | {format(rounds_fliop, 0, 2)} | {format(off_lan_fliop+on_lan_fliop, 2, 4)} (x{format((off_lan_fliop+on_lan_fliop)/(off_lan_semi+on_lan_semi), 1, 4)}) | {format(off_wan_fliop+on_wan_fliop, 2, 4)} (x{format((off_wan_fliop+on_wan_fliop)/(off_wan_semi+on_wan_semi), 1, 3)})'

if __name__ == "__main__":
    if not LATEX:
        print(" #dp | dim. |  communication | r. |   time LAN   |   time WAN   ")
    for number, d in DIM:
        with ExitStack() as stack:
            files_LAN_semi = [stack.enter_context(open(f"LAN/p{p}/semi-dotp-d{d}-nn{number}-n{PARTIES}-d{DEPTH}-pking-t{THREADS}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            files_WAN_semi = [stack.enter_context(open(f"WAN/p{p}/semi-dotp-d{d}-nn{number}-n{PARTIES}-d{DEPTH}-pking-t{THREADS}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            assert all(len(f) == len(files_LAN_semi[0]) for f in files_LAN_semi)
            assert all(len(f) == len(files_WAN_semi[0]) for f in files_WAN_semi)
            files_LAN_fliop = [stack.enter_context(open(f"LAN/p{p}/fliop-dotp-d{d}-nn{number}-n{PARTIES}-c{COMPR}-d{DEPTH}-pking-1-t{THREADS}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            files_WAN_fliop = [stack.enter_context(open(f"WAN/p{p}/fliop-dotp-d{d}-nn{number}-n{PARTIES}-c{COMPR}-d{DEPTH}-pking-1-t{THREADS}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            assert all(len(f) == len(files_LAN_fliop[0]) for f in files_LAN_fliop)
            assert all(len(f) == len(files_WAN_fliop[0]) for f in files_WAN_fliop)
            if LATEX:
                print(f'{M_LABELS[number]} & ${d}$ & {get_row(files_LAN_semi, files_LAN_fliop, files_WAN_semi, files_WAN_fliop)} \\\\')
            else:
                print(f'{M_LABELS[number]:>4} | {d:>4} | {get_row(files_LAN_semi, files_LAN_fliop, files_WAN_semi, files_WAN_fliop)}')
