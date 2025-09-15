/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)

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

#include <atomic>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "../memory.h"
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

enum class EmbeddedNNUEType {
    BIG,
    SMALL,
    FALCON,
};

using NetworkOutput = std::tuple<Value, Value>;

template<typename Arch, typename Transformer>
class Network {
    static constexpr IndexType FTDimensions = Arch::TransformedFeatureDimensions;

   public:
    struct Weights {
        Weights();
        LargePagePtr<Transformer> featureTransformer;
        AlignedPtr<Arch[]>        network;
    };
    using WeightsPtr = std::shared_ptr<Weights>;

    Network(EvalFile file, EmbeddedNNUEType type) :
        weights(std::make_shared<Weights>()),
        evalFile(file),
        embeddedType(type) {}

    Network(const Network& other);
    Network(Network&& other) = default;

    Network& operator=(const Network& other);
    Network& operator=(Network&& other) = default;

    void load(const std::string& rootDirectory, std::string evalfilePath);
    bool save(const std::optional<std::string>& filename) const;

    WeightsPtr weights_handle() const;

    NetworkOutput evaluate(const Position&                         pos,
                           AccumulatorStack&                       accumulatorStack,
                           AccumulatorCaches::Cache<FTDimensions>* cache) const;


    void verify(std::string evalfilePath, const std::function<void(std::string_view)>&) const;
    NnueEvalTrace trace_evaluate(const Position&                         pos,
                                 AccumulatorStack&                       accumulatorStack,
                                 AccumulatorCaches::Cache<FTDimensions>* cache) const;

   private:
    void load_user_net(const std::string&, const std::string&);
    void load_internal();

    WeightsPtr allocate_weights() const;

    bool save(std::ostream&, const std::string&, const std::string&, const Weights&) const;
    std::optional<std::string> load(std::istream&);

    bool read_header(std::istream&, std::uint32_t*, std::string*) const;
    bool write_header(std::ostream&, std::uint32_t, const std::string&) const;

    bool read_parameters(std::istream&, std::string&, Weights&) const;
    bool write_parameters(std::ostream&, const std::string&, const Weights&) const;

    // Input feature converter
    WeightsPtr weights;

    EvalFile         evalFile;
    EmbeddedNNUEType embeddedType;

    // Hash value of evaluation function structure
    static constexpr std::uint32_t hash = Transformer::get_hash_value() ^ Arch::get_hash_value();

    template<IndexType Size>
    friend struct AccumulatorCaches::Cache;

    friend class AccumulatorStack;
};

// Definitions of the network types
using SmallFeatureTransformer = FeatureTransformer<TransformedFeatureDimensionsSmall>;
using SmallNetworkArchitecture =
  NetworkArchitecture<TransformedFeatureDimensionsSmall, L2Small, L3Small>;

using BigFeatureTransformer  = FeatureTransformer<TransformedFeatureDimensionsBig>;
using BigNetworkArchitecture = NetworkArchitecture<TransformedFeatureDimensionsBig, L2Big, L3Big>;

using NetworkBig   = Network<BigNetworkArchitecture, BigFeatureTransformer>;
using NetworkSmall = Network<SmallNetworkArchitecture, SmallFeatureTransformer>;
// Falcon network now uses the small-network architecture
using NetworkFalcon = Network<SmallNetworkArchitecture, SmallFeatureTransformer>;


struct Networks {
    Networks(NetworkBig&& nB, NetworkSmall&& nS, NetworkFalcon&& nF) :
        big(std::move(nB)),
        small(std::move(nS)),
        falcon(std::move(nF)) {}

    NetworkBig    big;
    NetworkSmall  small;
    NetworkFalcon falcon;
};


}  // namespace Stockfish

#endif
