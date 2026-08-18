#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "../io/netmp.h"
#include "../utils/circuit.h"
#include "preproc.h"
#include "rand_gen_pool.h"
#include "sharing.h"
#include "../utils/types.h"

using namespace common::utils;

namespace semi
{
  class Evaluator
  {
  protected:
    int nP_; /// The number of parties (excluding the dealer)
    int id_; /// id of the party, Dealer: 0, others: 1,...,nP_
    rzkf::RandGenPool rgen_; /// RNGs with pre-shared keys
    std::shared_ptr<io::NetIOMP> network_; /// Network connections to other parties
    common::utils::StrictLevelOrderedCircuit circ_; /// Circuit to evaluate
    std::vector<rzkf::StarShare<Ring>> wires_; /// Mapping wire index to contained share
    /// Mapping gate output wire to preprocessing information to be stored for that gate
    std::vector<std::unique_ptr<rzkf::SetupGate<Ring>>> setup_gates_;

    /// List of zero-sharings for rerandomization over the standard domain Ring, for the circuit output.
    /// This is only used for P_1's share, as all other shares are derived from RNGs.
    std::vector<Ring> output_randomizers_ring;
    bool pking_semi_;

  public:
    Evaluator(int nP, int id, std::shared_ptr<io::NetIOMP> network,
                    common::utils::StrictLevelOrderedCircuit circ,
                    bool pking_semi);

    /// Runs the Setup/Preprocessing phase
    /// for the given mapping of input wire IDs to the party providing the input
    virtual void runSetup(const std::unordered_map<common::utils::wire_t, int> &input_mapping);

    /// Sets the protocol inputs,
    /// for the given mappings of input wire IDs to the party providing the input and the actual value
    void setInputs(const std::unordered_map<common::utils::wire_t, int> &input_mapping, const std::unordered_map<common::utils::wire_t, Ring> &inputs);

    /// Computation preparing data to be sent for one circuit layer.
    virtual void evaluateGatesAtDepthPartySend(size_t depth, std::vector<Ring> &mult_vals);

    /// Computation for one circuit layer, using the received data for this layer to finalize the computations.
    virtual void evaluateGatesAtDepthPartyRecv(size_t depth, std::vector<Ring> mult_vals);

    /// Evaluate all gates at the given layer.
    void evaluateGatesAtDepth(size_t depth);

    /**
     * Lets the online party reconstruct a vector of additive or XOR sharings.
     * Depending on the arguments, this does not necessarily check that the reconstruction went correctly.
     * 
     * include_in_pairwise_consistency_check and checkConsistency must not both be true.
     * 
     * @tparam R Type of the sharings, i.e., Ring or LargeRing
     * @param vals Vector of sharings, each value is the Party's own share, will be set to the plaintext value by running this method.
     * @param include_in_pairwise_consistency_check true if pairwise consistency check is needed, i.e., this is a consistent reconstruction.
     * @param use_pking true if using P_king strategy, false if using broadcasting instead
     * @param use_xor true if the sharings are XOR sharings, false if they are standard additive sharings
     * @param piggyback_nonce true to reconstruct an additional XOR-shared nonce in parallel, for the verify of actively secure reconstruction.
     * @param nonce Pointer to the nonce, if piggyback_nonce set to true, nullptr per default
     */
    template <class R>
    void reconstruct(std::vector<R> &vals, bool use_pking);

    /// See reconstruct(), but this is the case where use_pking is set to true
    template <class R>
    void reconstructPking(std::vector<R> &vals);

    /// See reconstruct(), but this is the case where use_pking is set to false
    template <class R>
    void reconstructBroadcast(std::vector<R> &vals);

    /// Runs the required setup for the output phase of getOutputs()
    /// Run this before running the main runSetup()
    void setupOutputPhase();

    // Returns list of output values, all parties receive all outputs in our code.
    std::vector<Ring> getOutputs();

    // Evaluate online phase for circuit
    void evaluateCircuit();
  };

  /// Just for convenience: Generates one random Ring element from the provided PRG
  Ring getRandomElem(emp::PRG &prg);

}; // semi
