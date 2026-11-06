// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

namespace pmtest
{

/**
    Counts heap allocations made by the current thread while it is alive.

    Every platform: the global operator new family is replaced in the test executable.
    Linux (glibc): malloc/calloc/realloc are interposed too, which also catches allocations made
    with malloc directly (JUCE's HeapBlock, for example).
*/
class ScopedAllocationCounter
{
public:
    ScopedAllocationCounter() noexcept;
    ~ScopedAllocationCounter() noexcept;

    long long getCount() const noexcept;

    ScopedAllocationCounter (const ScopedAllocationCounter&) = delete;
    ScopedAllocationCounter& operator= (const ScopedAllocationCounter&) = delete;

private:
    long long startCount = 0;
    bool previouslyEnabled = false;
};

/** True when malloc-level tracking is available on this platform. */
bool tracksMalloc() noexcept;

} // namespace pmtest
