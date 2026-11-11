// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "core/ChainOrder.h"

namespace pm::chain
{

namespace
{
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

bool isModuleIndex (int index) noexcept { return index >= 0 && index < numModules; }

/** Completes a partial order (no repeats) with the missing modules in their default order. */
Order complete (const std::array<int, numModules>& partial, int count) noexcept
{
    std::array<bool, numModules> used {};
    Order order {};
    for (int p = 0; p < count; ++p)
    {
        order[(size_t) p] = moduleAt (partial[(size_t) p]);
        used[(size_t) partial[(size_t) p]] = true;
    }
    for (int i = 0; i < numModules; ++i)
        if (! used[(size_t) i])
            order[(size_t) count++] = moduleAt (i);
    return order;
}
} // namespace

Order defaultOrder() noexcept
{
    Order order {};
    for (int i = 0; i < numModules; ++i)
        order[(size_t) i] = moduleAt (i);
    return order;
}

bool isValid (const Order& order) noexcept
{
    std::array<bool, numModules> seen {};
    for (auto id : order)
    {
        const auto i = toIndex (id);
        if (! isModuleIndex (i) || seen[(size_t) i])
            return false;
        seen[(size_t) i] = true;
    }
    return true;
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

juce::String describe (const Order& order)
{
    juce::String text;
    for (int p = 0; p < numModules; ++p)
    {
        if (p > 0)
            text << " > ";
        text << shortName (order[(size_t) p]);
    }
    return text;
}

const char* keyOf (ModuleId id) noexcept
{
    switch (id)
    {
        case ModuleId::eq:         return "eq";
        case ModuleId::compressor: return "compressor";
        case ModuleId::saturation: return "saturation";
        case ModuleId::distortion: return "distortion";
    }
    return "";
}

juce::String toText (const Order& order)
{
    juce::StringArray keys;
    for (auto id : order)
        keys.add (keyOf (id));
    return keys.joinIntoString (",");
}

Order fromText (const juce::String& text)
{
    std::array<int, numModules> partial {};
    std::array<bool, numModules> used {};
    int count = 0;

    for (auto key : juce::StringArray::fromTokens (text, ",", {}))
    {
        key = key.trim();
        for (int i = 0; i < numModules; ++i)
        {
            if (! used[(size_t) i] && key == keyOf (moduleAt (i)))
            {
                used[(size_t) i] = true;
                partial[(size_t) count++] = i;
                break;
            }
        }
    }

    return complete (partial, count);
}

std::uint64_t pack (const Order& order) noexcept
{
    std::uint64_t packed = 0;
    for (int p = 0; p < numModules; ++p)
        packed |= (std::uint64_t) toIndex (order[(size_t) p]) << (4 * p);
    return packed;
}

Order unpack (std::uint64_t packed) noexcept
{
    Order order {};
    for (int p = 0; p < numModules; ++p)
        order[(size_t) p] = moduleAt ((int) ((packed >> (4 * p)) & 0xf));
    return isValid (order) ? order : defaultOrder();
}

Order fromLegacyIndex (int index) noexcept
{
    // The old index described the order of the first four modules only (4! = 24 values).
    constexpr int legacyModules = 4;
    constexpr std::array<int, legacyModules> factorials { 6, 2, 1, 1 }; // (n-1-i)! for n = 4
    static_assert (numModules >= legacyModules);

    index = juce::jlimit (0, 23, index);

    std::array<int, legacyModules> available { 0, 1, 2, 3 };
    std::array<int, numModules> partial {};
    int remaining = legacyModules;

    for (int position = 0; position < legacyModules; ++position)
    {
        const auto digit = index / factorials[(size_t) position];
        index %= factorials[(size_t) position];

        partial[(size_t) position] = available[(size_t) digit];

        for (int k = digit; k < remaining - 1; ++k)
            available[(size_t) k] = available[(size_t) k + 1];

        --remaining;
    }

    return complete (partial, legacyModules);
}

} // namespace pm::chain
