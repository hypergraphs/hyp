// Copyright 2014-2015 SDL plc
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/** \file

    Pool pool(allocsz); pool.free(pool.malloc())
*/

#ifndef POOL_GRAEHL_2015_10_21_HPP
#define POOL_GRAEHL_2015_10_21_HPP
#pragma once

#include <sdl/Pool-fwd.hpp>
#include <sdl/Pool/pool.hpp>
#include <utility>

namespace sdl {

template <class T>
void deletePool(T* t, ChunkPool& pool) {
  t->~T();
  assert(pool.get_requested_size() >= sizeof(T));  // subclass could be larger
  pool.free(t);
}

template <class T>
void deletePool(T* t, ChunkPool* pool) {
  if (pool)
    deletePool(t, *pool);
  else
    delete t;
}

template <class I>
void deleteRangePool(I begin, I end, ChunkPool& pool) {
  for (; begin != end; ++begin) deletePool(*begin, pool);
}

template <class T, class... Args>
void constructFromPool(T*& t, ChunkPool& pool, Args&&... args) {
  assert(pool.get_requested_size() == sizeof(T));
  t = (T*)pool.malloc();
  new (t) T(std::forward<Args>(args)...);
}

template <class T>
struct Constructed {
  T* p_;
  template <class... Args>
  Constructed(T* p = 0, Args&&... args)
      : p_() {
    new (p) T(std::forward<Args>(args)...);
    p_ = p;  // exception safety for T ctor
  }
  void release() { p_ = 0; }
  ~Constructed() {
    if (p_) p_->~T();
  }
};


}

#endif
