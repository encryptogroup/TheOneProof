#pragma once

#include <algorithm>
#include <array>
#include <boost/format.hpp>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "types.h"

namespace common::utils {

using wire_t = size_t;

enum GateType {
  kInp,
  kAdd,
  kMul,
  kConstMul,
  kDotprod,
  kInvalid,
  NumGates
};

std::ostream& operator<<(std::ostream& os, GateType type);

// Gates represent primitive operations.
// All gates have one output.
struct Gate {
  GateType type{GateType::kInvalid};
  wire_t out;

  Gate() = default;
  Gate(GateType type, wire_t out);

  virtual ~Gate() = default;
};

// Represents a gate with fan-in 2.
struct FIn2Gate : public Gate {
  wire_t in1{0};
  wire_t in2{0};

  FIn2Gate() = default;
  FIn2Gate(GateType type, wire_t in1, wire_t in2, wire_t out);
};

// Represents a gate used to denote SIMD operations.
// These type is used to represent operations that take vectors of inputs but
// might not necessarily be SIMD e.g., dot product.
struct SIMDGate : public Gate {
  std::vector<wire_t> in1{0};
  std::vector<wire_t> in2{0};

  SIMDGate() = default;
  SIMDGate(GateType type, std::vector<wire_t> in1, std::vector<wire_t> in2,
           wire_t out);
};

// Represents gates where one input is a constant.
template <class R>
struct ConstOpGate : public Gate {
  wire_t in{0};
  R cval;

  ConstOpGate() = default;
  ConstOpGate(GateType type, wire_t in, R cval, wire_t out)
      : Gate(type, out), in(in), cval(std::move(cval)) {}
};

using gate_ptr_t = std::shared_ptr<Gate>;

// Gates ordered by multiplicative depth.
//
// Addition gates are not considered to increase the depth.
// Moreover, if gates_by_level[l][i]'s output is input to gates_by_level[l][j]
// then i < j.
struct LevelOrderedCircuit {
  size_t num_gates;
  std::array<uint64_t, GateType::NumGates> count;
  std::vector<wire_t> outputs;
  std::vector<std::vector<gate_ptr_t>> gates_by_level;

  friend std::ostream& operator<<(std::ostream& os,
                                  const LevelOrderedCircuit& circ);
};

/**
 * This extends the idea of LevelOrderedCircuit by requiring strict ordering:
 * If any gate gets an input from any other gate, the other gate must be on some prior level.
 * Contrast to LevelOrderedCircuit: Here, non-independent non-interactive gates are
 * NOT anymore allowed on the same layer.
 * Reason: Easy local parallelization over all gates on the same layer.
 * 
 * Furthermore, each layer of the circuit is required to either only consist of multiplication
 * and dot product gates, or not contain any of them.
 */
struct StrictLevelOrderedCircuit {
  size_t num_gates;
  std::array<uint64_t, GateType::NumGates> count;
  std::vector<wire_t> outputs;
  std::vector<std::vector<gate_ptr_t>> gates_by_level;
  std::vector<size_t> level_mults;

  friend std::ostream& operator<<(std::ostream& os,
                                  const StrictLevelOrderedCircuit& circ);
};

// Represents an arithmetic circuit.
template <class R>
class Circuit {
  std::vector<wire_t> outputs_;
  std::vector<gate_ptr_t> gates_;

  bool isWireValid(wire_t wid) { return wid < gates_.size(); }

 public:
  Circuit() = default;

  // Methods to manually build a circuit.
  wire_t newInputWire() {
    wire_t wid = gates_.size();
    gates_.push_back(std::make_shared<Gate>(GateType::kInp, wid));
    return wid;
  }

  void setAsOutput(wire_t wid) {
    if (!isWireValid(wid)) {
      throw std::invalid_argument("Invalid wire ID.");
    }

    outputs_.push_back(wid);
  }

  // Function to add a gate with fan-in 2.
  wire_t addGate(GateType type, wire_t input1, wire_t input2) {
    if (type != GateType::kAdd && type != GateType::kMul) {
      throw std::invalid_argument("Invalid gate type.");
    }

    if (!isWireValid(input1) || !isWireValid(input2)) {
      throw std::invalid_argument("Invalid wire ID.");
    }

    wire_t output = gates_.size();
    gates_.push_back(std::make_shared<FIn2Gate>(type, input1, input2, output));

    return output;
  }

  // Function to add a gate with one input from a wire and a second constant
  // input.
  wire_t addConstOpGate(GateType type, wire_t wid, R cval) {
    if (type != kConstMul) {
      throw std::invalid_argument("Invalid gate type.");
    }

    if (!isWireValid(wid)) {
      throw std::invalid_argument("Invalid wire ID.");
    }

    wire_t output = gates_.size();
    gates_.push_back(std::make_shared<ConstOpGate<R>>(type, wid, cval, output));

    return output;
  }

  // Function to add a multiple fan-in gate.
  wire_t addGate(GateType type, const std::vector<wire_t>& input1,
                 const std::vector<wire_t>& input2) {
    if (type != GateType::kDotprod) {
      throw std::invalid_argument("Invalid gate type.");
    }

    if (input1.size() != input2.size()) {
      throw std::invalid_argument("Expected same length inputs.");
    }

    for (size_t i = 0; i < input1.size(); ++i) {
      if (!isWireValid(input1[i]) || !isWireValid(input2[i])) {
        throw std::invalid_argument("Invalid wire ID.");
      }
    }

    wire_t output = gates_.size();
    gates_.push_back(std::make_shared<SIMDGate>(type, input1, input2, output));
    return output;
  }

