# One Proof to Rule Them All: Practical, Sublinear Verification for Actively Secure MPC on Z2k with Dishonest Majority and a Dealer

This repository contains the implementation of our novel (n+1)-party protocol with active security, utilizing zk-FLIOPS.
The protocol is implemented in C++17 and [CMake](https://cmake.org/) is used as the build system.
It is based on [Asterisk](https://github.com/cris-coders-iisc/Asterisk), using its circuit representation, communication layer, PRFs, etc.

## External Dependencies
We also provide a Dockerfile, documented at the end of this document.
This automatically takes care of all dependencies listed here.

The following libraries need to be installed separately and should be available to the build system and compiler.

- [GMP](https://gmplib.org/)
- [Boost](https://www.boost.org/) (1.72.0 or later)
- [Nlohmann JSON](https://github.com/nlohmann/json)
- [EMP Tool](https://github.com/emp-toolkit/emp-tool)

Our code is tested and benchmarked on Arch Linux on x86-64, using the following tool and dependency versions:
- CMake 4.1.2
- clang 21.1.5
- Boost 1.89.0
- Nlohmann JSON 3.12.0
- emp-tool commit 8052d95

## Building

```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -D ENABLE_PROTOCOL_OMP=[OFF/ON] ..
make -j8 benchmarks
```

Using ```ENABLE_PROTOCOL_OMP```, it is possible to completely deactivate the use of OpenMP for parallelization.
This will cause the protocol to run on a single thread, ignoring the provided number of desired threads.
Use this if you do not plan to use parallelization, as the resulting binary is more efficient than the one with parallelization
support if this is not utilized.

If the proof requires 256 bit integers, navigate to ../src/utils/types.h and swap the definition of ```LargeRing```, using the commented out part.
256 bit integers only work if ```ENABLE_PROTOCOL_OMP=OFF```

## Running the protocol

After compiling, there will be multiple binaries inside of the benchmarks directory.
```fliop``` denotes our actively secure protocol while ```semi``` is the passively secure baseline protocol.
Run, e.g., ```./fliop -h``` to access documentation on possible arguments.

Example for running with 2 parties and a dealer on the local machine, printing the results for party 2 and letting all parties write their results to logfiles:
```sh
# Make sure to navigate to build/benchmarks first
./fliop --num-parties 2 --depth 10 --gates-per-level 100 --localhost --compression 2 --pid 0 >> p0log.txt 2>&1 &
./fliop --num-parties 2 --depth 10 --gates-per-level 100 --localhost --compression 2 --pid 1 >> p1log.txt 2>&1 &
./fliop --num-parties 2 --depth 10 --gates-per-level 100 --localhost --compression 2 --pid 2 2>&1 | tee -a p2log.txt
```

## Full Benchmark Routine
The following is what we did use for our full benchmarks (including compilation).
```sh
mkdir build
cd build
# Create fliop_omp, semi_omp as versions with OpenMP parallelization
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -D ENABLE_PROTOCOL_OMP=ON ..
make -j8 benchmarks
cp benchmarks/fliop benchmarks/fliop_omp
cp benchmarks/semi benchmarks/semi_omp
# Create fliop_256 as version running verification on 256 bit integers
nano ../src/utils/types.h 
# change to 256 bit, using the commented out definition of LargeRing
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -D ENABLE_PROTOCOL_OMP=OFF ..
make -j8 benchmarks
cp benchmarks/fliop benchmarks/fliop_256
# Create fliop, semi as versions without OpenMP and verifying on 128 bit integers
git restore ../src/utils/types.h # If not using git, nano ../src/utils/types.h and revert the changes done before for 256 bits.
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -D ENABLE_PROTOCOL_OMP=OFF ..
make -j8 benchmarks
cd benchmarks

# Increase network buffer sizes (remember to first output your current values to later reset to this if not using inside of a container)
sudo sysctl -w net.core.wmem_max=2129920 &
sudo sysctl -w net.core.rmem_max=2129920 &
sudo sysctl -w net.ipv4.tcp_rmem='40960 1310720 62914560' &
sudo sysctl -w net.ipv4.tcp_wmem='40960 163840 41943040'

# Use LAN or WAN depending on the desired network setting
sudo python3 network.py start 31 [LAN/WAN]

# This will take a while...
nohup sudo ./benchmark_full.sh &
# Results will be written to files in build/benchmarks/p[i] for party i, party 0 is the dealer
```

## Updated Full Benchmark Routine
The following is what we did use for our full benchmarks (including compilation).
```sh
mkdir build
cd build
# Create fliop_omp, semi_omp (and dotp versions) as versions with OpenMP parallelization
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -D ENABLE_PROTOCOL_OMP=ON ..
make -j8 benchmarks
cp benchmarks/fliop benchmarks/fliop_omp
cp benchmarks/fliop_dotp benchmarks/fliop_dotp_omp
cp benchmarks/semi benchmarks/semi_omp
cp benchmarks/semi_dotp benchmarks/semi_dotp_omp
# Create fliop, semi (and dotp versions) as versions without OpenMP
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -D ENABLE_PROTOCOL_OMP=OFF ..
make -j8 benchmarks
cd benchmarks

# Increase network buffer sizes (remember to first output your current values to later reset to this if not using inside of a container)
sudo sysctl -w net.core.wmem_max=2129920 &
sudo sysctl -w net.core.rmem_max=2129920 &
sudo sysctl -w net.ipv4.tcp_rmem='40960 1310720 62914560' &
sudo sysctl -w net.ipv4.tcp_wmem='40960 163840 41943040'

# Use LAN or WAN depending on the desired network setting
sudo python3 network.py start 31 [LAN/WAN]

# This will take a while...
nohup sudo ./benchmark_full.sh &
# Results will be written to files in build/benchmarks/p[i] for party i, party 0 is the dealer
```

## Docker
All required dependencies to compile and run the project are available through the docker image.
This only works if you install docker-buildx on your device.
To build and run the docker image, execute the following commands from the root directory of the repository:

```sh
# First, clone the repository and navigate into its main directory

# Then:
# Setting up and running container
sudo docker buildx build -t zkfliop . 
sudo docker run -it -v $(pwd):$(pwd) -w $(pwd) --cap-add=NET_ADMIN zkfliop
# Proceed working inside the container...
```


