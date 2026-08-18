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

namespace zkfliop
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

    /// Number of additive shares (dealer knows all) which have been opened without verifying they
    /// were opened correctly. This just is to keep track, as this number should in the end be 0.
    size_t number_unchecked_openings_;
    /// Hash used to verify openings of additive shares where the dealer knows all.
    emp::Hash check_opening_hash_;
    /// List of all commits for opened additive shares (excluding output phase)
    std::deque<std::array<char, emp::Hash::DIGEST_SIZE>> commits;
    /// Commit for opened additive shares for the output phase
    std::array<char, emp::Hash::DIGEST_SIZE> output_commit;
    /// Additive share of the commit nonce for the output phase
    Seed output_nonce[3];

    /// List of zero-sharings for rerandomization (inside the proof) over the standard domain Ring.
    /// This is only used for P_1's share, as all other shares are derived from RNGs.
    std::vector<Ring> randomizers_ring;
    /// List of zero-sharings for rerandomization over the extended domain LargeRing.
    /// This is only used for P_1's share, as all other shares are derived from RNGs.
    std::vector<LargeRing> randomizers_large_ring;
    /// List of zero-sharings for rerandomization over the standard domain Ring, for the circuit output.
    /// This is only used for P_1's share, as all other shares are derived from RNGs.
    std::vector<Ring> output_randomizers_ring;

    /// Number of additive shares (unknown to dealer) which have been opened without verifying
    /// consistency, i.e., that each honest party got the same resulting value.
    /// This just is to keep track, as this number should in the end be 0.
    size_t number_unchecked_consistency_;
    /// Hash used to verify consistency of openings of additive shares unknown to the dealer.
    emp::Hash pking_consistency_hash_;

    size_t proof_rounds_; /// Counter for the proof's communication rounds
    size_t compression_factor_; /// Compression factor parameter \omega
    /// true if P_king strategy to be used for the multiplication protocol, false if broadcasting instead
    bool pking_semi_;
    /// 2 to use P_king strategy for all reconstructions during verification, 
    /// 1 to use P_king strategy only if many values are reconstructed in parallel and broadcast otherwise,
    /// 0 to always use broadcast.
    int pking_verify_;

    /// Flag indicating that the consistent reconstruction check detected a fault
    bool pairwise_consistency_mismatch_;
    /// Flag indicating that the actively secure reconstruction check detected a fault
    bool opening_commit_mismatch_;

  public:
    Evaluator(int nP, int id, std::shared_ptr<io::NetIOMP> network,
                    common::utils::StrictLevelOrderedCircuit circ,
                    size_t compression_factor, bool pking_semi, int pking_verify);

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
     * @param checkConsistency true if pairwise consistency check is to be run in parallel for all prior outstanding consistency checks, false by default.
     */
    template <class R>
    void reconstruct(std::vector<R> &vals, bool include_in_pairwise_consistency_check, bool use_pking, bool use_xor = false, bool piggyback_nonce = false, Seed *nonce = nullptr, bool checkConsistency = false);

    /// See reconstruct(), but this is the case where use_pking is set to true
    template <class R>
    void reconstructPking(std::vector<R> &vals, bool include_in_pairwise_consistency_check, bool use_xor = false, bool piggyback_nonce = false, Seed *nonce = nullptr, bool checkConsistency = false);

    /// See reconstruct(), but this is the case where use_pking is set to false
    template <class R>
    void reconstructBroadcast(std::vector<R> &vals, bool include_in_pairwise_consistency_check, bool use_xor = false, bool piggyback_nonce = false, Seed *nonce = nullptr, bool checkConsistency = false);

    /// Runs the pairwise consistency check (for consistent reconstruction), aborts protocol if there is a mismatch
    void checkConsistency();

    /**
     * Opens an additive sharing where the dealer also knows all shares, i.e., the actively secure opening protocol.
     * This does not necessarily immediately run verification and detect cheating.
     * 
     * opening_commit_mismatch_ will be set to true if verify was set to true and any issue was detected.
     * 
     * @tparam R Type of the sharings, i.e., Ring or LargeRing
     * @param shares Vector of sharings, each value is the Party's own share, will be set to the plaintext value by running this method.
     *              For the Dealer, this will already be the plaintext value as input.
     * @param verify true if verification should also run in parallel (for this and prior unchecked openings), false otherwise
     * @param use_pking true if using P_king strategy, false if using broadcasting instead
     * @param use_xor true if the sharings are XOR sharings, false if they are standard additive sharings
     * @param checkConsistency Run an independent pairwise consistency check in parallel, just to parallelize its communication.
     */
    template <class R>
    void openAdditive(std::vector<R> &shares, bool verify, bool use_pking, bool use_xor, bool checkConsistency = false);

    /**
     * The shareSum protocol (vectorized over multiple instances), where each party provides an input
     * and the output is a star sharing of the sum of all inputs.
     * 
     * @tparam R Type of the sharings, i.e., Ring or LargeRing
     * @param inputs Vector of the party's inputs for all shareSum instances (empty vector for the Dealer)
     * @param n Number of vectorized shareSum instances (= inputs.size() for the parties, but not the Dealer)
     * @param use_pking true if using P_king strategy, false if using broadcasting instead
     * @return std::pair<std::vector<R>, std::vector<R>> Vectors of each sum's m and lambda (shares), m uninitialized for the dealer.
     */
    template <class R>
    std::pair<std::vector<R>, std::vector<R>> shareSum(std::vector<R> &inputs, size_t n, bool use_pking);

    /**
     * Mostly the rnd protocol from the paper, but instead of returning many randoms, it just returns
     * the PRG for generating these randoms.
     * 
     * @param verify true if verification should also run in parallel (for actively secure openings), false otherwise
     * @param checkConsistency Run an independent pairwise consistency check in parallel, just to parallelize its communication, false by default.
     * @return emp::PRG The PRG with the newly derived seed.
     */
    emp::PRG generateNewPRG(bool verify, bool checkConsistency = false);

    /**
     * This runs part of the verification setup/preprocessing and will automatically be called by runSetup().
     * 
     * To be exact, verify() is used by the Dealer in the setup and the other parties online.
     * During setup, the Dealer uses verify() to populate collections of data that it needs to send
     * to other parties. setup_verify() then lets the dealer send this data to the parties, who receive
     * and store it. The other parties run verify() online where they then consume this data.
     */
    void setup_verify();

    /**
     * Runs the verification.
     * 
     * Note that the dealer will run this in the setup phase (called by setup_verify()) to distribute
     * the correct data to the online parties. The other online parties will run verify() at another time,
     * in the online phase, where they consume the data received before.
     */
    virtual void verify();

    /**
     * Aborts the protocol execution if accept_self of any party is false.
     * 
     * @param accept_self The party's own accept_self
     * @param error_message Error message to print of any party wants to abort, i.e., has accept_self set to false
     */
    void checkForAccept(bool accept_self, std::string error_message);

    /// Like checkForAccept, but running two checks with their individual error messages in parallel.
    void checkForDoubleAccept(bool accept_self_a, bool accept_self_b, std::string error_message_a, std::string error_message_b);

    /// Returns the number of communication rounds required by the proof
    size_t getProofRounds();

    /// Runs part A of the required setup for the output phase of getOutputs()
    /// Run this before running the main runSetup()
    void setupOutputPhase_A();

    /// Runs part B of the required setup for the output phase of getOutputs()
    /// Run this after running the main runSetup()
    void setupOutputPhase_B();

    // Returns list of output values, all parties receive all outputs in our code.
    std::vector<Ring> getOutputs();

    // Evaluate online phase for circuit
    void evaluateCircuit();
  };

  /// Just for convenience: Generates one random Ring element from the provided PRG
  Ring getRandomElem(emp::PRG &prg);

  /// Just for convenience: Generates one random LargeRing element from the provided PRG
  LargeRing getRandomLargeElem(emp::PRG &prg);

  /// Just for convenience: Generates one random (PRG) Seed from the provided PRG
  Seed getRandomSeed(emp::PRG &prg);

}; // zkfliop
