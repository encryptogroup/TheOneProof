from contextlib import ExitStack
import sys

from benchmark_parser import aggregate, format

MULTS = 1000000
COMPR = 20
DEPTH = 30

if len(sys.argv) > 1 and sys.argv[1] == "latex":
    LATEX = True
else:
    LATEX = False

def aggregate_mpspdz(lines):
    times = []
    datas = []
    rounds = []
    for pl in lines:
        p_times = []
        p_datas = []
        p_rounds = []
        for l in pl:
            if l.startswith("Time = "):
                assert "seconds" in l
                p_times.append(float(l.split(" ")[2]))
            if l.startswith("Data sent = "):
                assert "MB" in l
                p_datas.append(float(l.split(" ")[3]))
                p_rounds.append(int(l.split(" ")[6][1:]))
        times.append(p_times)
        datas.append(p_datas)
        rounds.append(p_rounds)
    actual_times = []
    actual_datas = []
    actual_rounds = []
    for i in range(10):
        actual_times.append(max(tms[i] for tms in times))
        actual_datas.append(sum(tms[i] for tms in datas) / len(datas))
        actual_rounds.append(min(tms[i] for tms in rounds))
    
    return sum(actual_datas) / len(actual_datas), int(sum(actual_rounds) / len(actual_rounds)), sum(actual_times) / len(actual_times)

# Creates a table row for the given benchmark results for our protocol
def get_row_ours(lan_fliop, wan_fliop, parties):
    off_com_fliop, on_com_fliop, rounds_fliop, off_lan_fliop, on_lan_fliop, off_wan_fliop, on_wan_fliop = aggregate(lan_fliop, wan_fliop)
    # If 2 parties, we do not use p_king, but aggregate assumes each multiplication to use two rounds.
    # Just have depth DEPTH circuits here, so to correct:
    if parties == 2:
        rounds_fliop -= DEPTH
    # We want online comm. per party, so divide by parties, offline is only Dealer
    if LATEX:
        return f'${format(off_com_fliop, 2, 5)}+{format(on_com_fliop / parties, 2, 5)}$ & ${format(rounds_fliop, 0, 2)}$ & ${format(off_lan_fliop, 2, 4)}+{format(on_lan_fliop, 2, 4)}$ & ${format(off_wan_fliop, 2, 4)}+{format(on_wan_fliop, 2, 5)}$'
    else:
        return f'{format(off_com_fliop, 2, 5)} +{format(on_com_fliop / parties, 2, 5)} |   {format(rounds_fliop, 0, 2)}   | {format(off_lan_fliop, 2, 4)}+{format(on_lan_fliop, 2, 4)} | {format(off_wan_fliop, 2, 4)}+{format(on_wan_fliop, 2, 5)}'

# Creates a table row for the given benchmark results (compares MP-SPDZ to ours)
def get_row_difference(lan_fliop, wan_fliop, parties_fliop, off_com_mpspdz, on_com_mpspdz, rounds_mpspdz, off_lan_mpspdz, on_lan_mpspdz, off_wan_mpspdz, on_wan_mpspdz, parties_mp_spdz):
    off_com_fliop, on_com_fliop, rounds_fliop, off_lan_fliop, on_lan_fliop, off_wan_fliop, on_wan_fliop = aggregate(lan_fliop, wan_fliop)
    # If 2 parties, we do not use p_king, but aggregate assumes each multiplication to use two rounds.
    # Just have depth DEPTH circuits here, so to correct:
    if parties_fliop == 2:
        rounds_fliop -= DEPTH
    if LATEX:
        return f'${format(100*(off_com_fliop+on_com_fliop)/(off_com_mpspdz+on_com_mpspdz*parties_mp_spdz)-100, 1, 3, True)}\\%$ & ${format(100*rounds_fliop/rounds_mpspdz-100, 1, 5, True)}\\%$ & ${format(100*(off_lan_fliop+on_lan_fliop)/(off_lan_mpspdz+on_lan_mpspdz)-100, 1, 5, True)}\\%$ & ${format(100*(off_wan_fliop+on_wan_fliop)/(off_wan_mpspdz+on_wan_mpspdz)-100, 1, 5, True)}\\%$'
    else:
        return f'   {format(100*(off_com_fliop+on_com_fliop)/(off_com_mpspdz+on_com_mpspdz*parties_mp_spdz)-100, 1, 3, True)}%    | {format(100*rounds_fliop/rounds_mpspdz-100, 1, 5, True)}% |   {format(100*(off_lan_fliop+on_lan_fliop)/(off_lan_mpspdz+on_lan_mpspdz)-100, 1, 5, True)}%  |   {format(100*(off_wan_fliop+on_wan_fliop)/(off_wan_mpspdz+on_wan_mpspdz)-100, 1, 5, True)}%'

