from math import ceil, log

# Boyle et al., CRYPTO'21
# https://eprint.iacr.org/2022/261.pdf (page numbers below reference to this version)
# m: number multiplications
# n: number of parties
# k: ring size [bits]
# ssec: statistical security parameter
# csec: computational security parameter
#
# Given that the paper does assume a compression factor of 2, we just go with that
# (leading to optimal communication) and not also generalize the code to arbitrary factors.
def BGIN21_compiler(m, n, k, ssec, csec):
    offline = 0
    online = 0
    rounds = 0

    # We slightly adapt the proof here, as in 2022/261, it uses Beaver-style multiplications which
    # are 2x more expensive than our protocol with function-dependent preprocessing. So, we replace
    # occurences of |W| in 2022/261 by m (the number of multiplications). W originally was the set
    # of all circuit output wires and wires going into multiplications. We instead run the proof
    # not for multiplication inputs (and circuit outputs), but multiplication outputs, given the
    # differences between Beaver-like multiplication and multiplication on masked secret sharing.

    # For rings, the proof needs to use an extension ring. According to p. 18: The soundness error
    # is at most |W| / 2^d for extension degree d. To have this below 2^-ssec, we need
    # d >= ssec + log_2 |W| ==> d >= ssec + log_2 m.
    # (Internally, the zkFLIOP also needs to run on an extension ring, but there, the degree needs
    # to be at least ssec + log log m, see https://eprint.iacr.org/2025/787.pdf p. 60, so our larger
    # extension also works for the zkFLIOP and is needed for the first random linear combination.)
    d = ssec + log(m, 2)
    # Continue to assume all numbers (not seeds) are k * d bits

    # Sample random alpha_w (p. 14): instantiate via our Pi_rnd ==> reconstruct one seed using
    # P_king (verification batched, see later)
    online += (2 * n - 2) * csec # seed has size csec
    rounds += 2

    # Each party secret shares Gamma_i (p. 14), no need for D to send s_i in advance if we assume
    # pre-shared keys
    online += n * (n - 1) * k * d
    rounds += 1

    # Recover single value p using P_king (p. 14) (verification batched, see later)
    online += (2 * n - 2) * k * d
    rounds += 2

    # Still, we need to prove all Gamma_i to be correct via zk-FLIOPs (p. 14f.).
    # Note that each of n parties will need to act as a prover in its own zk-FLIOP.
    # For the zk-FLIOPs instantiation, we follow §B.2 from the Appendix of
    # https://eprint.iacr.org/2022/261.pdf

    rho = ceil(log(2 * m, 2)) - 1 # p. 38

    # The following is done for rho - 1 proof rounds (omitting local operations):
    # - Prover sends proof (consisting of three values) to all other parties (p. 38)
    # - Dealer samples and hands to parties a random challenge (p. 38)
    
    # step 1:
    online += n * (rho - 1) * 3 * (n - 1) * k * d # n proofs, rho - 1 rounds, 3 elements to n-1 parties
    rounds += 1
    # step 2: use one seed and use across all n proofs, this is an obvious optimization and the
    # original protocol would be even less efficient. Using our Pi_rnd ==> reconstruct one seed using
    # P_king (verification batched, see later)
    online += (rho - 1) * (2 * n - 2) * csec # rho - 1 rounds, P_king, 1 seed
    rounds += 2

    # Last round (omitting local operations):
    # - Prover sends proof (consisting of 7 values) to all other parties (p. 39)
    #       (on p. 40, 8 instead of 7 are used, but 7 seems correct, also see
    #           https://eprint.iacr.org/2025/787.pdf p. 60)
    # - Dealer samples and hands to parties a random challenge (p. 39)
    # - Dealer samples and hands to the parties random challenges (p. 40)
    # - Dealer hands to the parties 4 values (p. 40)

    # step 1:
    online += n * 7 * (n - 1) * k * d # n proofs, 7 elements to n-1 parties
    rounds += 1
    # step 2: use one seed and use across all n proofs as above with Pi_rnd
    online += (2 * n - 2) * csec # rho - 1 rounds, P_king, 1 seed
    rounds += 2
    # step 3: use one seed and use across all n proofs as above with Pi_rnd
    online += (2 * n - 2) * csec # rho - 1 rounds, P_king, 1 seed
    rounds += 2 # this may be parallelized, but not described in https://eprint.iacr.org/2025/787.pdf
                # and we in the end anyway only report communication volume
    # step 4: we can have parties hold additive shares that they now just have to reconstruct via P_king
    online += n * 4 * (2 * n - 2) * k * d # n proofs, 4 elements, P_king
    rounds += 2

    # Now, we need some verify calls, i.e., for sampling seeds and other reconstructions.
    # They are batched and run in parallel with some of the prior rounds.

    # Need verify for Pi_rec^consistent at least once:
    online += (n * n - n) * 2 * csec # each party sends to each other party one hash of size 2*csec
    # Need at least one verify for Pi_rec^active (for random seeds and final step 4) for the zk-FLIOPs
    # ==> trivial to batch over all n individual zk-FLIOPs
    offline += n * 2 * csec # dealer sends to each party one 2*csec bit hash
    online += (2 * n - 2) * 3 * csec # P_king for the nonce r
    # Also, parties coordinate among themselves if zk-FLIOPs succeeded:
    online += n * n - n
    rounds += 1
    # Now, the final check for p = 0 on p. 14 again needs a Pi_rec^active check as this is a
    # reconstruction. Cannot batch with the prior one, as p must only be reconstructed after running
    # all zk-FLIOPs successfully.
    offline += n * 2 * csec # dealer sends to each party one 2*csec bit hash
    online += (2 * n - 2) * 3 * csec # P_king for the nonce r

    return (offline, online, rounds)

