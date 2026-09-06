#include <io/netmp.h>
#include <protocol/semi_evaluator_dotp.h>
#include <utils/circuit.h>
#include <omp.h>

#include <algorithm>
#include <boost/program_options.hpp>
#include <cmath>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>

// #include "utils.h"

using namespace semi;
using json = nlohmann::json;
namespace bpo = boost::program_options;

common::utils::Circuit<Ring> generateCircuit() {
    common::utils::Circuit<Ring> circ;

    /* Some arbitrary circuit using additions, multiplications, and multiplications by constant, plus some dot products

        a       b       c       d       e       f       g               h[0:500]        i[0:500]        j[0:100]
        |       |       |       |       |       |       |                   |               |               |
        x4      o-- + --o-- + --o-- x --o       o-- x --o-------o           o------ x ------o          o----o----o
        |           |   |   |       |   |       |   |   |       |                   |       |[0:100]   |         |
        o---- x ----o  x321 o-- x --o   o-- + --o   x9  o-- x --o                   |       o--- + --- o--- x ---o
              |         |       |       |   |       |       |                       |            |     |    |
        o-----o--- x ---o       o-- x --o   o-- x --o       x999999                 |            o- x -o    |
        |          |    |           |           |           |                       |               |       |
        o---- x ---o    o---- + ----o           o---- + ----o------------ x --------o               o-- + --o
        |     |               |     |                 |                   |                         |   |
        o- x -o               |     |                 x2                  |                         |   |
           |                  |     |                 |                   |                         |   |
           o----- x ----------o     |                 |                   |                         |   |
                  |           |     |                 |                   |                         |   |
                  t           u     v                 w                   x                         y   z
    */

    auto a = circ.newInputWire();
    auto b = circ.newInputWire();
    auto c = circ.newInputWire();
    auto d = circ.newInputWire();
    auto e = circ.newInputWire();
    auto f = circ.newInputWire();
    auto g = circ.newInputWire();
    std::vector<common::utils::wire_t> h(500);
    std::generate(h.begin(), h.end(), [&]() { return circ.newInputWire(); });
    std::vector<common::utils::wire_t> i(500);
    std::generate(i.begin(), i.end(), [&]() { return circ.newInputWire(); });
    std::vector<common::utils::wire_t> j(100);
    std::generate(j.begin(), j.end(), [&]() { return circ.newInputWire(); });

    auto a4 = circ.addConstOpGate(common::utils::GateType::kConstMul, a, 4);
    auto bpc = circ.addGate(common::utils::GateType::kAdd, b, c);
    auto cpd = circ.addGate(common::utils::GateType::kAdd, c, d);
    auto de = circ.addGate(common::utils::GateType::kMul, d, e);
    auto fg = circ.addGate(common::utils::GateType::kMul, f, g);
    auto hi = circ.addGate(common::utils::GateType::kDotprod, h, i);

    auto a4bpc = circ.addGate(common::utils::GateType::kMul, a4, bpc);
    auto c321 = circ.addConstOpGate(common::utils::GateType::kConstMul, a, 321);
    auto cpdde = circ.addGate(common::utils::GateType::kMul, cpd, de);
    auto epf = circ.addGate(common::utils::GateType::kAdd, e, f);
    auto fg9 = circ.addConstOpGate(common::utils::GateType::kConstMul, fg, 9);
    auto gg = circ.addGate(common::utils::GateType::kMul, g, g);
    std::vector<common::utils::wire_t> ipj(100);
    for (size_t index = 0; index < 100; index++) // add component-wise
        ipj[index] = circ.addGate(common::utils::GateType::kAdd, i[index], j[index]);
    auto jj = circ.addGate(common::utils::GateType::kDotprod, j, j);

    auto a4bpcc321 = circ.addGate(common::utils::GateType::kMul, a4bpc, c321);
    auto cpddee = circ.addGate(common::utils::GateType::kMul, cpdde, e);
    auto epffg9 = circ.addGate(common::utils::GateType::kMul, epf, fg9);
    auto gg999999 = circ.addConstOpGate(common::utils::GateType::kConstMul, gg, 999999);
    auto ipjj = circ.addGate(common::utils::GateType::kDotprod, ipj, j);

    auto a4bpca4bpcc321 = circ.addGate(common::utils::GateType::kMul, a4bpc, a4bpcc321);
    auto a4bpcc321cpddee = circ.addGate(common::utils::GateType::kAdd, a4bpcc321, cpddee);
    auto epffg9pgg999999 = circ.addGate(common::utils::GateType::kAdd, epffg9, gg999999);
    auto gg999999hi = circ.addGate(common::utils::GateType::kMul, gg999999, hi);
    auto ipjjpjj = circ.addGate(common::utils::GateType::kAdd, ipjj, jj);

    auto a4bpca4bpca4bpcc321 = circ.addGate(common::utils::GateType::kMul, a4bpc, a4bpca4bpcc321);
    auto epffg9pgg9999992 = circ.addConstOpGate(common::utils::GateType::kConstMul, epffg9pgg999999, 2);

    auto a4bpca4bpca4bpcc321a4bpcc321cpddee = circ.addGate(common::utils::GateType::kMul, a4bpca4bpca4bpcc321, a4bpcc321cpddee);

    circ.setAsOutput(a4bpca4bpca4bpcc321a4bpcc321cpddee);
    circ.setAsOutput(a4bpcc321cpddee);
    circ.setAsOutput(cpddee);
    circ.setAsOutput(epffg9pgg9999992);
    circ.setAsOutput(gg999999hi);
    circ.setAsOutput(ipjj);
    circ.setAsOutput(ipjjpjj);

    return circ;
}

