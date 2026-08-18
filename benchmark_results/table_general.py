import sys
from contextlib import ExitStack

from benchmark_parser import aggregate, format

PARTIES = 3
COMPR = 20
DEPTH = [10, 30, 100]
MULTS = 1000000
THREADS = [0, 8] # 0 threads means single thread, compiled without OMP

if len(sys.argv) > 1 and sys.argv[1] == "latex":
    LATEX = True
else:
    LATEX = False

# Creates a table row for the given benchmark results (passive baseline)
def get_row_semi(lan_semi, wan_semi, lan_semi_t8, wan_semi_t8):
    off_com_semi, on_com_semi, rounds_semi, off_lan_semi, on_lan_semi, off_wan_semi, on_wan_semi = aggregate(lan_semi, wan_semi)
    _, _, _, off_lan_semi_t8, on_lan_semi_t8, off_wan_semi_t8, on_wan_semi_t8 = aggregate(lan_semi_t8, wan_semi_t8)
    # We want online comm. per party, so divide by PARTIES, offline is only Dealer
    if LATEX:
        return f'${format(off_com_semi, 2, 4)}+{format(on_com_semi / PARTIES, 2, 4)}$ & ${format(rounds_semi, 0, 3)}$ & ${format(off_lan_semi, 2, 4)}+{format(on_lan_semi, 2, 4)}$ & ${format(off_wan_semi, 2, 4)}+{format(on_wan_semi, 2, 5)}$ & ${format(off_lan_semi_t8, 2, 4)}+{format(on_lan_semi_t8, 2, 4)}$ & ${format(off_wan_semi_t8, 2, 4)}+{format(on_wan_semi_t8, 2, 5)}$'
    else:
        return f'{format(off_com_semi, 2, 4)}+{format(on_com_semi / PARTIES, 2, 4)} |    {format(rounds_semi, 0, 3)}  | {format(off_lan_semi, 2, 4)}+{format(on_lan_semi, 2, 4)} | {format(off_wan_semi, 2, 4)}+{format(on_wan_semi, 2, 5)} | {format(off_lan_semi_t8, 2, 4)}+{format(on_lan_semi_t8, 2, 4)} | {format(off_wan_semi_t8, 2, 4)}+{format(on_wan_semi_t8, 2, 5)}'

# Creates a table row for the given benchmark results (actively secure protocol)
def get_row_active(lan_fliop, wan_fliop, lan_fliop_t8, wan_fliop_t8):
    off_com_fliop, on_com_fliop, rounds_fliop, off_lan_fliop, on_lan_fliop, off_wan_fliop, on_wan_fliop = aggregate(lan_fliop, wan_fliop)
    _, _, _, off_lan_fliop_t8, on_lan_fliop_t8, off_wan_fliop_t8, on_wan_fliop_t8 = aggregate(lan_fliop_t8, wan_fliop_t8)
    # We want online comm. per party, so divide by PARTIES, offline is only Dealer
    if LATEX:
        return f'${format(off_com_fliop, 2, 4)}+{format(on_com_fliop / PARTIES, 2, 4)}$ & ${format(rounds_fliop, 0, 3)}$ & ${format(off_lan_fliop, 2, 4)}+{format(on_lan_fliop, 2, 4)}$ & ${format(off_wan_fliop, 2, 4)}+{format(on_wan_fliop, 2, 5)}$& ${format(off_lan_fliop_t8, 2, 4)}+{format(on_lan_fliop_t8, 2, 4)}$ & ${format(off_wan_fliop_t8, 2, 4)}+{format(on_wan_fliop_t8, 2, 5)}$'
    else:
        return f'{format(off_com_fliop, 2, 4)}+{format(on_com_fliop / PARTIES, 2, 4)} |    {format(rounds_fliop, 0, 3)}  | {format(off_lan_fliop, 2, 4)}+{format(on_lan_fliop, 2, 4)} | {format(off_wan_fliop, 2, 4)}+{format(on_wan_fliop, 2, 5)} | {format(off_lan_fliop_t8, 2, 4)}+{format(on_lan_fliop_t8, 2, 4)} | {format(off_wan_fliop_t8, 2, 4)}+{format(on_wan_fliop_t8, 2, 5)}'

