// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/Module.h"
#include "ui/eq/EqPanel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace pm
{
class PMelodiesProcessor;
}

namespace pm::ui
{

/**
    Right panel: the selected module at full size. The EQ has its page since phase 1; the other
    modules show their title until their own phase.
*/
class ModulePanel final : public juce::Component
{
public:
    explicit ModulePanel (PMelodiesProcessor& processor);

    void setModule (ModuleId id);
    ModuleId getModule() const noexcept { return module; }

    /** 30 times a second from the editor. */
    void refresh();

    /** The EQ page (always exists; visible while the EQ is selected). */
    EqPanel& getEqPanel() noexcept { return eqPanel; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ModuleId module = ModuleId::eq;
    EqPanel eqPanel;
};

} // namespace pm::ui
