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

#include "network.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <type_traits>
#include <vector>
#include <filesystem>

#define INCBIN_SILENCE_BITCODE_WARNING
#include "../incbin/incbin.h"

#include "../evaluate.h"
#include "../misc.h"
#include "../position.h"
#include "../types.h"
#include "nnue_architecture.h"
#include "nnue_common.h"
#include "nnue_misc.h"
#include "nnz_helper.h"

// Macro to embed the default efficiently updatable neural network (NNUE) file
// data in the engine binary (using incbin.h, by Dale Weiler).
// This macro invocation will declare the following three variables
//     const unsigned char        gEmbeddedNNUEData[];  // a pointer to the embedded data
//     const unsigned char *const gEmbeddedNNUEEnd;     // a marker to the end
//     const unsigned int         gEmbeddedNNUESize;    // the size of the embedded file
// Note that this does not work in Microsoft Visual Studio.
#if !defined(UNIVERSAL_BINARY) && !defined(_MSC_VER) && !defined(NNUE_EMBEDDING_OFF)
INCBIN(EmbeddedNNUE, EvalFileDefaultName);
#elif defined(UNIVERSAL_BINARY_MACOS_X86_SLICE)
// Determined at runtime, see universal/nnue_embed.cpp
extern const unsigned char* const gEmbeddedNNUEData;
extern const unsigned int         gEmbeddedNNUESize;
#else
const unsigned char gEmbeddedNNUEData[1] = {0x0};
const unsigned int  gEmbeddedNNUESize    = 1;
#endif


