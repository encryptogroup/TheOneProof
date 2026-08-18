import json
import statistics
from contextlib import ExitStack
import matplotlib.pyplot as plt
import numpy as np

import table_general

COMPR = 20
DEPTH = 30
PARTIES = [2,3,4,5,10,15,20,25,30]
MULTS = 1000000
THREADS = 0 # no OMP

# Wrapper, as table_general.aggregate aggregates LAN and WAN in parallel and we only want to
# aggregate one here, so just feed as LAN and WAN and ignore WAN results.
def aggregate(lines):
    off_com, on_com, rounds, off_time, on_time, _, _ = table_general.aggregate(lines, lines)
    return off_com, on_com, rounds, off_time, on_time

# offline + online
def get_data_total(semi, fliop, num_parties):
    off_com_semi, on_com_semi, rounds_semi, off_time_semi, on_time_semi = aggregate(semi)
    off_com_fliop, on_com_fliop, rounds_fliop, off_time_fliop, on_time_fliop = aggregate(fliop)
    print(f"n={num_parties:2}: verification run time overhead +{((off_time_fliop + on_time_fliop) / (off_time_semi + on_time_semi) - 1) * 100:.2f}%")
    return off_com_semi + on_com_semi, rounds_semi, off_time_semi + on_time_semi, off_com_fliop + on_com_fliop - off_com_semi - on_com_semi, rounds_fliop - rounds_semi, off_time_fliop + on_time_fliop - off_time_semi - on_time_semi

# offline
def get_data_offline(semi, fliop, num_parties):
    off_com_semi, on_com_semi, rounds_semi, off_time_semi, on_time_semi = aggregate(semi)
    off_com_fliop, on_com_fliop, rounds_fliop, off_time_fliop, on_time_fliop = aggregate(fliop)
    return off_com_semi, 1, off_time_semi, off_com_fliop - off_com_semi, 0, off_time_fliop - off_time_semi

# online
def get_data_online(semi, fliop, num_parties):
    off_com_semi, on_com_semi, rounds_semi, off_time_semi, on_time_semi = aggregate(semi)
    off_com_fliop, on_com_fliop, rounds_fliop, off_time_fliop, on_time_fliop = aggregate(fliop)
    return on_com_semi / num_parties, rounds_semi, on_time_semi, (on_com_fliop - on_com_semi) / num_parties, rounds_fliop - rounds_semi, on_time_fliop - on_time_semi

def do_plotting(semi, verify, fig, ax):
    weight_counts = {
        "passive": semi,
        "verify": verify,
    }
    width = [0.5,0.5,0.5,0.5,3,3,3,3,3]

    bottom = np.zeros(len(PARTIES))

    for boolean, weight_count in weight_counts.items():
        p = ax.bar(bars, weight_count, width, label=boolean, bottom=bottom)
        bottom += weight_count

