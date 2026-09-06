# One Proof to Rule Them All: Practical, Sublinear Verification for Actively Secure MPC on Z2k with Dishonest Majority and a Dealer

This repository contains the implementation of our novel (n+1)-party protocol with active security, utilizing one single distributed zero-knowledge proof,
as introduced by our paper at CCS 2026 (full version on [eprint](https://ia.cr/2026/1741)).
In more detail, we provide MPC implementations for the following:
* Passively secure MPC with n parties (dishonest majority) and a trusted dealer, designed for computation on Z2k and function-dependent preprocessing.
* Actively secure MPC, using a distributed zero-knowledge proof on top of the passively secure protocol to protect against cheaters.
* Versions of the passively and actively secure MPC that also support computation of dot-products.

Parts of the code are based on [Asterisk](https://github.com/cris-coders-iisc/Asterisk), using its circuit representation, communication layer, PRFs, etc.

> [!WARNING]
> This code is a research prototype implementation and, hence, should not be used in production.

> Check out our paper (full version on [eprint](https://ia.cr/2026/1741)), accepted at CCS'26!
>
> When using this code, please cite our paper:
> ```bibtex
> @inproceedings{CCS:BNS26,
>   author = {Andreas Br\"uggemann and Ariel Nof and Thomas Schneider},
>   title = {{One Proof to Rule Them All: Practical, Sublinear Verification for Actively Secure MPC on $\mathbb{Z}_{2^k}$ with Dishonest Majority and a Dealer}},
>   booktitle = {33. {ACM} Conference on Computer and Communications Security (CCS'26)},
>   year = {2026},
>   month = {November 15-19,},
>   publisher = {ACM},
>   note = {Full version: \url{https://ia.cr/2026/1741}. Code: \url{https://encrypto.de/code/TheOneProof}
> }
> ```


## :books: Table of Contents

* [:clipboard: Requirements](#clipboard-requirements): Hard- and software requirements to run this artifact's code.
* [:fast_forward: Quickstart Guide](#fast_forward-quickstart-guide): Quick walkthrough, demonstrating how to run the protocols.
* [:bar_chart: Reproducing our Benchmark Results](#bar_chart-reproducing-our-benchmark-results): Guide to reproduce the paper's results.
* [:seedling: Environment](#seedling-environment): Setting up the environment to compile and run the protocols, either using Docker or standalone.
* [:gear: Compiling the Protocols](#gear-compiling-the-protocols): Instructions on compiling the code and available options.
* [:play_or_pause_button: Running the Protocols](#play_or_pause_button-running-the-protocols): Guide on running the protocols as benchmarks or tests.
* [:chart_with_upwards_trend: Processing Benchmark Data](#chart_with_upwards_trend-processing-benchmark-data): Processing of raw benchmark outputs to obtain the tables and plots from the paper.
* [:page_facing_up: Repository Structure](#page_facing_up-repository-structure): Contents and structure of this repository.
* [:building_construction: Extending the Code](#building_construction-extending-the-code): Pointers on extending the code.


## :clipboard: Requirements

This code has been designed and tested for Linux on an x86_64 architecture.
(It has also been successfully compiled and run on MacOS with Apple Silicon, but this platform does
not support the scripts to locally benchmark and reproduce all experiments, and it requires
to manually set up a proper clang toolchain as Apple's clang does not support OpenMP.)
We have designed our benchmarks so that they can easily be run on a single device, locally emulating a network, which is the
setting that we focus on here. 

There are no strict requirements on the number of CPU cores, RAM, etc.
Yet, if the goal is to reproduce our full benchmark results, please note the following:
* **Strict:** At least **20 GB of RAM** should be **available and free** (so the system should have 20 GB **plus** what is needed by OS, other open programs)
* *Optional:* The benchmarks utilize up to 32 CPU hardware threads (usually 16 CPU cores) as it runs multiple parties, each potentially multithreading.
                Benchmarking with less works, but will yield higher run times.
*We ran our benchmarks on a single machine with an Intel Core i9-7960X CPU @ 2.8 GHz (16 cores, 32 threads) and 128 GB of DDR4 RAM @ 2666 MHz.*

Software requirements are as follows:
* docker and docker-buildx to use the code inside a Docker container (not needed if the container is not used)
* Further requirements only if code is not run using the Docker container, see [here](#standalone).


## :fast_forward: Quickstart Guide

This guide is intended to demonstrate how to run our MPC protocols manually inside a Docker container.
If you want to reproduce our benchmark results, please instead continue [here](#bar_chart-reproducing-our-benchmark-results).

```sh
# First, clone/download/extract the repository and navigate into its main directory
# Then, build the Docker container
sudo docker buildx build --network=host -t zkfliop . # takes a few minutes
# Run the Docker container
sudo docker run -it -v $(pwd):$(pwd) -w $(pwd) --privileged zkfliop # --privileged needed for network emulation
# (proceed working inside the container)

# Compile the protocol(s)
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -D ENABLE_PROTOCOL_OMP=ON ..
make -j8 benchmarks
make -j8 tests
cd benchmarks

# Run the protocol
./fliop --num-parties 3 --depth 10 --gates-per-level 100 --localhost --compression 20 --pid 0 > /dev/null 2>&1 &  # Dealer
./fliop --num-parties 3 --depth 10 --gates-per-level 100 --localhost --compression 20 --pid 1 > /dev/null 2>&1 &  # Party 1
./fliop --num-parties 3 --depth 10 --gates-per-level 100 --localhost --compression 20 --pid 2 > /dev/null 2>&1 &  # Party 2
./fliop --num-parties 3 --depth 10 --gates-per-level 100 --localhost --compression 20 --pid 3                     # Party 3
```

This example runs a simple benchmark: our actively secure MPC protocol ```fliop```, with 3 parties (plus a dealer),
10 layers of 100 multiplications each, communication over the localhost interface, and a compression factor of 20.
The outputs reveal the following:

The full settings, including those not explicitly set but using default values: (for the full documentation, check [here](#arguments))
```
--- Details ---
comp_security_param: 128
compression_factor: 20
depth: 10
gates_per_level: 100
n_bits: 32
n_bits_extended: 128
num-parties: 3
pid: 3
pking-semi: true
pking-verify: 1
repeat: 1
stat_security_param: 40
threads: 4
```

Checks that the chosen parameters provide security (otherwise, the execution will abort):
```
Check if conditions for Theorem 1 are satisfied
Protocol security requires that s >= 74
Computing on 32 bit integers, verifying over 128 bit integers
==> actual s = 96
==> Protocol is secure for the provided arguments!
```

A summary of the generated circuit: 
```
--- Circuit ---
Input: 100
Addition: 1000
Multiplication: 1000
Multiplication with constant: 0
Dotproduct: 0
Invalid: 0
Total: 2100
Depth: 21 # this includes addition and input layers
```

Logging outputs from the distributed zero-knowledge proof:
```
Starting Verification...
Reducing vector size from 3000...
==> reduced to 150
Reducing vector size from 150...
==> reduced to 8
Reducing vector size from 8...
==> reduced to scalars
Verification finished successfully, no cheating was detected
```

And, finally, measurements of the protocol performance (communication is constant,
other values may vary):
```
--- Repetition 1 ---
setup time: 1.274034 ms
setup sent: 0 bytes
time: 1.352613 ms
sent: 25534 bytes
verification rounds: 19

--- Statistics ---
peak_resident_set_size: 15788 KB
peak_virtual_memory: 308908 KB
```

Use ```./fliop -h``` to get an overview over all arguments.
To continue with details on running benchmarks for (different) protocols, go [here](#benchmarks).


## :bar_chart: Reproducing our Benchmark Results

**Please recall that 20 GB of RAM need to be available and free to reproduce our benchmark results!**
Less than 32 CPU hardware threads can be used, but this will negatively impact run time measurements.

Reproducing the results is expected to take ca. 3h of compute time and 15 person-minutes.
Each individual experiment is run 10 times and we average over the resulting values.
Note that these 10 iterations are split into 5 runs of the respecive benchmark program, which does 2
iterations on each run.

> [!NOTE]
> Optional benchmarks for related work.
> 
> In our paper, we empirically compare to three related works:
> [Asterisk](https://ia.cr/2023/1098), [RSS with zk-FLIOPs over rings](https://ia.cr/2024/700), and [SPDZ2k](https://ia.cr/2018/482).
> For our comparisons, we utilize existing codebases:
> * [Asterisk on GitHub](https://github.com/cris-coders-iisc/Asterisk)
> * [malicious_3pc_arithmetic on GitHub](https://github.com/AntCPLab/malicious_3pc_arithmetic) which, as fork of MP-SPDZ, also contains
>     an implementation of SPDZ2k.
>
> These reference implementations **are not** part of our artifact, as they belong to different papers by different authors.
> Hence, we make it **optional** to also reproduce their benchmarks here, providing our measurements as a fallback.
>
> Our artifact contains scripts to clone the respective repositories which we did only find archived on GitHub, use a fixed commit which was
> the most recent one when we benchmarked our paper, and apply minor patches to them for adjusting their domain sizes for a fair comparison
> and running our benchmarks.
> The patches can be found inside the [related_works/](related_works/) folder.
>
> As a backup, we also provide forks of these repositories [here](https://github.com/andreasbrueg/Asterisk_benchmarking) and [here](https://github.com/andreasbrueg/malicious_3pc_arithmetic).
> 
> In the following, we are marking all optional steps for the related work with a :star:.

Run our protocol benchmarks by following these steps:
```sh
sudo docker buildx build --network=host -t zkfliop . # takes a few minutes
sudo docker run -it -v $(pwd):$(pwd) -w $(pwd) --privileged zkfliop # --privileged needed for network emulation
./compile.sh # should take <2 minutes
./reproduce.sh # should take <2 hours (runs 240 benchmarks twice, once in LAN, once in WAN)
# stay inside the Docker container for the next steps
```

**:star: Optional:** Run related work protocol benchmarks by following these steps:
```sh
./related_works_setup.sh # should take ca. 10 minutes (downloads and compiles related works and their dependencies)
./related_works_reproduce.sh # should take <30 minutes (runs 15 benchmarks for Asterisk twice (LAN+WAN), then 20 benchmarks for the others twice (LAN+WAN))
# stay inside the Docker container for the next steps
```

Now, generate tables, plots, etc. as follows (if you skipped the optional related works benchmarks):
```sh
./parse_reproduced.sh all # should take <5 seconds
```

**:star: Optional:** If you did run the optional benchmarks for related works before, use the following instead
to utilize your benchmark data instead of the fallback data:
```sh
./parse_reproduced.sh all relatedworks # should take <5 seconds
```

The output contain results regarding all empirical performance claims from our paper.
In particular, it contains Tables 3-7, Figures 9-10, and additional claims made in the text of our evaluation section.
Please refer to the CLI output which documents to which claimed results in the paper each part of the output corresponds.
Also, please note that Figures 9-10 are exported as PDFs which need to be manually opened from [this folder](benchmark_results/plots/).

**:warning: Possible deviations between results from the paper and reproduced results:**
Our benchmarks test for communication, round complexity, and run time.
While communication and round complexity only depend on the implemented protocols and, hence, are constant,
run time depends on the following factors, some of them quite variable:
* Communication and round complexity: This is constant.
* Network characteristics: These are emulated for constant targets (LAN 1ms RTT, 1Gbit/s; WAN 100ms RTT, 100Mbit/s),
        but the emulation, sockets, and other factors might lead to some variation.
* Compute performance: This can be very variable and, given that especially our actively secure protocol is relatively
    heavy on computation, it can have a significant impact on the resulting run times. Note that single-core performance
    generally impacts run times, whereas the number of available cores impacts run times especially when using many
    parties and/or many threads per party. E.g., on a CPU with few cores but high single-core performance, we can
    expect single-thread run times for few parties to be significantly better than what is reported in our paper, while
    multi-thread run times or run times with many parties may be worse than in our paper.

**Our primary goal is to evaluate the cost overhead of running our verification to get active security.**
Run times for the passively secure baseline and the actively secure protocol depend on the used hardware,
but the overheads should be comparable to those reported in the paper.

To summarize, run times (and especially verification overheads and improvement factors) should follow a similar trend
to what is reported in the paper, but different hardware may still have a significant impact by dictating the compute performance.
Furthermore, in LAN, compute performance plays a significantly more important role compared to WAN, whereas in WAN, run time
is more dependent on communication (which is constant). Hence, it is to be expected that WAN run time results are closer to
our paper than those for LAN.

Also, please note that without running the optional benchmarks for related works, all comparisons between them and our protocol
will be skewed by the fact that we then use our own benchmark results as fallback for the related work, leading to different
compute performance for different benchmarks.


## :seedling: Environment

The environment to compile and run our code in can be set up in a Docker container or standalone.
Additionally, we provide an environment to locally emulate networks.

### Using Docker

We provide a Dockerfile to set up the environment in a simple manner.

Run the following command to build and run the container:
```sh
sudo docker buildx build --network=host -t zkfliop . # takes a few minutes
sudo docker run -it -v $(pwd):$(pwd) -w $(pwd) --privileged zkfliop # --privileged needed for network emulation
# (proceed working inside the container)
```

### Standalone

The protocol is implemented in C++17 and [CMake](https://cmake.org/) is used as the build system.
It has been tested on Arch Linux with clang version 22.1.8 and Ubuntu 22.04 with clang version 14.0.0-1ubuntu1.1, and on an x86_64 architecture.

The following tools/libraries need to be available:
* [Boost](https://www.boost.org/), e.g., ```libboost-all-dev``` on Ubuntu (tested on version 1.74)
* [clang](https://clang.llvm.org/), e.g., ```clang``` on Ubuntu (tested on version 14.0.0-1ubuntu1.1)
* [CMake](https://cmake.org/), e.g., ```cmake``` on Ubuntu (tested on version 3.22.1)
* [EMP Tool](https://github.com/emp-toolkit/emp-tool) commit ```8052d95```, compiled with clang, e.g., install as follows:
    * download
    * ```git checkout 8052d95```
    * ```cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_POLICY_VERSION_MINIMUM=3.5 .```
    * ```make -j8```
    * ```make install```
* [git](https://git-scm.com/), e.g., ```git``` on Ubuntu (tested on version 2.34.1)
* [GMP](https://gmplib.org/), e.g., ```libgmp3-dev``` on Ubuntu (tested on version 6.2.1)
* [iproute2](https://www.kernel.org/pub/linux/utils/net/iproute2/), e.g., ```iproute2``` on Ubuntu (tested on version 5.15.0)
* [Nlohmann JSON](https://github.com/nlohmann/json), e.g., ```nlohmann-json3-dev``` on Ubuntu (tested on version 3.10.15)
* [OpenMP](https://www.openmp.org/), e.g., ```libomp-dev``` on Ubuntu
* [OpenSSL](https://github.com/openssl/openssl), e.g., ```libssl-dev``` on Ubuntu (tested on version 3.0.2)
* ping, e.g., ```iputils-ping``` on Ubuntu
* [Python3](https://www.python.org/), e.g., ```python3``` on Ubuntu (tested on version 3.10.12)

Additionally, draw plots from benchmark data, we require:
* [Matplotlib](https://matplotlib.org/), e.g., ```pip3 install matplotlib``` (after installing pip3: ```python3-pip```) on Ubuntu (tested on version 3.10.9)

When also running benchmarks for the related work, we require:
* [g++](https://gcc.gnu.org/), e.g., ```g++``` on Ubuntu (tested on version 11.4.0)
* [libsodium](https://github.com/jedisct1/libsodium), e.g., ```libsodium-dev``` on Ubuntu (tested on version 1.0.18)
* [NTL](https://www.shoup.net/ntl/), e.g., ```libntl-dev``` on Ubuntu (tested on version 11.5.1)

### Local Network Emulation

For testing and benchmarking purposes, it is useful to run all parties on the same device.
They can either connect over localhost or a locally emulated network, where the second option has the
benefit that it enables to set specific RTTs and bandwidths to emulate realistic network behavior.

To use an emulated network, use [network.py](scripts/network.py) as follows:
```sh
python3 network.py <start/stop> <number of parties, including dealer> <LAN/WAN>
```
e.g., to start a LAN with 5 parties,
```sh
python3 network.py start 5 LAN
```
and to stop it,
```sh
python3 network.py stop 5 LAN
```

It is important to always correctly stop the network emulation (and not start one while there is still one running).
Otherwise, the script may lead to unexpected behavior, in which case restarting the Docker container resolves the issue.

To run a command on behalf of party i=0,1,..., execute the following:
```sh
ip netns exec neon_ns<i> <command> <args to the command, etc. ...>
```

The IP addresses of the parties are numbered
```
172.16.1.11
172.16.1.12
172.16.1.13
...
```
They are also provided in [net_config.json](scripts/net_config.json) which can be passed as argument to our protocol for easy management of IP addresses.

The network emulation is based on [NEON and XENON](https://gitlab.com/rwth-itsec/neon-and-xenon).
It is licensed under GPLv3 in contrast to the remaining repository.


## :gear: Compiling the Protocols

To compile (inside a Docker container or a standalone environment), run the following commands from the root directory of the repository.

You may simply run
```sh
./compile.sh
```
That will automatically compile everything in Release mode with verification on 128 bits.
It will create two versions, one with a suffix ```_omp``` compiled with OpenMP parallelization, and one
without support for parallelization that the compiler can better optimize for single-threaded performance.

Alternatively, follow the next steps to compile step-by-step:

```sh
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=[Release/Debug] -DCMAKE_CXX_COMPILER=clang++ -D ENABLE_PROTOCOL_OMP=[ON/OFF] -D VERIFY_MOD_2_256=[ON/OFF] ..
make -j8 benchmarks
make -j8 tests
```

Using ```ENABLE_PROTOCOL_OMP```, it is possible to activate or completely deactivate the use of OpenMP for parallelization.
The latter will cause the protocol to run on a single thread, ignoring the provided number of desired threads.
Use this if you do not plan to use parallelization, as the resulting binary is more efficient than the one with parallelization
support if that is not utilized.

If the proof requires 256 bit integers instead of 128 bit integers, set ```VERIFY_MOD_2_256``` to ```ON```.
This only work if ```ENABLE_PROTOCOL_OMP=OFF``` as the 256 bit version unfortunately has no OpenMP support.
Also, platforms support is quite limited, as the underlying Clang's ```BitInt``` extension for 256 bits is experimental.


## :play_or_pause_button: Running the Protocols

### Benchmarks

> For reproducing all benchmarks as provided in the paper, see [here](#bar_chart-reproducing-our-benchmark-results) for instructions
> and scripts to automate the benchmarks. 

The compiled benchmark binaries are located in the ```build/benchmarks``` directory.
```fliop``` denotes our actively secure protocol while ```semi``` is the passively secure baseline protocol.
In addition, the ```_dotp``` variants support computation of dot-products (and all scalar operations supported by the
default version). Yet, it is less optimized than the default version, so it should only be used if support for
dot-products is needed.
Run, e.g., ```./fliop -h``` from within the directory to access documentation on possible arguments.

Example for running with 2 parties and a dealer on the local machine, printing the results for party 2 and letting all parties write their results to logfiles:
```sh
# Make sure to navigate to build/benchmarks first
./fliop --num-parties 2 --depth 10 --gates-per-level 100 --localhost --compression 5 --pid 0 >> p0log.txt 2>&1 &
./fliop --num-parties 2 --depth 10 --gates-per-level 100 --localhost --compression 5 --pid 1 >> p1log.txt 2>&1 &
./fliop --num-parties 2 --depth 10 --gates-per-level 100 --localhost --compression 5 --pid 2 2>&1 | tee -a p2log.txt
```

The non-dot-product variants benchmark a circuit with a chosen number of gates per level.
The circuit contains ```depth``` many levels of multiplication gates, each followed by a level of
addition gates of the same dimension.

#### Arguments

* ```-h/--help```: Output help message, documenting all arguments
* ```-n/--num-parties [N]```: Number of parties, excluding the dealer
* ```-p/--pid [N]```: Party ID (0 for dealer, 1,...,num-parties for the online parties)
* ```--net-config [path]``` (either this or ```--localhost``` required): Path to a JSON file containing IP addresses of all parties; for an example, see [scripts/net_config.json](scripts/net_config.json)
* ```--localhost``` (either this or ```--net-config``` required): Connect all parties over loopback interface on same machine
* ```-g/--gates-per-level [N]``` (Only ```fliop``` and ```semi```): Number of multiplication/addition gates per level
* ```-d/--depth [N]``` (Only ```fliop``` and ```semi```): Number of levels of multiplication gates (after each, there also comes a level of addition gates), hence, the circuit's multiplicative depth
* ```--number [N]``` (Only ```fliop_dotp``` and ```semi_dotp```): Number of dot-products to evaluate (only one single layer)
* ```--dimension [N]``` (Only ```fliop_dotp``` and ```semi_dotp```): Dimension of the dot-products to evaluate (uniform)
* ```--compression [N]``` (Only ```fliop``` and ```fliop_dotp```) (default: 20): Compression factor for the verification protocol
* ```-t/--threads [N]``` (default: 4): Number of threads per party
* ```--pking-semi [true/false]``` (default: true): Use Pking strategy for the passively secure baseline (2 rounds, O(n) communication); otherwise, reconstruction is done via broadcast (1 round, O(n^2) communication)
* ```--pking-verify [0/1/2]``` (Only ```fliop``` and ```fliop_dotp```) (default: 1): How to use Pking strategy in the verification protocol (instead of broadcasting); 0: Never, 1: Only if reconstructing more than low constant values at once; 2: Always
* ```--port [N]``` (default: 10000): Base port for network connections
* ```-o/--output [path]``` (optional): Path to save benchmark logs to; no files written if flag not set
* ```-r/--repeat [N]``` (default: 1): Number of times to rerun the benchmark in one program execution
* ```--security-param [N]``` (default: 128): Computational security parameter; code currently only supports 128

### Tests

The compiled test binaries are located in the ```build/tests``` directory.
```fliop``` denotes our actively secure protocol while ```semi``` is the passively secure baseline protocol.
In addition, the ```_dotp``` variants support computation of dot-products.
Run, e.g., ```./fliop_test -h``` from within the directory to access documentation on possible arguments.

The tests run on hardscripted test circuits and random input data.
To run all tests, from the main directory, simply run
```sh
./test.sh [N]
```
where N is the number of test iterations for each testcase.
Different iterations sample different random input values to the circuit.

#### Arguments

* ```-h/--help```: Output help message, documenting all arguments
* ```-n/--num-parties [N]```: Number of parties, excluding the dealer
* ```-p/--pid [N]```: Party ID (0 for dealer, 1,...,num-parties for the online parties)
* ```--net-config [path]``` (either this or ```--localhost``` required): Path to a JSON file containing IP addresses of all parties; for an example, see [scripts/net_config.json](scripts/net_config.json)
* ```--localhost``` (either this or ```--net-config``` required): Connect all parties over loopback interface on same machine
* ```--compression [N]``` (Only ```fliop_test``` and ```fliop_dotp_test```) (default: 20): Compression factor for the verification protocol
* ```-t/--threads [N]``` (default: 4): Number of threads per party
* ```--pking-semi [true/false]``` (default: true): Use Pking strategy for the passively secure baseline (2 rounds, O(n) communication); otherwise, reconstruction is done via broadcast (1 round, O(n^2) communication)
* ```--pking-verify [0/1/2]``` (Only ```fliop``` and ```fliop_dotp```) (default: 1): How to use Pking strategy in the verification protocol (instead of broadcasting); 0: Never, 1: Only if reconstructing more than low constant values at once; 2: Always
* ```--port [N]``` (default: 10000): Base port for network connections
* ```-r/--repeat [N]``` (default: 1): Number of times to rerun the test with different random inputs in one program execution
* ```--security-param [N]``` (default: 128): Computational security parameter; code currently only supports 128


## :chart_with_upwards_trend: Processing Benchmark Data

To process benchmark outputs, we provide multiple scripts inside [benchmark_results](benchmark_results/).
These can be used to parse freshly reproduced raw benchmark data (see [here](#bar_chart-reproducing-our-benchmark-results) on how to generate it),
or parse the raw benchmark output that we have obtained in our paper's benchmarks.

We provide the following scripts to reproduce the tables from the paper.
Provide ```r``` as an argument to take freshly reproduced benchmark data or crash if this data is missing or incomplete.
Otherwise, the script will parse our raw benchmark data contained inside this repository.
* ```python3 table_general.py```: Table 3, benchmarks with 1 or 8 threads, depths 10/30/100 and 1M multiplications.
* ```python3 table_size.py```: Table 4, benchmarks for different numbers of multiplications.
* ```python3 table_asterisk.py```: Table 5, comparison to Asterisk. Use argument ```r-asterisk``` to use freshly reproduced data for Asterisk instead of our benchmark data.
* ```python3 table_dotp.py```: Table 6, benchmarks for dot-products.
* ```python3 table_RSS_and_SPDZ2k.py```: Table 7, comparison to RSS zk-FLIOPS and SPDZ2k. Use argument ```r-RSS``` to use freshly reproduced data for RSS instead of our benchmark data. Use argument ```r-SPDZ2k``` to use freshly reproduced data for SPDZ2k instead of our benchmark data. 

Additionally, the following scripts reproduce the plots from the paper, which are written to [benchmark_results/plots](benchmark_results/plots/).
* ```python3 plot_theoretical.py```: Figure 9, analytical comparison of verification communication.
* ```python3 plot_parties.py```: Figure 10, benchmarks for different numbers of parties. Use argument ```r``` to use freshly reproduced data instead of our benchmark data.

Finally, ```python3 analyze_memory.py``` outputs the RAM utilization of our protocol, again supporting the ```r``` argument to use freshly reproduced data.


## :page_facing_up: Repository Structure

This repository is structures as follows:

### benchmark

Targets to run benchmarks, essentially taking as input circuit sizes, from that generating circuits, setting synthetic inputs,
and executing the protocols. Further information can be found [here](#benchmarks).

### benchmark_results

Directories for the raw benchmark data utilized in our paper, next to directories with ```_reproduced```-suffix where [reproducing the benchmarks](#bar_chart-reproducing-our-benchmark-results) will write new raw data to.
Also contains scripts to parse and format the raw data, as documented [here](#processing-benchmark-data).

### related_works

[Asterisk](https://github.com/cris-coders-iisc/Asterisk) and [malicious_3pc_arithmetic](https://github.com/AntCPLab/malicious_3pc_arithmetic) are cloned
to this location by running ```./related_works_setup.sh``` for comparing our protocol to these related works. This script also applies the patches provided in that directory.
Finally, it contains a benchmark circuit for MP-SPDZ.

### scripts

Scripts and other useful configs to run benchmarks:
* ```benchmark_*.sh```: Routines to benchmark protocols after network emulation has been set up. We recommend to simply follow the steps [here](#bar_chart-reproducing-our-benchmark-results) instead of using the scripts manually.
* ```net_config.*```: Network configurations, containing the IP addresses in our emulated network, to be used as input config file to the benchmarks.
* ```network.py```: Network emulation script, see [the documentation here](#local-network-emulation).

### src

* [io](src/io/): Network communication handler.
* [protocol](src/protocol/): Core protocol implementation.
    * ```preproc```: This defines the format of preprocessing information to be stored.
    * ```sharing```: This defines the format of secret sharings.
    * ```rand_gen_pool```: This sets up and manages pre-shared keys so that parties can share PRFs with the same state.
    * :rocket: **```semi_evaluator```**: This is our implementation of the passively secure baseline MPC protocol. It essentially matches ```zkfliop_evaluator``` minus all steps used to reach active security.
    * :rocket: **```zkfliop_evaluator```**: This is our implementation of the actively secure MPC protocol. Centrally, the method ```verify()``` implements our novel sublinear verification protocol.
    * :rocket: **```*_dotp```**: The variants of the protocol that support dot-product gates. More general, but less optimized for scalar operations.
* [utils](src/utils/): Circuit representation and rings for computation and verification domains.

### test

Targets to run tests, using random inputs on fixed testing circuits, executing the protocols on them, and checking the outputs.
Further information can be found [here](#tests).

### Main directory

* ```compile.sh```: Automatically compiles all benchmarks and tests as variants with and without OpenMP.
* ```Dockerfile```: Dockerfile to run the code within a Docker container. Automatically sets up all required dependencies.
* ```parse_reproduced.sh```: Parses and formats all benchmark data after it has been reproduced. See [here](#processing-benchmark-data).
* ```quicktest.sh```: Runs a quick test to check if compilation was successful.
* ```related_works_setup.sh```: Downloads and installs the related works to compare to, so that we can reproduce their benchmarks. See [here](#bar_chart-reproducing-our-benchmark-results).
* ```related_works_reproduce.sh```: Reproduces benchmarks for related works. See [here](#bar_chart-reproducing-our-benchmark-results).
* ```reproduce.sh```: Reproduces benchmarks for our protocol. See [here](#bar_chart-reproducing-our-benchmark-results).
* ```test.sh```: Runs tests for our protocols. See [here](#tests).


## :building_construction: Extending the Code

To extend and build on top of this code, we provide the following pointers:

First, we provide a [template](test/template.cpp) for writing a custom wrapper (e.g., benchmark, test, ...) to use our protocol.
The template guides through defining a custom circuit, defining party inputs, and finally calling the MPC protocol.

Furthermore, the implementation of our verification protocol is thoroughly documented and may be used as a basis to implement similar verifications in other settings.
See [here](src/protocol/zkfliop_evaluator.h) for the details.

## No AI Use

We did **not** use any LLM-based tools or similar for this implementation.
All code, documentation, etc. was written by hand.