# Creates a table row for the given benchmark results (overhead fliop version vs passive baseline)
def get_row_overhead(lan_semi, lan_fliop, wan_semi, wan_fliop, lan_semi_t8, lan_fliop_t8, wan_semi_t8, wan_fliop_t8):
    off_com_semi, on_com_semi, rounds_semi, off_lan_semi, on_lan_semi, off_wan_semi, on_wan_semi = aggregate(lan_semi, wan_semi)
    off_com_fliop, on_com_fliop, rounds_fliop, off_lan_fliop, on_lan_fliop, off_wan_fliop, on_wan_fliop = aggregate(lan_fliop, wan_fliop)
    _, _, _, off_lan_semi_t8, on_lan_semi_t8, off_wan_semi_t8, on_wan_semi_t8 = aggregate(lan_semi_t8, wan_semi_t8)
    _, _, _, off_lan_fliop_t8, on_lan_fliop_t8, off_wan_fliop_t8, on_wan_fliop_t8 = aggregate(lan_fliop_t8, wan_fliop_t8)
    if LATEX:
        return f'$+{format(100*(off_com_fliop+on_com_fliop)/(off_com_semi+on_com_semi)-100, 1, 3)}\\%$ & $+{format(100*rounds_fliop/rounds_semi-100, 1, 5)}\\%$ & $+{format(100*(off_lan_fliop+on_lan_fliop)/(off_lan_semi+on_lan_semi)-100, 1, 5)}\\%$ & $+{format(100*(off_wan_fliop+on_wan_fliop)/(off_wan_semi+on_wan_semi)-100, 1, 4)}\\%$ & $+{format(100*(off_lan_fliop_t8+on_lan_fliop_t8)/(off_lan_semi_t8+on_lan_semi_t8)-100, 1, 5)}\\%$ & $+{format(100*(off_wan_fliop_t8+on_wan_fliop_t8)/(off_wan_semi_t8+on_wan_semi_t8)-100, 1, 4)}\\%$'
    else:
        return f'  +{format(100*(off_com_fliop+on_com_fliop)/(off_com_semi+on_com_semi)-100, 1, 3)}%   | +{format(100*rounds_fliop/rounds_semi-100, 1, 5)}% |  +{format(100*(off_lan_fliop+on_lan_fliop)/(off_lan_semi+on_lan_semi)-100, 1, 5)}%  |   +{format(100*(off_wan_fliop+on_wan_fliop)/(off_wan_semi+on_wan_semi)-100, 1, 4)}%   |  +{format(100*(off_lan_fliop_t8+on_lan_fliop_t8)/(off_lan_semi_t8+on_lan_semi_t8)-100, 1, 5)}%  |   +{format(100*(off_wan_fliop_t8+on_wan_fliop_t8)/(off_wan_semi_t8+on_wan_semi_t8)-100, 1, 4)}%'

if __name__ == "__main__":
    if not LATEX:
        print("depth |          |   comm.   |  rounds |  time LAN |  time WAN  | t. 8t LAN | t. 8t WAN")
    for depth in DEPTH:
        with ExitStack() as stack:
            files_LAN_semi = [stack.enter_context(open(f"LAN/p{p}/semi-m{MULTS}-n{PARTIES}-d{depth}-pking-t{0}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            files_WAN_semi = [stack.enter_context(open(f"WAN/p{p}/semi-m{MULTS}-n{PARTIES}-d{depth}-pking-t{0}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            files_LAN_semi_t8 = [stack.enter_context(open(f"LAN/p{p}/semi-m{MULTS}-n{PARTIES}-d{depth}-pking-t{8}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            files_WAN_semi_t8 = [stack.enter_context(open(f"WAN/p{p}/semi-m{MULTS}-n{PARTIES}-d{depth}-pking-t{8}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            assert all(len(f) == len(files_LAN_semi[0]) for f in files_LAN_semi)
            assert all(len(f) == len(files_WAN_semi[0]) for f in files_WAN_semi)
            assert all(len(f) == len(files_LAN_semi_t8[0]) for f in files_LAN_semi_t8)
            assert all(len(f) == len(files_WAN_semi_t8[0]) for f in files_WAN_semi_t8)

            if LATEX:
                print(f'\\Block{{3-1}}{{${depth:3}$}} & passive  & {get_row_semi(files_LAN_semi, files_WAN_semi, files_LAN_semi_t8, files_WAN_semi_t8)} \\\\')
            else:
                print(f'  {depth:3} | passive  | {get_row_semi(files_LAN_semi, files_WAN_semi, files_LAN_semi_t8, files_WAN_semi_t8)}')

            files_LAN_fliop = [stack.enter_context(open(f"LAN/p{p}/fliop-m{MULTS}-n{PARTIES}-c{COMPR}-d{depth}-pking-1-t{0}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            files_WAN_fliop = [stack.enter_context(open(f"WAN/p{p}/fliop-m{MULTS}-n{PARTIES}-c{COMPR}-d{depth}-pking-1-t{0}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            files_LAN_fliop_t8 = [stack.enter_context(open(f"LAN/p{p}/fliop-m{MULTS}-n{PARTIES}-c{COMPR}-d{depth}-pking-1-t{8}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            files_WAN_fliop_t8 = [stack.enter_context(open(f"WAN/p{p}/fliop-m{MULTS}-n{PARTIES}-c{COMPR}-d{depth}-pking-1-t{8}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            assert all(len(f) == len(files_LAN_fliop[0]) for f in files_LAN_fliop)
            assert all(len(f) == len(files_WAN_fliop[0]) for f in files_WAN_fliop)
            assert all(len(f) == len(files_LAN_fliop_t8[0]) for f in files_LAN_fliop_t8)
            assert all(len(f) == len(files_WAN_fliop_t8[0]) for f in files_WAN_fliop_t8)

            if LATEX:
                print(f' & active & {get_row_active(files_LAN_fliop, files_WAN_fliop, files_LAN_fliop_t8, files_WAN_fliop_t8)} \\\\')
            else:
                print(f'      | active   | {get_row_active(files_LAN_fliop, files_WAN_fliop, files_LAN_fliop_t8, files_WAN_fliop_t8)}')

            if LATEX:
                print(f' & overhead & {get_row_overhead(files_LAN_semi, files_LAN_fliop, files_WAN_semi, files_WAN_fliop, files_LAN_semi_t8, files_LAN_fliop_t8, files_WAN_semi_t8, files_WAN_fliop_t8)} \\\\ \\midrule')
            else:
               print(f'      | overhead | {get_row_overhead(files_LAN_semi, files_LAN_fliop, files_WAN_semi, files_WAN_fliop, files_LAN_semi_t8, files_LAN_fliop_t8, files_WAN_semi_t8, files_WAN_fliop_t8)}') 
