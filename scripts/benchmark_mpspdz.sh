set -e

rm -f log_p0_RSS_FLIOP
rm -f log_p1_RSS_FLIOP
rm -f log_p2_RSS_FLIOP
rm -f log_p0_SPDZ2k
rm -f log_p1_SPDZ2k
rm -f log_p2_SPDZ2k

# name
log_bench_progress_start() {
    echo -n "Starting bench $bench_counter/20 (iteration $iter/10) at $(date +%H:%M:%S): $1 ....."
}

log_bench_progress_stop() {
    echo " done ($(date +%H:%M:%S))"
    bench_counter=$((bench_counter+1))    
}

bench_counter=1
for iter in {1..10}; do
    log_bench_progress_start "RSS+zk-FLIOP n=3 depth=30 mult/layer=33333.3333333"
    echo ===iter $iter=== >> log_p0_RSS_FLIOP.txt
    echo ===iter $iter=== >> log_p1_RSS_FLIOP.txt
    echo ===iter $iter=== >> log_p2_RSS_FLIOP.txt
    ip netns exec neon_ns0 ./test-ring-party.x -tn 1 -ip net_config.txt 0 1M-30 >> log_p0_RSS_FLIOP.txt 2>&1 &
    ip netns exec neon_ns1 ./test-ring-party.x -tn 1 -ip net_config.txt 1 1M-30 >> log_p1_RSS_FLIOP.txt 2>&1 &
    ip netns exec neon_ns2 ./test-ring-party.x -tn 1 -ip net_config.txt 2 1M-30 >> log_p2_RSS_FLIOP.txt 2>&1
    log_bench_progress_stop

    sleep 2

    log_bench_progress_start "SPDZ2k n=3 depth=30 mult/layer=33333.3333333"
    echo ===iter $iter=== >> log_p0_SPDZ2k.txt
    echo ===iter $iter=== >> log_p1_SPDZ2k.txt
    echo ===iter $iter=== >> log_p2_SPDZ2k.txt
    ip netns exec neon_ns0 ./spdz2k-party.x -ip net_config.txt -F -SP 32 -N 3 0 1M-30 >> log_p0_SPDZ2k.txt 2>&1 &
    ip netns exec neon_ns1 ./spdz2k-party.x -ip net_config.txt -F -SP 32 -N 3 1 1M-30 >> log_p1_SPDZ2k.txt 2>&1 &
    ip netns exec neon_ns2 ./spdz2k-party.x -ip net_config.txt -F -SP 32 -N 3 2 1M-30 >> log_p2_SPDZ2k.txt 2>&1
    log_bench_progress_stop

    sleep 2
done


echo DONE