void assert_equal(Ring x, Ring y) {
    if (x != y) {
        std::cout << std::endl;
        std::cout << x << " != " << y << std::endl;
        throw std::runtime_error("assert_equal mismatch detected!");
    }
}

void test(const bpo::variables_map& opts) {
    auto nP = opts["num-parties"].as<size_t>();
    auto pid = opts["pid"].as<size_t>();
    auto threads = opts["threads"].as<size_t>();
    auto repeat = opts["repeat"].as<size_t>();
    auto port = opts["port"].as<int>();
    auto pking_semi = opts["pking-semi"].as<bool>();

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
        std::array<char*, 128> ip{}; // maximum of 128 parties supported
        for (size_t i = 0; i < nP+1; ++i) {
            ipaddress[i] = netdata[i].get<std::string>();
            ip[i] = ipaddress[i].data();
        }

        network = std::make_shared<io::NetIOMP>(pid, nP+1, port, ip.data(), false);
    }

    omp_set_num_threads(threads);
    assert((int) threads == omp_get_max_threads());

    auto circ = generateCircuit().strictlyOrderGatesByLevel();
    std::cout << "--- Circuit ---\n";
    std::cout << circ << std::endl;

    std::unordered_map<common::utils::wire_t, int> input_pid_map;
    std::unordered_map<common::utils::wire_t, Ring> input_map;
    size_t counter = 0;
    for (const auto& g : circ.gates_by_level[0]) {
        if (g->type == common::utils::GateType::kInp) {
            input_pid_map[g->out] = 1 + (counter % nP); // It rotates around which party provides which input
            counter++;
        }
    }
    
    emp::PRG input_prg;
    emp::block seed_block = emp::makeBlock(0, 0); // make tests deterministic
    input_prg.reseed(&seed_block, 0);
    for (size_t run = 0; run < repeat; run++) {
        std::cout << "semi_dotp.cpp test run " << run << "..." << std::endl;

        // Random inputs
        for (const auto& g : circ.gates_by_level[0]) {
            if (g->type == common::utils::GateType::kInp) {
                Ring input;
                input_prg.random_data(&input, sizeof(Ring));
                input_map[g->out] = input; // Only input party will actually read this, but do for
                                           // all so that each party later knows all inputs to be
                                           // able to run the test assertions.
            }
        }

        DotPEvaluator eval(nP, pid, network, circ, pking_semi);

        network->sync();
        eval.setupOutputPhase();
        eval.runSetup(input_pid_map);
        network->sync();
        eval.setInputs(input_pid_map, input_map);
        eval.evaluateCircuit();
        auto res = eval.getOutputs();
        network->sync();
        
        if (pid > 0) { // As the dealer learns no outputs
            Ring a, b, c, d, e, f, g;
            std::vector<Ring> h(500);
            std::vector<Ring> i(500);
            std::vector<Ring> j(100);
            size_t counter = 0;
            for (const auto& gt : circ.gates_by_level[0]) {
                if (gt->type == common::utils::GateType::kInp) {
                    switch (counter) {
                        case 0: {a = input_map[gt->out]; break;}
                        case 1: {b = input_map[gt->out]; break;}
                        case 2: {c = input_map[gt->out]; break;}
                        case 3: {d = input_map[gt->out]; break;}
                        case 4: {e = input_map[gt->out]; break;}
                        case 5: {f = input_map[gt->out]; break;}
                        case 6: {g = input_map[gt->out]; break;}
                        default: {
                            size_t cc = counter - 7;
                            if (cc < 500) {
                                h[cc] = input_map[gt->out];
                            } else if (cc < 1000) {
                                i[cc - 500] = input_map[gt->out];
                            } else if (cc < 1100) {
                                j[cc - 1000] = input_map[gt->out];
                            } else throw std::runtime_error("unexpected number of input gates");
                        }
                    }
                    counter++;
                }
            }

            Ring a4 = a * 4;
            Ring bpc = b + c;
            Ring cpd = c + d;
            Ring de = d * e;
            Ring fg = f * g;
            Ring hi = 0;
            for (size_t index = 0; index < 500; index++) hi += h[index] * i[index];

            Ring a4bpc = a4 * bpc;
            Ring c321 = a * 321;
            Ring cpdde = cpd * de;
            Ring epf = e + f;
            Ring fg9 = fg * 9;
            Ring gg = g * g;
            std::vector<Ring> ipj(100);
            for (size_t index = 0; index < 100; index++) ipj[index] = i[index] + j[index];
            Ring jj = 0;
            for (size_t index = 0; index < 100; index++) jj += j[index] * j[index];

            Ring a4bpcc321 = a4bpc * c321;
            Ring cpddee = cpdde * e;
            Ring epffg9 = epf * fg9;
            Ring gg999999 = gg * 999999;
            Ring ipjj = 0;
            for (size_t index = 0; index < 100; index++) ipjj += ipj[index] * j[index];

            Ring a4bpca4bpcc321 = a4bpc * a4bpcc321;
            Ring a4bpcc321cpddee = a4bpcc321 + cpddee;
            Ring epffg9pgg999999 = epffg9 + gg999999;
            Ring gg999999hi = gg999999 * hi;
            Ring ipjjpjj = ipjj + jj;

            Ring a4bpca4bpca4bpcc321 = a4bpc * a4bpca4bpcc321;
            Ring epffg9pgg9999992 = epffg9pgg999999 * 2;

            Ring a4bpca4bpca4bpcc321a4bpcc321cpddee = a4bpca4bpca4bpcc321 * a4bpcc321cpddee;

            assert_equal(res[0], a4bpca4bpca4bpcc321a4bpcc321cpddee);
            assert_equal(res[1], a4bpcc321cpddee);
            assert_equal(res[2], cpddee);
            assert_equal(res[3], epffg9pgg9999992);
            assert_equal(res[4], gg999999hi);
            assert_equal(res[5], ipjj);
            assert_equal(res[6], ipjjpjj);

            std::cout << "Test succeeded!" << std::endl;
        }
    }
}

