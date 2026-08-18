import matplotlib.pyplot as plt
from calc_theoretical import BGIN21_compiler, our_verification, LEDHHZS24
from math import ceil, log

C1 = (0.12156862745098039, 0.4666666666666667, 0.7058823529411765)
C2 = (1.0, 0.4980392156862745, 0.054901960784313725)
C3 = (0.17254901960784313, 0.6274509803921569, 0.17254901960784313)

MULTS = 1000000 # 1 million
NUM_BITS = 32
SSEC = 40
CSEC = 128

T = 2 * ceil(log(3 * MULTS, 3)) + 1
s = ceil(max(3 * T, SSEC + T * (0.5 + log(2.5 + 3.0 * SSEC / T, 2))))
assert T <= SSEC

fig, ax = plt.subplots(1,2, layout="constrained")

for mode in range(2):
    if mode == 0:
        print("offline:")
    else:
        print("online:")

    ax[mode].set_xlim([2, 30])
    if mode == 0:
        ax[mode].set_ylim([0, 4.3])
        ax[mode].set_xlabel("number of parties $n$")
        ax[mode].set_ylabel(f"dealer comm. [KiB]")
        ax[mode].set_title("offline")
    else:
        ax[mode].set_ylim([0, 100])
        ax[mode].set_xlabel("number of parties $n$")
        ax[mode].set_ylabel(f"comm. per party [KiB]")
        ax[mode].set_title("online")

    parties = []
    cost_BGIN21 = []
    cost_ours = []
    cost_LEDHHZS24 = []

    for n in range(2, 31, 1):

        divider = 1
        if mode == 1:
            divider = n # show communication per party
        # KiB
        divider *= 8 * 1024

        parties.append(n)
        # Run all protocols fully using P_king ==> no broadcast, as we care about minimal
        # possible communication here
        cost_BGIN21.append(BGIN21_compiler(MULTS, n, NUM_BITS, SSEC, CSEC)[mode] / divider)
        cost_ours.append(our_verification(MULTS, 3, n, NUM_BITS, s, SSEC, CSEC, False, False)[mode] / divider)
        cost_LEDHHZS24.append(LEDHHZS24(MULTS, 3, n, NUM_BITS, s, SSEC, CSEC, False, False)[mode] / divider)

    ax[mode].plot(parties, cost_BGIN21, color=C1, label="[13]", linestyle="dotted")
    ax[mode].plot(parties, cost_LEDHHZS24, color=C2, label="[46]", linestyle="dashed")
    ax[mode].plot(parties, cost_ours, color=C3, label=r"$\bf{ours}$")
    print(f"[13]: {min(cost_BGIN21):6.2f}--{max(cost_BGIN21):6.2f} KiB")
    print(f"[46]: {min(cost_LEDHHZS24):6.2f}--{max(cost_LEDHHZS24):6.2f} KiB")
    print(f"ours: {min(cost_ours):6.2f}--{max(cost_ours):6.2f} KiB")

# online per party, all units to MiB
semi_setup_cost = MULTS * NUM_BITS / (8 * 1024 * 1024) # Per mult, send one gamma share
semi_online_n2 = MULTS * (2 * 2 - 2) * NUM_BITS / (2 * 8 * 1024 * 1024) # Per mult, reconstruct one value
semi_online_n30 = MULTS * (2 * 30 - 2) * NUM_BITS / (30 * 8 * 1024 * 1024) # Per mult, reconstruct one value
print(f"\nsemi-honest: offline {semi_setup_cost:6.2f} MiB, online {(semi_online_n2):6.2f}--{(semi_online_n30):6.2f} MiB per party")

fig.set_size_inches(5.5, 2.2)
ax[0].legend()
plt.savefig('plots/plot_theoretical.pdf')
plt.show()