# Our Verification Protocol
# m: number multiplications
# compr: compression factor
# n: number of parties
# k: ring size [bits]
# s: additional bits for extended ring (verification running on k+s bits)
# ssec: statistical security parameter
# csec: computational security parameter
# bc_const: Use broadcast (instead of P_king) when reconstructing a constant number of elements
# bc_others: Use broadcast (instead of P_king) when reconstructing more than constant many elements
#
# Note that pking-verify from our protocol implementation relates to bc_const, bc_others as follows:
# - pking-verify = 0 : bc_const = bc_others = True
# - pking-verify = 1 : bc_const = True, bc_others = False
# - pking-verify = 2 : bc_const = bc_others = False
def our_verification(m, compr, n, k, s, ssec, csec, bc_const, bc_others):
    offline = 0
    online = 0
    rounds = 0

    p_king_factor = 2 * n - 2 # Number of messages sent for reconstructing with P_king
    broadcast_factor = n * n - n # Number of messages sent for reconstructing by broadcasting

    # Pi_verify

    # step 1: Pi_rnd, only reconstruct seed (verify delayed until later)
    if bc_const:
        online += broadcast_factor * csec
        rounds += 1
    else:
        online += p_king_factor * csec
        rounds += 2

    # step 7: Pi_shareSum (ssec instances, each one reconstruction over the large ring)
    if bc_others:
        online += ssec * broadcast_factor * (k + s)
        rounds += 1
    else:
        online += ssec * p_king_factor * (k + s)
        rounds += 2

    # step 9: Pi_rnd, only reconstruct seed (verify delayed until later)
    if bc_const:
        online += broadcast_factor * csec
        rounds += 1
    else:
        online += p_king_factor * csec
        rounds += 2

    # step 12: Repeat Pi_reduceDeg until dimensions reduced from 3m to at most compr
    dim = 3 * m
    while dim > compr:
        (sub_offline, sub_online, sub_rounds, new_dim) = reduce_degree(dim, compr, n, k, s, csec, bc_const, bc_others)
        offline += sub_offline
        online += sub_online
        rounds += sub_rounds
        dim = new_dim

    # step 13: Pi_checkTriple
    (sub_offline, sub_online, sub_rounds) = check_triple(dim, n, k, s, csec, bc_const, bc_others)
    offline += sub_offline
    online += sub_online
    rounds += sub_rounds

    # step 14: open ssec many values
    offline += ssec * k # zero-sharings for rerandomization
    if bc_others:
        online += ssec * broadcast_factor * k
        rounds += 1
    else:
        online += ssec * p_king_factor * k
        rounds += 2

    # Run verify steps: (also for subprotocols)
    # - Pi_rec^consistent.verify and Pi_rec^active.verify run during Pi_checkTriple step 5 (parallel)
    # - Then, the parties sync if any party detected a fault
    # - Pi_checkTriple step 7 runs Pi_rec^active.verify in parallel
    # - Then, the parties sync if any party detected a fault or the triple is incorrect
    # - Pi_verify step 14 runs Pi_rec^active.verify in parallel
    # - Then, the parties sync if any party detected a fault or and linear combination was != 0
    online += broadcast_factor * 2 * csec # Hashes for consistent
    offline += n * 2 * csec # Distribute commits for active
    if bc_const:
        online += broadcast_factor * 3 * csec # Open nonces for active
    else:
        online += p_king_factor * 3 * csec # Open nonces for active
    online += broadcast_factor # sync
    rounds += 1 # sync
    offline += n * 2 * csec # Distribute commits for active
    if bc_const:
        online += broadcast_factor * 3 * csec # Open nonces for active
    else:
        online += p_king_factor * 3 * csec # Open nonces for active
    online += broadcast_factor # sync
    rounds += 1 # sync
    offline += n * 2 * csec # Distribute commits for active
    if bc_others: # parallel with opening ssec many values ==> also use bc then, otherwise not
        online += broadcast_factor * 3 * csec # Open nonces for active
    else:
        online += p_king_factor * 3 * csec # Open nonces for active
    online += broadcast_factor # sync
    rounds += 1 # sync

    # Check conditions for Theorem 1
    assert 2 * ceil(log(3 * m, compr)) + 1 <= ssec
    assert s >= 3 * (2 * ceil(log(3 * m, compr)) + 1)
    assert s >= ssec + (2 * ceil(log(3 * m, compr)) + 1) * (0.5 + log(2.5 + 3.0 * ssec / (2 * ceil(log(3 * m, compr)) + 1), 2))

    return (offline, online, rounds)