namespace Stockfish::Eval::NNUE {

namespace fs = std::filesystem;


namespace Detail {

// Read evaluation function parameters
template<typename T>
bool read_parameters(std::istream& stream, T& reference) {

    std::uint32_t header;
    header = read_little_endian<std::uint32_t>(stream);
    if (!stream || header != T::get_hash_value())
        return false;
    return reference.read_parameters(stream);
}

// Write evaluation function parameters
template<typename T>
bool write_parameters(std::ostream& stream, const T& reference) {

    write_little_endian<std::uint32_t>(stream, T::get_hash_value());
    return reference.write_parameters(stream);
}

}  // namespace Detail

template<typename Arch, typename Transformer>
void NetworkImpl<Arch, Transformer>::load(const fs::path& rootDirectory, fs::path evalfilePath,
                                           EvalFile& evalFile) {
#if defined(DEFAULT_NNUE_DIRECTORY)
    const std::vector<fs::path> dirs = {"<internal>", fs::path{}, rootDirectory,
                                        stringify(DEFAULT_NNUE_DIRECTORY)};
#else
    const std::vector<fs::path> dirs = {"<internal>", fs::path{}, rootDirectory};
#endif
    if (evalfilePath.empty())
        evalfilePath = EvalFile::defaultName;
    for (const auto& directory : dirs)
        if (!evalFile.current || *evalFile.current != evalfilePath)
        {
            if (directory != fs::path("<internal>"))
                load_user_net(directory, evalfilePath, evalFile);
            else if (evalfilePath == fs::path(EvalFile::defaultName))
                load_internal(evalFile);
        }
}

template<typename Arch, typename Transformer>
bool NetworkImpl<Arch, Transformer>::save(
  const EvalFile& evalFile, const std::optional<fs::path>& filename) const {
    fs::path actualFilename;
    if (filename)
        actualFilename = *filename;
    else
    {
        if (!evalFile.current || *evalFile.current != fs::path(EvalFile::defaultName))
        {
            sync_cout << "Failed to export a net. A non-embedded net can only be saved if the filename is specified" << sync_endl;
            return false;
        }
        actualFilename = EvalFile::defaultName;
    }
    std::ofstream stream(actualFilename, std::ios_base::binary);
    const bool saved = save(stream, *evalFile.current, evalFile.netDescription);
    sync_cout << (saved ? "Network saved successfully to " + actualFilename.string()
                        : "Failed to export a net") << sync_endl;
    return saved;
}

template<typename Arch, typename Transformer>
NetworkOutput
NetworkImpl<Arch, Transformer>::evaluate(const Position&                         pos,
                                     AccumulatorStack&                       accumulatorStack,
                                     AccumulatorCaches::Cache<FTDimensions>& cache) const {

    constexpr uint64_t alignment = CacheLineSize;

    alignas(alignment)
      TransformedFeatureType transformedFeatures[FeatureTransformer<FTDimensions>::BufferSize];

    ASSERT_ALIGNED(transformedFeatures, alignment);

    NNZInfo<FTDimensions> nnzInfo;
    const int  bucket = (pos.count<ALL_PIECES>() - 1) / 4;
    const auto psqt =
      featureTransformer.transform(pos, accumulatorStack, cache, transformedFeatures, bucket, nnzInfo);
    const auto positional = network[bucket].propagate(transformedFeatures, nnzInfo);
    return {static_cast<Value>(psqt / OutputScale), static_cast<Value>(positional / OutputScale)};
}


template<typename Arch, typename Transformer>
void NetworkImpl<Arch, Transformer>::verify(
  const std::function<void(std::string_view)>& f, const EvalFile& evalFile,
  fs::path evalfilePath) const {
    if (evalfilePath.empty())
        evalfilePath = EvalFile::defaultName;

    if (!evalFile.current || *evalFile.current != evalfilePath)
    {
        if (f)
        {
            std::string msg1 =
              "Network evaluation parameters compatible with the engine must be available.";
            std::string msg2 = "The network file " + evalfilePath.string() + " was not loaded successfully.";
            std::string msg3 = "The UCI option EvalFile might need to specify the full path, "
                               "including the directory name, to the network file.";
            std::string msg4 = "The default net can be downloaded from: "
                               "https://tests.stockfishchess.org/api/nn/"
                             + std::string(EvalFile::defaultName);
            std::string msg5 = "The engine will be terminated now.";

            std::string msg = "ERROR: " + msg1 + '\n' + "ERROR: " + msg2 + '\n' + "ERROR: " + msg3
                            + '\n' + "ERROR: " + msg4 + '\n' + "ERROR: " + msg5 + '\n';

            f(msg);
        }

        exit(EXIT_FAILURE);
    }

    if (f)
    {
        size_t size = sizeof(featureTransformer) + sizeof(Arch) * LayerStacks;
        f("NNUE evaluation using " + evalfilePath.string() + " (" + std::to_string(size / (1024 * 1024))
          + "MiB, (" + std::to_string(featureTransformer.TotalInputDimensions) + ", "
          + std::to_string(network[0].TransformedFeatureDimensions) + ", "
          + std::to_string(network[0].FC_0_OUTPUTS) + ", " + std::to_string(network[0].FC_1_OUTPUTS)
          + ", 1))");
    }
}


template<typename Arch, typename Transformer>
NnueEvalTrace
NetworkImpl<Arch, Transformer>::trace_evaluate(const Position&                         pos,
                                           AccumulatorStack&                       accumulatorStack,
                                           AccumulatorCaches::Cache<FTDimensions>& cache) const {

    constexpr uint64_t alignment = CacheLineSize;

    alignas(alignment)
      TransformedFeatureType transformedFeatures[FeatureTransformer<FTDimensions>::BufferSize];

    ASSERT_ALIGNED(transformedFeatures, alignment);

    NnueEvalTrace t{};
    t.correctBucket = (pos.count<ALL_PIECES>() - 1) / 4;
    for (IndexType bucket = 0; bucket < LayerStacks; ++bucket)
    {
        NNZInfo<FTDimensions> nnzInfo;
        const auto materialist = featureTransformer.transform(
          pos, accumulatorStack, cache, transformedFeatures, bucket, nnzInfo);
        const auto positional = network[bucket].propagate(transformedFeatures, nnzInfo);

        t.psqt[bucket]       = static_cast<Value>(materialist / OutputScale);
        t.positional[bucket] = static_cast<Value>(positional / OutputScale);
    }

    return t;
}


template<typename Arch, typename Transformer>
void NetworkImpl<Arch, Transformer>::load_user_net(const fs::path& dir,
                                                    const fs::path& evalfilePath,
                                                    EvalFile& evalFile) {
    std::ifstream stream(dir / evalfilePath, std::ios::binary);
    auto description = load(stream);
    if (description)
    {
        evalFile.current = evalfilePath;
        evalFile.netDescription = *description;
    }
}

template<typename Arch, typename Transformer>
void NetworkImpl<Arch, Transformer>::load_internal(EvalFile& evalFile) {
    class MemoryBuffer: public std::basic_streambuf<char> {
       public:
        MemoryBuffer(char* p, size_t n) { setg(p, p, p + n); setp(p, p + n); }
    };
#ifdef UNIVERSAL_BINARY_MACOS_X86_SLICE
    if (gEmbeddedNNUEData == nullptr)
        return;
#endif
    MemoryBuffer buffer(const_cast<char*>(reinterpret_cast<const char*>(gEmbeddedNNUEData)),
                        size_t(gEmbeddedNNUESize));
    std::istream stream(&buffer);
    auto description = load(stream);
    if (description)
    {
        evalFile.current = fs::path(EvalFile::defaultName);
        evalFile.netDescription = *description;
    }
}

template<typename Arch, typename Transformer>
void NetworkImpl<Arch, Transformer>::initialize() {
    initialized = true;
}


template<typename Arch, typename Transformer>
bool NetworkImpl<Arch, Transformer>::save(std::ostream&      stream,
                                      const fs::path& name,
                                      const std::string& netDescription) const {
    if (name.empty())
        return false;

    return write_parameters(stream, netDescription);
}


template<typename Arch, typename Transformer>
std::optional<std::string> NetworkImpl<Arch, Transformer>::load(std::istream& stream) {
    initialize();
    std::string description;

    return read_parameters(stream, description) ? std::make_optional(description) : std::nullopt;
}


template<typename Arch, typename Transformer>
std::size_t NetworkImpl<Arch, Transformer>::get_content_hash() const {
    if (!initialized)
        return 0;

    std::size_t h = 0;
    hash_combine(h, featureTransformer);
    for (auto&& layerstack : network)
        hash_combine(h, layerstack);
    return h;
}

// Read network header
template<typename Arch, typename Transformer>
bool NetworkImpl<Arch, Transformer>::read_header(std::istream&  stream,
                                             std::uint32_t* hashValue,
                                             std::string*   desc) const {
    std::uint32_t version, size;

    version    = read_little_endian<std::uint32_t>(stream);
    *hashValue = read_little_endian<std::uint32_t>(stream);
    size       = read_little_endian<std::uint32_t>(stream);
    if (!stream || version != Version)
        return false;
    desc->resize(size);
    stream.read(&(*desc)[0], size);
    return !stream.fail();
}


// Write network header
template<typename Arch, typename Transformer>
bool NetworkImpl<Arch, Transformer>::write_header(std::ostream&      stream,
                                              std::uint32_t      hashValue,
                                              const std::string& desc) const {
    write_little_endian<std::uint32_t>(stream, Version);
    write_little_endian<std::uint32_t>(stream, hashValue);
    write_little_endian<std::uint32_t>(stream, std::uint32_t(desc.size()));
    stream.write(&desc[0], desc.size());
    return !stream.fail();
}


template<typename Arch, typename Transformer>
bool NetworkImpl<Arch, Transformer>::read_parameters(std::istream& stream,
                                                 std::string&  netDescription) {
    std::uint32_t hashValue;
    if (!read_header(stream, &hashValue, &netDescription))
        return false;
    if (hashValue != NetworkImpl<Arch, Transformer>::hash)
        return false;
    if (!Detail::read_parameters(stream, featureTransformer))
        return false;
    for (std::size_t i = 0; i < LayerStacks; ++i)
    {
        if (!Detail::read_parameters(stream, network[i]))
            return false;
    }
    return stream && stream.peek() == std::ios::traits_type::eof();
}


template<typename Arch, typename Transformer>
bool NetworkImpl<Arch, Transformer>::write_parameters(std::ostream&      stream,
                                                  const std::string& netDescription) const {
    if (!write_header(stream, NetworkImpl<Arch, Transformer>::hash, netDescription))
        return false;
    if (!Detail::write_parameters(stream, featureTransformer))
        return false;
    for (std::size_t i = 0; i < LayerStacks; ++i)
    {
        if (!Detail::write_parameters(stream, network[i]))
            return false;
    }
    return bool(stream);
}

// Explicit template instantiations

template class NetworkImpl<NetworkArchitecture<TransformedFeatureDimensions, L2, L3>,
                       FeatureTransformer<TransformedFeatureDimensions>>;


}  // namespace Stockfish::Eval::NNUE
