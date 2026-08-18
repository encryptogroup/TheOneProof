#pragma once

#include "zkfliop_evaluator.h"

using namespace common::utils;

namespace zkfliop
{
  /*
  We are using a slightly changed Evaluator to support circuits that also contain dot products.
  Note that DotPEvaluator also supports circuits without dot products and is strictly more general
  than Evaluator. Yet, variances in the dot product dimensions make this general case more difficult
  to parallelize, so for circuits without dot products, the standard Evaluator should always be
  used.

  This is a subclass of Evaluator as we only need to add changes to some of its components.
  */
  class DotPEvaluator : public Evaluator {
    public:
      DotPEvaluator(int nP, int id, std::shared_ptr<io::NetIOMP> network,
                    common::utils::StrictLevelOrderedCircuit circ,
                    size_t compression_factor, bool pking_semi, int pking_verify) : Evaluator(nP, id, network, circ, compression_factor, pking_semi, pking_verify){};

      void runSetup(const std::unordered_map<common::utils::wire_t, int> &input_mapping) override;
      void evaluateGatesAtDepthPartySend(size_t depth, std::vector<Ring> &mult_vals) override;
      void evaluateGatesAtDepthPartyRecv(size_t depth, std::vector<Ring> mult_vals) override;
      void verify() override;
  };

}; // zkfliop
