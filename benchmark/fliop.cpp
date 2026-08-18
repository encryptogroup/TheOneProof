#include <io/netmp.h>
#include <protocol/zkfliop_evaluator.h>
#include <utils/circuit.h>
#include <omp.h>

#include <algorithm>
#include <boost/program_options.hpp>
#include <cmath>
#include <iostream>
#include <memory>

#include "utils.h"

using namespace zkfliop;
using json = nlohmann::json;
namespace bpo = boost::program_options;


common::utils::Circuit<Ring> generateCircuit(size_t gates_per_level, size_t depth) {
    common::utils::Circuit<Ring> circ;

    std::vector<common::utils::wire_t> level_inputs(gates_per_level);
    std::generate(level_inputs.begin(), level_inputs.end(),
                [&]() { return circ.newInputWire(); });
    for (size_t i = 0; i < gates_per_level; ++i) {
        circ.setAsOutput(level_inputs[i]);
    }

    for (size_t d = 0; d < depth; ++d) {
        std::vector<common::utils::wire_t> level_inter(gates_per_level);
        std::vector<common::utils::wire_t> level_outputs(gates_per_level);

        for (size_t i = 0; i < gates_per_level - 1; ++i) {
            level_inter[i] = circ.addGate(common::utils::GateType::kMul, level_inputs[i],
                                      level_inputs[i + 1]);
        }
        level_inter[gates_per_level - 1] =
            circ.addGate(common::utils::GateType::kMul, level_inputs[gates_per_level - 1],
                     level_inputs[0]);

        for (size_t i = 0; i < gates_per_level - 1; ++i) {
            level_outputs[i] = circ.addGate(common::utils::GateType::kAdd, level_inter[i],
                                      level_inter[i + 1]);
        }
        level_outputs[gates_per_level - 1] =
            circ.addGate(common::utils::GateType::kAdd, level_inter[gates_per_level - 1],
                     level_inter[0]);
        
        level_inputs = std::move(level_outputs);
    }
    for (auto i : level_inputs) {
        circ.setAsOutput(i);
    }

    return circ;
}