if __name__ == "__main__":
    if not LATEX:
        print("                  protocol                    |     comm.    | rounds | time LAN time WAN")
    with ExitStack() as stack:
        NETWORK = "LAN"
        PROTO = "RSS_FLIOP"
        n = 3
        files = [stack.enter_context(open(f"MPSPDZ_{NETWORK}/log_p{p}_{PROTO}.txt", 'r')).readlines() for p in range(n)]
        comm, rounds, time_LAN = aggregate_mpspdz(files)
        NETWORK = "WAN"
        files = [stack.enter_context(open(f"MPSPDZ_{NETWORK}/log_p{p}_{PROTO}.txt", 'r')).readlines() for p in range(n)]
        _, _, time_WAN = aggregate_mpspdz(files)

        files_LAN_fliop = [stack.enter_context(open(f"LAN/p{p}/fliop-m{MULTS}-n{2}-c{COMPR}-d{DEPTH}-broadcast-0-t0.txt", 'r')).readlines() for p in range(2 + 1)]
        files_WAN_fliop = [stack.enter_context(open(f"WAN/p{p}/fliop-m{MULTS}-n{2}-c{COMPR}-d{DEPTH}-broadcast-0-t0.txt", 'r')).readlines() for p in range(2 + 1)]
        assert all(len(f) == len(files_LAN_fliop[0]) for f in files_LAN_fliop)
        assert all(len(f) == len(files_WAN_fliop[0]) for f in files_WAN_fliop)

        if LATEX:
            print(f"RSS zk-FLIOP~\\cite{{CCS24}} & 3 non-colluding parties & $\\phantom{{00.00+0}}{format(comm, 2, 4)}$ & ${rounds}$ & $\\phantom{{0.00+}}\\,\\,{format(time_LAN, 2, 4)}$ & $\\phantom{{0.00+0}}{format(time_WAN, 2, 3)}$ \\\\")
            print(f"our protocol & 2 parties + 1 non-colluding dealer & {get_row_ours(files_LAN_fliop, files_WAN_fliop, 2)} \\\\")
            print(f"decrease/increase & & {get_row_difference(files_LAN_fliop, files_WAN_fliop, 2, 0, comm, rounds, 0, time_LAN, 0, time_WAN, 3)} \\\\ \\midrule")
        else:
            print(f"   CCS:LEDHHZS24 (3 non-colluding parties)    |  ---  +{format(comm, 2, 5)} |   {rounds}   | --- +{format(time_LAN, 2, 4)} | --- +{format(time_WAN, 2, 4)}")
            print(f"  ours (2 parties + 1 non-colluding dealer)   | {get_row_ours(files_LAN_fliop, files_WAN_fliop, 2)}")
            print(f"              decrease/increase               | {get_row_difference(files_LAN_fliop, files_WAN_fliop, 2, 0, comm, rounds, 0, time_LAN, 0, time_WAN, 3)}")
    with ExitStack() as stack:
        NETWORK = "LAN"
        PROTO = "SPDZ2k"
        n = 3
        files = [stack.enter_context(open(f"MPSPDZ_{NETWORK}/log_p{p}_{PROTO}.txt", 'r')).readlines() for p in range(n)]
        comm, rounds, time_LAN = aggregate_mpspdz(files)
        NETWORK = "WAN"
        files = [stack.enter_context(open(f"MPSPDZ_{NETWORK}/log_p{p}_{PROTO}.txt", 'r')).readlines() for p in range(n)]
        _, _, time_WAN = aggregate_mpspdz(files)

        files_LAN_fliop = [stack.enter_context(open(f"LAN/p{p}/fliop-m{MULTS}-n{3}-c{COMPR}-d{DEPTH}-pking-1-t0.txt", 'r')).readlines() for p in range(3 + 1)]
        files_WAN_fliop = [stack.enter_context(open(f"WAN/p{p}/fliop-m{MULTS}-n{3}-c{COMPR}-d{DEPTH}-pking-1-t0.txt", 'r')).readlines() for p in range(3 + 1)]
        assert all(len(f) == len(files_LAN_fliop[0]) for f in files_LAN_fliop)
        assert all(len(f) == len(files_WAN_fliop[0]) for f in files_WAN_fliop)

        comm_offline = MULTS * 4 * 64 / 8 / 1024 / 1024 # larger ring: 64 bits, MACs for a, b, c per triple and c itself (a,b from pre-shared keys)

        if LATEX:
            print(f"SPD$\\Z{{k}}$~\\cite{{SPDZ2K}} & 3 parties + 1 non-colluding dealer$^*$ & ${format(comm_offline, 2, 5)}^*+{format(comm, 2, 5)}$ & ${rounds}$ & $\\text{{?}}  +{format(time_LAN, 2, 4)}$ & $ \\text{{?}}  +{format(time_WAN, 2, 5)}$ \\\\")
            print(f"our protocol & 3 parties + 1 non-colluding dealer & {get_row_ours(files_LAN_fliop, files_WAN_fliop, 3)} \\\\")
            print(f"decrease/increase & & {get_row_difference(files_LAN_fliop, files_WAN_fliop, 3, comm_offline, comm, rounds, 0, time_LAN, 0, time_WAN, 3)} \\\\")
        else:
            print(f"SPDZ2k (3 parties + (1 non-colluding dealer)) | {format(comm_offline, 2, 5)}*+{format(comm, 2, 5)} |   {rounds}   |  ?  +{format(time_LAN, 2, 4)} |  ?  +{format(time_WAN, 2, 5)}")
            print(f"  ours (3 parties + 1 non-colluding dealer)   | {get_row_ours(files_LAN_fliop, files_WAN_fliop, 3)}")
            print(f"              decrease/increase               | {get_row_difference(files_LAN_fliop, files_WAN_fliop, 3, comm_offline, comm, rounds, 0, time_LAN, 0, time_WAN, 3)}")


