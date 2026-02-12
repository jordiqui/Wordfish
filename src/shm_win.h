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

#ifndef SHM_WIN_H_INCLUDED
#define SHM_WIN_H_INCLUDED

#include <string>

namespace Stockfish::shm {

template<typename T>
class SharedMemory {
   public:
    explicit SharedMemory(const std::string&) noexcept {}

    [[nodiscard]] bool open(const T&) noexcept { return false; }
    void               close() noexcept {}

    [[nodiscard]] bool is_open() const noexcept { return false; }
    [[nodiscard]] bool is_initialized() const noexcept { return false; }

    [[nodiscard]] const T& get() const noexcept {
        static const T dummy{};
        return dummy;
    }
};

template<typename T>
SharedMemory<T> create_shared(const std::string& name, const T& initial_value) {
    SharedMemory<T> shm(name);
    shm.open(initial_value);
    return shm;
}

}  // namespace Stockfish::shm

#endif
