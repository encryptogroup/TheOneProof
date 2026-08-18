#include "zkfliop_evaluator.h"

#include <array>
#include <numeric>

namespace zkfliop
{
    Evaluator::Evaluator(int nP, int id, std::shared_ptr<io::NetIOMP> network,
                                     common::utils::StrictLevelOrderedCircuit circ,
                                     size_t compression_factor,
                                     bool pking_semi, int pking_verify)
        : nP_(nP),
          id_(id),
          rgen_(id, nP, network),
          network_(std::move(network)),
          circ_(std::move(circ)),
          wires_(circ.num_gates),
          setup_gates_(circ.num_gates),
          number_unchecked_openings_(0),
          commits(),
          output_commit(),
          randomizers_ring(),
          randomizers_large_ring(),
          output_randomizers_ring(),
          number_unchecked_consistency_(0),
          proof_rounds_(0),
          compression_factor_(compression_factor),
          pking_semi_(pking_semi),
          pking_verify_(pking_verify),
          pairwise_consistency_mismatch_(false),
          opening_commit_mismatch_(false) {}

    /// Just for convenience: Generates one random Ring element from the provided PRG
    Ring getRandomElem(emp::PRG &prg) {
        Ring out;
        prg.random_data(&out, sizeof(Ring));
        return out;
    }

    /// Just for convenience: Generates one random LargeRing element from the provided PRG
    LargeRing getRandomLargeElem(emp::PRG &prg) {
        LargeRing out;
        prg.random_data(&out, sizeof(LargeRing));
        return out;
    }

    /// Just for convenience: Generates one random (PRG) Seed from the provided PRG
    Seed getRandomSeed(emp::PRG &prg) {
        Seed out;
        prg.random_data(&out, sizeof(Seed));
        return out;
    }

    void Evaluator::runSetup(const std::unordered_map<common::utils::wire_t, int> &input_mapping)
    {
        /*
        Parallelization approach here:

        ==> We do not actually parallelize, as we empirically noticed that parallelization will not
        make the setup faster, but even slower in some cases.

        If one would still want to parallelize:
        We do not want to parallelize over gates because the setup for interactive gates
        uses PRG calls that we have to keep in order.
        Instead, we parallelize PRG calls to different PRGs done by the dealer as this
        party is the bottleneck.
        For non-interactive layers, we still parallelize over them.
        */

        std::vector<Ring> gamma_to_send; /// Gamma shares to send to the last party.
        size_t message_size = 0;

        for (size_t depth = 0; depth < circ_.gates_by_level.size(); ++depth)
        {
            if (circ_.level_mults[depth] == 0 && depth > 0) { // Only noninteractive gates
                // Safe to parallelize because non-interactive gates do not utilize the PRGs
                // require depth > 0 because input gates also use PRGs (they are somehwat
                // semi-interactively in a sense that while they are interactive, their 
                // interaction is handled outside the normal circuit evaluation later).

                // #pragma omp parallel for // If parallelization is desired here, completely disabled as documented above
                for (size_t i = 0; i < circ_.gates_by_level[depth].size(); i++) {
                    auto &gate = circ_.gates_by_level[depth][i];
                    switch (gate->type) {
                        case common::utils::GateType::kAdd: {
                            // For each addition gate, we just locally add the masks of both input wires:
                            auto *g = static_cast<common::utils::FIn2Gate *>(gate.get());
                            wires_[g->out].getLambda() = wires_[g->in1].getLambda() + wires_[g->in2].getLambda();
                            if (id_ == 0) { // Also carry along all individual shares for proof later
                                auto *full_lambda_1 = static_cast<rzkf::DealerAllShares<Ring, LargeRing> *>(setup_gates_[g->in1].get());
                                auto *full_lambda_2 = static_cast<rzkf::DealerAllShares<Ring, LargeRing> *>(setup_gates_[g->in2].get());
                                std::vector<Ring> lambdaShares(nP_ + 1);
                                for (size_t i = 1; i <= nP_; i++) {
                                    lambdaShares[i] = full_lambda_1->lambda_shares[i] + full_lambda_2->lambda_shares[i];
                                }
                                setup_gates_[g->out] = std::make_unique<rzkf::DealerAllShares<Ring, LargeRing>>(lambdaShares);
                            }
                            break;
                        }
                        case common::utils::GateType::kConstMul: {
                            // For each constant multiplication gate, we just locally multiply the input wire's mask with the constant
                            auto *g = static_cast<common::utils::ConstOpGate<Ring> *>(gate.get());
                            wires_[g->out].getLambda() = wires_[g->in].getLambda() * g->cval;
                            if (id_ == 0) { // Also carry along all individual shares for proof later
                                auto *full_lambda = static_cast<rzkf::DealerAllShares<Ring, LargeRing> *>(setup_gates_[g->in].get());
                                std::vector<Ring> lambdaShares(nP_ + 1);
                                for (size_t i = 1; i <= nP_; i++) {
                                    lambdaShares[i] = full_lambda->lambda_shares[i] * g->cval;
                                }
                                setup_gates_[g->out] = std::make_unique<rzkf::DealerAllShares<Ring, LargeRing>>(lambdaShares);
                            }
                            break;
                        }
                        case common::utils::GateType::kInp: // interactive
                        case common::utils::GateType::kMul: // interactive
                        {
                            std::cout << gate->type << std::endl;
                            throw std::runtime_error("Interactive gate not allowed here in parallelized setup context.");
                        }
                        default: {
                            std::cout << gate->type << std::endl;
                            throw std::runtime_error("Unsupported GateType.");
                        }
                    }
                }
            } else { // only interactive gates (we do the layering so that each layer is fully noninteractive or fully interactive)
                // Interactive gates, not to be processed in parallel
                for (size_t i = 0; i < circ_.gates_by_level[depth].size(); i++) {
                    auto &gate = circ_.gates_by_level[depth][i];
                    switch (gate->type) {
                        case common::utils::GateType::kInp: {
                            /*
                            Say, party Pi provides the input, i.e., input_mapping[input_wire] == i.
                            Then, lambda^i is sampled as usual with a key known to D and Pi.
                            Meanwhile, all other lambda^j are set to 0.
                            This only works as we rerandomize masks on opening values.
                            */
                            size_t i = input_mapping.at(gate->out);
                            if (id_ == 0) {
                                Ring fullLambda = 0;
                                std::vector<Ring> lambdaShares(nP_ + 1);
                                for (size_t j = 1; j <= nP_; j++) {
                                    Ring x;
                                    if (i == j) {
                                        x = getRandomElem(rgen_.as_D_with_i(i));
                                    } else {
                                        x = 0;
                                    }
                                    fullLambda += x;
                                    lambdaShares[j] = x;
                                }
                                wires_[gate->out].getLambda() = fullLambda;
                                // Dealer needs to carry this mask along the circuit later for the proof
                                setup_gates_[gate->out] = std::make_unique<rzkf::DealerAllShares<Ring, LargeRing>>  (lambdaShares);
                            } else if (id_ == i) {
                                Ring lambda = getRandomElem(rgen_.me_and_D());
                                wires_[gate->out].getLambda() = lambda;
                                setup_gates_[gate->out] = std::make_unique<rzkf::SetupInput<Ring>>(lambda);
                            } else {
                                wires_[gate->out].getLambda() = 0;
                            }
                            break;
                        }
                        case common::utils::GateType::kMul: {
                            // Compute gamma.
                            // This corresponds to Pi_mult steps 1 and 2.
                            // Dealer and each party except nP_ non-interactively sample their share,
                            // Dealer computes the remaining share and adds it to gamma_to_send which
                            // will later be sent to party nP_ as one single, big message.

                            // Also compute the output mask lambda_z

                            // We also let the Dealer already compute here sum_lifted_gamma_minus_lambda,
                            // which is the sum over all i (lift(gamma_i - lambdaz_i)), which will
                            // later be used in verify(). Computing this already here saves storage.
                            std::vector<Ring> lambdaShares(nP_ + 1);
                            LargeRing sum_lifted_gamma_minus_lambda = 0;
                            if (id_ == 0) {
                                auto *g = static_cast<common::utils::FIn2Gate *>(gate.get());
                                Ring gamma = wires_[g->in1].getLambda() * wires_[g->in2].getLambda();

                                Ring fullLambda = 0; // for output mask

                                for (size_t i = 1; i < nP_; i++) {
                                    Ring x = getRandomElem(rgen_.as_D_with_i(i));

                                    Ring y = getRandomElem(rgen_.as_D_with_i(i));
                                    lambdaShares[i] = y;
                                    fullLambda += y;

                                    gamma -= x;
                                    sum_lifted_gamma_minus_lambda += (LargeRing) (x - y);
                                }
                                Ring last_x = gamma;
                                Ring last_y = getRandomElem(rgen_.as_D_with_i(nP_));
                                lambdaShares[nP_] = last_y;
                                fullLambda += last_y;
                                sum_lifted_gamma_minus_lambda += (LargeRing) (last_x - last_y);
                                gamma_to_send.push_back(last_x); // remaining share

                                wires_[gate->out].getLambda() = fullLambda;
                                // Dealer saves all individual shares of its output mask and sum_lifted_gamma_minus_lambda
                                setup_gates_[gate->out] = std::make_unique<rzkf::DealerAllShares<Ring, LargeRing>>(lambdaShares, sum_lifted_gamma_minus_lambda);
                            } else if (id_ == nP_) {
                                // Receive later
                                message_size++;

                                wires_[gate->out].getLambda() = getRandomElem(rgen_.me_and_D());
                            } else {
                                Ring share = getRandomElem(rgen_.me_and_D());
                                // Party stores its share of gamma for the online phase
                                setup_gates_[gate->out] = std::make_unique<rzkf::SetupMult<Ring>>(share);

                                wires_[gate->out].getLambda() = getRandomElem(rgen_.me_and_D());
                            }
                            break;
                        }
                        case common::utils::GateType::kAdd: // not interactive
                        case common::utils::GateType::kConstMul: // not interactive
                        {
                            std::cout << gate->type << std::endl;
                            throw std::runtime_error("Non-interactive gate should not be here in un-parallelized context.");
                        }
                        default: {
                            std::cout << gate->type << std::endl;
                            throw std::runtime_error("Unsupported GateType.");
                        }
                    }
                }
            }
        }

        // Dealer sends all gamma shares for party nP_ to that party.
        if (id_ == 0) {
            network_->send(nP_, gamma_to_send.data(), sizeof(Ring) * gamma_to_send.size());
        } else if (id_ == nP_) {
            gamma_to_send.resize(message_size);
            network_->recv(0, gamma_to_send.data(), sizeof(Ring) * message_size);
        }

        // Party nP_ stores all its gamma shares as preprocessing material for the specific corresponding gates.
        message_size = 0;
        for (size_t depth = 0; depth < circ_.gates_by_level.size(); ++depth)
        {
            for (auto &gate : circ_.gates_by_level[depth])
            {
                switch (gate->type)
                {
                    case common::utils::GateType::kInp:
                    case common::utils::GateType::kAdd:
                    case common::utils::GateType::kConstMul:
                        break;
                    case common::utils::GateType::kMul: {
                        if (id_ == nP_) {
                            Ring share = gamma_to_send[message_size];
                            setup_gates_[gate->out] = std::make_unique<rzkf::SetupMult<Ring>>(share);
                            message_size++;
                        }
                        break;
                    }
                    default: {
                        std::cout << gate->type << std::endl;
                        throw std::runtime_error("Unsupported GateType.");
                    }
                }
            }
        }

        // Now, run the setup for the verification protocol.
        setup_verify();
        // TODO test that send does not block until received!!!
        // TODO didn't we already do that??
    }

