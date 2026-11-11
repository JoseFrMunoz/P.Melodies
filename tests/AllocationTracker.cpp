// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "AllocationTracker.h"

#include <cstdlib>
#include <new>

#if defined(_MSC_VER)
 #include <malloc.h>
#endif

namespace
{
thread_local bool trackingEnabled = false;
thread_local long long allocationCount = 0;

inline void noteAllocation() noexcept
{
    if (trackingEnabled)
        ++allocationCount;
}

void* allocateOrThrow (std::size_t size)
{
    noteAllocation();
    if (auto* p = std::malloc (size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc();
}

void* allocateAligned (std::size_t size, std::align_val_t alignment)
{
    noteAllocation();
    const auto a = static_cast<std::size_t> (alignment);
#if defined(_MSC_VER)
    if (auto* p = _aligned_malloc (size == 0 ? 1 : size, a))
        return p;
#else
    const auto rounded = ((size == 0 ? 1 : size) + a - 1) / a * a;
    if (auto* p = std::aligned_alloc (a, rounded))
        return p;
#endif
    throw std::bad_alloc();
}

void freeAligned (void* p) noexcept
{
#if defined(_MSC_VER)
    _aligned_free (p);
#else
    std::free (p);
#endif
}
} // namespace

//==============================================================================
void* operator new (std::size_t size) { return allocateOrThrow (size); }
void* operator new[] (std::size_t size) { return allocateOrThrow (size); }
void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{
    noteAllocation();
    return std::malloc (size == 0 ? 1 : size);
}
void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept
{
    noteAllocation();
    return std::malloc (size == 0 ? 1 : size);
}
void* operator new (std::size_t size, std::align_val_t a) { return allocateAligned (size, a); }
void* operator new[] (std::size_t size, std::align_val_t a) { return allocateAligned (size, a); }

void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }
void operator delete (void* p, const std::nothrow_t&) noexcept { std::free (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept { std::free (p); }
void operator delete (void* p, std::align_val_t) noexcept { freeAligned (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { freeAligned (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept { freeAligned (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { freeAligned (p); }

//==============================================================================
#if defined(__linux__) && defined(__GLIBC__)
extern "C"
{
    void* __libc_malloc (std::size_t);
    void* __libc_calloc (std::size_t, std::size_t);
    void* __libc_realloc (void*, std::size_t);

    void* malloc (std::size_t size)
    {
        noteAllocation();
        return __libc_malloc (size);
    }

    void* calloc (std::size_t n, std::size_t size)
    {
        noteAllocation();
        return __libc_calloc (n, size);
    }

    void* realloc (void* p, std::size_t size)
    {
        noteAllocation();
        return __libc_realloc (p, size);
    }
}
#endif

namespace pmtest
{

ScopedAllocationCounter::ScopedAllocationCounter() noexcept
    : startCount (allocationCount), previouslyEnabled (trackingEnabled)
{
    trackingEnabled = true;
}

ScopedAllocationCounter::~ScopedAllocationCounter() noexcept
{
    trackingEnabled = previouslyEnabled;
}

long long ScopedAllocationCounter::getCount() const noexcept
{
    return allocationCount - startCount;
}

bool tracksMalloc() noexcept
{
#if defined(__linux__) && defined(__GLIBC__)
    return true;
#else
    return false;
#endif
}

} // namespace pmtest
