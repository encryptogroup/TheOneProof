
set -e

echo "Starting related work benchmarks at $(date +%H:%M:%S) (container timezone might differ from host timezone)"

cd related_works

sysctl -w net.core.wmem_max=2129920
sysctl -w net.core.rmem_max=2129920
sysctl -w net.ipv4.tcp_rmem='40960 1310720 62914560'
sysctl -w net.ipv4.tcp_wmem='40960 163840 41943040'

# cd Asterisk/build/benchmarks
# echo "Starting Asterisk LAN benchmarks at $(date +%H:%M:%S) (container timezone might differ from host timezone)"
# python3 network.py start 31 LAN silent
# ./benchmark_asterisk.sh
# echo "Finished Asterisk LAN benchmarks at $(date +%H:%M:%S) (container timezone might differ from host timezone)"
# echo "short pause..."
# sleep 10
# python3 network.py stop 31 LAN silent
# echo "Saving Asterisk LAN benchmark data..."
# rm -rf ../../../../benchmark_results/Asterisk_LAN_reproduced/p*
# mv p* ../../../../benchmark_results/Asterisk_LAN_reproduced/

# echo "Starting Asterisk WAN benchmarks at $(date +%H:%M:%S) (container timezone might differ from host timezone)"
# python3 network.py start 31 WAN silent
# ./benchmark_asterisk.sh
# echo "Finished Asterisk WAN benchmarks at $(date +%H:%M:%S) (container timezone might differ from host timezone)"
# echo "short pause..."
# sleep 10
# python3 network.py stop 31 WAN silent
# echo "Saving Asterisk WAN benchmark data..."
# rm -rf ../../../../benchmark_results/Asterisk_WAN_reproduced/p*
# mv p* ../../../../benchmark_results/Asterisk_WAN_reproduced/

# echo "All Asterisk benchmarks done at $(date +%H:%M:%S) (container timezone might differ from host timezone)"
# cd ../../..




cd malicious_3pc_arithmetic
echo "Starting MP-SPDZ LAN benchmarks at $(date +%H:%M:%S) (container timezone might differ from host timezone)"
python3 network.py start 31 LAN silent
./benchmark_mpspdz.sh
echo "Finished MP-SPDZ LAN benchmarks at $(date +%H:%M:%S) (container timezone might differ from host timezone)"
echo "short pause..."
sleep 10
python3 network.py stop 31 LAN silent
echo "Saving MP-SPDZ LAN benchmark data..."
rm -rf ../../benchmark_results/MPSPDZ_LAN_reproduced/p*
mv log_* ../../benchmark_results/MPSPDZ_LAN_reproduced/

echo "Starting MP-SPDZ WAN benchmarks at $(date +%H:%M:%S) (container timezone might differ from host timezone)"
python3 network.py start 31 WAN silent
./benchmark_mpspdz.sh
echo "Finished MP-SPDZ WAN benchmarks at $(date +%H:%M:%S) (container timezone might differ from host timezone)"
echo "short pause..."
sleep 10
python3 network.py stop 31 WAN silent
echo "Saving MP-SPDZ WAN benchmark data..."
rm -rf ../../benchmark_results/MPSPDZ_WAN_reproduced/p*
mv log_* ../../benchmark_results/MPSPDZ_WAN_reproduced/

echo "All MP-SPDZ benchmarks done at $(date +%H:%M:%S) (container timezone might differ from host timezone)"
cd ..

cd ..
echo "All related works benchmarks done at $(date +%H:%M:%S) (container timezone might differ from host timezone)"
