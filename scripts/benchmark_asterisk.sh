
set -e

for i in {0..3}; do
    rm -rf p$i
    mkdir p$i
done

# num_parties depth gates_per_level compression pking pking_verify filename threads
run_asterisk_benchmark() {
    log_bench_progress_start "asterisk n=$1 depth=$2 mult/layer=$3"
    # Dealer
    ip netns exec neon_ns0 ./asterisk_comparison --num-parties $1 --depth $2 --gates-per-level $3 --net-config net_config.json -o p0/$7.txt --repeat 2 --pid 0 >> p0/log.txt &
    # Parties 2, 3, ..., N
    for i in $( eval echo {2..$1} ); do
        ip netns exec neon_ns$i ./asterisk_comparison --num-parties $1 --depth $2 --gates-per-level $3 --net-config net_config.json -o p$i/$7.txt --repeat 2 --pid $i >> p$i/log.txt &
    done
    # Party 1, not async to wait until this is finished
    ip netns exec neon_ns1 ./asterisk_comparison --num-parties $1 --depth $2 --gates-per-level $3 --net-config net_config.json -o p1/$7.txt --repeat 2 --pid 1 >> p1/log.txt

    sleep 2
    log_bench_progress_stop
}

# name
log_bench_progress_start() {
    echo -n "Starting bench $bench_counter/15 (iteration $iter/5) at $(date +%H:%M:%S): $1 ....."
}

log_bench_progress_stop() {
    echo " done ($(date +%H:%M:%S))"
    bench_counter=$((bench_counter+1))    
}

bench_counter=1
for iter in {1..5}; do
    #####
    ### Table 5: 1M multiplications, 3+1 parties, 10/30/100 layers, 1 thread, malicious
    #####
    # d = 10 ==> per layer = 100 000
    run_asterisk_benchmark 3 10 100000 2 true 0 n3-c2-d10-pking-0 1
    # d = 30 ==> per layer = 33 334
    run_asterisk_benchmark 3 30 33334 2 true 0 n3-c2-d30-pking-0 1
    # d = 100 ==> per layer = 10 000
    run_asterisk_benchmark 3 100 10000 2 true 0 n3-c2-d100-pking-0 1
done

echo DONE