if __name__ == "__main__":
    fig, ax = plt.subplots(2, 2, layout="constrained")

    bars = []
    c_semi_off = []
    c_verify_off = []
    c_semi_on = []
    c_verify_on = []
    t_semi = []
    t_verify = []
    NETWORK = "LAN"
    print(NETWORK)
    for n in PARTIES:
        bars.append(n)
        with ExitStack() as stack:
            if n == 2: # always use broadcasting for n=2
                rec_strat_fliop = "broadcast-0"
                rec_strat_semi = "broadcast"
            else:
                rec_strat_fliop = "pking-1"
                rec_strat_semi = "pking"

            files_fliop = [stack.enter_context(open(f"{NETWORK}/p{p}/fliop-m{MULTS}-n{n}-c{COMPR}-d{DEPTH}-{rec_strat_fliop}-t{THREADS}.txt", 'r')).readlines() for p in range(n + 1)]
            files_semi = [stack.enter_context(open(f"{NETWORK}/p{p}/semi-m{MULTS}-n{n}-d{DEPTH}-{rec_strat_semi}-t{THREADS}.txt", 'r')).readlines() for p in range(n + 1)]
            assert all(len(f) == len(files_fliop[0]) for f in files_fliop)
            assert all(len(f) == len(files_semi[0]) for f in files_semi)

            # LAN run time:
            _, _, time_semi, _, _, time_verify = get_data_total(files_semi, files_fliop, n)
            # offline communication:
            comm_semi_off, _, _, comm_verify_off, _, _ = get_data_offline(files_semi, files_fliop, n)
            # online communication:
            comm_semi_on, _, _, comm_verify_on, _, _ = get_data_online(files_semi, files_fliop, n)

            c_semi_off.append(comm_semi_off)
            c_verify_off.append(comm_verify_off)
            c_semi_on.append(comm_semi_on)
            c_verify_on.append(comm_verify_on)
            t_semi.append(time_semi)
            t_verify.append(time_verify)

    do_plotting(c_semi_off, c_verify_off, fig, ax[0][0])
    do_plotting(c_semi_on, c_verify_on, fig, ax[0][1])
    do_plotting(t_semi, t_verify, fig, ax[1][0])

    bars = []
    c_semi = []
    c_verify = []
    t_semi = []
    t_verify = []
    NETWORK = "WAN"
    print(NETWORK)
    for n in PARTIES:
        bars.append(n)
        with ExitStack() as stack:
            if n == 2: # always use broadcasting for n=2
                rec_strat_fliop = "broadcast-0"
                rec_strat_semi = "broadcast"
            else:
                rec_strat_fliop = "pking-1"
                rec_strat_semi = "pking"

            files_fliop = [stack.enter_context(open(f"{NETWORK}/p{p}/fliop-m{MULTS}-n{n}-c{COMPR}-d{DEPTH}-{rec_strat_fliop}-t{THREADS}.txt", 'r')).readlines() for p in range(n + 1)]
            files_semi = [stack.enter_context(open(f"{NETWORK}/p{p}/semi-m{MULTS}-n{n}-d{DEPTH}-{rec_strat_semi}-t{THREADS}.txt", 'r')).readlines() for p in range(n + 1)]
            assert all(len(f) == len(files_fliop[0]) for f in files_fliop)
            assert all(len(f) == len(files_semi[0]) for f in files_semi)

            # WAN run time:
            _, _, time_semi, _, _, time_verify = get_data_total(files_semi, files_fliop, n)
            
            t_semi.append(time_semi)
            t_verify.append(time_verify)


    do_plotting(t_semi, t_verify, fig, ax[1][1])

    ax[0][0].set_title("offline communication")
    ax[0][0].set_ylabel(f"comm. [MiB]")
    ax[0][0].set_ylim([0, 4.3])
    ax[0][0].set_xticks([2, 5, 10, 15, 20, 25, 30])
    ax[0][1].set_title("online communication")
    ax[0][1].set_ylabel(f"comm. [MiB]")
    ax[0][1].set_ylim([0, 8.3])
    ax[0][1].set_xticks([2, 5, 10, 15, 20, 25, 30])
    ax[1][0].set_title("total run time (LAN)")
    ax[1][0].set_xlabel("number of parties $n$")
    ax[1][0].set_ylabel(f"run time [s]")
    ax[1][0].set_xticks([2, 5, 10, 15, 20, 25, 30])
    ax[1][1].set_title("total run time (WAN)")
    ax[1][1].set_xlabel("number of parties $n$")
    ax[1][1].set_ylabel(f"run time [s]")
    ax[1][1].set_xticks([2, 5, 10, 15, 20, 25, 30])

    fig.get_layout_engine().set(w_pad=8 / 72, h_pad=4 / 72, hspace=0,
                            wspace=0)
    fig.set_size_inches(5, 3.5)
    ax[1][0].legend(loc="upper left")
    plt.savefig('plots/plot_parties_square.pdf')
    plt.show()
