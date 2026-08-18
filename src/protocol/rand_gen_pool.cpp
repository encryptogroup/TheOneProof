#include "rand_gen_pool.h"

#include <algorithm>

namespace rzkf {

  RandGenPool::RandGenPool(int my_id, int num_parties, std::shared_ptr<io::NetIOMP> network) 
    : id_{my_id} {

  /*
  Let dealer sample all keys and send them to the respective parties.
  */
  if (my_id == 0) {
    prgs.resize(num_parties + 1);

    // prgs[0] has secure random seed from default constructor, use prgs[0] to derive other seeds.
    for (size_t i = 1; i < num_parties + 1; i++) {
      std::array<uint64_t, 2> seed = {0, 0};
      prgs[0].random_data(seed.data(), 2 * sizeof(uint64_t));
      network->send(i, seed.data(), 2 * sizeof(uint64_t)); // Send seed to P_i
      emp::block seed_block = emp::makeBlock(seed[1], seed[0]);
      prgs[i].reseed(&seed_block, 0);
    }
  } else {
    prgs.resize(1);

    // Receive seed from dealer:
    std::array<uint64_t, 2> seed = {0, 0};
    network->recv(0, seed.data(), 2 * sizeof(uint64_t));
    emp::block seed_block = emp::makeBlock(seed[1], seed[0]);
    prgs[0].reseed(&seed_block, 0);
  }
}

emp::PRG& RandGenPool::me_and_D() {
  assert(id_ > 0);
  return prgs[0];
}

emp::PRG& RandGenPool::as_D_with_i(size_t i){
  assert(id_ == 0);
  assert(i > 0);
  return prgs[i];
}

}  // namespace rzkf
