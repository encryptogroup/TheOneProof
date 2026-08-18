import statistics
import json
import sys

if len(sys.argv) > 1 and sys.argv[1] == "latex":
    LATEX = True
else:
    LATEX = False

# aggregates the provided benchmark outputs into averages
def aggregate(lines_lan, lines_wan, depth = None): # depth to overwrite if needed

    off_com = []
    on_com = []
    rounds = []
    off_lan = []
    on_lan = []
    off_wan = []
    on_wan = []

    # each element of lines_lan/lines_wan is an array over all parties, for each party
    # having the following json:
    # {"benchmarks":[ # this is the online phase
    #       {"communication":[<bytes sent to P0>,<bytes sent to P1>,...],"time":<in ms>},
    #       ... # values for each benchmark run
    # ],
    # "benchmarks_setup":[ # this is the offline phase
    #       {"communication":[<bytes sent to P0>,<bytes sent to P1>,...],"time":<in ms>},
    #       ... # values for each benchmark run
    # ],
    # "details":{
    #       ...,
    #       "repeat":<number of benchmark repetitions>,
    #       ...
    # },
    # "proof_rounds":[<verify rounds for benchmark repetition 0>,...],
    # "stats":{"peak_resident_set_size":<in KiB>,"peak_virtual_memory":<in KiB>}}

    # Each individual file includes multiple of these lines, as we partition 10 benchmarks
    # into 5 blocks of 2 each.

    # Collect results of all individual benchmarks
    for i in range(len(lines_lan[0])):
        line_lan = [json.loads(x[i]) for x in lines_lan]
        line_wan = [json.loads(x[i]) for x in lines_wan]
        for p in line_lan:
            assert p['details']['repeat'] == 2
        for p in line_wan:
            assert p['details']['repeat'] == 2
        for r in range(2):
            # total offline communication: sum over all parties, per party sum over receivers that it sends to
            off_com.append(sum(sum(p['benchmarks_setup'][r]['communication']) for p in line_lan))
            off_com.append(sum(sum(p['benchmarks_setup'][r]['communication']) for p in line_wan))
            # total online communication: sum over all parties, per party sum over receivers that it sends to
            on_com.append(sum(sum(p['benchmarks'][r]['communication']) for p in line_lan))
            on_com.append(sum(sum(p['benchmarks'][r]['communication']) for p in line_wan))

            # offline run time is max over all parties
            off_lan.append(max(p['benchmarks_setup'][r]['time'] for p in line_lan))
            off_wan.append(max(p['benchmarks_setup'][r]['time'] for p in line_wan))
            on_lan.append(max(p['benchmarks'][r]['time'] for p in line_lan))
            on_wan.append(max(p['benchmarks'][r]['time'] for p in line_wan))

            if 'proof_rounds' in line_lan[0]:
                # actively secure protocol: max over all parties, rounds from proof
                # plus 2*depth from passive circuit evaluation using P_king
                if depth is None:
                    rounds.append(max(p['proof_rounds'][r] + p['details']['depth'] * 2 for p in line_lan))
                    rounds.append(max(p['proof_rounds'][r] + p['details']['depth'] * 2 for p in line_wan))
                else:
                    rounds.append(max(p['proof_rounds'][r] + depth * 2 for p in line_lan))
                    rounds.append(max(p['proof_rounds'][r] + depth * 2 for p in line_wan))
            else:
                # Then, this is the passive baseline, simply 2*depth from using P_king
                if depth is None:
                    rounds.append(max(p['details']['depth'] * 2 for p in line_lan))
                    rounds.append(max(p['details']['depth'] * 2 for p in line_wan))
                else:
                    rounds.append(max(depth * 2 for p in line_lan))
                    rounds.append(max(depth * 2 for p in line_wan))

    # communication is deterministic ==> same across benchmarks
    assert off_com[0] == statistics.mean(off_com) 
    assert on_com[0] == statistics.mean(on_com)
    assert rounds[0] == statistics.mean(rounds)

    off_com = off_com[0] / (1024 * 1024) # convert B -> MiB
    on_com = on_com[0] / (1024 * 1024) # convert B -> MiB
    rounds = rounds[0]

    off_lan = statistics.mean(off_lan) / 1000 # convert ms -> s
    on_lan = statistics.mean(on_lan) / 1000 # convert ms -> s
    off_wan = statistics.mean(off_wan) / 1000 # convert ms -> s
    on_wan = statistics.mean(on_wan) / 1000 # convert ms -> s

    return off_com, on_com, rounds, off_lan, on_lan, off_wan, on_wan

def format(number, decimals, total, always_sign = False):
    if LATEX:
        out = f'{number:.{decimals}f}'
        if len(out) < total:
            out = "\\phantom{" + ("0" * (total - len(out))) + "}" + out
        return out
    else:
        if always_sign:
            return f'{number:+{total}.{decimals}f}'
        else:
            return f'{number:{total}.{decimals}f}'
    