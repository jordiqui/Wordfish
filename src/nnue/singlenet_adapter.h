#ifndef NNUE_SINGLENET_ADAPTER_H_INCLUDED
#define NNUE_SINGLENET_ADAPTER_H_INCLUDED

#include "../evaluate.h"
#include "network.h"

namespace Stockfish::Eval::NNUE::Adapter {

inline AccumulatorCaches::Cache<TransformedFeatureDimensions>&
active_cache(AccumulatorCaches& caches) noexcept {
    return caches.cache;
}

inline const AccumulatorCaches::Cache<TransformedFeatureDimensions>&
active_cache(const AccumulatorCaches& caches) noexcept {
    return caches.cache;
}



inline const char* active_eval_file_name() noexcept { return EvalFileDefaultName; }

}  // namespace Stockfish::Eval::NNUE::Adapter

#endif