  // Level ordered gates are helpful for evaluation.
  [[nodiscard]] LevelOrderedCircuit orderGatesByLevel() const {
    LevelOrderedCircuit res;
    res.outputs = outputs_;
    res.num_gates = gates_.size();

    // Map from output wire id to multiplicative depth/level.
    // Input gates have a depth of 0.
    std::vector<size_t> gate_level(res.num_gates, 0);
    size_t depth = 0;

    // This assumes that if gates_[i]'s output is input to gates_[j] then
    // i < j.
    for (const auto& gate : gates_) {
      switch (gate->type) {
        case GateType::kAdd: {
          const auto* g = static_cast<FIn2Gate*>(gate.get());
          gate_level[g->out] = std::max(gate_level[g->in1], gate_level[g->in2]);
          break;
        }

        case GateType::kMul: {
          const auto* g = static_cast<FIn2Gate*>(gate.get());
          gate_level[g->out] =
              std::max(gate_level[g->in1], gate_level[g->in2]) + 1;
          break;
        }

        case GateType::kConstMul: {
          const auto* g = static_cast<ConstOpGate<R>*>(gate.get());
          gate_level[g->out] = gate_level[g->in];
          break;
        }

        case GateType::kDotprod: {
          const auto* g = static_cast<SIMDGate*>(gate.get());
          size_t gate_depth = 0;
          for (size_t i = 0; i < g->in1.size(); ++i) {
            gate_depth = std::max(
                {gate_level[g->in1[i]], gate_level[g->in2[i]], gate_depth});
          }
          gate_level[g->out] = gate_depth + 1;
          break;
        }

        default:
          break;
      }

      depth = std::max(depth, gate_level[gate->out]);
    }

    std::fill(res.count.begin(), res.count.end(), 0);

    std::vector<std::vector<gate_ptr_t>> gates_by_level(depth + 1);
    for (const auto& gate : gates_) {
      res.count[gate->type]++;
      gates_by_level[gate_level[gate->out]].push_back(gate);
    }

    res.gates_by_level = std::move(gates_by_level);

    return res;
  }

  [[nodiscard]] StrictLevelOrderedCircuit strictlyOrderGatesByLevel() const {
    // First, do normal leveling to minimize round complexity.
    // If we would immediately strictly level, the levels might become less
    // but at the price of requiring more interactive levels ==> more round complexity.
    LevelOrderedCircuit std_leveled = orderGatesByLevel();

    StrictLevelOrderedCircuit res;
    res.outputs = std_leveled.outputs;
    res.num_gates = std_leveled.num_gates;
    res.count = std_leveled.count;

    std::vector<size_t> gate_level(res.num_gates, 0);
    size_t current_layer_base_level = 0;
    size_t depth = 0;

    // Now, go multiplicative layer by layer
    for (const auto& mult_layer : std_leveled.gates_by_level) {
      for (const auto& gate : mult_layer) {
        switch (gate->type) {
          case GateType::kAdd:
          case GateType::kMul:
          {
            const auto* g = static_cast<FIn2Gate*>(gate.get());
            // gate is 1 higher than highest input gate, but also must be at least the current layer's base level.
            gate_level[g->out] = std::max(current_layer_base_level, std::max(gate_level[g->in1], gate_level[g->in2]) + 1);
            break;
          }

          case GateType::kConstMul: {
            const auto* g = static_cast<ConstOpGate<R>*>(gate.get());
            // gate is 1 higher than input gate, but also must be at least the current layer's base level.
            gate_level[g->out] = std::max(current_layer_base_level, gate_level[g->in] + 1);
            break;
          }

          case GateType::kDotprod: {
            const auto* g = static_cast<SIMDGate*>(gate.get());
            size_t gate_depth = 0;
            for (size_t i = 0; i < g->in1.size(); ++i) {
              gate_depth = std::max(
                  {gate_level[g->in1[i]], gate_level[g->in2[i]], gate_depth});
            }
            // gate is 1 higher than highest input gate, but also must be at least the current layer's base level.
            gate_level[g->out] = std::max(current_layer_base_level, gate_depth + 1);
            break;
          }

          case GateType::kInp: break;

          default: {
            throw std::runtime_error("Don't know how to handle this gate");
          }
        }

        depth = std::max(depth, gate_level[gate->out]);
      }

      current_layer_base_level = depth + 1;
    }


    std::vector<std::vector<gate_ptr_t>> gates_by_level(depth + 1);
    for (const auto& mult_layer : std_leveled.gates_by_level) {
      for (const auto& gate : mult_layer) {
        gates_by_level[gate_level[gate->out]].push_back(gate);
      }
    }

    std::vector<size_t> level_mults(depth + 1);
    for (size_t i = 0; i < depth + 1; i++) {
      size_t num_mults = 0;
      for (const auto& gate : gates_by_level[i]) {
        if (gate->type == GateType::kMul || gate->type == GateType::kDotprod) {
          num_mults ++;
        }
      }
      // Otherwise, will get problems with index of gate in layer not corresponding
      // to the right place to save communication to.
      // Should be naturally given as first, each layer has multiplications followed by 
      // additions and would these additions not depend on the multiplications, hence
      // being on the same layer, they would even originally be on an earlier layer.
      assert(num_mults == gates_by_level[i].size() || num_mults == 0);
      level_mults[i] = num_mults;
    }

    res.gates_by_level = std::move(gates_by_level);
    res.level_mults = std::move(level_mults);

    return res;
  }
};
};  // namespace common::utils
