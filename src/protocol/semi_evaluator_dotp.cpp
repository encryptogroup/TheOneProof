#include "semi_evaluator_dotp.h"

#include <array>
#include <numeric>

namespace semi
{
    /*
    override Evaluator as we need to add custom setup for dot products
    */
    void DotPEvaluator::runSetup(const std::unordered_map<common::utils::wire_t, int> &input_mapping)
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
                        case common::utils::GateType::kDotprod: // interactive
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
                        case common::utils::GateType::kDotprod: {
                            // Compute gamma.
                            // This corresponds to Pi_mult steps 1 and 2.
                            // Dealer and each party except nP_ non-interactively sample their share,
                            // Dealer computes the remaining share and adds it to gamma_to_send which
                            // will later be sent to party nP_ as one single, big message.

                            // Also compute the output mask lambda_z
                            std::vector<Ring> lambdaShares(nP_ + 1);
                            if (id_ == 0) {
                                auto *g = static_cast<common::utils::SIMDGate *>(gate.get());
                                Ring gamma = 0;
                                for (size_t i = 0; i < g->in1.size(); i++) {
                                    gamma += wires_[g->in1[i]].getLambda() * wires_[g->in2[i]].getLambda();
                                }

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
                    case common::utils::GateType::kMul:
                    case common::utils::GateType::kDotprod:
                    {
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

    /*
    override Evaluator as we need to add custom setup for dot products
    */
    void DotPEvaluator::evaluateGatesAtDepthPartySend(size_t depth, std::vector<Ring> &mult_vals)
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

            case common::utils::GateType::kDotprod:
            {
                auto *g = static_cast<common::utils::SIMDGate *>(gate.get());
                if (id_ != 0)
                {
                    auto *setup_mat = static_cast<rzkf::SetupMult<Ring> *>(setup_gates_[gate->out].get());

                    // This is Pi_mult step 3, generalized to vectors:
                    Ring val = 0;
                    for (size_t i = 0; i < g->in1.size(); i++) {
                        val += wires_[g->in1[i]].getM() * wires_[g->in2[i]].getLambda()
                                + wires_[g->in1[i]].getLambda() * wires_[g->in2[i]].getM();
                        if (id_ == 1) // Only one party must add the constant m_x * m_y
                            val += wires_[g->in1[i]].getM() * wires_[g->in2[i]].getM();
                    }
                    val += setup_mat->gamma_share - wires_[gate->out].getLambda();

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

    /*
    override Evaluator as we need to add custom setup for dot products
    */
    void DotPEvaluator::evaluateGatesAtDepthPartyRecv(size_t depth, std::vector<Ring> mult_vals)
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

            case common::utils::GateType::kDotprod:
            {
                auto *g = static_cast<common::utils::SIMDGate *>(gate.get());
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

}; // namespace semi
