/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "pw_allocator/allocator.h"
#include "pw_allocator/layout.h"

namespace chre {

//! A class that provides allocation and deallocation functionality for
//! containers using a Pigweed allocator.
class PwAllocatorProvider {
 public:
  PwAllocatorProvider() = delete;
  PwAllocatorProvider(pw::Allocator &allocator) : mAllocator(allocator) {}

  void *allocate(size_t size) {
    return mAllocator.Allocate(pw::allocator::Layout(size));
  }

  //! Returns memory of suitable alignment to hold an array of the given object
  //! type, which may exceed alignment of std::max_align_t and therefore
  //! excludes the use of use allocate().
  //!
  //! @param count the number of elements to allocate.
  //! @return a pointer to allocated memory or nullptr if allocation failed.
  template <typename T>
  inline T *allocateAlignedArray(size_t count) {
    return static_cast<T *>(mAllocator.Allocate(
        pw::allocator::Layout(sizeof(T) * count, alignof(T))));
  }

  void deallocate(void *ptr) {
    mAllocator.Deallocate(ptr);
  }

  bool resize(void *ptr, size_t newSize) {
    return mAllocator.Resize(ptr, newSize);
  }

 private:
  pw::Allocator &mAllocator;
};

}  // namespace chre