void benchmark(const bpo::variables_map& opts) {
    bool save_output = false;
    std::string save_file;
    if (opts.count("output") != 0) {
        save_output = true;
        save_file = opts["output"].as<std::string>();
    }

    auto gates_per_level = opts["gates-per-level"].as<size_t>();
    auto depth = opts["depth"].as<size_t>();
    auto nP = opts["num-parties"].as<size_t>();
    auto pid = opts["pid"].as<size_t>();
    auto security_param = opts["security-param"].as<size_t>();
    assert(security_param == 128);
    auto threads = opts["threads"].as<size_t>();
    auto repeat = opts["repeat"].as<size_t>();
    auto port = opts["port"].as<int>();
    auto compression_factor = opts["compression"].as<size_t>();
    auto pking_semi = opts["pking-semi"].as<bool>();
    auto pking_verify = opts["pking-verify"].as<int>();
    if (pking_verify < 0 || pking_verify > 2) {
        throw std::runtime_error("Invalid value for pking-verify");
    }

    std::shared_ptr<io::NetIOMP> network = nullptr;
    if (opts["localhost"].as<bool>()) {
        network = std::make_shared<io::NetIOMP>(pid, nP+1, port, nullptr, true);
    }
    else {
        std::ifstream fnet(opts["net-config"].as<std::string>());
        if (!fnet.good()) {
        fnet.close();
        throw std::runtime_error("Could not open network config file");
        }
        json netdata;
        fnet >> netdata;
        fnet.close();

        std::vector<std::string> ipaddress(nP+1);
        std::array<char*, 5> ip{};
        for (size_t i = 0; i < nP+1; ++i) {
            ipaddress[i] = netdata[i].get<std::string>();
            ip[i] = ipaddress[i].data();
        }

        network = std::make_shared<io::NetIOMP>(pid, nP+1, port, ip.data(), false);
    }

    omp_set_num_threads(threads);
    assert((int) threads == omp_get_max_threads());

    json output_data;
    output_data["details"] = {{"gates_per_level", gates_per_level},
                                {"depth", depth},
                                {"num-parties", nP},
                                {"pid", pid},
                                {"comp_security_param", security_param},
                                {"stat_security_param", SSEC},
                                {"compression_factor", compression_factor},
                                {"threads", threads},
                                {"repeat", repeat},
                                {"n_bits", 8 * sizeof(Ring)},
                                {"n_bits_extended", 8 * sizeof(LargeRing)},
                                {"pking-semi", pking_semi},
                                {"pking-verify", pking_verify}};
    output_data["benchmarks_setup"] = json::array();
    output_data["benchmarks"] = json::array();
    output_data["proof_rounds"] = json::array();

    std::cout << "--- Details ---\n";
    for (const auto& [key, value] : output_data["details"].items()) {
        std::cout << key << ": " << value << "\n";
    }
    std::cout << std::endl;

    // Check Theorem 1 conditions
    std::cout << "Check if conditions for Theorem 1 are satisfied" << std::endl;
    int T = 2 * ceil(log(3 * gates_per_level * depth) / log(compression_factor)) + 1;
    if (T > SSEC) {
        std::cout << "Issue: T = " << T << " > SSEC = " << SSEC << std::endl;
        throw std::runtime_error("Protocol insecure with the given arguments. Decrease the number of multiplications or increase the compression factor.");
    }
    int min_s = 3 * T;
    int temp = ceil(SSEC + T * (0.5 + log(2.5 + 3.0 * SSEC / T) / log(2)));
    if (temp > min_s) min_s = temp;
    std::cout << "Protocol security requires that s >= " << min_s << std::endl;
    int provided_s = (sizeof(LargeRing) - sizeof(Ring)) * 8;
    std::cout << "Computing on " << sizeof(Ring) * 8 << " bit integers, verifying over " << sizeof(LargeRing) * 8 << " bit integers" << std::endl;
    std::cout << "==> actual s = " << provided_s << std::endl;
    if (provided_s < min_s)
        throw std::runtime_error("Protocol insecure with the given arguments. Decrease the number of multiplications, increase the compression factor, or verify on a larger ring.");
    std::cout << "==> Protocol is secure for the provided arguments!" << std::endl;
    std::cout << std::endl;

    auto circ = generateCircuit(gates_per_level, depth).strictlyOrderGatesByLevel();
    std::cout << "--- Circuit ---\n";
    std::cout << circ << std::endl;

    std::unordered_map<common::utils::wire_t, int> input_pid_map;
    std::unordered_map<common::utils::wire_t, Ring> input_map;
    Ring c_val = 0;
    for (const auto& g : circ.gates_by_level[0]) {
        if (g->type == common::utils::GateType::kInp) {
            input_pid_map[g->out] = 1;
            input_map[g->out] = c_val;
            c_val++;
        }
    }
    

    for (size_t r = 0; r < repeat; ++r) {
        Evaluator eval(nP, pid, network, circ, compression_factor, pking_semi, pking_verify);
        
        /*
        We are interested in benchmarking the circuit evaluation, which excludes the MPC input and
        output phases. This is as our work focuses on making the evaluation efficient, and as
        the numbers of inputs and outputs is highly application-dependent, whereas by benchmarking
        only the evaluation, we get a more general statement, e.g., on the efficiency of running
        one million multiplications. TODO add pointer paper

        Hence, some operations here (for inputs and outputs) are used outside of the code segments
        which are benchmarked.
        */

        eval.setupOutputPhase_A(); // Setup for output phase, excluded from benchmark

        network->sync();

        StatsPoint start_setup(*network);
        eval.runSetup(input_pid_map);
        StatsPoint end_setup(*network);
        auto rbench_setup = end_setup - start_setup;
        output_data["benchmarks_setup"].push_back(rbench_setup);
        
        eval.setupOutputPhase_B(); // Setup for output phase, excluded from benchmark
        eval.setInputs(input_pid_map, input_map); // Input phase, excluded from benchmark

        network->sync();

        StatsPoint start(*network);
        eval.evaluateCircuit();
        StatsPoint end(*network);
        auto rbench = end - start;
        output_data["benchmarks"].push_back(rbench);
        size_t proof_rounds = eval.getProofRounds();
        output_data["proof_rounds"].push_back(proof_rounds);

        auto res = eval.getOutputs(); // Output phase, excluded from benchmark
        
        if (pid > 0) {

            std::vector<Ring> expected;
            for (size_t i = 0; i < gates_per_level; i++) {
                expected.push_back(i);
            }
            for (size_t l = 0; l < depth; l++) {
                Ring first = expected[0];
                for (size_t i = 0; i < gates_per_level - 1; i++) {
                    expected[i] = expected[i] * expected[i + 1];
                }
                expected[gates_per_level - 1] = expected[gates_per_level - 1] * first;

                first = expected[0];
                for (size_t i = 0; i < gates_per_level - 1; i++) {
                    expected[i] = expected[i] + expected[i + 1];
                }
                expected[gates_per_level - 1] = expected[gates_per_level - 1] + first;
            }

            for (size_t i = 0; i < gates_per_level; i++) {
                assert(res[i] == i);
            }
            for (size_t i = gates_per_level; i < 2 * gates_per_level; i++) {
                assert(res[i] == expected[i - gates_per_level]);
            }
        }

        size_t bytes_sent = 0;
        for (const auto& val : rbench["communication"]) {
            bytes_sent += val.get<int64_t>();
        }
        size_t bytes_sent_setup = 0;
        for (const auto& val : rbench_setup["communication"]) {
            bytes_sent_setup += val.get<int64_t>();
        }

        std::cout << "--- Repetition " << r + 1 << " ---\n";
        std::cout << "setup time: " << rbench_setup["time"] << " ms\n";
        std::cout << "setup sent: " << bytes_sent_setup << " bytes\n";
        std::cout << "time: " << rbench["time"] << " ms\n";
        std::cout << "sent: " << bytes_sent << " bytes\n";
        std::cout << "verification rounds: " << proof_rounds << std::endl;

        std::cout << std::endl;
    }
    output_data["stats"] = {{"peak_virtual_memory", peakVirtualMemory()},
                            {"peak_resident_set_size", peakResidentSetSize()}};

    std::cout << "--- Statistics ---\n";
    for (const auto& [key, value] : output_data["stats"].items()) {
        std::cout << key << ": " << value << "\n";
    }
    std::cout << std::endl;

    if (save_output) {
        saveJson(output_data, save_file);
    }
}

