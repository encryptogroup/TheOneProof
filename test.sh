test_counter=1
num_tests=36

COUNT=${1:-1}

# repetitions compression pking pking_verify binary
run_test_2P() {
    echo -n "Starting test $test_counter/$num_tests at $(date +%H:%M:%S): ....."
    $5 --num-parties 2 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 0 >> p0log.txt &
    PID0=$!
    $5 --num-parties 2 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 1 >> p1log.txt &
    PID1=$!
    $5 --num-parties 2 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 2 >> p2log.txt &
    PID2=$!
    wait $PID0
    if [ $? -ne 0 ]; then
        echo "ERROR: P0 failed with exit code $?"
        exit 1
    fi
    wait $PID1
    if [ $? -ne 0 ]; then
        echo "ERROR: P1 failed with exit code $?"
        exit 1
    fi
    wait $PID2
    if [ $? -ne 0 ]; then
        echo "ERROR: P2 failed with exit code $?"
        exit 1
    fi

    sleep 0.3
    test_counter=$((test_counter+1))
    echo " done ($(date +%H:%M:%S))"
}

# repetitions compression pking pking_verify
run_test_3P() {
    echo -n "Starting test $test_counter/$num_tests at $(date +%H:%M:%S): ....."
    $5 --num-parties 3 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 0 >> p0log.txt &
    PID0=$!
    $5 --num-parties 3 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 1 >> p1log.txt &
    PID1=$!
    $5 --num-parties 3 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 2 >> p2log.txt &
    PID2=$!
    $5 --num-parties 3 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 3 >> p3log.txt &
    PID3=$!
    wait $PID0
    if [ $? -ne 0 ]; then
        echo "ERROR: P0 failed with exit code $?"
        exit 1
    fi
    wait $PID1
    if [ $? -ne 0 ]; then
        echo "ERROR: P1 failed with exit code $?"
        exit 1
    fi
    wait $PID2
    if [ $? -ne 0 ]; then
        echo "ERROR: P2 failed with exit code $?"
        exit 1
    fi
    wait $PID3
    if [ $? -ne 0 ]; then
        echo "ERROR: P3 failed with exit code $?"
        exit 1
    fi

    sleep 1
    test_counter=$((test_counter+1))
    echo " done ($(date +%H:%M:%S))"
}

# repetitions compression pking pking_verify
run_test_10P() {
    echo -n "Starting test $test_counter/$num_tests at $(date +%H:%M:%S): ....."
    $5 --num-parties 10 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 0 >> p0log.txt &
    PID0=$!
    $5 --num-parties 10 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 1 >> p1log.txt &
    PID1=$!
    $5 --num-parties 10 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 2 >> p2log.txt &
    PID2=$!
    $5 --num-parties 10 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 3 >> p3log.txt &
    PID3=$!
    $5 --num-parties 10 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 4 >> p4log.txt &
    PID4=$!
    $5 --num-parties 10 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 5 >> p5log.txt &
    PID5=$!
    $5 --num-parties 10 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 6 >> p6log.txt &
    PID6=$!
    $5 --num-parties 10 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 7 >> p7log.txt &
    PID7=$!
    $5 --num-parties 10 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 8 >> p8log.txt &
    PID8=$!
    $5 --num-parties 10 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 9 >> p9log.txt &
    PID9=$!
    $5 --num-parties 10 --repeat $1 --localhost --compression $2 --pking-semi $3 --pking-verify $4 --pid 10 >> p10log.txt &
    PID10=$!
    wait $PID0
    if [ $? -ne 0 ]; then
        echo "ERROR: P0 failed with exit code $?"
        exit 1
    fi
    wait $PID1
    if [ $? -ne 0 ]; then
        echo "ERROR: P1 failed with exit code $?"
        exit 1
    fi
    wait $PID2
    if [ $? -ne 0 ]; then
        echo "ERROR: P2 failed with exit code $?"
        exit 1
    fi
    wait $PID3
    if [ $? -ne 0 ]; then
        echo "ERROR: P3 failed with exit code $?"
        exit 1
    fi
    wait $PID4
    if [ $? -ne 0 ]; then
        echo "ERROR: P4 failed with exit code $?"
        exit 1
    fi
    wait $PID5
    if [ $? -ne 0 ]; then
        echo "ERROR: P5 failed with exit code $?"
        exit 1
    fi
    wait $PID6
    if [ $? -ne 0 ]; then
        echo "ERROR: P6 failed with exit code $?"
        exit 1
    fi
    wait $PID7
    if [ $? -ne 0 ]; then
        echo "ERROR: P7 failed with exit code $?"
        exit 1
    fi
    wait $PID8
    if [ $? -ne 0 ]; then
        echo "ERROR: P8 failed with exit code $?"
        exit 1
    fi
    wait $PID9
    if [ $? -ne 0 ]; then
        echo "ERROR: P8 failed with exit code $?"
        exit 1
    fi
    wait $PID10
    if [ $? -ne 0 ]; then
        echo "ERROR: P10 failed with exit code $?"
        exit 1
    fi

    sleep 1
    test_counter=$((test_counter+1))
    echo " done ($(date +%H:%M:%S))"
}

