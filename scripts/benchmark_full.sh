set -o xtrace

for i in {0..30}; do
    mkdir -p p$i
done

# num_parties depth gates_per_level compression pking pking_verify filename threads
run_fliop_benchmark() {
    # Dealer
    ip netns exec neon_ns0 ./fliop --num-parties $1 --depth $2 --gates-per-level $3 --net-config net_config.json --compression $4 --pking-semi $5 --pking-verify $6 -o p0/$7.txt --threads $8 --repeat 2 --pid 0 >> p0/log.txt 2>&1 &
    # Parties 2, 3, ..., N
    for i in $( eval echo {2..$1} ); do
        ip netns exec neon_ns$i ./fliop --num-parties $1 --depth $2 --gates-per-level $3 --net-config net_config.json --compression $4 --pking-semi $5 --pking-verify $6 -o p$i/$7.txt --threads $8 --repeat 2 --pid $i >> p$i/log.txt 2>&1 &
    done
    # Party 1 where we also display log
    ip netns exec neon_ns1 ./fliop --num-parties $1 --depth $2 --gates-per-level $3 --net-config net_config.json --compression $4 --pking-semi $5 --pking-verify $6 -o p1/$7.txt --threads $8 --repeat 2 --pid 1 2>&1 | tee -a p1/log.txt

    sleep 2
}

# num_parties depth gates_per_level compression pking pking_verify filename threads
run_fliop_benchmark_omp() {
    # Dealer
    ip netns exec neon_ns0 ./fliop_omp --num-parties $1 --depth $2 --gates-per-level $3 --net-config net_config.json --compression $4 --pking-semi $5 --pking-verify $6 -o p0/$7.txt --threads $8 --repeat 2 --pid 0 >> p0/log.txt 2>&1 &
    # Parties 2, 3, ..., N
    for i in $( eval echo {2..$1} ); do
        ip netns exec neon_ns$i ./fliop_omp --num-parties $1 --depth $2 --gates-per-level $3 --net-config net_config.json --compression $4 --pking-semi $5 --pking-verify $6 -o p$i/$7.txt --threads $8 --repeat 2 --pid $i >> p$i/log.txt 2>&1 &
    done
    # Party 1 where we also display log
    ip netns exec neon_ns1 ./fliop_omp --num-parties $1 --depth $2 --gates-per-level $3 --net-config net_config.json --compression $4 --pking-semi $5 --pking-verify $6 -o p1/$7.txt --threads $8 --repeat 2 --pid 1 2>&1 | tee -a p1/log.txt

    sleep 2
}

# num_parties dimension number compression pking pking_verify filename threads
run_fliop_dotp_benchmark_omp() {
    # Dealer
    ip netns exec neon_ns0 ./fliop_dotp_omp --num-parties $1 --dimension $2 --number $3 --net-config net_config.json --compression $4 --pking-semi $5 --pking-verify $6 -o p0/$7.txt --threads $8 --repeat 2 --pid 0 >> p0/log.txt 2>&1 &
    # Parties 2, 3, ..., N
    for i in $( eval echo {2..$1} ); do
        ip netns exec neon_ns$i ./fliop_dotp_omp --num-parties $1 --dimension $2 --number $3 --net-config net_config.json --compression $4 --pking-semi $5 --pking-verify $6 -o p$i/$7.txt --threads $8 --repeat 2 --pid $i >> p$i/log.txt 2>&1 &
    done
    # Party 1 where we also display log
    ip netns exec neon_ns1 ./fliop_dotp_omp --num-parties $1 --dimension $2 --number $3 --net-config net_config.json --compression $4 --pking-semi $5 --pking-verify $6 -o p1/$7.txt --threads $8 --repeat 2 --pid 1 2>&1 | tee -a p1/log.txt

    sleep 2
}