// clang-format off
bpo::options_description programOptions() {
    bpo::options_description desc("Following options are supported by config file too.");
    desc.add_options()
        ("gates-per-level,g", bpo::value<size_t>()->required(), "Number of gates at each level.")
        ("depth,d", bpo::value<size_t>()->required(), "Multiplicative depth of circuit.")
        ("num-parties,n", bpo::value<size_t>()->required(), "Number of parties.")
        ("pid,p", bpo::value<size_t>()->required(), "Party ID.")
        ("security-param", bpo::value<size_t>()->default_value(128), "Security parameter in bits, code only supports 128!.")
        ("compression", bpo::value<size_t>()->default_value(2), "Compression factor.")
        ("threads,t", bpo::value<size_t>()->default_value(4), "Number of threads (recommended 4).")
        ("net-config", bpo::value<std::string>(), "Path to JSON file containing network details of all parties.")
        ("localhost", bpo::bool_switch(), "All parties are on same machine.")
        ("port", bpo::value<int>()->default_value(10000), "Base port for networking.")
        ("output,o", bpo::value<std::string>(), "File to save benchmarks.")
        ("repeat,r", bpo::value<size_t>()->default_value(1), "Number of times to run benchmarks.")
        ("pking-semi", bpo::value<bool>()->default_value(true), "Use Pking strategy in semi-honest base protocol. Otherwise, reconstruction will use broadcasting")
        ("pking-verify", bpo::value<int>()->default_value(1), "Use Pking strategy in verification. Otherwise, reconstruction will use broadcasting. 0: Never; 1: Only if more than low constant values at once; 2: Always");

  return desc;
}
// clang-format on

int main(int argc, char* argv[]) {
    //ZZ_p::init(conv<ZZ>("17816577890427308801"));
    auto prog_opts(programOptions());

    bpo::options_description cmdline(
      "Benchmark online phase for multiplication gates.");
    cmdline.add(prog_opts);
    cmdline.add_options()(
      "config,c", bpo::value<std::string>(),
      "configuration file for easy specification of cmd line arguments")(
      "help,h", "produce help message");

    bpo::variables_map opts;
    bpo::store(bpo::command_line_parser(argc, argv).options(cmdline).run(), opts);

    if (opts.count("help") != 0) {
        std::cout << cmdline << std::endl;
        return 0;
    }

    if (opts.count("config") > 0) {
        std::string cpath(opts["config"].as<std::string>());
        std::ifstream fin(cpath.c_str());

        if (fin.fail()) {
            std::cerr << "Could not open configuration file at " << cpath << "\n";
            return 1;
        }

        bpo::store(bpo::parse_config_file(fin, prog_opts), opts);
    }

    try {
        bpo::notify(opts);

        // Check if output file already exists.
        /*if (opts.count("output") != 0) {
            std::ifstream ftemp(opts["output"].as<std::string>());
            if (ftemp.good()) {
                ftemp.close();
                throw std::runtime_error("Output file aready exists.");
            }
            ftemp.close();
        }*/

        if (!opts["localhost"].as<bool>() && (opts.count("net-config") == 0)) {
            throw std::runtime_error("Expected one of 'localhost' or 'net-config'");
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    try {
        benchmark(opts);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\nFatal error" << std::endl;
        return 1;
    }

    return 0;
}
