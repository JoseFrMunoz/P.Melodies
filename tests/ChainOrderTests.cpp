// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "core/ChainOrder.h"

#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace pm;

TEST_CASE ("Chain order: index 0 is EQ > Comp > Sat > Dist", "[chain]")
{
    const chain::Order expected { ModuleId::eq, ModuleId::compressor, ModuleId::saturation, ModuleId::distortion };
    CHECK (chain::decode (0) == expected);
    CHECK (chain::describe (0) == "EQ > Comp > Sat > Dist");
}

TEST_CASE ("Chain order: the 24 indices are 24 distinct permutations and round-trip", "[chain]")
{
    std::set<std::array<int, numModules>> seen;

    for (int index = 0; index < chain::numOrders; ++index)
    {
        const auto order = chain::decode (index);
        std::array<int, numModules> asInts {};
        std::set<int> members;

        for (int p = 0; p < numModules; ++p)
        {
            asInts[(size_t) p] = toIndex (order[(size_t) p]);
            members.insert (asInts[(size_t) p]);
        }

        CHECK (members.size() == (size_t) numModules); // a real permutation
        CHECK (chain::encode (order) == index);
        seen.insert (asInts);
    }

    CHECK (seen.size() == (size_t) chain::numOrders);
}

TEST_CASE ("Chain order: moving a module behaves like drag and drop", "[chain]")
{
    const auto start = chain::decode (0); // EQ, Comp, Sat, Dist

    const auto distFirst = chain::move (start, 3, 0);
    CHECK (distFirst == chain::Order { ModuleId::distortion, ModuleId::eq, ModuleId::compressor, ModuleId::saturation });

    const auto eqLast = chain::move (start, 0, 3);
    CHECK (eqLast == chain::Order { ModuleId::compressor, ModuleId::saturation, ModuleId::distortion, ModuleId::eq });

    const auto swapMiddle = chain::move (start, 1, 2);
    CHECK (swapMiddle == chain::Order { ModuleId::eq, ModuleId::saturation, ModuleId::compressor, ModuleId::distortion });

    CHECK (chain::move (start, 2, 2) == start);
    CHECK (chain::positionOf (distFirst, ModuleId::eq) == 1);
}