# num_parties depth gates_per_level compression pking pking_verify filename threads 
run_semi_benchmark() {
    # Dealer
    ip netns exec neon_ns0 ./semi --num-parties $1 --depth $2 --gates-per-level $3 --net-config net_config.json --pking-semi $5 -o p0/$7.txt --threads $8 --repeat 2 --pid 0 >> p0/log.txt 2>&1 &
    # Parties 2, 3, ..., N
    for i in $( eval echo {2..$1} ); do
        ip netns exec neon_ns$i ./semi --num-parties $1 --depth $2 --gates-per-level $3 --net-config net_config.json --pking-semi $5 -o p$i/$7.txt --threads $8 --repeat 2 --pid $i >> p$i/log.txt 2>&1 &
    done
    # Party 1 where we also display log
    ip netns exec neon_ns1 ./semi --num-parties $1 --depth $2 --gates-per-level $3 --net-config net_config.json --pking-semi $5 -o p1/$7.txt --threads $8 --repeat 2 --pid 1 2>&1 | tee -a p1/log.txt

    sleep 2
}

# num_parties depth gates_per_level compression pking pking_verify filename threads 
run_semi_benchmark_omp() {
    # Dealer
    ip netns exec neon_ns0 ./semi_omp --num-parties $1 --depth $2 --gates-per-level $3 --net-config net_config.json --pking-semi $5 -o p0/$7.txt --threads $8 --repeat 2 --pid 0 >> p0/log.txt 2>&1 &
    # Parties 2, 3, ..., N
    for i in $( eval echo {2..$1} ); do
        ip netns exec neon_ns$i ./semi_omp --num-parties $1 --depth $2 --gates-per-level $3 --net-config net_config.json --pking-semi $5 -o p$i/$7.txt --threads $8 --repeat 2 --pid $i >> p$i/log.txt 2>&1 &
    done
    # Party 1 where we also display log
    ip netns exec neon_ns1 ./semi_omp --num-parties $1 --depth $2 --gates-per-level $3 --net-config net_config.json --pking-semi $5 -o p1/$7.txt --threads $8 --repeat 2 --pid 1 2>&1 | tee -a p1/log.txt

    sleep 2
}

# num_parties dimension number compression pking pking_verify filename threads 
run_semi_dotp_benchmark_omp() {
    # Dealer
    ip netns exec neon_ns0 ./semi_dotp_omp --num-parties $1 --dimension $2 --number $3 --net-config net_config.json --pking-semi $5 -o p0/$7.txt --threads $8 --repeat 2 --pid 0 >> p0/log.txt 2>&1 &
    # Parties 2, 3, ..., N
    for i in $( eval echo {2..$1} ); do
        ip netns exec neon_ns$i ./semi_dotp_omp --num-parties $1 --dimension $2 --number $3 --net-config net_config.json --pking-semi $5 -o p$i/$7.txt --threads $8 --repeat 2 --pid $i >> p$i/log.txt 2>&1 &
    done
    # Party 1 where we also display log
    ip netns exec neon_ns1 ./semi_dotp_omp --num-parties $1 --dimension $2 --number $3 --net-config net_config.json --pking-semi $5 -o p1/$7.txt --threads $8 --repeat 2 --pid 1 2>&1 | tee -a p1/log.txt

    sleep 2
}