# Pi_reduceDeg of our verification protocol
# dim: Vectors' dimension
# compr: compression factor
# n: number of parties
# k: ring size [bits]
# s: additional bits for extended ring (verification running on k+s bits)
# csec: computational security parameter
# bc_const: Use broadcast (instead of P_king) when reconstructing a constant number of elements
# bc_others: Use broadcast (instead of P_king) when reconstructing more than constant many elements
def reduce_degree(dim, compr, n, k, s, csec, bc_const, bc_others):
    offline = 0
    online = 0
    rounds = 0
    new_dim = ceil (dim / compr)

    p_king_factor = 2 * n - 2 # Number of messages sent for reconstructing with P_king
    broadcast_factor = n * n - n # Number of messages sent for reconstructing by broadcasting

    # Step 3: compr * compr - 1 instances of Pi_shareSum
    sharings = compr * compr - 1
    if bc_others:
        online += sharings * broadcast_factor * (k + s)
        rounds += 1
    else:
        online += sharings * p_king_factor * (k + s)
        rounds += 2

    # Step 5: Pi_rnd, only reconstruct seed (verify delayed until later)
    if bc_const:
        online += broadcast_factor * csec
        rounds += 1
    else:
        online += p_king_factor * csec
        rounds += 2

    return (offline, online, rounds, new_dim)

# Pi_checkTriple of our verification protocol
# m: number multiplications
# compr: compression factor
# n: number of parties
# k: ring size [bits]
# s: additional bits for extended ring (verification running on k+s bits)
# csec: computational security parameter
# bc_const: Use broadcast (instead of P_king) when reconstructing a constant number of elements
# bc_others: Use broadcast (instead of P_king) when reconstructing more than constant many elements
#
# !!! Pi_[...].verify NOT included !!!
# As many of the .verify calls in the protocol are delayed and batched, it makes more sense to
# analyze them while viewing the verification as a whole. Hence, their complexities are included 
# in our_verification() and not here, so check_triple() only makes sense in the context of
# our_verification()
def check_triple(d, n, k, s, csec, bc_const, bc_others):
    offline = 0
    online = 0
    rounds = 0

    p_king_factor = 2 * n - 2 # Number of messages sent for reconstructing with P_king
    broadcast_factor = n * n - n # Number of messages sent for reconstructing by broadcasting

    # Step 3: d * d + d - 1 instances of Pi_shareSum
    sharings = d * d + d - 1
    if bc_others:
        online += sharings * broadcast_factor * (k + s)
        rounds += 1
    else:
        online += sharings * p_king_factor * (k + s)
        rounds += 2
    
    # Step 5: Pi_rnd, only reconstruct seed (verify delayed until later)
    if bc_const:
        online += broadcast_factor * csec
        rounds += 1
    else:
        online += p_king_factor * csec
        rounds += 2

    # Step 7: 2 instances of Pi_rec^{active,rerand}
    offline += 2 * (k + s) # zero-sharings for rerandomization
    if bc_const:
        online += 2 * broadcast_factor * (k + s)
        rounds += 1
    else:
        online += 2 * p_king_factor * (k + s)
        rounds += 2

    return (offline, online, rounds)

