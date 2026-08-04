/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include "../misc.h"
#include "../types.h"
#include "nnue_accumulator.h"
#include "nnue_architecture.h"
#include "nnue_common.h"
#include "nnue_feature_transformer.h"
#include "nnue_misc.h"

namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE {

using NetworkOutput = std::tuple<Value, Value>;

// The network must be a trivial type, i.e. the memory must be in-line.
// This is required to allow sharing the network via shared memory, as
// there is no way to run destructors.
template<typename Arch, typename Transformer>
class NetworkImpl {
    static constexpr IndexType FTDimensions = Arch::TransformedFeatureDimensions;

   public:
    NetworkImpl() = default;

    NetworkImpl(const NetworkImpl& other) = default;
    NetworkImpl(NetworkImpl&& other)      = default;

    NetworkImpl& operator=(const NetworkImpl& other) = default;
    NetworkImpl& operator=(NetworkImpl&& other)      = default;

    void load(const std::filesystem::path& rootDirectory, std::filesystem::path evalfilePath,
              EvalFile& evalFile);
    bool save(const EvalFile& evalFile,
              const std::optional<std::filesystem::path>& filename) const;

    std::size_t get_content_hash() const;

    NetworkOutput evaluate(const Position&                         pos,
                           AccumulatorStack&                       accumulatorStack,
                           AccumulatorCaches::Cache<FTDimensions>& cache) const;


    void verify(const std::function<void(std::string_view)>&, const EvalFile& evalFile,
                std::filesystem::path evalfilePath) const;
    NnueEvalTrace trace_evaluate(const Position&                         pos,
                                 AccumulatorStack&                       accumulatorStack,
                                 AccumulatorCaches::Cache<FTDimensions>& cache) const;

   private:
    void load_user_net(const std::filesystem::path&, const std::filesystem::path&, EvalFile&);
    void load_internal(EvalFile&);

    void initialize();

    bool                       save(std::ostream&, const std::filesystem::path&, const std::string&) const;
    std::optional<std::string> load(std::istream&);

    bool read_header(std::istream&, std::uint32_t*, std::string*) const;
    bool write_header(std::ostream&, std::uint32_t, const std::string&) const;

    bool read_parameters(std::istream&, std::string&);
    bool write_parameters(std::ostream&, const std::string&) const;

    // Input feature converter
    Transformer featureTransformer;

    // Evaluation function
    Arch network[LayerStacks];


    bool initialized = false;

    // Hash value of evaluation function structure
    static constexpr std::uint32_t hash = Transformer::get_hash_value() ^ Arch::get_hash_value();

    template<IndexType Size>
    friend struct AccumulatorCaches::Cache;
};

// Definitions of the network types


class Network:
    public NetworkImpl<NetworkArchitecture<TransformedFeatureDimensions, L2, L3>,
                       FeatureTransformer<TransformedFeatureDimensions>> {
   public:
    using NetworkImpl::NetworkImpl;
};


}  // namespace Stockfish

template<typename ArchT, typename FeatureTransformerT>
struct std::hash<Stockfish::Eval::NNUE::NetworkImpl<ArchT, FeatureTransformerT>> {
    std::size_t operator()(
      const Stockfish::Eval::NNUE::NetworkImpl<ArchT, FeatureTransformerT>& network) const noexcept {
        return network.get_content_hash();
    }
};

template<>
struct std::hash<Stockfish::Eval::NNUE::Network> {
    std::size_t operator()(const Stockfish::Eval::NNUE::Network& network) const noexcept {
        return network.get_content_hash();
    }
};

#endif
