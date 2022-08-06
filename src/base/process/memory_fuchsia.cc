// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/buildflags.h"
#include "base/process/memory.h"

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "base/allocator/allocator_shim.h"
#endif

#include <stdlib.h>

namespace base {

void EnableTerminationOnOutOfMemory() {
  // Nothing to be done here.
}

void EnableTerminationOnHeapCorruption() {
  // Nothing to be done here.
}

bool UncheckedMalloc(size_t size, void** result) {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  *result = allocator::UncheckedAlloc(size);
#else
  *result = malloc(size);
#endif
  return *result != nullptr;
}

void UncheckedFree(void* ptr) {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  allocator::UncheckedFree(ptr);
#else
  free(ptr);
#endif
}

}  // namespace base
