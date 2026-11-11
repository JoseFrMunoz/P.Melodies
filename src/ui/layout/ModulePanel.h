// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/Module.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace pm::ui
{

/**
    Right panel: the selected module at full size. In phase 0 it only shows the module title;
    each module's controls arrive in its own phase.
*/
class ModulePanel final : public juce::Component
{
public:
    ModulePanel();

    void setModule (ModuleId id);
    ModuleId getModule() const noexcept { return module; }

    void paint (juce::Graphics&) override;

private:
    ModuleId module = ModuleId::eq;
};

} // namespace pm::ui
