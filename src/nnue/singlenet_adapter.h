#ifndef NNUE_SINGLENET_ADAPTER_H_INCLUDED
#define NNUE_SINGLENET_ADAPTER_H_INCLUDED

#include "../evaluate.h"
#include "network.h"

namespace Stockfish::Eval::NNUE::Adapter {

inline NetworkBig& active_network(Networks& networks) noexcept { return networks.big; }

inline const NetworkBig& active_network(const Networks& networks) noexcept { return networks.big; }

inline AccumulatorCaches::Cache<TransformedFeatureDimensionsBig>&
active_cache(AccumulatorCaches& caches) noexcept {
    return caches.big;
}

inline const AccumulatorCaches::Cache<TransformedFeatureDimensionsBig>&
active_cache(const AccumulatorCaches& caches) noexcept {
    return caches.big;
}


inline NetworkSmall& secondary_network(Networks& networks) noexcept { return networks.small; }

inline const NetworkSmall& secondary_network(const Networks& networks) noexcept {
    return networks.small;
}

inline AccumulatorCaches::Cache<TransformedFeatureDimensionsSmall>&
secondary_cache(AccumulatorCaches& caches) noexcept {
    return caches.small;
}

inline const AccumulatorCaches::Cache<TransformedFeatureDimensionsSmall>&
secondary_cache(const AccumulatorCaches& caches) noexcept {
    return caches.small;
}

inline const char* active_eval_file_name() noexcept { return EvalFileDefaultName; }

}  // namespace Stockfish::Eval::NNUE::Adapter

#endif