echo "Running each test $COUNT times"
cd build
cd tests
run_test_2P $COUNT 2 false 0 ./fliop_test
run_test_10P $COUNT 2 true 0 ./fliop_test
run_test_3P $COUNT 2 true 0 ./fliop_test
run_test_3P $COUNT 2 true 1 ./fliop_test
run_test_3P $COUNT 2 true 2 ./fliop_test
run_test_3P $COUNT 2 false 0 ./fliop_test
run_test_3P $COUNT 2 false 1 ./fliop_test
run_test_3P $COUNT 2 false 2 ./fliop_test
run_test_3P $COUNT 3 true 1 ./fliop_test
run_test_3P $COUNT 4 true 1 ./fliop_test
run_test_3P $COUNT 5 true 1 ./fliop_test
run_test_3P $COUNT 10 true 1 ./fliop_test
run_test_3P $COUNT 20 true 1 ./fliop_test
run_test_3P $COUNT 30 true 1 ./fliop_test

run_test_2P $COUNT 0 false 0 ./semi_test
run_test_10P $COUNT 0 true 0 ./semi_test
run_test_3P $COUNT 0 true 0 ./semi_test
run_test_3P $COUNT 0 false 0 ./semi_test

run_test_2P $COUNT 2 false 0 ./fliop_dotp_test
run_test_10P $COUNT 2 true 0 ./fliop_dotp_test
run_test_3P $COUNT 2 true 0 ./fliop_dotp_test
run_test_3P $COUNT 2 true 1 ./fliop_dotp_test
run_test_3P $COUNT 2 true 2 ./fliop_dotp_test
run_test_3P $COUNT 2 false 0 ./fliop_dotp_test
run_test_3P $COUNT 2 false 1 ./fliop_dotp_test
run_test_3P $COUNT 2 false 2 ./fliop_dotp_test
run_test_3P $COUNT 3 true 1 ./fliop_dotp_test
run_test_3P $COUNT 4 true 1 ./fliop_dotp_test
run_test_3P $COUNT 5 true 1 ./fliop_dotp_test
run_test_3P $COUNT 10 true 1 ./fliop_dotp_test
run_test_3P $COUNT 20 true 1 ./fliop_dotp_test
run_test_3P $COUNT 30 true 1 ./fliop_dotp_test

run_test_2P $COUNT 0 false 0 ./semi_dotp_test
run_test_10P $COUNT 0 true 0 ./semi_dotp_test
run_test_3P $COUNT 0 true 0 ./semi_dotp_test
run_test_3P $COUNT 0 false 0 ./semi_dotp_test
echo "ALL TESTS SUCCEEDED!"
rm p0log.txt
rm p1log.txt
rm p2log.txt
rm p3log.txt
rm p4log.txt
rm p5log.txt
rm p6log.txt
rm p7log.txt
rm p8log.txt
rm p9log.txt
rm p10log.txt
cd ..
cd ..
