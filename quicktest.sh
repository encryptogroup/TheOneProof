cd build
cd tests

./fliop_test --num-parties 3 --localhost --compression 2 --pid 0 > /dev/null 2>&1 &  # Dealer
PID0=$!
./fliop_test --num-parties 3 --localhost --compression 2 --pid 1 > /dev/null 2>&1 &  # Party 1
PID1=$!
./fliop_test --num-parties 3 --localhost --compression 2 --pid 2 > /dev/null 2>&1 &  # Party 2
PID2=$!
./fliop_test --num-parties 3 --localhost --compression 2 --pid 3 &                   # Party 3
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
echo "ALL TESTS SUCCEEDED!"
cd ..
cd ..
