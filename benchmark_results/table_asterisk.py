import json
import statistics
from contextlib import ExitStack
import sys

from benchmark_parser import aggregate, format

PARTIES = 3
COMPR = 20
DEPTH = [10, 30, 100]
MULTS = 1000000
THREADS = 0

if len(sys.argv) > 1 and sys.argv[1] == "latex":
    LATEX = True
else:
    LATEX = False

def get_row(lan_semi, wan_semi):
    off_com_semi, on_com_semi, rounds_semi, off_lan_semi, on_lan_semi, off_wan_semi, on_wan_semi = aggregate(lan_semi, wan_semi)
    # We want online comm. per party, so divide by PARTIES, offline is only Dealer
    if LATEX:
        return f'${format(off_com_semi, 2, 5)}+{format(on_com_semi / PARTIES, 2, 4)}$& ${format(off_lan_semi, 2, 5)}+{format(on_lan_semi, 2, 4)}$& ${format(off_wan_semi, 2, 4)}+{format(on_wan_semi, 2, 5)}$'
    else:
        return f'{format(off_com_semi, 2, 5)}+{format(on_com_semi / PARTIES, 2, 4)} | {format(off_lan_semi, 2, 5)}+{format(on_lan_semi, 2, 4)} | {format(off_wan_semi, 2, 4)}+{format(on_wan_semi, 2, 5)}'

def get_row_impr(lan_asterisk, wan_asterisk, lan_ours, wan_ours):
    off_com_a, on_com_a, _, off_lan_a, on_lan_a, off_wan_a, on_wan_a = aggregate(lan_asterisk, wan_asterisk)
    off_com_o, on_com_o, _, off_lan_o, on_lan_o, off_wan_o, on_wan_o = aggregate(lan_ours, wan_ours)
    if LATEX:
        return f'$\\,\\,\\,{format(off_com_a / off_com_o, 1, 3)}{{\\times}}+{format(on_com_a / on_com_o, 1, 3)}{{\\times}}$& ${format(off_lan_a / off_lan_o, 1, 4)}{{\\times}}+{format(on_lan_a / on_lan_o, 1, 3)}{{\\times}}$& ${format(off_wan_a / off_wan_o, 1, 3)}{{\\times}}+{format(on_wan_a / on_wan_o, 1, 3)}{{\\times}}\\,\\,\\,$'
    else:
        return f'{format(off_com_a / off_com_o, 1, 3)}x+{format(on_com_a / on_com_o, 1, 3)}x | {format(off_lan_a / off_lan_o, 1, 4)}x+{format(on_lan_a / on_lan_o, 1, 3)}x | {format(off_wan_a / off_wan_o, 1, 3)}x+{format(on_wan_a / on_wan_o, 1, 3)}x'

if __name__ == "__main__":
    if not LATEX:
        print(" depth |          |    comm.   |  time LAN  |  time WAN  ")
    for depth in DEPTH:
        with ExitStack() as stack:
            files_LAN_asterisk = [stack.enter_context(open(f"Asterisk_LAN/p{p}/n{PARTIES}-c2-d{depth}-pking-0.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            files_WAN_asterisk = [stack.enter_context(open(f"Asterisk_WAN/p{p}/n{PARTIES}-c2-d{depth}-pking-0.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            assert all(len(f) == len(files_LAN_asterisk[0]) for f in files_LAN_asterisk)
            assert all(len(f) == len(files_WAN_asterisk[0]) for f in files_WAN_asterisk)

            if LATEX:
                print(f'\\Block{{3-1}}{{{depth}}} & \\cite{{ASTERISK}} & {get_row(files_LAN_asterisk, files_WAN_asterisk)} \\\\')
            else:
                print(f'   {depth:>3} | Asterisk | {get_row(files_LAN_asterisk, files_WAN_asterisk)}')

            files_LAN_fliop = [stack.enter_context(open(f"LAN/p{p}/fliop-m{MULTS}-n{PARTIES}-c{COMPR}-d{depth}-pking-1-t{THREADS}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            files_WAN_fliop = [stack.enter_context(open(f"WAN/p{p}/fliop-m{MULTS}-n{PARTIES}-c{COMPR}-d{depth}-pking-1-t{THREADS}.txt", 'r')).readlines() for p in range(PARTIES + 1)]
            assert all(len(f) == len(files_LAN_fliop[0]) for f in files_LAN_fliop)
            assert all(len(f) == len(files_WAN_fliop[0]) for f in files_WAN_fliop)

            if LATEX:
                print(f'& ours & {get_row(files_LAN_fliop, files_WAN_fliop)} \\\\')
            else:
                print(f'       |   ours   | {get_row(files_LAN_fliop, files_WAN_fliop)}')

            if LATEX:
                print(f'& impr. & {get_row_impr(files_LAN_asterisk, files_WAN_asterisk, files_LAN_fliop, files_WAN_fliop)} \\\\ \\midrule')
            else:
                print(f'       |   impr.  | {get_row_impr(files_LAN_asterisk, files_WAN_asterisk, files_LAN_fliop, files_WAN_fliop)}')
