// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "dsp/LockFreeFifo.h"

#include <atomic>

namespace pm::dsp::comp
{

/** One point of the compressor's level graph: 5 ms of audio. */
struct MeterPoint
{
    float inputDb = -120.0f;  // input peak (aligned with the output)
    float outputDb = -120.0f; // output peak
    float gainDb = 0.0f;      // deepest gain reduction (≤ 0)
};

/**
    What the compressor tells the interface: the points of the scrolling level graph through the
    lock-free FIFO of phase 0, and a few current values in atomics. One producer (the compressor
    of the module set that is heard) and one consumer (the interface).
*/
class CompressorMeter
{
public:
    static constexpr double pointSeconds = 0.005;
    static constexpr int capacity = 2048; // about 10 s of points: plenty between two frames

    /** Allocates the FIFO. Not real-time safe. */
    void prepare()
    {
        fifo.prepare (capacity);
        gainDb.store (0.0f);
        levelDb.store (-120.0f);
        sidechainMissing.store (false);
    }

    LockFreeFifo<MeterPoint>& getPoints() noexcept { return fifo; }

    /** Gain reduction now (dB, ≤ 0). */
    std::atomic<float> gainDb { 0.0f };

    /** Level the detector sees now (dB): the dot on the transfer curve. */
    std::atomic<float> levelDb { -120.0f };

    /** The external sidechain is selected but no signal arrives. */
    std::atomic<bool> sidechainMissing { false };

private:
    LockFreeFifo<MeterPoint> fifo;
};

} // namespace pm::dsp::comp