    void Evaluator::setInputs(const std::unordered_map<common::utils::wire_t, int> &input_mapping, const std::unordered_map<common::utils::wire_t, Ring> &inputs)
    {
        if (id_ == 0) // setting inputs happens online, Dealer absent from online
            return;
            
        std::vector<Ring> to_broadcast;
        std::vector<std::vector<Ring>> to_receive_from(nP_ + 1);
        std::vector<size_t> to_receive_from_size(nP_ + 1);
        std::vector<size_t> to_receive_from_index(nP_ + 1);

        // Input gates have depth 0
        for (auto &gate : circ_.gates_by_level[0])
        {
            if (gate->type == common::utils::GateType::kInp)
            {
                size_t provider = input_mapping.at(gate->out);
                if (id_ == provider) {
                    auto *setup_mat = static_cast<rzkf::SetupInput<Ring> *>(setup_gates_[gate->out].get());
                    Ring m =  inputs.at(gate->out) - setup_mat->full_lambda;
                    to_broadcast.push_back(m);
                    wires_[gate->out].getM() = m;
                } else {
                    to_receive_from_size[provider] ++;
                }
            }
        }

        // Broadcast all m
        for (size_t i = 1; i <= nP_; i++) { // to i
            if (i != id_) {
                network_->send(i, to_broadcast.data(), sizeof(Ring) * to_broadcast.size());
            }
            // if (to_broadcast.size() > 0) to_broadcast[i] += 1; // Test party sending inconsistent m_x
        }

        // Receive all broadcasts
        for (size_t i = 1; i <= nP_; i++) { // from i
            if (i != id_) {
                to_receive_from[i].resize(to_receive_from_size[i]);
                network_->recv(i, to_receive_from[i].data(), sizeof(Ring) * to_receive_from_size[i]);
            }
        }

        // Check consistency, compare hash over all messages of P1, P2, P3, ... in that order
        /*
        This corresponds to §C.1 in the paper, batching the pairwise exchange over all inputs.
        One may also batch this check with the check during verification, saving more communication.
        Yet, inputs are a one time thing, and we want to benchmark complexity for the circuit
        evaluation phase, so it is better to exclude operations meant for the input phase.
        */
        emp::Hash consistency_hash;
        for (size_t i = 1; i <= nP_; i++) {
            if (i == id_) {
                if (to_broadcast.size() > 0)
                    consistency_hash.put(to_broadcast.data(), to_broadcast.size() * sizeof(Ring));
            } else {
                if (to_receive_from[i].size() > 0)
                    consistency_hash.put(to_receive_from[i].data(), to_receive_from[i].size() * sizeof(Ring));
            }
        }
        std::array<char, emp::Hash::DIGEST_SIZE> digest;
        consistency_hash.digest(digest.data());
        for (size_t i = 1; i <= nP_; i++) {
            if (i != id_) {
                network_->send(i, digest.data(), emp::Hash::DIGEST_SIZE * sizeof(char));
            }
        }
        bool match = true;
        for (size_t i = 1; i <= nP_; i++) {
            if (i != id_) {
                std::array<char, emp::Hash::DIGEST_SIZE> received_digest;
                network_->recv(i, received_digest.data(), emp::Hash::DIGEST_SIZE * sizeof(char));
                match &= (received_digest == digest);
            }
        }
        checkForAccept(match, "ABORT: A party distributed inconsistent m_x in the input phase");

        // Now, write the m_x values of the input wires
        for (auto &gate : circ_.gates_by_level[0])
        {
            if (gate->type == common::utils::GateType::kInp)
            {
                size_t provider = input_mapping.at(gate->out);
                if (id_ == provider) {
                    // Do nothing, already written before
                } else {
                    Ring m = to_receive_from[provider][to_receive_from_index[provider]];
                    to_receive_from_index[provider] ++;
                    wires_[gate->out].getM() = m;
                }
            }
        }

        for (size_t i = 1; i <= nP_; i++) {
            assert(to_receive_from_index[i] == to_receive_from_size[i]);
        }
    }

    void Evaluator::evaluateGatesAtDepthPartySend(size_t depth, std::vector<Ring> &mult_vals)
    {
        #pragma omp parallel for
        for (size_t i = 0; i < circ_.gates_by_level[depth].size(); i++) {
            auto &gate = circ_.gates_by_level[depth][i];
            switch (gate->type)
            {
            case common::utils::GateType::kMul:
            {
                auto *g = static_cast<common::utils::FIn2Gate *>(gate.get());
                if (id_ != 0)
                {
                    auto *setup_mat = static_cast<rzkf::SetupMult<Ring> *>(setup_gates_[gate->out].get());

                    // This is Pi_mult step 3:
                    Ring val = wires_[g->in1].getM() * wires_[g->in2].getLambda()
                                + wires_[g->in1].getLambda() * wires_[g->in2].getM()
                                + setup_mat->gamma_share
                                - wires_[gate->out].getLambda();
                    if (id_ == 1) // Only one party must add the constant m_x * m_y
                        val += wires_[g->in1].getM() * wires_[g->in2].getM();

                    mult_vals[i] = val; // Own share to later be communicated
                }

                break;
            }

            case ::common::utils::GateType::kAdd:
            case ::common::utils::GateType::kConstMul:
            case ::common::utils::GateType::kInp:
            {
                break;
            }

            default:
            {
                std::cout << gate->type << std::endl;
                throw std::runtime_error("Unsupported GateType.");
            }
            }
        }
    }

    void Evaluator::evaluateGatesAtDepthPartyRecv(size_t depth, std::vector<Ring> mult_vals)
    {
        #pragma omp parallel for
        for (size_t i = 0; i < circ_.gates_by_level[depth].size(); i++) {
            auto &gate = circ_.gates_by_level[depth][i];
            switch (gate->type)
            {
            case common::utils::GateType::kAdd:
            {
                auto *g = static_cast<common::utils::FIn2Gate *>(gate.get());
                if (id_ != 0)
                    wires_[g->out].getM() = wires_[g->in1].getM() + wires_[g->in2].getM(); // Online simply adds up m values
                break;
            }

            case common::utils::GateType::kConstMul:
            {
                auto *g = static_cast<common::utils::ConstOpGate<Ring> *>(gate.get());
                if (id_ != 0)
                    wires_[g->out].getM() = wires_[g->in].getM() * g->cval; // Online simply multiplies m value by const
                break;
            }

            case common::utils::GateType::kMul:
            {
                auto *g = static_cast<common::utils::FIn2Gate *>(gate.get());
                if (id_ != 0)
                {
                    wires_[g->out].getM() = mult_vals[i]; // use as m what has been reconstructed in the interaction phase
                }
                break;
            }

            case ::common::utils::GateType::kInp:
            {
                break;
            }

            default:
            {
                std::cout << gate->type << std::endl;
                throw std::runtime_error("Unsupported GateType.");
            }
            }
        }
    }

    void Evaluator::evaluateGatesAtDepth(size_t depth)
    {
        size_t mult_num = circ_.level_mults[depth];
        std::vector<Ring> mult_vals;
        mult_vals.resize(mult_num);

        evaluateGatesAtDepthPartySend(depth, mult_vals);
        if (mult_num > 0) {
            /*
            Pi_mult step 3 computed additive shares (stored in mult_vals) of the m values.
            Here, we simply reconstruct these additive sharings, including them in the delayed verification
            to check for consistent reconstruction.
            */
            reconstruct(mult_vals, true, pking_semi_);
        }
        evaluateGatesAtDepthPartyRecv(depth, mult_vals);
    }

    void Evaluator::setupOutputPhase_A() {
        /*
        Part A is responsible for operations using rgen_.
        It is crucial that this happens before runSetup(), as runSetup(), and in it, verify(), has
        the dealer make PRG  calls to a shared PRG with parties that make their corresponding calls
        in the online phase. ence, after running runSetup(), the parties must not to any other PRG
        calls before running the online verification; otherwise, one would get an access offset,
        inconsistencies, and resulting crashes.
        */
        if (id_ == 0) {
            // Output phase uses Pi_rec^{active,rerand}, so we need rerandomizers and commits on
            // the  output wires' masks.
            // Rerandomizers are additive sharings of zero. We let the dealer non-interactively
            // sample random shares with P_2, ..., P_n, and then compute a matching share for P_1
            // so that everything sums up to zero.
            output_randomizers_ring.resize(circ_.outputs.size());
            for (size_t k = 0; k < circ_.outputs.size(); k++) {
                Ring sum_other_shares = 0;
                for (size_t i = 2; i <= nP_; i++)
                    sum_other_shares += getRandomElem(rgen_.as_D_with_i(i));
                output_randomizers_ring[k] = 0 - sum_other_shares;
            }

            // Pi_rec^active step 1: sample random nonce shares, accumulate as output_nonce
            for (size_t k = 0; k < 3; k++) {
                output_nonce[k] = 0;
            }
            for (size_t i = 1; i <= nP_; i++) {
                Seed nonce_share[3];
                rgen_.as_D_with_i(i).random_data(nonce_share, 3 * sizeof(Seed));
                for (size_t k = 0; k < 3; k++) {
                    output_nonce[k] ^= nonce_share[k];
                }
            }
        } else {
            if (id_ != 1) {
                // Non-interactively sample shares of rerandomizers
                output_randomizers_ring.resize(circ_.outputs.size());
                for (size_t k = 0; k < circ_.outputs.size(); k++) {
                    output_randomizers_ring[k] = getRandomElem(rgen_.me_and_D());
                }
            }
            // Pi_rec^active step 1: sample random nonce share
            rgen_.me_and_D().random_data(output_nonce, 3 * sizeof(Seed));
        }
    }