for i in {1..5}; do
    #####
    ### Table 2: 1M multiplications, 3+1 parties, 10/30/100 layers, 1/8 threads, malicious and semi
    #####
    echo Iteration $i Benchmark for Table 2 | tee -a iteration_log.txt
    # d = 10 ==> per layer = 100 000
    run_fliop_benchmark 3 10 100000 20 true 1 fliop-m1000000-n3-c20-d10-pking-1-t0 1
    run_fliop_benchmark_omp 3 10 100000 20 true 1 fliop-m1000000-n3-c20-d10-pking-1-t8 8
    run_semi_benchmark 3 10 100000 20 true 1 semi-m1000000-n3-d10-pking-t0 1
    run_semi_benchmark_omp 3 10 100000 20 true 1 semi-m1000000-n3-d10-pking-t8 8
    # d = 30 ==> per layer = 33 334
    run_fliop_benchmark 3 30 33334 20 true 1 fliop-m1000000-n3-c20-d30-pking-1-t0 1
    run_fliop_benchmark_omp 3 30 33334 20 true 1 fliop-m1000000-n3-c20-d30-pking-1-t8 8
    run_semi_benchmark 3 30 33334 20 true 1 semi-m1000000-n3-d30-pking-t0 1
    run_semi_benchmark_omp 3 30 33334 20 true 1 semi-m1000000-n3-d30-pking-t8 8
    # d = 100 ==> per layer = 10 000
    run_fliop_benchmark 3 100 10000 20 true 1 fliop-m1000000-n3-c20-d100-pking-1-t0 1
    run_fliop_benchmark_omp 3 100 10000 20 true 1 fliop-m1000000-n3-c20-d100-pking-1-t8 8
    run_semi_benchmark 3 100 10000 20 true 1 semi-m1000000-n3-d100-pking-t0 1
    run_semi_benchmark_omp 3 100 10000 20 true 1 semi-m1000000-n3-d100-pking-t8 8

    #####
    ### Table 3: 1K/10K/100K/1M/10M multiplications, 3+1 parties, 30 layers, 8 threads, malicious and semi
    #####
    echo Iteration $i Benchmark for Table 3 | tee -a iteration_log.txt
    # m = 1 000 ==> per layer = 34
    run_fliop_benchmark_omp 3 30 34 20 true 1 fliop-m1000-n3-c20-d30-pking-1-t8 8
    run_semi_benchmark_omp 3 30 34 20 true 1 semi-m1000-n3-d30-pking-t8 8
    # m = 10 000 ==> per layer = 334
    run_fliop_benchmark_omp 3 30 334 20 true 1 fliop-m10000-n3-c20-d30-pking-1-t8 8
    run_semi_benchmark_omp 3 30 334 20 true 1 semi-m10000-n3-d30-pking-t8 8
    # m = 100 000 ==> per layer = 3 334
    run_fliop_benchmark_omp 3 30 3334 20 true 1 fliop-m100000-n3-c20-d30-pking-1-t8 8
    run_semi_benchmark_omp 3 30 3334 20 true 1 semi-m100000-n3-d30-pking-t8 8
    # m = 1 000 000 ==> duplicate of table 2, reuse data from there
    # m = 10 000 000 ==> per layer = 333 334
    run_fliop_benchmark_omp 3 30 333334 20 true 1 fliop-m10000000-n3-c20-d30-pking-1-t8 8
    run_semi_benchmark_omp 3 30 333334 20 true 1 semi-m10000000-n3-d30-pking-t8 8

    #####
    ### Figure 10: 1M multiplications, 2/3/4/5/10/15/20/25/30+1 parties, 30 layers, 1 thread, malicious and semi
    #####
    echo Iteration $i Benchmark for Figure 10 | tee -a iteration_log.txt
    # 2+1 ==> never use pking
    run_fliop_benchmark 2 30 33334 20 false 0 fliop-m1000000-n2-c20-d30-broadcast-0-t0 1
    run_semi_benchmark 2 30 33334 20 false 0 semi-m1000000-n2-d30-broadcast-t0 1
    # 3+1 ==> duplicate of table 1, reuse data from there
    for n in 4 5 10 15 20 25 30; do
        # n+1
        run_fliop_benchmark $n 30 33334 20 true 1 fliop-m1000000-n$n-c20-d30-pking-1-t0 1
        run_semi_benchmark $n 30 33334 20 true 1 semi-m1000000-n$n-d30-pking-t0 1
    done

    #####
    ### Table 5: m|d=10K|10/10K|100/10K|1K/100K|10/100K|100/1M|10, 3+1 parties, 1 layer, 8 threads, malicious and semi
    #####
    echo Iteration $i Benchmark for Table 5 | tee -a iteration_log.txt
    for d in 10 100 1000; do
        run_fliop_dotp_benchmark_omp 3 $d 10000 20 true 1 fliop-dotp-d$d-nn10000-n3-c20-d1-pking-1-t8 8
        run_semi_dotp_benchmark_omp 3 $d 10000 20 true 1 semi-dotp-d$d-nn10000-n3-d1-pking-t8 8
    done
    for d in 10 100; do
        run_fliop_dotp_benchmark_omp 3 $d 100000 20 true 1 fliop-dotp-d$d-nn100000-n3-c20-d1-pking-1-t8 8
        run_semi_dotp_benchmark_omp 3 $d 100000 20 true 1 semi-dotp-d$d-nn100000-n3-d1-pking-t8 8
    done
    run_fliop_dotp_benchmark_omp 3 10 1000000 20 true 1 fliop-dotp-d10-nn1000000-n3-c20-d1-pking-1-t8 8
    run_semi_dotp_benchmark_omp 3 10 1000000 20 true 1 semi-dotp-d10-nn1000000-n3-d1-pking-t8 8

done

echo DONE
