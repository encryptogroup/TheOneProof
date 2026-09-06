#include <io/netmp.h>
#include <protocol/zkfliop_evaluator.h> // import the protocol to use
#include <utils/circuit.h>
#include <omp.h>

#include <algorithm>
#include <boost/program_options.hpp>
#include <cmath>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>

using namespace zkfliop; // or semi for passively-secure baseline
using json = nlohmann::json;
namespace bpo = boost::program_options;

common::utils::Circuit<Ring> generateCircuit() {
    // Construct new, empty circuit
    common::utils::Circuit<Ring> circ;

    /*
    Say we have inputs a, b, c.
    We want compute:
    d = a + b
    e = a * c
    f = a * a
    g = d * f
    h = 10 * f
    i = d + h
    Then, we output e, g, i.

    (this is equivalent to a circuit, I will spare you the ASCII art circuit here...)

    !! Feel free to replace the circuit by your own custom circuit !!
    input_pid_map and input_map in run_protocol need to be adapted accordingly if the number of inputs is changed.
    */

    // Define input wires:
    auto a = circ.newInputWire();
    auto b = circ.newInputWire();
    auto c = circ.newInputWire();

    // Define intermediate wires as outputs of corresponding gates
    auto d = circ.addGate(common::utils::GateType::kAdd, a, b); // a + b
    auto e = circ.addGate(common::utils::GateType::kMul, a, c); // a * c
    auto f = circ.addGate(common::utils::GateType::kMul, a, a); // a * a
    auto g = circ.addGate(common::utils::GateType::kMul, d, f); // d * f
    auto h = circ.addConstOpGate(common::utils::GateType::kConstMul, f, 10); // 10 * f
    auto i = circ.addGate(common::utils::GateType::kAdd, d, h); // d + h

    // Declare what is an output wire
    circ.setAsOutput(e);
    circ.setAsOutput(g);
    circ.setAsOutput(i);

    return circ;
}

void run_protocol(const bpo::variables_map& opts) {
    // Own party ID from CLI args
    auto pid = opts["pid"].as<size_t>();
    // Set up network
    std::shared_ptr<io::NetIOMP> network = nullptr;
    if (opts["localhost"].as<bool>()) {
        network = std::make_shared<io::NetIOMP>(pid, 3+1, 10000, nullptr, true); // 3+1 parties, base port 10000
    } else {
        std::ifstream fnet(opts["net-config"].as<std::string>());
        if (!fnet.good()) {
        fnet.close();
        throw std::runtime_error("Could not open network config file");
        }
        json netdata;
        fnet >> netdata;
        fnet.close();

        std::vector<std::string> ipaddress(3+1); // 3+1 parties
        std::array<char*, 4> ip{};
        for (size_t i = 0; i < 3+1; ++i) {
            ipaddress[i] = netdata[i].get<std::string>();
            ip[i] = ipaddress[i].data();
        }

        network = std::make_shared<io::NetIOMP>(pid, 3+1, 10000, ip.data(), false); // 3+1 parties, base port 10000
    }

    // Generate the circuit, automatically assign gates to layers.
    auto circ = generateCircuit().strictlyOrderGatesByLevel();

    // Extract input wires from circuit representation
    std::vector<wire_t> input_wires;
    for (const auto& g : circ.gates_by_level[0]) { // All inputs on level 0
        if (g->type == common::utils::GateType::kInp) input_wires.push_back(g->out);
    }

    // Declare which party provides which input (we assume 3 parties + a dealer here)
    std::unordered_map<common::utils::wire_t, int> input_pid_map;
    input_pid_map[input_wires[0]] = 1; // P1 provides input a
    input_pid_map[input_wires[1]] = 3; // P3 provides input b
    input_pid_map[input_wires[2]] = 2; // P2 provides input c

    // Define inputs (of course, normally the code would not be able to know all parties' inputs)
    std::unordered_map<common::utils::wire_t, Ring> input_map;
    if (pid == 1) input_map[input_wires[0]] = 42; else input_map[input_wires[0]] = 0; // Only P1 has this input, set to 0 for others
    if (pid == 2) input_map[input_wires[2]] = 1337; else input_map[input_wires[2]] = 0; // Only P2 has this input, set to 0 for others
    if (pid == 3) input_map[input_wires[1]] = 100; else input_map[input_wires[1]] = 0; // Only P3 has this input, set to 0 for others

    // Create MPC protocol evaluator, 3 parties, compression factor 2, using Pking for the circuit evaluation, using Pking for verification except for low number of reconstructed values.
    Evaluator eval(3, pid, network, circ, 2, true, 1);
    network->sync(); // All parties synchronize
    // Run setup (this must consist of these three methods in the given order):
    eval.setupOutputPhase_A();
    eval.runSetup(input_pid_map);
    eval.setupOutputPhase_B();
    network->sync(); // All parties synchronize
    // Provide inputs
    eval.setInputs(input_pid_map, input_map);
    // Evaluate the circuit
    eval.evaluateCircuit();
    // Reconstruct the outputs to all parties
    auto res = eval.getOutputs();

    if (pid > 0) { // As the dealer learns no outputs
        for (auto r : res) {
            std::cout << "Output: " << r << std::endl;
        }
    }
}

bpo::options_description programOptions() {
    // Read party ID and network configuration from CLI arguments
    bpo::options_description desc("Options");
    desc.add_options()
        ("pid,p", bpo::value<size_t>()->required(), "Party ID.")
        ("net-config", bpo::value<std::string>(), "Path to JSON file containing network details of all parties.")
        ("localhost", bpo::bool_switch(), "All parties are on same machine.");

  return desc;
}

int main(int argc, char* argv[]) {
    // CLI argument handling:
    auto prog_opts(programOptions());
    bpo::options_description cmdline("Template executable");
    cmdline.add(prog_opts);
    cmdline.add_options()("help,h", "produce help message");
    bpo::variables_map opts;
    bpo::store(bpo::command_line_parser(argc, argv).options(cmdline).run(), opts);
    if (opts.count("help") != 0) {
        std::cout << cmdline << std::endl;
        return 0;
    }
    // Check that --localhost or --net-config is set
    try {
        bpo::notify(opts);
        if (!opts["localhost"].as<bool>() && (opts.count("net-config") == 0)) {
            throw std::runtime_error("Expected one of 'localhost' or 'net-config'");
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    // Run the protocol
    try {
        run_protocol(opts);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\nFatal error" << std::endl;
        return 1;
    }

    return 0;
}