    void Evaluator::setupOutputPhase_B() {
        /*
        Part B handles communication after the Dealer has access to the output wire's full masks,
        which is the case after running runSetup().
         */
        if (id_ == 0) {
            // Hand P_1 its rerandomizer shares
            network_->send(1, output_randomizers_ring.data(), circ_.outputs.size() * sizeof(Ring));

            // Pi_rec^active step 2: compute H(lambda_x || lambda_y || ... || nonce)
            // First, collect all lambdas
            std::vector<Ring> lambdas(circ_.outputs.size());
            for (size_t i = 0; i < circ_.outputs.size(); i++) {
                lambdas[i] = wires_[circ_.outputs[i]].getLambda();
            }
            emp::Hash lambdas_hash;
            lambdas_hash.put(lambdas.data(), lambdas.size() * sizeof(Ring));
            lambdas_hash.put(output_nonce, 3 * sizeof(Seed));
            lambdas_hash.digest(output_commit.data());
            // Pi_rec^active step 3: Send commit to all parties
            for (size_t i = 1; i <= nP_; i++) {
                network_->send(i, output_commit.data(), emp::Hash::DIGEST_SIZE);
            }
        } else {
            if (id_ == 1) {
                // Receive all rerandomizer shares as party 1
                output_randomizers_ring.resize(circ_.outputs.size());
                network_->recv(0, output_randomizers_ring.data(), circ_.outputs.size() * sizeof(Ring));
            }
            // Pi_rec^active step 3: Receive commit
            network_->recv(0, output_commit.data(), emp::Hash::DIGEST_SIZE);
        }
    }

    std::vector<Ring> Evaluator::getOutputs()
    {
        std::vector<Ring> outvals(circ_.outputs.size());
        if (id_ == 0) return outvals; // Dealer absent from online phase

        // Rerandomize mask and collect lambdas
        std::vector<Ring> lambdas(circ_.outputs.size());
        for (size_t i = 0; i < circ_.outputs.size(); ++i) {
            wires_[circ_.outputs[i]].getLambda() += output_randomizers_ring[i];
            lambdas[i] = wires_[circ_.outputs[i]].getLambda();
        };

        // open shares and nonce, Pi_rec^active steps 4 and 5
        reconstruct(lambdas, false, pking_semi_, false, true, output_nonce, false);

        // Pi_rec^active step 6: check that H(lambda_x || lambda_y || ... || nonce) equals what
        // was received from Dealer before.
        emp::Hash lambdas_hash;
        lambdas_hash.put(lambdas.data(), lambdas.size() * sizeof(Ring));
        lambdas_hash.put(output_nonce, 3 * sizeof(Seed));
        std::array<char, emp::Hash::DIGEST_SIZE> digest;
        lambdas_hash.digest(digest.data());
        bool match = output_commit == digest;
        checkForAccept(match, "ABORT: Detected cheating during output reconstruction phase");

        // Finalize outputs
        for (size_t i = 0; i < circ_.outputs.size(); ++i) {
            outvals[i] = wires_[circ_.outputs[i]].getM() + lambdas[i];
        }
        return outvals;
    }

    void Evaluator::evaluateCircuit()
    {
        if (id_ == 0) return; // Dealer does not participate in the online phase

        // Evaluate circuit layer by layer
        for (size_t i = 0; i < circ_.gates_by_level.size(); ++i)
        {
            evaluateGatesAtDepth(i);
        }
        // Run the final verification
        verify();
    }

    template <class R>
    void Evaluator::reconstruct(std::vector<R> &vals, bool include_in_pairwise_consistency_check, bool use_pking, bool use_xor, bool piggyback_nonce, Seed *nonce, bool checkConsistency) {
        // Reconstruction does not support being included in the pairwise consistency check while also running
        // the pairwise consistency check in parallel.
        assert(!include_in_pairwise_consistency_check || !checkConsistency);
        if (use_pking) {
            reconstructPking(vals, include_in_pairwise_consistency_check, use_xor, piggyback_nonce, nonce, checkConsistency);
        } else {
            reconstructBroadcast(vals, include_in_pairwise_consistency_check, use_xor, piggyback_nonce, nonce, checkConsistency);
        }
    }

    template <class R>
    void Evaluator::reconstructPking(std::vector<R> &vals, bool include_in_pairwise_consistency_check, bool use_xor, bool piggyback_nonce, Seed *nonce, bool checkConsistency) {
        assert(id_ != 0);
        size_t per_party_comm = floor(vals.size() / nP_);
        size_t last_party_comm = per_party_comm + (vals.size() % nP_);
        /*
        At the end, each party's val vector should be the sum over all val vectors at the start.
        We keep all intermediate data inside vals.
        For the first  per_party_comm many mults, 1 is P_king, then 2 is P_king, etc.
        Hence, party i is P_king for the block with indices
        (i - 1) * per_party_comm, (i - 1) * per_party_comm + 1, ..., i * per_party_comm - 1
        or for the last party, it goes until (i - 1) * per_party_comm + last_party_comm - 1 instead.

        Also, just always send nonce to party 1 as potentially, last party has more traffic than others.
        Nonce always consists of 3 Seed, further splitting will just cause overhead for small vectors vals.
        */

        // Parallelizing aggregation at pking over single elements or over providers each
        // does not improve run time, but sometimes even makes it slightly worse.
        // ==> No parallelization here

        // This part is merged from checkConsistency()
        // Integrated here so that it runs in the same round(s)
        // It is ugly, but more efficient :(
        // Part 1/2
        std::array<char, emp::Hash::DIGEST_SIZE> digest;
        if (checkConsistency) {
            pking_consistency_hash_.digest(digest.data());
            // Exchange hash with all
            for (size_t i = 1; i <= nP_; i++) {
                if (i != id_) {
                    network_->send(i, digest.data(), emp::Hash::DIGEST_SIZE * sizeof(char));
                }
            }
        }

        // Send own shares to P_king
        for (size_t king = 1; king <= nP_; king++) {
            if (id_ == king) // I am P_king ==> Do nothing
                continue;
            // Need to send to P_king:
            if (king != nP_) { // Not last block
                network_->send(king, vals.data() + (king - 1) * per_party_comm, sizeof(R) * per_party_comm);
            } else { // Last block
                network_->send(king, vals.data() + (king - 1) * per_party_comm, sizeof(R) * last_party_comm);
            }
        }
        if (piggyback_nonce && id_ > 1) {
            network_->send(1, nonce, sizeof(Seed) * 3);
        }

        // This part is merged from checkConsistency()
        // Part 2/2
        bool match = true;
        if (checkConsistency) {
            for (size_t i = 1; i <= nP_; i++) {
                if (i != id_) {
                    std::array<char, emp::Hash::DIGEST_SIZE> received_digest;
                    network_->recv(i, received_digest.data(), emp::Hash::DIGEST_SIZE * sizeof(char));
                    match &= (received_digest == digest);
                }
            }
            pking_consistency_hash_.reset();
            number_unchecked_consistency_ = 0;
            if (!match) {
                pairwise_consistency_mismatch_ = true;
            }
        }

        // P_king receives shares and sums up all shares
        for (size_t provider = 1; provider <= nP_; provider++) { // Iterate over batches per provider
            if (id_ == provider) // What is provided by me is already written to vals as initial value
                continue;
            // Receive data
            if (id_ != nP_) { // Not last block
                std::vector<R> received(per_party_comm);
                network_->recv(provider, received.data(), sizeof(R) * per_party_comm);
                // add or XOR this share to the value to be reconstructed
                if (use_xor) {
                    for (size_t i = 0; i < per_party_comm; i++) {
                        vals[(id_ - 1) * per_party_comm + i] ^= received[i];
                    }
                } else {
                    for (size_t i = 0; i < per_party_comm; i++) {
                        vals[(id_ - 1) * per_party_comm + i] += received[i];
                    }
                }
            } else { // Last block
                std::vector<R> received(last_party_comm);
                network_->recv(provider, received.data(), sizeof(R) * last_party_comm);
                // add or XOR this share to the value to be reconstructed
                if (use_xor) {
                    for (size_t i = 0; i < last_party_comm; i++) {
                        vals[(id_ - 1) * per_party_comm + i] ^= received[i];
                    }
                } else {
                    for (size_t i = 0; i < last_party_comm; i++) {
                        vals[(id_ - 1) * per_party_comm + i] += received[i];
                    }
                }
            }
            if (piggyback_nonce && id_ == 1) {
                std::vector<Seed> received(3);
                network_->recv(provider, received.data(), sizeof(Seed) * 3);
                for (size_t k = 0; k < 3; k++) {
                    nonce[k] ^= received[k];
                }
            }
        }
        proof_rounds_++; // everybody sends and receives ==> one round passed

        // Now, vals already contains final result where party also is P_king

        // P_king distributes
        for (size_t receiver = 1; receiver <= nP_; receiver++) {
            if (id_ == receiver) // No need to send to myself
                continue;
            if (id_ != nP_) { // Not last block
                network_->send(receiver, vals.data() + (id_ - 1) * per_party_comm, sizeof(R) * per_party_comm);
            } else { // Last block
                network_->send(receiver, vals.data() + (id_ - 1) * per_party_comm, sizeof(R) * last_party_comm);
            }
            if (piggyback_nonce && id_ == 1) {
                network_->send(receiver, nonce, sizeof(Seed) * 3);
            }
        }

        // Parties receive from P_king
        for (size_t king = 1; king <= nP_; king++) {
            if (id_ == king) // I am Pking ==> I already knew the value before
                continue;
            // Receive from P_king
            if (king != nP_) { // Not last block
                std::vector<R> received(per_party_comm);
                network_->recv(king, received.data(), sizeof(R) * per_party_comm);
                for (size_t i = 0; i < per_party_comm; i++) {
                    vals[(king - 1) * per_party_comm + i] = received[i];
                }
            } else { // Last block
                std::vector<R> received(last_party_comm);
                network_->recv(king, received.data(), sizeof(R) * last_party_comm);
                for (size_t i = 0; i < last_party_comm; i++) {
                    vals[(king - 1) * per_party_comm + i] = received[i];
                }
            }
        }
        if (piggyback_nonce && id_ > 1) {
            std::vector<Seed> received(3);
            network_->recv(1, received.data(), sizeof(Seed) * 3);
            for (size_t k = 0; k < 3; k++) {
                nonce[k] = received[k];
            }
        }
        proof_rounds_++; // everybody sends and receives ==> one round passed

        // Only used for vals, not piggypacked things which are commit checked.
        if (include_in_pairwise_consistency_check) {
            pking_consistency_hash_.put(vals.data(), vals.size() * sizeof(R));
            number_unchecked_consistency_ += vals.size();
        }
    }

