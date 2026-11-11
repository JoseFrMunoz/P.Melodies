// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/ModuleBase.h"
#include "modules/eq/EqModule.h"

#include <memory>

namespace pm
{

/**
    Phase 0 modules: they already have their final latency and oversampling machinery, but no DSP
    yet, so they always report a neutral core and the base class passes the latency-aligned input
    through untouched. Each one is replaced by its real implementation in its own phase.
*/
class PlaceholderModule final : public ModuleBase
{
public:
    PlaceholderModule (ModuleId moduleId, bool moduleUsesOversampling) noexcept
        : ModuleBase (moduleId, moduleUsesOversampling)
    {
    }

protected:
    bool isCoreNeutral() const noexcept override { return true; }
};

/** Whether a module's DSP needs oversampling (the EQ is zero-latency in v1, so it does not). */
constexpr bool moduleUsesOversampling (ModuleId id) noexcept
{
    return id != ModuleId::eq;
}

/** Creates the production module for each slot: the real ones so far, placeholders for the rest. */
inline std::unique_ptr<ModuleBase> createModule (ModuleId id)
{
    if (id == ModuleId::eq)
        return std::make_unique<eq::EqModule>();
    return std::make_unique<PlaceholderModule> (id, moduleUsesOversampling (id));
}

} // namespace pm
