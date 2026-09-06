
set -e

echo "Preparing related works ... ($(date +%H:%M:%S), container timezone might differ from host timezone)"

cd related_works
# clear any potentially prior installations:
rm -rf Asterisk
rm -rf malicious_3pc_arithmetic

git clone https://github.com/cris-coders-iisc/Asterisk.git
git clone https://github.com/AntCPLab/malicious_3pc_arithmetic.git

echo "Preparing Asterisk ... ($(date +%H:%M:%S), container timezone might differ from host timezone)"
cd Asterisk
# Switch to benchmarked version and apply our patch (adding benchmark target and setting field size to 40 bits)
git checkout b20b1aa
git apply --reject --whitespace=fix ../asterisk.patch
mkdir build
cd build
# Compile
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ ..
make -j8 benchmarks
# Add network emulation and benchmark scripts, add net-config
cp ../../../scripts/network.py benchmarks/
cp ../../../scripts/benchmark_asterisk.sh benchmarks/
cp ../../../scripts/net_config.json benchmarks/
cd ..
cd ..

echo "Preparing MP-SPDZ ... ($(date +%H:%M:%S), container timezone might differ from host timezone)"
cd malicious_3pc_arithmetic
# Switch to benchmarked version and apply our patch (removing NTL, setting k=32 for MP-SPDZ args and in zk-FLIOP source code)
git checkout a919609
git apply --reject --whitespace=fix ../mpspdz.patch
# Setup, including compilation of dependencies
make setup
make -j8 libote
Scripts/setup-ssl.sh 5
# Insert benchmark circuit
cp ../1M-30.mpc Programs/Source/
# Compile benchmark circuit
python3 compile.py 1M-30.mpc -R 32
# Compile protocols
make -j8 test-ring-party.x
make -j8 Fake-Offline.x
make -j8 spdz2k-party.x
# Create preprocessing material for SPDZ2k
./Fake-Offline.x 3 -Z 32 -S 32
# Add network emulation and benchmark scripts, add net-config (different format for MP-SPDZ)
cp ../../scripts/network.py ./
cp ../../scripts/benchmark_mpspdz.sh ./
cp ../../scripts/net_config.txt ./
cd ..

cd ..

echo "Finished related works setup! ($(date +%H:%M:%S), container timezone might differ from host timezone)"
