#include "semi_evaluator.h"

#include <array>
#include <numeric>

namespace semi
{
    Evaluator::Evaluator(int nP, int id, std::shared_ptr<io::NetIOMP> network,
                                     common::utils::StrictLevelOrderedCircuit circ,
                                     bool pking_semi)
        : nP_(nP),
          id_(id),
          rgen_(id, nP, network),
          network_(std::move(network)),
          circ_(std::move(circ)),
          wires_(circ.num_gates),
          setup_gates_(circ.num_gates),
          output_randomizers_ring(),
          pking_semi_(pking_semi) {}

    /// Just for convenience: Generates one random Ring element from the provided PRG
    Ring getRandomElem(emp::PRG &prg) {
        Ring out;
        prg.random_data(&out, sizeof(Ring));
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
                            break;
                        }
                        case common::utils::GateType::kConstMul: {
                            // For each constant multiplication gate, we just locally multiply the input wire's mask with the constant
                            auto *g = static_cast<common::utils::ConstOpGate<Ring> *>(gate.get());
                            wires_[g->out].getLambda() = wires_[g->in].getLambda() * g->cval;
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
                                for (size_t j = 1; j <= nP_; j++) {
                                    Ring x;
                                    if (i == j) {
                                        x = getRandomElem(rgen_.as_D_with_i(i));
                                    } else {
                                        x = 0;
                                    }
                                    fullLambda += x;
                                }
                                wires_[gate->out].getLambda() = fullLambda;
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
                            std::vector<Ring> lambdaShares(nP_ + 1);
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
                                }
                                Ring last_x = gamma;
                                Ring last_y = getRandomElem(rgen_.as_D_with_i(nP_));
                                lambdaShares[nP_] = last_y;
                                fullLambda += last_y;
                                gamma_to_send.push_back(last_x); // remaining share

                                wires_[gate->out].getLambda() = fullLambda;
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
        }

        // Receive all broadcasts
        for (size_t i = 1; i <= nP_; i++) { // from i
            if (i != id_) {
                to_receive_from[i].resize(to_receive_from_size[i]);
                network_->recv(i, to_receive_from[i].data(), sizeof(Ring) * to_receive_from_size[i]);
            }
        }

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
            reconstruct(mult_vals, pking_semi_);
        }
        evaluateGatesAtDepthPartyRecv(depth, mult_vals);
    }

    void Evaluator::setupOutputPhase() {
        if (id_ == 0) {
            // Output phase uses Pi_rec^{rerand}, so we need rerandomizers.
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
            // Hand P_1 its rerandomizer shares
            network_->send(1, output_randomizers_ring.data(), circ_.outputs.size() * sizeof(Ring));
        } else {
            if (id_ == 1) {
                // Receive all rerandomizer shares as party 1
                output_randomizers_ring.resize(circ_.outputs.size());
                network_->recv(0, output_randomizers_ring.data(), circ_.outputs.size() * sizeof(Ring));
            } else {
                // Non-interactively sample shares of rerandomizers
                output_randomizers_ring.resize(circ_.outputs.size());
                for (size_t k = 0; k < circ_.outputs.size(); k++) {
                    output_randomizers_ring[k] = getRandomElem(rgen_.me_and_D());
                }
            }
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

        // open shares
        reconstruct(lambdas, pking_semi_);

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
    }

    template <class R>
    void Evaluator::reconstruct(std::vector<R> &vals, bool use_pking) {
        if (use_pking) {
            reconstructPking(vals);
        } else {
            reconstructBroadcast(vals);
        }
    }

    template <class R>
    void Evaluator::reconstructPking(std::vector<R> &vals) {
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

        // P_king receives shares and sums up all shares
        for (size_t provider = 1; provider <= nP_; provider++) { // Iterate over batches per provider
            if (id_ == provider) // What is provided by me is already written to vals as initial value
                continue;
            // Receive data
            if (id_ != nP_) { // Not last block
                std::vector<R> received(per_party_comm);
                network_->recv(provider, received.data(), sizeof(R) * per_party_comm);
                // add this share to the value to be reconstructed
                for (size_t i = 0; i < per_party_comm; i++) {
                    vals[(id_ - 1) * per_party_comm + i] += received[i];
                }
            } else { // Last block
                std::vector<R> received(last_party_comm);
                network_->recv(provider, received.data(), sizeof(R) * last_party_comm);
                // add this share to the value to be reconstructed
                for (size_t i = 0; i < last_party_comm; i++) {
                    vals[(id_ - 1) * per_party_comm + i] += received[i];
                }
            }
        }
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
    }

    template <class R>
    void Evaluator::reconstructBroadcast(std::vector<R> &vals) {
        assert(id_ != 0);

        // Send own share to all others
        for (size_t p = 1; p <= nP_; p++) {
            if (id_ == p) continue;
            network_->send(p, vals.data(), sizeof(R) * vals.size());
        }
        // Receive from all others
        for (size_t p = 1; p <= nP_; p++) {
            if (id_ == p) continue;
            std::vector<R> received(vals.size());
            network_->recv(p, received.data(), sizeof(R) * vals.size());
            // add this share to the value to be reconstructed
            for (size_t i = 0; i < vals.size(); i++) {
                vals[i] += received[i];
            }
        }
    }

}; // namespace semi
