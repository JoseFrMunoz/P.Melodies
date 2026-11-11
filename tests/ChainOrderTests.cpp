// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "core/ChainOrder.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>

using namespace pm;

namespace
{
/** Every order of the modules, in lexicographic order of their indices. */
std::vector<chain::Order> allOrders()
{
    std::array<int, numModules> indices {};
    for (int i = 0; i < numModules; ++i)
        indices[(size_t) i] = i;

    std::vector<chain::Order> orders;
    do
    {
        chain::Order order {};
        for (int p = 0; p < numModules; ++p)
            order[(size_t) p] = moduleAt (indices[(size_t) p]);
        orders.push_back (order);
    } while (std::next_permutation (indices.begin(), indices.end()));
    return orders;
}

/** The encoder of the phase 0 builds (Lehmer code), to check old projects still read the same. */
int legacyEncode (const chain::Order& order)
{
    constexpr std::array<int, 4> factorials { 6, 2, 1, 1 };
    int index = 0;
    for (int position = 0; position < 4; ++position)
    {
        int smallerAfter = 0;
        for (int later = position + 1; later < 4; ++later)
            if (toIndex (order[(size_t) later]) < toIndex (order[(size_t) position]))
                ++smallerAfter;
        index += smallerAfter * factorials[(size_t) position];
    }
    return index;
}
} // namespace

TEST_CASE ("Chain order: the default is EQ > Comp > Sat > Dist", "[chain]")
{
    const chain::Order expected { ModuleId::eq, ModuleId::compressor, ModuleId::saturation, ModuleId::distortion };
    CHECK (chain::defaultOrder() == expected);
    CHECK (chain::describe (expected) == "EQ > Comp > Sat > Dist");
    CHECK (chain::toText (expected) == "eq,compressor,saturation,distortion");
}

TEST_CASE ("Chain order: every order round-trips through the saved text and the audio-thread word", "[chain]")
{
    const auto orders = allOrders();
    REQUIRE (orders.size() == 24);

    std::set<std::uint64_t> packedValues;
    for (const auto& order : orders)
    {
        CHECK (chain::isValid (order));
        CHECK (chain::fromText (chain::toText (order)) == order);
        CHECK (chain::unpack (chain::pack (order)) == order);
        packedValues.insert (chain::pack (order));
    }
    CHECK (packedValues.size() == orders.size());
}

TEST_CASE ("Chain order: damaged or older saved text still gives a complete order", "[chain]")
{
    using enum ModuleId;

    // Unknown modules and repeats are skipped; spaces are tolerated.
    CHECK (chain::fromText (" distortion , reverb, eq,eq ,compressor,saturation") == chain::Order { distortion, eq, compressor, saturation });

    // A module missing from the text (saved before it existed) goes after the others.
    CHECK (chain::fromText ("saturation,eq") == chain::Order { saturation, eq, compressor, distortion });
    CHECK (chain::fromText ("") == chain::defaultOrder());

    // Not a permutation: the audio thread falls back to the default order.
    CHECK_FALSE (chain::isValid (chain::Order { eq, eq, saturation, distortion }));
    CHECK (chain::unpack (0) == chain::defaultOrder()); // every position would be the EQ
    CHECK (chain::unpack (~std::uint64_t (0)) == chain::defaultOrder());
}

TEST_CASE ("Chain order: projects saved with the old order parameter keep their order", "[chain]")
{
    std::set<std::uint64_t> seen;
    for (int index = 0; index < 24; ++index)
    {
        const auto order = chain::fromLegacyIndex (index);
        CHECK (chain::isValid (order));
        CHECK (legacyEncode (order) == index);
        seen.insert (chain::pack (order));
    }
    CHECK (seen.size() == 24);
    CHECK (chain::fromLegacyIndex (0) == chain::defaultOrder());
    CHECK (chain::describe (chain::fromLegacyIndex (23)) == "Dist > Sat > Comp > EQ");
    CHECK (chain::fromLegacyIndex (99) == chain::fromLegacyIndex (23)); // out of range: clamped
}

TEST_CASE ("Chain order: moving a module behaves like drag and drop", "[chain]")
{
    const auto start = chain::defaultOrder(); // EQ, Comp, Sat, Dist

    const auto distFirst = chain::move (start, 3, 0);
    CHECK (distFirst == chain::Order { ModuleId::distortion, ModuleId::eq, ModuleId::compressor, ModuleId::saturation });

    const auto eqLast = chain::move (start, 0, 3);
    CHECK (eqLast == chain::Order { ModuleId::compressor, ModuleId::saturation, ModuleId::distortion, ModuleId::eq });

    const auto swapMiddle = chain::move (start, 1, 2);
    CHECK (swapMiddle == chain::Order { ModuleId::eq, ModuleId::saturation, ModuleId::compressor, ModuleId::distortion });

    CHECK (chain::move (start, 2, 2) == start);
    CHECK (chain::positionOf (distFirst, ModuleId::eq) == 1);
}
