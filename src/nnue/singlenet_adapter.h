#ifndef NNUE_SINGLENET_ADAPTER_H_INCLUDED
#define NNUE_SINGLENET_ADAPTER_H_INCLUDED

#include "../evaluate.h"
#include "network.h"

namespace Stockfish::Eval::NNUE::Adapter {

inline auto& active_network(Networks& networks) noexcept { return networks.network; }

inline const auto& active_network(const Networks& networks) noexcept { return networks.network; }

inline AccumulatorCaches::Cache<TransformedFeatureDimensionsBig>&
active_cache(AccumulatorCaches& caches) noexcept {
    return caches.big;
}

inline const AccumulatorCaches::Cache<TransformedFeatureDimensionsBig>&
active_cache(const AccumulatorCaches& caches) noexcept {
    return caches.big;
}



inline const char* active_eval_file_name() noexcept { return EvalFileDefaultName; }

}  // namespace Stockfish::Eval::NNUE::Adapter

#endif