    template <class R>
    void Evaluator::reconstructBroadcast(std::vector<R> &vals, bool include_in_pairwise_consistency_check, bool use_xor, bool piggyback_nonce, Seed *nonce, bool checkConsistency) {
        assert(id_ != 0);

        // This part is merged from checkConsistency()
        // Integrated here so that it runs in the same round(s)
        // It is ugly, but more efficient :(
        // Part 1/2
        std::array<char, emp::Hash::DIGEST_SIZE> digest;
        if (checkConsistency) {
            pking_consistency_hash_.digest(digest.data());
            // Exchange hash with all
            for (size_t i = 1; i <= nP_; i++) {
                if (i != id_) {
                    network_->send(i, digest.data(), emp::Hash::DIGEST_SIZE * sizeof(char));
                }
            }
        }

        // Send own share to all others
        for (size_t p = 1; p <= nP_; p++) {
            if (id_ == p) continue;
            network_->send(p, vals.data(), sizeof(R) * vals.size());
            if (piggyback_nonce) {
                network_->send(p, nonce, sizeof(Seed) * 3);
            }
        }

        // This part is merged from checkConsistency()
        // Part 2/2
        bool match = true;
        if (checkConsistency) {
            for (size_t i = 1; i <= nP_; i++) {
                if (i != id_) {
                    std::array<char, emp::Hash::DIGEST_SIZE> received_digest;
                    network_->recv(i, received_digest.data(), emp::Hash::DIGEST_SIZE * sizeof(char));
                    match &= (received_digest == digest);
                }
            }
            pking_consistency_hash_.reset();
            number_unchecked_consistency_ = 0;
            if (!match) {
                pairwise_consistency_mismatch_ = true;
            }
        }

        // Receive from all others
        for (size_t p = 1; p <= nP_; p++) {
            if (id_ == p) continue;
            std::vector<R> received(vals.size());
            network_->recv(p, received.data(), sizeof(R) * vals.size());
            // add or XOR this share to the value to be reconstructed
            if (use_xor) {
                for (size_t i = 0; i < vals.size(); i++) {
                    vals[i] ^= received[i];
                }
            } else {
                for (size_t i = 0; i < vals.size(); i++) {
                    vals[i] += received[i];
                }
            }
            if (piggyback_nonce) {
                std::vector<Seed> received(3);
                network_->recv(p, received.data(), sizeof(Seed) * 3);
                for (size_t k = 0; k < 3; k++) {
                    nonce[k] ^= received[k];
                }
            }
        }
        proof_rounds_++; // each party sends and then receives ==> 1 round

        // Only used for vals, not piggypacked things which are commit checked.
        if (include_in_pairwise_consistency_check) {
            pking_consistency_hash_.put(vals.data(), vals.size() * sizeof(R));
            number_unchecked_consistency_ += vals.size();
        }
    }

    void Evaluator::checkConsistency() {
        if (id_ == 0) return;
        // Runs step 2 of Pi_rec^consistent:

        // Values have already been included in the hash as they arrived
        std::array<char, emp::Hash::DIGEST_SIZE> digest;
        pking_consistency_hash_.digest(digest.data());

        // Exchange hash with all
        for (size_t i = 1; i <= nP_; i++) {
            if (i != id_) {
                network_->send(i, digest.data(), emp::Hash::DIGEST_SIZE * sizeof(char));
            }
        }
        bool match = true;
        for (size_t i = 1; i <= nP_; i++) {
            if (i != id_) {
                std::array<char, emp::Hash::DIGEST_SIZE> received_digest;
                network_->recv(i, received_digest.data(), emp::Hash::DIGEST_SIZE * sizeof(char));
                match &= (received_digest == digest);
            }
        }
        proof_rounds_++; // Everybody sends and then receives ==> 1 round
        checkForAccept(match, "ABORT: checkConsistency failed, some Pking distributed inconsistent values");

        // Reset: Now, there again are no unchecked instances of Pi_rec^consistent
        pking_consistency_hash_.reset();
        number_unchecked_consistency_ = 0;
    }

    template <class R>
    void Evaluator::openAdditive(std::vector<R> &shares, bool verify, bool use_pking, bool use_xor, bool checkConsistency) {
        // This is Pi_rec^active from the paper

        if (id_ == 0) {
            // Add values to hash as in Pi_rec^active step 2, when values arrive instead of all at once.
            check_opening_hash_.put(shares.data(), shares.size() * sizeof(R));
            number_unchecked_openings_ += shares.size();

            if (verify) {
                // sample nonce, Pi_rec^active step 1 and computing r in step 2
                Seed nonce[3] = {0, 0, 0}; // nonce has size 3 kappa = 3 Seed size
                for (size_t i = 1; i <= nP_; i++) {
                    Seed nonce_share[3];
                    rgen_.as_D_with_i(i).random_data(nonce_share, 3 * sizeof(Seed));
                    for (size_t k = 0; k < 3; k++) {
                        nonce[k] ^= nonce_share[k];
                    }
                }

                // Pi_rec^active step 2: append r/nonce to hash and compute digest
                check_opening_hash_.put(nonce, 3 * sizeof(Seed));
                std::array<char, emp::Hash::DIGEST_SIZE> digest;
                check_opening_hash_.digest(digest.data());
                // Pi_rec^active step 3, but everything by the Dealer will be send later as one big message
                commits.push_back(digest);

                // Reset: Now, there again are no unchecked instances of Pi_rec^active
                check_opening_hash_.reset();
                number_unchecked_openings_ = 0;
            }
        } else {
            if (verify) {
                // sample nonce share, Pi_rec^active step 1
                Seed nonce[3]; // so far, only own share of nonce
                rgen_.me_and_D().random_data(nonce, 3 * sizeof(Seed));

                // Get next commit provided in Pi_rec^active step 3
                std::array<char, emp::Hash::DIGEST_SIZE> expected_digest = commits.front();
                commits.pop_front();

                // open shares and nonce, Pi_rec^active steps 4 and 5
                reconstruct(shares, false, use_pking, use_xor, true, nonce, checkConsistency); // pairwise consistency check not needed here as we use commit
                // append data to hash when it arrives instead of all at once.
                check_opening_hash_.put(shares.data(), shares.size() * sizeof(R));
                number_unchecked_openings_ += shares.size();

                // Pi_rec^active step 6 (all values x were already added to hash before)
                check_opening_hash_.put(nonce, 3 * sizeof(Seed));
                std::array<char, emp::Hash::DIGEST_SIZE> actual_digest;
                check_opening_hash_.digest(actual_digest.data());
                bool match = expected_digest == actual_digest;
                if (!match) {
                    opening_commit_mismatch_ = true;
                }
                // Reset: Now, there again are no unchecked instances of Pi_rec^active
                check_opening_hash_.reset();
                number_unchecked_openings_ = 0;
            } else {
                // open shares, Pi_rec^active step 4.
                reconstruct(shares, false, use_pking, use_xor, false, nullptr, checkConsistency); // pairwise consistency check not needed here as we use commit
                // immediately add to hash for later
                check_opening_hash_.put(shares.data(), shares.size() * sizeof(R));
                number_unchecked_openings_ += shares.size();
            }            
        }
    }

    template <class R>
    std::pair<std::vector<R>, std::vector<R>> Evaluator::shareSum(std::vector<R> &inputs, size_t n, bool use_pking) {
        assert(id_ == 0 || inputs.size() == n);
        std::vector<R> ms;
        std::vector<R> ls;
        ls.reserve(n);
        if (id_ == 0) {
            // Cannot parallelize here due to internal rng calls
            for (size_t k = 0; k < n; k++) {
                // Compute mask, Pi_shareSum step 1
                R mask = 0;
                for (size_t i = 1; i <= nP_; i++) {
                    R mask_part;
                    rgen_.as_D_with_i(i).random_data(&mask_part, sizeof(R));
                    mask += mask_part;
                }
                ls.push_back(mask);
            }
        } else {
            ms.reserve(n);
            // Cannot parallelize here due to internal rng calls
            for (size_t k = 0; k < n; k++) {
                // Compute mask share (Pi_rec^active step 1) and own share of <m_x>' (Pi_rec^active step 2)
                R mask;
                rgen_.me_and_D().random_data(&mask, sizeof(R));
                ls.push_back(mask);
                ms.push_back(inputs[k] - mask); // for now, only own additive share
            }
            // Pi_rec^active step 3.
            reconstruct(ms, true, use_pking);
        }
        return std::make_pair(ms, ls);
    }

    emp::PRG Evaluator::generateNewPRG(bool verify, bool checkConsistency) {
        Seed seed;
        // Pi_rnd step 1:
        if (id_ == 0) {
            seed = 0;
            for (size_t i = 1; i <= nP_; i++) {
                seed ^= getRandomSeed(rgen_.as_D_with_i(i));
            }
        } else {
            seed = getRandomSeed(rgen_.me_and_D()); // for now, this is only own XOR share
        }

        // Pi_rnd step 2:
        std::vector<Seed> packed;
        packed.push_back(seed);
        openAdditive(packed, verify, pking_verify_ == 2, true, checkConsistency);

        // Construct and return PRG
        seed = packed[0];
        emp::PRG prg;
        auto seed_block = emp::makeBlock((uint64_t) (seed >> 64), (uint64_t) seed); 
        prg.reseed(&seed_block, 0);
        return prg;
    }