# Verification protocol if built via LEDHHZS24 instead
# (https://eprint.iacr.org/2024/700.pdf)
# m: number multiplications
# compr: compression factor
# n: number of parties
# k: ring size [bits]
# s: additional bits for extended ring (verification running on k+s bits)
# ssec: statistical security parameter
# csec: computational security parameter
# bc_const: Use broadcast (instead of P_king) when reconstructing a constant number of elements
# bc_others: Use broadcast (instead of P_king) when reconstructing more than constant many elements
#
# Note that pking-verify from our protocol implementation relates to bc_const, bc_others as follows:
# - pking-verify = 0 : bc_const = bc_others = True
# - pking-verify = 1 : bc_const = True, bc_others = False
# - pking-verify = 2 : bc_const = bc_others = False
#
# This is similar to our_verification(). Changes and important notes are highlighted by "#!!".
# For fairness, we still use the same randoms over all n individual proofs, as this is a trivial
# optimization.
def LEDHHZS24(m, compr, n, k, s, ssec, csec, bc_const, bc_others):
    offline = 0
    online = 0
    rounds = 0

    p_king_factor = 2 * n - 2 # Number of messages sent for reconstructing with P_king
    broadcast_factor = n * n - n # Number of messages sent for reconstructing by broadcasting

    # Pi_verify

    # step 1: Pi_rnd, only reconstruct seed (verify delayed until later)
    #!! LEDHHZS24 also uses a seed here too, only other random samplings to not use a PRG
    if bc_const:
        online += broadcast_factor * csec
        rounds += 1
    else:
        online += p_king_factor * csec
        rounds += 2

    # step 7: Pi_shareSum (ssec instances, each one reconstruction over the large ring)
    #!! LEDHHZS24 lets each party secret share one value instead of using one Pi_shareSum
    online += n * ssec * (n - 1) * (k + s)
    rounds += 1

    # step 9: Pi_rnd, only reconstruct seed (verify delayed until later)
    #!! §5.2 Reusing randomness in batches, thus, no *n
    #!! yet, LEDHHZS24 individually samples ssec many randoms without a PRG!
    #!! Hence, simply reconstruct ssec many coefficients
    if bc_others:
        online += ssec * broadcast_factor * (k + s)
        rounds += 1
    else:
        online += ssec * p_king_factor * (k + s)
        rounds += 2

    # step 12: Repeat Pi_reduceDeg until dimensions reduced from 3m to at most compr
    dim = 3 * m
    while dim > compr:
        #!! just swapped function here to LEDHHZS24 version
        (sub_offline, sub_online, sub_rounds, new_dim) = LEDHHZS24_reduce_degree(dim, compr, n, k, s, csec, bc_const, bc_others)
        offline += sub_offline
        online += sub_online
        rounds += sub_rounds
        dim = new_dim

    # step 13: Pi_checkTriple
    #!! just swapped function here to LEDHHZS24 version
    (sub_offline, sub_online, sub_rounds) = LEDHHZS24_check_triple(dim, n, k, s, csec, bc_const,bc_others)
    offline += sub_offline
    online += sub_online
    rounds += sub_rounds

    # step 14: open ssec many values
    offline += ssec * k # zero-sharings for rerandomization
    if bc_others:
        online += ssec * broadcast_factor * k
        rounds += 1
    else:
        online += ssec * p_king_factor * k
        rounds += 2

    # Run verify steps: (also for subprotocols)
    # - Pi_rec^consistent.verify and Pi_rec^active.verify run during Pi_checkTriple step 5(parallel)
    # - Then, the parties sync if any party detected a fault
    # - Pi_checkTriple step 7 runs Pi_rec^active.verify in parallel
    # - Then, the parties sync if any party detected a fault or the triple is incorrect
    # - Pi_verify step 14 runs Pi_rec^active.verify in parallel
    # - Then, the parties sync if any party detected a fault or and linear combination was != 0
    online += broadcast_factor * 2 * csec # Hashes for consistent
    offline += n * 2 * csec # Distribute commits for active
    if bc_const:
        online += broadcast_factor * 3 * csec # Open nonces for active
    else:
        online += p_king_factor * 3 * csec # Open nonces for active
    online += broadcast_factor # sync
    rounds += 1 # sync
    offline += n * 2 * csec # Distribute commits for active
    if bc_const:
        online += broadcast_factor * 3 * csec # Open nonces for active
    else:
        online += p_king_factor * 3 * csec # Open nonces for active
    online += broadcast_factor # sync
    rounds += 1 # sync
    offline += n * 2 * csec # Distribute commits for active
    if bc_others: # parallel with opening ssec many values ==> also use bc then, otherwise not
        online += broadcast_factor * 3 * csec # Open nonces for active
    else:
        online += p_king_factor * 3 * csec # Open nonces for active
    online += broadcast_factor # sync
    rounds += 1 # sync

    # Check conditions for Theorem 1
    assert 2 * ceil(log(3 * m, compr)) + 1 <= ssec
    assert s >= 3 * (2 * ceil(log(3 * m, compr)) + 1)
    assert s >= ssec + (2 * ceil(log(3 * m, compr)) + 1) * (0.5 + log(2.5 + 3.0 * ssec / (2 *ceil(log(3 * m, compr)) + 1), 2))

    return (offline, online, rounds)

