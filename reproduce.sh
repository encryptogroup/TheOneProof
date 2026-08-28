
set -e

cd build/benchmarks

sysctl -w net.core.wmem_max=2129920
sysctl -w net.core.rmem_max=2129920
sysctl -w net.ipv4.tcp_rmem='40960 1310720 62914560'
sysctl -w net.ipv4.tcp_wmem='40960 163840 41943040'

echo "Starting LAN benchmarks at $(date +%H:%M:%S) (container timezone might differ from host timezone)"
python3 network.py start 31 LAN silent
./benchmark_ours.sh
echo "Finished LAN benchmarks at $(date +%H:%M:%S) (container timezone might differ from host timezone)"
echo "short pause..."
sleep 10
python3 network.py stop 31 LAN silent
echo "Saving LAN benchmark data..."
rm -rf ../../benchmark_results/LAN_reproduced/p*
mv p* ../../benchmark_results/LAN_reproduced/

echo "Starting WAN benchmarks at $(date +%H:%M:%S) (container timezone might differ from host timezone)"
python3 network.py start 31 WAN silent
./benchmark_ours.sh
echo "Finished WAN benchmarks at $(date +%H:%M:%S) (container timezone might differ from host timezone)"
echo "short pause..."
sleep 10
python3 network.py stop 31 WAN silent
echo "Saving WAN benchmark data..."
rm -rf ../../benchmark_results/WAN_reproduced/p*
mv p* ../../benchmark_results/WAN_reproduced/

echo "All benchmarks done at $(date +%H:%M:%S) (container timezone might differ from host timezone)"

cd ../../
