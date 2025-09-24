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

// Class for difference calculation of NNUE evaluation function

#ifndef NNUE_ACCUMULATOR_H_INCLUDED
#define NNUE_ACCUMULATOR_H_INCLUDED

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "../types.h"
#include "nnue_architecture.h"
#include "nnue_common.h"

namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE {

template<IndexType Size>
struct alignas(CacheLineSize) Accumulator;

template<IndexType TransformedFeatureDimensions>
class FeatureTransformer;

// Class that holds the result of affine transformation of input features
template<IndexType Size>
struct alignas(CacheLineSize) Accumulator {
    std::int16_t               accumulation[COLOR_NB][Size];
    std::int32_t               psqtAccumulation[COLOR_NB][PSQTBuckets];
    std::array<bool, COLOR_NB> computed;
};


// AccumulatorCaches struct provides per-thread accumulator caches, where each
// cache contains multiple entries for each of the possible king squares.
// When the accumulator needs to be refreshed, the cached entry is used to more
// efficiently update the accumulator, instead of rebuilding it from scratch.
// This idea, was first described by Luecx (author of Koivisto) and
// is commonly referred to as "Finny Tables".
struct AccumulatorCaches {

    template<typename Networks>
    AccumulatorCaches(const Networks& networks) {
        clear(networks);
    }

    template<IndexType Size>
    struct alignas(CacheLineSize) Cache {

        struct alignas(CacheLineSize) Entry {
            BiasType       accumulation[Size];
            PSQTWeightType psqtAccumulation[PSQTBuckets];
            Bitboard       byColorBB[COLOR_NB];
            Bitboard       byTypeBB[PIECE_TYPE_NB];

            // To initialize a refresh entry, we set all its bitboards empty,
            // so we put the biases in the accumulation, without any weights on top
            void reset(const BiasType* biases) {

                std::memcpy(accumulation, biases, sizeof(accumulation));
                std::memset((uint8_t*) this + offsetof(Entry, psqtAccumulation), 0,
                            sizeof(Entry) - offsetof(Entry, psqtAccumulation));
            }
        };

        void reset_from_biases(const BiasType* biases) {
            assert(biases);

            for (auto& entries1D : entries)
                for (auto& entry : entries1D)
                    entry.reset(biases);
        }

        std::array<Entry, COLOR_NB>& operator[](Square sq) { return entries[sq]; }

        std::array<std::array<Entry, COLOR_NB>, SQUARE_NB> entries;
    };

    struct CacheBinding {
        template<typename Network, IndexType Size>
        void prime(const Network& network, Cache<Size>& cache) {
            auto handle = network.weights_handle();
            assert(handle);
            auto alias = std::shared_ptr<void>(handle, static_cast<void*>(handle.get()));
            cache.reset_from_biases(handle->featureTransformer->biases);
            weights = alias;
            version = network.version();
        }

        template<typename Network, IndexType Size>
        bool ensure(const Network& network, Cache<Size>& cache) {
            return ensure_internal(network, cache, false);
        }

        void invalidate() {
            version = 0;
            weights.reset();
        }

        template<typename Network, IndexType Size>
        bool ensure_internal(const Network& network, Cache<Size>& cache, bool force) {
            auto handle = network.weights_handle();
            if (!handle)
                return false;

            auto alias = std::shared_ptr<void>(handle, static_cast<void*>(handle.get()));
            auto locked = weights.lock();
            bool refresh = force || !locked || locked.get() != alias.get()
                           || version != network.version();

            if (refresh)
            {
                cache.reset_from_biases(handle->featureTransformer->biases);
                weights = alias;
                version = network.version();
            }

            return true;
        }

        std::weak_ptr<void> weights;
        std::uint64_t       version = 0;
    };

    template<typename Networks>
    void clear(const Networks& networks) {
        if (auto weights = networks.big.weights_handle())
        {
            bigCache.binding.prime(networks.big, bigCache.cache);
        }
        else
            bigCache.binding.invalidate();

        sharedSmall.reset();

        if (auto weights = networks.small.weights_handle())
        {
            sharedSmall.bindingSmall.prime(networks.small, sharedSmall.cache);
            sharedSmall.owner = SharedSmallCache::Owner::Small;
        }
    }

    template<typename Network>
    Cache<TransformedFeatureDimensionsBig>& cache_for_big(const Network& network) {
        bigCache.binding.ensure(network, bigCache.cache);
        return bigCache.cache;
    }

    template<typename Network>
    Cache<TransformedFeatureDimensionsSmall>& cache_for_small(const Network& network) {
        sharedSmall.ensure_owner(network, SharedSmallCache::Owner::Small);
        return sharedSmall.cache;
    }

    template<typename Network>
    Cache<TransformedFeatureDimensionsSmall>& cache_for_falcon(const Network& network) {
        if (!network.is_available())
        {
            if (sharedSmall.owner == SharedSmallCache::Owner::Falcon)
                sharedSmall.owner = SharedSmallCache::Owner::None;
            sharedSmall.bindingFalcon.invalidate();
            return sharedSmall.cache;
        }

        sharedSmall.ensure_owner(network, SharedSmallCache::Owner::Falcon);
        return sharedSmall.cache;
    }

    void invalidate_big() { bigCache.binding.invalidate(); }

    void invalidate_small() {
        if (sharedSmall.owner == SharedSmallCache::Owner::Small)
            sharedSmall.owner = SharedSmallCache::Owner::None;
        sharedSmall.bindingSmall.invalidate();
    }

    void invalidate_falcon() {
        if (sharedSmall.owner == SharedSmallCache::Owner::Falcon)
            sharedSmall.owner = SharedSmallCache::Owner::None;
        sharedSmall.bindingFalcon.invalidate();
    }

    struct BigCacheState {
        Cache<TransformedFeatureDimensionsBig> cache;
        CacheBinding                           binding;
    };

    struct SharedSmallCache {
        enum class Owner { None, Small, Falcon };

        void reset() {
            owner         = Owner::None;
            bindingSmall.invalidate();
            bindingFalcon.invalidate();
        }

        template<typename Network>
        void ensure_owner(const Network& network, Owner desiredOwner) {
            CacheBinding& binding = desiredOwner == Owner::Small ? bindingSmall : bindingFalcon;

            if (!binding.ensure(network, cache))
            {
                if ((desiredOwner == Owner::Falcon && owner == Owner::Falcon)
                    || desiredOwner == Owner::Small)
                    owner = Owner::None;
                return;
            }

            if (owner != desiredOwner)
            {
                owner = desiredOwner;
                (desiredOwner == Owner::Small ? bindingFalcon : bindingSmall).invalidate();
            }
        }

        Cache<TransformedFeatureDimensionsSmall> cache;
        CacheBinding                             bindingSmall;
        CacheBinding                             bindingFalcon;
        Owner                                    owner = Owner::None;
    };

    BigCacheState    bigCache;
    SharedSmallCache sharedSmall;
};


struct AccumulatorState {
    Accumulator<TransformedFeatureDimensionsBig>   accumulatorBig;
    Accumulator<TransformedFeatureDimensionsSmall> accumulatorSmall;
    DirtyPiece                                     dirtyPiece;

    template<IndexType Size>
    auto& acc() noexcept {
        static_assert(Size == TransformedFeatureDimensionsBig
                        || Size == TransformedFeatureDimensionsSmall,
                      "Invalid size for accumulator");

        if constexpr (Size == TransformedFeatureDimensionsBig)
            return accumulatorBig;
        else if constexpr (Size == TransformedFeatureDimensionsSmall)
            return accumulatorSmall;
    }

    template<IndexType Size>
    const auto& acc() const noexcept {
        static_assert(Size == TransformedFeatureDimensionsBig
                        || Size == TransformedFeatureDimensionsSmall,
                      "Invalid size for accumulator");

        if constexpr (Size == TransformedFeatureDimensionsBig)
            return accumulatorBig;
        else if constexpr (Size == TransformedFeatureDimensionsSmall)
            return accumulatorSmall;
    }

    void reset(const DirtyPiece& dp) noexcept;
};


class AccumulatorStack {
   public:
    AccumulatorStack() :
        accumulators(MAX_PLY + 1),
        size{1} {}

    [[nodiscard]] const AccumulatorState& latest() const noexcept;

    void reset() noexcept;
    void push(const DirtyPiece& dirtyPiece) noexcept;
    void pop() noexcept;

    template<IndexType Dimensions>
    void evaluate(const Position&                       pos,
                  const FeatureTransformer<Dimensions>& featureTransformer,
                  AccumulatorCaches::Cache<Dimensions>& cache) noexcept;

   private:
    [[nodiscard]] AccumulatorState& mut_latest() noexcept;

    template<Color Perspective, IndexType Dimensions>
    void evaluate_side(const Position&                       pos,
                       const FeatureTransformer<Dimensions>& featureTransformer,
                       AccumulatorCaches::Cache<Dimensions>& cache) noexcept;

    template<Color Perspective, IndexType Dimensions>
    [[nodiscard]] std::size_t find_last_usable_accumulator() const noexcept;

    template<Color Perspective, IndexType Dimensions>
    void forward_update_incremental(const Position&                       pos,
                                    const FeatureTransformer<Dimensions>& featureTransformer,
                                    const std::size_t                     begin) noexcept;

    template<Color Perspective, IndexType Dimensions>
    void backward_update_incremental(const Position&                       pos,
                                     const FeatureTransformer<Dimensions>& featureTransformer,
                                     const std::size_t                     end) noexcept;

    std::vector<AccumulatorState> accumulators;
    std::size_t                   size;
};

}  // namespace Stockfish::Eval::NNUE

#endif  // NNUE_ACCUMULATOR_H_INCLUDED
