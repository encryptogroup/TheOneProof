# Based on https://gitlab.com/rwth-itsec/neon-and-xenon

import subprocess
import sys
from typing import List

def start_virtual_network(n_parties, bandwidth, delay):
    #####
    # Bridge:
    #####
    commands = [
        # Create a bridge
        'ip link add name neon_bridge txqueuelen 10000 type bridge',

        # set bridge up
        'ip link set neon_bridge up',

        # Give bridge an IP
        'ip addr add 172.16.1.10/16 brd + dev neon_bridge'
    ]
    # Commands of intereset
    # sysctl -w net.bridge.bridge-nf-call-arptables=0
    # sysctl -w net.bridge.bridge-nf-call-iptables=0
    # sysctl -w net.bridge.bridge-nf-call-ip6tables=0
    # sysctl -w net.bridge.bridge-nf-call-ip6tables=0
    # sysctl -w net.ipv4.icmp_ratelimit=0

    run_commands(commands)

    #####
    # Virtual Networks
    #####
    def func_setup_network(i):
        ip = get_ip(i)
        run_commands([
            # Create a new network namespace nsi
            f'ip netns add neon_ns{i}',

            # Create a veth pair to tunnel data into the bridge. vethi is the nsi side, br-vethi the bridge side
            f'ip link add neon_veth{i} type veth peer name neon_br_v{i}',

            # Move the interface vethi into the namespace nsi
            f'ip link set neon_veth{i} netns neon_ns{i}',

            # Give the interface an ip depending on the value i
            f'ip netns exec neon_ns{i} ip addr add {ip}/24 dev neon_veth{i}',

            # Set bridge interface side of tunnel up from default namespace
            f'ip link set neon_br_v{i} up',

            # Set the other side of tunnel up from nsi
            f'ip netns exec neon_ns{i} ip link set neon_veth{i} up',

            # Set bridge br1 as the master of our bridge side interface of the tunnel
            f'ip link set neon_br_v{i} master neon_bridge',

            # Start local host so party 0 can communicate with itself in neon
            f'ip netns exec neon_ns{i} ip link set dev lo up',
        ])

    for i in range(n_parties):
        func_setup_network(i)

    def func_check_network(party_index):
        for j in range(n_parties):
            if party_index == j:
                continue

            run_commands([f'ip netns exec neon_ns{party_index} ping -c 1 {get_ip(j)} > /dev/null'])

            run_commands([f'ip netns exec neon_ns{j} ping -c 1 {get_ip(party_index)} > /dev/null'])

    for i in range(n_parties):
        func_check_network(i)

    # Limited neighbour table size might lead to problems if many parties run on the same machine.
    # Should not happen here, so we do not include the workaround. If needed, have a look at
    # check_and_set_neighbor_table_size in the original neon code.

    def ratelimit(i):
        run_commands([
            # Outgoing, including delay
            f'ip netns exec neon_ns{i} /sbin/tc qdisc add dev neon_veth{i} root handle 1:0 htb default 10',
            f'ip netns exec neon_ns{i} /sbin/tc class add dev neon_veth{i} parent 1:0 classid 1:10 htb rate {bandwidth} quantum 1500',
            f'ip netns exec neon_ns{i} /sbin/tc qdisc add dev neon_veth{i} parent 1:10 handle 10:0 netem delay {delay}',

            # Incoming = Outgoing from bridge, no delay as already added by sender
            f'/sbin/tc qdisc add dev neon_br_v{i} root handle 1:0 htb default 10',
            f'/sbin/tc class add dev neon_br_v{i} parent 1:0 classid 1:10 htb rate {bandwidth} quantum 1500',
        ])

    for i in range(n_parties):
        ratelimit(i)

def stop_virtual_network(n_parties):
    # ip link list
    # ip netns list
    commands = ["ip link delete neon_bridge"]
    commands += [f"ip netns delete neon_ns{i}" for i in range(n_parties)]
    run_commands(commands)

def get_ip(namespace: int) -> str:
    """Returns the IP address of the given client."""
    return f"172.16.{1 + namespace // 245}.{11 + namespace % 245}"

def run_commands(commands: List[str]) -> None:
    for command in commands:
        print('+ Executing command"{}"'.format(command))
        subprocess.check_call(command, shell=True)

if __name__ == "__main__":
    mode = sys.argv[1]
    n_parties = int(sys.argv[2])
    network = sys.argv[3]
    if network == "LAN":
        bandwidth = "1Gbit"
        delay = "0.5ms 0.03ms 5%"
    elif network == "WAN":
        bandwidth = "100Mbit"
        delay = "50ms 3ms 25%"
    elif network == "WANfast":
        bandwidth = "100Mbit"
        delay = "15ms 0.5ms 15%"
    else:
         sys.exit("Unsupported network setting")
    if mode == "start":
        start_virtual_network(n_parties, bandwidth, delay)
    elif mode == "stop":
        stop_virtual_network(n_parties)
    else:
        sys.exit("Unsupported Mode")