// clang-format off
bpo::options_description programOptions() {
    bpo::options_description desc("Options");
    desc.add_options()
        ("num-parties,n", bpo::value<size_t>()->required(), "Number of parties.")
        ("pid,p", bpo::value<size_t>()->required(), "Party ID.")
        ("security-param", bpo::value<size_t>()->default_value(128), "Security parameter in bits.")
        ("compression", bpo::value<size_t>()->default_value(20), "(ignored)")
        ("threads,t", bpo::value<size_t>()->default_value(4), "Number of threads (recommended 4).")
        ("net-config", bpo::value<std::string>(), "Path to JSON file containing network details of all parties.")
        ("localhost", bpo::bool_switch(), "All parties are on same machine.")
        ("port", bpo::value<int>()->default_value(10000), "Base port for networking.")
        ("repeat,r", bpo::value<size_t>()->default_value(1), "Number of times to run benchmarks.")
        ("pking-semi", bpo::value<bool>()->default_value(true), "Use Pking strategy in semi-honest base protocol. Otherwise, reconstruction will use broadcasting")
        ("pking-verify", bpo::value<int>()->default_value(1), "(ignored)");

  return desc;
}
// clang-format on

int main(int argc, char* argv[]) {
    auto prog_opts(programOptions());

    bpo::options_description cmdline("Test the protocol on a hardscripted test circuit.");
    cmdline.add(prog_opts);
    cmdline.add_options()("help,h", "produce help message");

    bpo::variables_map opts;
    bpo::store(bpo::command_line_parser(argc, argv).options(cmdline).run(), opts);

    if (opts.count("help") != 0) {
        std::cout << cmdline << std::endl;
        return 0;
    }

    try {
        bpo::notify(opts);

        if (!opts["localhost"].as<bool>() && (opts.count("net-config") == 0)) {
            throw std::runtime_error("Expected one of 'localhost' or 'net-config'");
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    try {
        test(opts);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\nFatal error" << std::endl;
        return 1;
    }

    return 0;
}