    void Evaluator::setup_verify() {
        if (id_ == 0) {
            verify(); // Dealer part of verification already runs here, this only fills commits, and randomizers, still to be sent
            // Distribute commits:
            std::vector<std::array<char, emp::Hash::DIGEST_SIZE>> serialized;
            size_t number_commits = commits.size();
            serialized.reserve(number_commits);
            while (!commits.empty()) {
                serialized.push_back(commits.front());
                commits.pop_front();
            }
            for (size_t i = 1; i <= nP_; i++) {
                network_->send(i, &number_commits, sizeof(size_t));
                network_->send(i, serialized.data(), emp::Hash::DIGEST_SIZE * number_commits);
            }
            assert(commits.size() == 0);

            // Distribute randomizer shares to party 1 only
            assert(randomizers_ring.size() == SSEC);
            assert(randomizers_large_ring.size() == 2);
            network_->send(1, randomizers_ring.data(), SSEC * sizeof(Ring));
            network_->send(1, randomizers_large_ring.data(), 2 * sizeof(LargeRing));
        } else {
            // Receive all commits from the dealer to later use online in verify()
            std::vector<std::array<char, emp::Hash::DIGEST_SIZE>> serialized;
            size_t number_commits;
            network_->recv(0, &number_commits, sizeof(size_t));
            serialized.resize(number_commits);
            network_->recv(0, serialized.data(), emp::Hash::DIGEST_SIZE * number_commits);
            for (std::array<char, emp::Hash::DIGEST_SIZE> commit : serialized) {
                commits.push_back(commit);
            }
            assert(commits.size() == number_commits);

            // Receive all randomizer shares as party 1
            if (id_ == 1) {
                randomizers_ring.resize(SSEC);
                randomizers_large_ring.resize(2);
                network_->recv(0, randomizers_ring.data(), SSEC * sizeof(Ring));
                network_->recv(0, randomizers_large_ring.data(), 2 * sizeof(LargeRing));
            }
        }
    }

