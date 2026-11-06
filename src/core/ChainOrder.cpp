// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "core/ChainOrder.h"

namespace pm::chain
{

namespace
{
constexpr std::array<int, numModules> factorials { 6, 2, 1, 1 }; // (n-1-i)! for n = 4

const char* shortName (ModuleId id) noexcept
{
    switch (id)
    {
        case ModuleId::eq:         return "EQ";
        case ModuleId::compressor: return "Comp";
        case ModuleId::saturation: return "Sat";
        case ModuleId::distortion: return "Dist";
    }
    return "?";
}
} // namespace

Order decode (int index) noexcept
{
    index = juce::jlimit (0, numOrders - 1, index);

    std::array<int, numModules> available { 0, 1, 2, 3 };
    int remaining = numModules;
    Order order {};

    for (int position = 0; position < numModules; ++position)
    {
        const auto digit = index / factorials[(size_t) position];
        index %= factorials[(size_t) position];

        order[(size_t) position] = moduleAt (available[(size_t) digit]);

        for (int k = digit; k < remaining - 1; ++k)
            available[(size_t) k] = available[(size_t) k + 1];

        --remaining;
    }

    return order;
}

int encode (const Order& order) noexcept
{
    int index = 0;

    for (int position = 0; position < numModules; ++position)
    {
        int smallerAfter = 0;

        for (int later = position + 1; later < numModules; ++later)
            if (toIndex (order[(size_t) later]) < toIndex (order[(size_t) position]))
                ++smallerAfter;

        index += smallerAfter * factorials[(size_t) position];
    }

    return index;
}

Order move (Order order, int fromPosition, int toPosition) noexcept
{
    fromPosition = juce::jlimit (0, numModules - 1, fromPosition);
    toPosition = juce::jlimit (0, numModules - 1, toPosition);

    const auto moving = order[(size_t) fromPosition];

    if (fromPosition < toPosition)
        for (int p = fromPosition; p < toPosition; ++p)
            order[(size_t) p] = order[(size_t) p + 1];
    else
        for (int p = fromPosition; p > toPosition; --p)
            order[(size_t) p] = order[(size_t) p - 1];

    order[(size_t) toPosition] = moving;
    return order;
}

int positionOf (const Order& order, ModuleId id) noexcept
{
    for (int p = 0; p < numModules; ++p)
        if (order[(size_t) p] == id)
            return p;

    jassertfalse;
    return 0;
}

juce::String describe (int index)
{
    const auto order = decode (index);
    juce::String text;

    for (int p = 0; p < numModules; ++p)
    {
        if (p > 0)
            text << " > ";
        text << shortName (order[(size_t) p]);
    }

    return text;
}

} // namespace pm::chain