# Pi_reduceDeg of our verification protocol if built via LEDHHZS24 instead
# dim: Vectors' dimension
# compr: compression factor
# n: number of parties
# k: ring size [bits]
# s: additional bits for extended ring (verification running on k+s bits)
# csec: computational security parameter
# bc_const: Use broadcast (instead of P_king) when reconstructing a constant number of elements
# bc_others: Use broadcast (instead of P_king) when reconstructing more than constant many elements
#
# This is similar to reduce_degree(). Changes and important notes are highlighted by "#!!".
# For fairness, we still use the same randoms over all n individual proofs, as this is a trivial
# optimization.
def LEDHHZS24_reduce_degree(dim, compr, n, k, s, csec, bc_const, bc_others):
    offline = 0
    online = 0
    rounds = 0
    new_dim = ceil (dim / compr)

    p_king_factor = 2 * n - 2 # Number of messages sent for reconstructing with P_king
    broadcast_factor = n * n - n # Number of messages sent for reconstructing by broadcasting

    # Step 3: compr * compr - 1 instances of Pi_shareSum
    #!! LEDHHZS24 lets each party secret share one value instead of using one Pi_shareSum
    sharings = compr * compr - 1
    online += n * sharings * (n - 1) * (k + s)
    rounds += 1

    # Step 5: Pi_rnd, only reconstruct seed (verify delayed until later)
    #!! §5.2 Reusing randomness in batches, thus, no *n
    #!! yet, LEDHHZS24 individually samples 2 * compr many randoms without a PRG!
    #!! Hence, simply reconstruct 2 * compr many coefficients
    if bc_const:
        online += 2 * compr * broadcast_factor * (k + s)
        rounds += 1
    else:
        online += 2 * compr * p_king_factor * (k + s)
        rounds += 2

    return (offline, online, rounds, new_dim)

# Pi_checkTriple of our verification protocol if built via LEDHHZS24 instead
# m: number multiplications
# compr: compression factor
# n: number of parties
# k: ring size [bits]
# s: additional bits for extended ring (verification running on k+s bits)
# csec: computational security parameter
# bc_const: Use broadcast (instead of P_king) when reconstructing a constant number of elements
# bc_others: Use broadcast (instead of P_king) when reconstructing more than constant many elements
#
# !!! Pi_[...].verify NOT included !!!
# As many of the .verify calls in the protocol are delayed and batched, it makes more sense to
# analyze them while viewing the verification as a whole. Hence, their complexities are included 
# in LEDHHZS24() and not here, so LEDHHZS24_check_triple() only makes sense in the context of
# LEDHHZS24()
#
# This is similar to check_triple(). Changes and important notes are highlighted by "#!!".
# For fairness, we still use the same randoms over all n individual proofs, as this is a trivial
# optimization.
def LEDHHZS24_check_triple(d, n, k, s, csec, bc_const, bc_others):
    offline = 0
    online = 0
    rounds = 0

    p_king_factor = 2 * n - 2 # Number of messages sent for reconstructing with P_king
    broadcast_factor = n * n - n # Number of messages sent for reconstructing by broadcasting

    # Step 3: d * d + d - 1 instances of Pi_shareSum
    #!! LEDHHZS24 lets each party secret share one value instead of using one Pi_shareSum
    sharings = d * d + d - 1
    online += n * sharings * (n - 1) * (k + s)
    rounds += 1
    
    # Step 5: Pi_rnd, only reconstruct seed (verify delayed until later)
    #!! §5.2 Reusing randomness in batches, thus, no *n
    #!! yet, LEDHHZS24 individually samples 2 * d many randoms without a PRG!
    #!! Hence, simply reconstruct 2 * d many coefficients
    if bc_const:
        online += 2 * d * broadcast_factor * (k + s)
        rounds += 1
    else:
        online += 2 * d * p_king_factor * (k + s)
        rounds += 2

    # Step 7: 2 instances of Pi_rec^{active,rerand}
    #!! This now instead runs in n individual proofs
    offline += n * 2 * (k + s) # zero-sharings for rerandomization
    if bc_const:
        online += n * 2 * broadcast_factor * (k + s)
        rounds += 1
    else:
        online += n * 2 * p_king_factor * (k + s)
        rounds += 2

    return (offline, online, rounds)