    void Evaluator::verify() {
        std::cout << "Starting Verification..." << std::endl;
        proof_rounds_ = 0;

        #pragma omp declare reduction(vec_ring_plus : std::vector<Ring> : \
                              std::transform(omp_out.begin(), omp_out.end(), omp_in.begin(), omp_out.begin(), std::plus<Ring>())) \
                    initializer(omp_priv = decltype(omp_orig)(omp_orig.size()))
        #pragma omp declare reduction(vec_largering_plus : std::vector<LargeRing> : \
                              std::transform(omp_out.begin(), omp_out.end(), omp_in.begin(), omp_out.begin(), std::plus<LargeRing>())) \
                    initializer(omp_priv = decltype(omp_orig)(omp_orig.size()))

        // This is kinda the online phase of verification.
        // Yet, for not having the same code structure twice, this also contains dealer
        // operations where instead of sending values, it adds it to an internal queue.
        // This is intended so that verify() is called in setup_verify() only for the dealer,
        // which then follows the proof structure, puts everything it intends to send into a queue,
        // and finally setup_verify() takes care of sending the content of the queue to the respective
        // parties. These parties, inside verify(), just read data from this queue that they already
        // receive in setup_verify().

        size_t m = circ_.count[common::utils::GateType::kMul];

        // ### Pi_verify step 1: sample r_i^k from new PRG seed. ###
        // Done by Dealer + parties.
        // Formatting as vectors r_i, each containing all SSEC many bits r_i^j in one uint64_t
        // (wasting the remaining bits, but they do not cost additional communication)
        emp::PRG prg = generateNewPRG(false); // verify delayed, see §4.2.5
        assert(sizeof(uint64_t) * 8 >= SSEC); // SSEC bits fit into one 64 bit int
        std::vector<uint64_t> r_bits;
        r_bits.resize(m);
        prg.random_data(r_bits.data(), m * sizeof(uint64_t));

        // ### Pi_verify step 2: Compute all SSEC Lambda^k terms ###
        // Done by parties only.
        std::vector<Ring> Lambdas;
        if (id_ != 0) { // Parties only
            Lambdas.resize(SSEC);
            size_t offset = 0;
            // Outer for-if-for-if is sum over all multiplication gates
            for (size_t layer_i = 0; layer_i < circ_.gates_by_level.size(); layer_i++) {
                if (circ_.level_mults[layer_i] > 0) {
                    #pragma omp parallel for reduction(vec_ring_plus : Lambdas)
                    for (size_t i = 0; i < circ_.gates_by_level[layer_i].size(); i++) {
                        auto &gate = circ_.gates_by_level[layer_i][i];
                        if (gate->type == common::utils::GateType::kMul) {
                            auto *g = static_cast<common::utils::FIn2Gate *>(gate.get());

                            // add r_i^k * (m_x_i * m_y_i - m_z_i) to Lambda^k
                            // strategy: compute m_x_i * m_y_i - m_z_i (same for all k), then for each
                            // Lambda^k, add only if r_i^k = 1/true
                            Ring inner = wires_[g->in1].getM() * wires_[g->in2].getM() - wires_[g->out].getM();
                            for (size_t k = 0; k < SSEC; k++) {
                                // r_i^k: (r_bits[offset + i] >> k) & 0x00000001, as we consider numbers
                                // r_i (using offset + i as index here due to the parallelization),
                                // and bit at position k corresponds to r_i^k.
                                if ((r_bits[offset + i] >> k) & 0x00000001) {
                                    Lambdas[k] += inner;
                                }
                            }
                        } else {
                            assert(false);
                        }
                    }
                    offset += circ_.gates_by_level[layer_i].size();
                }
            }
        }

        // ### Pi_verify steps 3, 4, 5 ###
        // Done by Dealer + parties.
        /*
        Step 3 builds the vectors a'^k and b' which are then immediately lifted in steps 4 and 5.
        We merge that by immediately lifting when building the vectors.

        We do the following code optimization:
        Let a' be what remains of the vectors a'^k when NOT having the factor r_i^k in each entry.
        Then, we note that
        a'^k = (r_1^k, r_1^k, r_1^k, r_2^k, r_2^k, r_2^k, ..., r_m^k, r_m^k, r_m^k) HADAMARD a',
        where HADAMARD stands for the hadamard product, i.e., the component-wise multiplication.
        Recall that a^k = lift(a'^k). Yet, it also holds that
        a^k = (r_1^k, r_1^k, r_1^k, r_2^k, r_2^k, r_2^k, ..., r_m^k, r_m^k, r_m^k) HADAMARD a
        where a = lift(a'), because the r_i^k all are in {0,1}.
        So, instead of computing SSEC many a^k, we compute a single a and later in steps 6 and 10
        of the protocol that use a^k simply replace that by
        (r_1^k, r_1^k, r_1^k, r_2^k, r_2^k, r_2^k, ..., r_m^k, r_m^k, r_m^k) HADAMARD a.
        Why? Besides saving memory, in steps 6 and 10, this will make some optimizations possible,
        seeing that the different a^k are structurally close to each other. Details explained later
        for steps 6 and 10.
        */
        std::vector<LargeRing> a;  // 1 a vector (see above)
        std::vector<LargeRing> b;  // 1 b vector

        // Compute b
        b.resize(3 * m);
        size_t offset = 0;
        for (size_t layer_i = 0; layer_i < circ_.gates_by_level.size(); layer_i++) {
            if (circ_.level_mults[layer_i] > 0) {
                #pragma omp parallel for
                for (size_t i = 0; i < circ_.gates_by_level[layer_i].size(); i++) {
                    auto &gate = circ_.gates_by_level[layer_i][i];
                    assert(gate->type == common::utils::GateType::kMul);
                    // For each multiplication, append lambda_y, lambda_x, and gamma_xy - lambda_z to b,
                    // for Dealer or individual shares for the other parties, after lifting the individual shares
                    // (counting shares of gamma_xy - lambda_z as one, not two).
                    if (id_ == 0) {
                        // Need to separately lift shares of all parties and then add up
                        // (recall that dealer later only cares about the sum over all shares instead of the individual shares)
                        auto *g = static_cast<common::utils::FIn2Gate *>(gate.get());
                        auto *complete_in1 = static_cast<rzkf::DealerAllShares<Ring, LargeRing> *>(setup_gates_[g->in1].get());
                        auto *complete_in2 = static_cast<rzkf::DealerAllShares<Ring, LargeRing> *>(setup_gates_[g->in2].get());
                        auto *complete_out = static_cast<rzkf::DealerAllShares<Ring, LargeRing> *>(setup_gates_[g->out].get());
                        LargeRing lx = 0;
                        LargeRing ly = 0;
                        for (size_t i = 1; i <= nP_; i++) {
                            lx += complete_in1->lambda_shares[i]; // implicitly lifts by casting from Ring to LargeRing
                            ly += complete_in2->lambda_shares[i]; // implicitly lifts by casting from Ring to LargeRing
                        }
                        b[offset + 3*i    ] = ly;
                        b[offset + 3*i + 1] = lx;
                        // Last part conventiently already computed by runSetup() before to save some storage
                        b[offset + 3*i + 2] = complete_out->sum_lifted_gamma_minus_lambda;
                    } else {
                        auto *g = static_cast<common::utils::FIn2Gate *>(gate.get());
                        auto *setup_mat = static_cast<rzkf::SetupMult<Ring> *>(setup_gates_[g->out].get());
                        b[offset + 3*i    ] = wires_[g->in2].getLambda(); // implicitly lifts by casting from Ring to LargeRing
                        b[offset + 3*i + 1] = wires_[g->in1].getLambda(); // implicitly lifts by casting from Ring to LargeRing
                        b[offset + 3*i + 2] = (LargeRing) (setup_mat->gamma_share - wires_[g->out].getLambda());
                    }
                }
                offset += circ_.gates_by_level[layer_i].size() * 3;
            }
        }
        // Compute a
        // No parallelization as this is just memory access anyway
        if (id_ != 0) { // Dealer does not have a
            a.reserve(3 * m);
            for (auto &layer: circ_.gates_by_level) {
                for (auto &gate : layer) {
                    if (gate->type == common::utils::GateType::kMul) {
                        auto *g = static_cast<common::utils::FIn2Gate *>(gate.get());
                        // Recall above optimization: We have a single a which omits all the r_i^k for now.
                        // Also, code below implicitly casts to LargeRing, doing the lifting.
                        a.push_back(wires_[g->in1].getM());
                        a.push_back(wires_[g->in2].getM());
                        a.push_back(1);
                    }
                }
            }
        }

        // ### Pi_verify step 6 ###
        // Done by parties
        // Each party locally computes SSEC many dot products
        // From prior optimization changes:
        // a^k * b = ((r_1^k, r_1^k, r_1^k, r_2^k, r_2^k, r_2^k, ..., r_m^k, r_m^k, r_m^k) HADAMARD a) * b
        /*
        Regarding our optimization, now observe the following:
        a^k * b = ((r_1^k, r_1^k, r_1^k, r_2^k, r_2^k, r_2^k, ...) HADAMARD (a_1, a_2, a_3, a_4, a_5, a_6, ...))
        * (b_1, b_2, b_3, b_4, b_5, b_6, ...)
        = (r_1^k a_1, r_1^k a_2, r_1^k a_3, r_2^k a_4, r_2^k a_5, r_2^k a_6, ...) * (b_1, b_2, b_3, b_4, b_5, b_6, ...)
        = r_1^k a_1 b_1 + r_1^k a_2 b_2 + r_1^k a_3 b_3 + r_2^k a_4 b_4 + r_2^k a_5 b_5 + r_2^k a_6 b_6 + ...
        = r_1^k (a_1 b_1 + a_2 b_2 + a_3 b_3) + r_2^k (a_4 b_4 + a_5 b_5 + a_6 b_6) + ...
        Hence, we will first compute the "sub-dot-products" (a_1 b_1 + a_2 b_2 + a_3 b_3) etc.,
        and then add these to each c_j^k = a^k * b if the bit r_1^k etc. is to 1.
        Using that, we drastically reduce the number of arithmetic operations, as they do not need
        to be repeated SSEC many times.
        */
        std::vector<LargeRing> local_cs;
        if (id_ != 0) {
            local_cs.resize(SSEC);
            #pragma omp parallel for reduction(vec_largering_plus : local_cs)
            for (size_t i = 0; i < m; i++) {
                uint64_t pre_r_bit = r_bits[i]; // LSB is r_i^1, then comes r_i^2, etc.
                LargeRing pre_product = 0; // (a_1 b_1 + a_2 b_2 + a_3 b_3) etc.
                for (size_t j = 0; j < 3; j++) {
                    pre_product += a[3 * i + j] * b[3 * i + j];
                }
                for (size_t k = 0; k < SSEC; k++) {
                    bool r_bit = (pre_r_bit >> k) & 0x00000001; // extract r_i^k
                    local_cs[k] += r_bit ? pre_product : 0; // only add pre_product if bit set to 1
                    // if (id_ == 1) local_cs[k] += k; // Test error
                }
            }
        }

        // ### Pi_verify step 7 ###
        // Done by Dealer + parties
        // run shareSum for all SSEC dot products
        auto [c_ms, c_ls] = shareSum(local_cs, SSEC, pking_verify_ >= 1);

        // ### Pi_verify step 8 ###
        // Done by Dealer + parties
        // truncate: go back from LargeRing to Ring, conferting from c to gamma
        // ==> simply set gamma_ms, gamma_ls to copies of c_ms, c_ls, but entries all cast to Ring.
        std::vector<Ring> gamma_ms;
        std::vector<Ring> gamma_ls;
        if (id_ != 0) // Dealer does not know m values
            gamma_ms.reserve(SSEC);
        gamma_ls.reserve(SSEC);
        for (size_t k = 0; k < SSEC; k++) {
            gamma_ls.push_back((Ring) c_ls[k]);
            if (id_ != 0)
                gamma_ms.push_back((Ring) c_ms[k]);
        }

        // ### Pi_verify step 9 ###
        // Done by Dealer + parties
        // sample SSEC many randoms => sample <seed>, expand
        emp::PRG prg_9 = generateNewPRG(false); // verify delayed, see §4.2.5
        std::vector<LargeRing> r_merge;
        r_merge.resize(SSEC);
        prg.random_data(r_merge.data(), SSEC * sizeof(LargeRing));

        // ### Pi_verify step 10 ###
        // Done by parties
        // set x = sum r_merge^k a^k and y = b
        // actually, we just modify a in-place and keep b unchanged, no need to create new vectors here
        /*
        Call r_merge rm here for now.
        x = rm^1 a^1 + ... + rm^SSEC a^SSEC
        = rm^1 ((r_1^1, r_1^1, r_1^1, r_2^1, r_2^1, r_2^1, ..., r_m^1, r_m^1, r_m^1) HADAMARD a) + ...
        + rm^SSEC ((r_1^SSEC, r_1^SSEC, r_1^SSEC, r_2^SSEC, r_2^SSEC, r_2^SSEC, ..., r_m^SSEC, r_m^SSEC, r_m^SSEC) HADAMARD a)
        = (rm^1 (r_1^1, r_1^1, r_1^1, ..., r_m^1, r_m^1, r_m^1) + ... + rm^SSEC (r_1^SSEC, r_1^SSEC, r_1^SSEC, ..., r_m^SSEC, r_m^SSEC, r_m^SSEC))
        HADAMARD a
        = (rm^1 r_1^1 + ... + rm^SSEC r_1^SSEC, rm^1 r_1^1 + ... + rm^SSEC r_1^SSEC, rm^1 r_1^1 + ... + rm^SSEC r_1^SSEC, 
        ..., rm^1 r_m^1 + ... + rm^SSEC r_m^SSEC, rm^1 r_m^1 + ... + rm^SSEC r_m^SSEC, rm^1 r_m^1 + ... + rm^SSEC r_m^SSEC)
        HADAMARD a
        ==> Strategy: compute rm^1 r_i^1 + ... + rm^SSEC r_i^SSEC for all i=1,...,m,
            use as factor for three entries of a
        */
        if (id_ != 0) { // Dealer not involved for x
            #pragma omp parallel for
            for (size_t i = 0; i < m; i++) {
                uint64_t pre_r_bit = r_bits[i]; // LSB is r_i^1, then comes r_i^2, etc.
                LargeRing factor = 0;
                for (size_t k = 0; k < SSEC; k++) {
                    if ((pre_r_bit >> k) & 0x00000001) // extract r_i^k, add rm^k if true
                        factor += r_merge[k];
                }
                for (size_t j = 0; j < 3; j++) {
                    a[3 * i + j] *= factor;
                }
            }
        }

        // ### Pi_verify step 11 ###
        // Done by parties
        // set [z] = sum r_merge^k [c^k]
        /*
        Note that this means m_z = sum r_merge^k m_c^k and lambda_z = sum r_merge^k lambda_c^k.
        which again means m_z = r_merge * m_c and lambda_z = r_merge * lambda_c expressed as dot products.
        */
        LargeRing z_m;
        if (id_ != 0) { // Dealer does not have m part
            z_m = std::inner_product(std::begin(r_merge), std::end(r_merge), std::begin(c_ms), LargeRing(0));
        }
        LargeRing z_l = std::inner_product(std::begin(r_merge), std::end(r_merge), std::begin(c_ls), LargeRing(0));

        // Now, we actually do the "renaming" to x and y from step 10, as x and y will be the variable
        // names across all iterations.
        std::vector<LargeRing> x = std::move(a);
        std::vector<LargeRing> y = std::move(b);

        // ### Pi_verify step 12 ###
        // Repeat Pi_reduceDeg until vector sizes are compression_factor_ or smaller
        while (y.size() > compression_factor_) { // use y as x empty for dealer
            std::cout << "Reducing vector size from " << y.size() << "..." << std::endl;

            // ### Pi_reduceDeg step 1 ###
            // Done by Dealer and parties
            // Split x, y into compression_factor_ evenly sized vectors, using zero-padding if needed
            /*
            We do not use actual new variables for the split results here; instead, we simply interpret
            the ranges [0:d], [d:2*d], [2*d:3*d], ... as the new vectors x^1, ..., x^compression_factor_
            and y^1, ... where d = ceil(y.size / compression_factor_).
            Note that 0 padding does not change the dot product result.
            */
            size_t d = y.size() / compression_factor_; // potentially rounded down.
            if (y.size() % compression_factor_ != 0) // if rounded down, +1 to have ceil
                d += 1;
            size_t padding = d * compression_factor_ - y.size();
            for (size_t i = 0; i < padding; i++) {
                if (id_ != 0) // Dealer does not have x
                    x.push_back(LargeRing(0));
                y.push_back(LargeRing(0));
            }

            // ### Pi_reduceDeg step 2 ###
            // Done by parties
            // Each party P_i locally computes compression_factor_^2-1 many dot products
            // z_i^{j,k} = x^j * y_i^k for all j, k in 1,...,compression_factor_ except j=k=1.
            // Here, this is x[j*d:(j+1)*d] * y[k*d:(k+1)*d] as in the code, we instead count j,k in
            // 0,...,compression_factor_-1.
            // Crucial to parallelize this, this is the main computational bottleneck.
            std::vector<LargeRing> local_zs; // to be filled with z_i^{0,1}, z_i^{0,2},...,z_i^{1,0}, z_i^{1,1}, z_i^{1,2},... (with 0-indexing)
            if (id_ != 0) {
                local_zs.resize(compression_factor_ * compression_factor_ - 1);
                #pragma omp parallel for collapse(2)
                for (size_t i = 0; i < compression_factor_; i++) {
                    for (size_t j = 0; j < compression_factor_; j++) {
                        if (i == 0 && j == 0)
                            continue;
                        local_zs[i * compression_factor_ + j - 1] = std::inner_product(std::begin(x) + i * d, std::begin(x) + i * d + d, std::begin(y) + j * d, LargeRing(0));
                    }
                }
            }

            // ### Pi_reduceDeg step 3 ###
            // Done by Dealer + parties
            // run shareSum for all compression_factor_^2-1 dot products
            auto [z_ms_temp, z_ls_temp] = shareSum(local_zs, compression_factor_ * compression_factor_ - 1, pking_verify_ >= 1);
            // Rename because otherwise, OpenMP will later complain "capturing a structured binding is not yet supported in OpenMP"...
            std::vector<LargeRing> z_ms = std::move(z_ms_temp);
            std::vector<LargeRing> z_ls = std::move(z_ls_temp);

            // ### Pi_reduceDeg step 4 ###
            // Done by Dealer + parties
            // Compute remaining z^{1,1} (z^{0,0} here with 0-indexing) as z minus the other z^{i,i}
            LargeRing remaining_z_m;
            if (id_ != 0) { // Dealer does not have m part
                LargeRing given = 0;
                for (size_t i = 1; i < compression_factor_; i++) {
                    // z_ms structure 0/1 0/2 ... 0/C-1 1/0 1/1 1/2 ... 1/C-1 2/0 2/1 2/2 ... 2/C-1 ...
                    // entry i/0 is at position i * C - 1, entry i/i is at position i * C + i - 1
                    given += z_ms[i * compression_factor_ + i - 1];
                }
                remaining_z_m = z_m - given;
            }
            LargeRing given = 0;
            for (size_t i = 1; i < compression_factor_; i++) {
                // same as above
                given += z_ls[i * compression_factor_ + i - 1];
            }
            LargeRing remaining_z_l = z_l - given;

            // ### Pi_reduceDeg step 5 ###
            // Done by Dealer + parties
            // sample compression_factor_ many random alphas and betas => sample <seed>, expand
            emp::PRG prg_reduce = generateNewPRG(false); // verify delayed, see §4.2.5
            std::vector<LargeRing> alpha_reduce, beta_reduce;
            alpha_reduce.resize(compression_factor_);
            prg.random_data(alpha_reduce.data(), compression_factor_ * sizeof(LargeRing));
            beta_reduce.resize(compression_factor_);
            prg.random_data(beta_reduce.data(), compression_factor_ * sizeof(LargeRing));

            // ### Pi_reduceDeg step 6 ###
            // Done by Dealer + parties
            // compute new x, y, z as:
            /*
            sum alpha^k x^k = sum alpha^k x[k*d,(k+1)*d]
            sum beta^k y^k = sum beta^k y[k*d,(k+1)*d]
            sum sum alpha^j beta^k z^{j,k}
            */
            std::vector<LargeRing> x_new, y_new;
            if (id_ != 0) { // Dealer does not have x
                x_new.resize(d);
                for (size_t k = 0; k < compression_factor_; k++) {
                    LargeRing alpha = alpha_reduce[k];
                    #pragma omp parallel for
                    for (size_t i = 0; i < d; i++) {
                        x_new[i] += alpha * x[k * d + i];
                    }
                }
            }
            y_new.resize(d);
            for (size_t k = 0; k < compression_factor_; k++) {
                LargeRing beta = beta_reduce[k];
                #pragma omp parallel for
                for (size_t i = 0; i < d; i++) {
                    y_new[i] += beta * y[k * d + i];
                }
            }
            LargeRing z_new_m;
            if (id_ != 0) { // Dealer does not have m part
                z_new_m = 0;
                #pragma omp parallel for reduction(+ : z_new_m) collapse(2)
                for (size_t i = 0; i < compression_factor_; i++) {
                    for (size_t j = 0; j < compression_factor_; j++) {
                        // z_ms structure 0/1 0/2 ... 0/C-1 1/0 1/1 1/2 ... 1/C-1 2/0 2/1 2/2 ... 2/C-1 ...
                        // entry i/0 is at position i * C - 1, entry i/j is at position i * C + j - 1
                        if (i == 0 && j == 0)
                            z_new_m += alpha_reduce[0] * beta_reduce[0] * remaining_z_m;
                        else
                            z_new_m += alpha_reduce[i] * beta_reduce[j] * z_ms[i * compression_factor_ + j - 1];
                    }
                }
            }
            LargeRing z_new_l = 0;
            #pragma omp parallel for reduction(+ : z_new_l) collapse(2)
            for (size_t i = 0; i < compression_factor_; i++) {
                for (size_t j = 0; j < compression_factor_; j++) {
                    // same as above
                    if (i == 0 && j == 0)
                        z_new_l += alpha_reduce[0] * beta_reduce[0] * remaining_z_l;
                    else
                        z_new_l += alpha_reduce[i] * beta_reduce[j] * z_ls[i * compression_factor_ + j - 1];
                }
            }
            // Replace prior x, y, [z] by new values for the next iteration
            x = std::move(x_new);
            y = std::move(y_new);
            z_m = std::move(z_new_m);
            z_l = std::move(z_new_l);

            std::cout << "==> reduced to " << y.size() << std::endl;
        }

        // ### Pi_verify step 13 ###
        // Run Pi_checkTriple
        std::cout << "Reducing vector size from " << y.size() << "..." << std::endl;
        size_t d = y.size();

        // ### Pi_checkTriple step 1 ###
        // Done by Dealer + parties
        // Compute random <.> sharing for y_0, we instead write to the back y[d] which is more efficient
        // carefully taking this re-indexing into consideration in the next steps
        if (id_ == 0) {
            LargeRing rnd = 0;
            for (size_t i = 1; i <= nP_; i++) {
                rnd += getRandomLargeElem(rgen_.as_D_with_i(i));
            }
            y.push_back(rnd);
        } else {
            y.push_back(getRandomLargeElem(rgen_.me_and_D()));
        }

        // ### Pi_checkTriple step 2 ###
        // Done by parties
        // Each party P_i locally computes d^2+d-1 many products
        // z_i^{j,k} = x^j * y_i^k for all j=1,...,d and k=0,...,d except j=k=1.
        // Here, this is x[j] * y[k] as in the code, we instead count j,k in 0,...,compression_factor_-1,
        // and instead of k=0 in the paper, we have the appended entry from step 1 at k=d
        std::vector<LargeRing> local_zs; // to be filled with z_i^{0,1}, z_i^{0,2},...,z_i^{0,d},
                                         // z_i^{1,0}, z_i^{1,1}, z_i^{1,2},...,z_i^{1,d}, ... (with 0-indexing)
        if (id_ != 0) {
            local_zs.resize(d * d + d - 1);
            #pragma omp parallel for collapse(2)
            for (size_t i = 0; i < d; i++) {
                for (size_t j = 0; j < d + 1; j++) {
                    if (i == 0 && j == 0)
                        continue;
                    local_zs[i * (d + 1) + j - 1] = x[i] * y[j];
                }
            }
        }

        // ### Pi_checkTriple step 3 ###
        // Done by Dealer + parties
        // run shareSum for all d^2+d-1 products
        auto [z_ms_temp, z_ls_temp] = shareSum(local_zs, d * d + d - 1, pking_verify_ >= 1);
        // Rename because otherwise, OpenMP will later complain "capturing a structured binding is not yet supported in OpenMP"...
        std::vector<LargeRing> z_ms = std::move(z_ms_temp);
        std::vector<LargeRing> z_ls = std::move(z_ls_temp);

        // ### Pi_checkTriple step 4 ###
        // Done by Dealer + parties
        // Compute remaining z^{1,1} (z^{0,0} here with 0-indexing) as z minus the other z^{i,i}
        LargeRing remaining_z_m;
        if (id_ != 0) { // Dealer does not have m part
            LargeRing given = 0;
            for (size_t i = 1; i < d; i++) {
                // z_ms structure 0/1 0/2 ... 0/d-1 0/d 1/0 1/1 1/2 ... 1/d-1 1/d 2/0 2/1 2/2 ... 2/d-1 2/d ...
                // entry i/0 is at position i * (d+1) - 1, entry i/i is at position i * (d+1) + i - 1
                given += z_ms[i * (d + 1) + i - 1];
            }
            remaining_z_m = z_m - given;
        }
        LargeRing given = 0;
        for (size_t i = 1; i < d; i++) {
            // same as above
            given += z_ls[i * (d + 1) + i - 1];
        }
        LargeRing remaining_z_l = z_l - given;

        // ### Pi_checkTriple step 5 ###
        // Done by Dealer + parties
        // sample d many random alphas and betas => sample <seed>, expand
        /*
        Run both, verify for Pi_rec^active and Pi_rec^consistent in parallel with getting new PRG
        seed (which is also included in the verification), which will detect any faults in these
        operations for the protocol up to this stage. See §4.2.5 on why we do this here.
        This will only lead to individual parties detecting if something went wrong. After step 6, we
        then let the parties communicate if they detected cheating to jointly decide to abort if
        necessary. This again is valid, because as per §4.2.5, the verification only needs to be done
        before Pi_checkTriple step 7. We here first proceed with non-interactive operations, as the
        goal is to verify just before step 7, we just already piggyback the communication on this
        last interaction before step 7 so that it does not require more communication rounds than
        necessary.
        */
        emp::PRG prg_reduce = generateNewPRG(true, true);
        std::vector<LargeRing> alpha_reduce, beta_reduce;
        alpha_reduce.resize(d);
        prg.random_data(alpha_reduce.data(), d * sizeof(LargeRing));
        beta_reduce.resize(d);
        prg.random_data(beta_reduce.data(), d * sizeof(LargeRing));
        beta_reduce.push_back(LargeRing(1)); // beta^0 from the protocol, which we have at position beta^d instead

        // ### Pi_checkTriple step 6 ###
        // Done by Dealer + parties
        // compute scalar x', y', z' as:
        /*
        sum alpha^k x_k
        sum beta^k y_k
        sum sum alpha^j beta^k z^{j,k}
        */
        LargeRing x_scalar, y_scalar;
        if (id_ != 0) { // Dealer does not have x
            x_scalar = 0;
            #pragma omp parallel for reduction(+ : x_scalar)
            for (size_t k = 0; k < d; k++) {
                x_scalar += alpha_reduce[k] * x[k];
            }
        }
        y_scalar = 0;
        #pragma omp parallel for reduction(+ : y_scalar)
        for (size_t k = 0; k < d; k++) {
            y_scalar += beta_reduce[k] * y[k];
        }
        // this sum also includes beta^d * y_d, but we set beta^d=1 above, so:
        y_scalar += y[d];
        LargeRing z_scalar_m;
        if (id_ != 0) { // Dealer does not have m part
            z_scalar_m = 0;
            #pragma omp parallel for reduction(+ : z_scalar_m) collapse(2)
            for (size_t i = 0; i < d; i++) {
                for (size_t j = 0; j < d + 1; j++) {
                    // z_ms structure 0/1 0/2 ... 0/d-1 0/d 1/0 1/1 1/2 ... 1/d-1 1/d 2/0 2/1 2/2 ... 2/d-1 2/d ...
                    // entry i/0 is at position i * (d+1) - 1, entry i/j is at position i * (d+1) + j - 1
                    if (i == 0 && j == 0)
                        z_scalar_m += alpha_reduce[0] * beta_reduce[0] * remaining_z_m;
                    else
                        z_scalar_m += alpha_reduce[i] * beta_reduce[j] * z_ms[i * (d + 1) + j - 1];
                }
            }
        }
        LargeRing z_scalar_l = 0;
        #pragma omp parallel for reduction(+ : z_scalar_l) collapse(2)
        for (size_t i = 0; i < d; i++) {
            for (size_t j = 0; j < d + 1; j++) {
                // same as above
                if (i == 0 && j == 0)
                    z_scalar_l += alpha_reduce[0] * beta_reduce[0] * remaining_z_l;
                else
                    z_scalar_l += alpha_reduce[i] * beta_reduce[j] * z_ls[i * (d + 1) + j - 1];
            }
        }
        std::cout << "==> reduced to scalars" << std::endl;
        // Check if all Pi_rec^active and Pi_rec^consistent were successfully verified, communication
        // for that was piggybacked onto Pi_checkTriple step 5 above to merge rounds and has set
        // pairwise_consistency_mismatch_ or opening_commit_mismatch_ if it detected any cheating.
        checkForDoubleAccept(!pairwise_consistency_mismatch_, !opening_commit_mismatch_, "ABORT: checkConsistency failed, some Pking distributed inconsistent values", "ABORT: openAddShare.verify failed, inconsistent hash values");

        // ### Pi_checkTriple step 7 ###
        // Done by Dealer + parties
        // Reconstruct y, z, check x * y = z
        // We want to start with reconstructing values. As per §4.2.5, this requires that all subprotocol
        // verifications for prior steps were executed and successful:
        assert(id_ == 0 || (number_unchecked_openings_ == 0 && number_unchecked_consistency_ == 0));
        // First, rerandomize y_scalar and z_scalar_l as we are running the "rerand" reconstruction
        // protocols on them.
        if (id_ == 0) {
            // For both, we need additive sharings of zero. We let the dealer non-interactively sample
            // random shares with P_2, ..., P_n, and then compute a matching share for P_1 so that
            // everything sums up to zero.
            randomizers_large_ring.resize(2);
            for (size_t k = 0; k < 2; k++) {
                LargeRing sum_other_shares = 0;
                for (size_t i = 2; i <= nP_; i++)
                    sum_other_shares += getRandomLargeElem(rgen_.as_D_with_i(i));
                randomizers_large_ring[k] = 0 - sum_other_shares;
                // No need for the dealer to add the randomizers to the full y_scalar and z_scalar_l
                // that it holds, as the full randomizers are zero.
            }
        } else if (id_ == 1) {
            // P_1 reads and uses randomizers that it received from Dealer in setup
            y_scalar += randomizers_large_ring[0];
            z_scalar_l += randomizers_large_ring[1];
        } else {
            // Other parties just add their non-interactively sampled shares
            y_scalar += getRandomLargeElem(rgen_.me_and_D());
            z_scalar_l += getRandomLargeElem(rgen_.me_and_D());
        }
        // Now, we can actually reconstruct the freshly randomized y_scalar and z_scalar_l
        std::vector<LargeRing> toOpen;
        toOpen.push_back(y_scalar);
        toOpen.push_back(z_scalar_l);
        /*
        As per §4.2.5 and as this is an opening, we also immediately run the verification for
        Pi_rec^active in parallel when we also run this protocol. Note that since the last verify of
        Pi_rec^consistent, this protocol has not been used, so we do not need a pairwise consistency
        check here. Note that we can let the parties exchange in parallel if something in Pi_rec^active.verify
        went wrong and if their final triple is incorrect. That is because if the verify rejects,
        this is because the adversary has sent incorrect shares to some honest parties which then
        reconstruct incorrect y_scalar or z_scalar_l. The error goes into this part additively, so
        the adversary knows the incorrect y_scalar or z_scalar_l that these honest parties have, while
        also learning the correct values itself. From that, it already knows if
        x_scalar * y_scalar == z_scalar_m + z_scalar_l for the honest parties, even if these detect
        an issue from the verify.
        */
        openAdditive(toOpen, true, pking_verify_ == 2, false);
        y_scalar = toOpen[0];
        z_scalar_l = toOpen[1];
        checkForDoubleAccept(!opening_commit_mismatch_, x_scalar * y_scalar == z_scalar_m + z_scalar_l, "ABORT: openAddShare.verify failed, inconsistent hash values", "ABORT: CheckTriple rejected");
        // finished Pi_checkTriple

        // ### Pi_verify step 14 ###
        // Done by Dealer and parties
        // Compute SSEC many v = Lambda + Gamma, open them, check if results are 0
        /*
        Each v^k has an m and a lambda (l) part.
        The m part is the m park of Gamma^k plus Lambda^k, the latter known by all online parties.
        The l part is the l part of Gamma^k.
        We need to rerandomize the l part, open it, add it to the m part, and check if that is 0.
        */
        // First, rerandomize the gamma_ls entries
        if (id_ == 0) {
            // We need additive sharings of zero. We let the dealer non-interactively sample
            // random shares with P_2, ..., P_n, and then compute a matching share for P_1 so that
            // everything sums up to zero.
            randomizers_ring.resize(SSEC);
            for (size_t k = 0; k < SSEC; k++) {
                Ring sum_other_shares = 0;
                for (size_t i = 2; i <= nP_; i++)
                    sum_other_shares += getRandomElem(rgen_.as_D_with_i(i));
                randomizers_ring[k] = 0 - sum_other_shares;
                // No need for the dealer to add the randomizers to the full gamma_ls[k]s
                // that it holds, as the full randomizers are zero.
            }
        } else if (id_ == 1) {
            // P_1 reads and uses randomizers that it received from Dealer in setup
            for (size_t k = 0; k < SSEC; k++)
                gamma_ls[k] += randomizers_ring[k];
        } else {
            // Other parties just add their non-interactively sampled shares
            for (size_t k = 0; k < SSEC; k++)
                gamma_ls[k] += getRandomElem(rgen_.me_and_D());
        }
        /*
        Again (as in Pi_checkTriple step 7), as per §4.2.5, we also immediately run the verification for
        Pi_rec^active in parallel. Again, since the last verify of Pi_rec^consistent, this protocol
        has not been used, so we do not need a pairwise consistency check here. We can let the parties
        exchange in parallel if something in Pi_rec^active.verify went wrong and if the result is
        unequal zero. That is because if the verify rejects, this is because the adversary has sent
        incorrect shares to some honest parties which then reconstruct incorrect v^k. The error goes
        into this part additively, so the adversary knows the incorrect v^k that these honest parties
        have, while also learning the correct values itself. From that, it already knows if v^k!=0
        for the honest parties, even if these detect an issue from the verify.
        */
        openAdditive(gamma_ls, true, pking_verify_ >= 1, false);
        if (id_ != 0) { // Besides the verification, the Dealer does not provide anything to the remaining check.
            bool accept = true;
            for (size_t k = 0; k < SSEC; k++) {
                accept &= (Lambdas[k] + gamma_ms[k] + gamma_ls[k] == 0);
            }
            checkForDoubleAccept(!opening_commit_mismatch_, accept, "ABORT: openAddShare.verify failed, inconsistent hash values", "ABORT: Final top-level verification check failed");
        }

        // There should be no unchecked opening remaining
        assert(id_ == 0 || number_unchecked_openings_ == 0);
        assert(id_ == 0 || number_unchecked_consistency_ == 0);

        if (id_ == 0)
            std::cout << "Finished precomputing verification protocol" << std::endl;
        else
            std::cout << "Verification finished successfully, no cheating was detected" << std::endl;
    }

    void Evaluator::checkForAccept(bool accept_self, std::string error_message) {
        if (id_ == 0) return; // Dealer runs no check as it runs in advance, on its own
        for (size_t i = 1; i <= nP_; i++) {
            if (i != id_) {
                network_->send(i, &accept_self, sizeof(bool));
            }
        }
        bool accept = accept_self;
        for (size_t i = 1; i <= nP_; i++) {
            if (i != id_) {
                bool accept_other;
                network_->recv(i, &accept_other, sizeof(bool));
                accept &= accept_other;
            }
        }
        proof_rounds_++; // 1 round: everybody sends and receives
        if (!accept) {
            throw std::runtime_error(error_message);
        }
    }

    void Evaluator::checkForDoubleAccept(bool accept_self_a, bool accept_self_b, std::string error_message_a, std::string error_message_b) {
        if (id_ == 0) return; // Dealer runs no check as it runs in advance, on its own
        char accept_self = (((char) accept_self_a) << 1) + (char) accept_self_b; // encode 2 bits in one char
        for (size_t i = 1; i <= nP_; i++) {
            if (i != id_) {
                network_->send(i, &accept_self, sizeof(char));
            }
        }
        char accept = accept_self;
        for (size_t i = 1; i <= nP_; i++) {
            if (i != id_) {
                char accept_other;
                network_->recv(i, &accept_other, sizeof(char));
                accept &= accept_other;
            }
        }
        proof_rounds_++; // 1 round: everybody sends and receives
        if (accept == 0) {
            throw std::runtime_error(error_message_a + " " + error_message_b);
        } else if (accept == 1) {
            throw std::runtime_error(error_message_a);
        } else if (accept == 2) {
            throw std::runtime_error(error_message_b);
        }
    }

    size_t Evaluator::getProofRounds() {
        return proof_rounds_;
    }

}; // namespace zkfliop
