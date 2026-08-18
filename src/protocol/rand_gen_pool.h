#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wvla-cxx-extension"
#include <emp-tool/emp-tool.h>
#pragma GCC diagnostic pop

#include <vector>
#include <algorithm>

#include "../io/netmp.h"

namespace rzkf {

// Collection of PRGs.
class RandGenPool {
  int id_;

  /**
   * Interpretation depends on the party.
   * For P0 a.k.a. the Dealer, index i contains the PRG shared by Dealer and Pi.
   * For Pi (i>0), index j contains the PRG shared by Dealer, Pi and Pj. If i=j, this is just Dealer and Pi.
   */
  std::vector<emp::PRG> prgs;

  /**
   * This shall only be constructed by the Dealer (P0).
   * Location (i,j) contains the PRG shared by Dealer, Pi and Pj.
   * We require that i > j.
   */
  std::vector<std::vector<emp::PRG>> dealer_double_prgs;
  

 public:
  explicit RandGenPool(int my_id, int num_parties, std::shared_ptr<io::NetIOMP> network);

  emp::PRG& me_and_D();
  emp::PRG& as_D_with_i(size_t i);
};

};  // namespace rzkf
