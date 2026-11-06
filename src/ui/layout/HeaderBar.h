// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "ui/components/Buttons.h"
#include "ui/components/Dropdown.h"
#include "ui/components/SegmentedSelector.h"

#include <functional>

namespace pm::ui
{

/**
    Top bar: logo, presets (previous, menu, next, save), A/B with copy, undo/redo, theme and
    settings. In phase 0 only the settings button works; the rest is shown disabled and arrives
    in phase 5 (decision §12).
*/
class HeaderBar final : public juce::Component
{
public:
    HeaderBar();

    std::function<void()> onSettingsClicked;

    void setSettingsOpen (bool isOpen) { settings.setActive (isOpen); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    IconButton previousPreset { "Preset anterior", icons::Icon::chevronLeft };
    Dropdown presetMenu;
    IconButton nextPreset { "Preset siguiente", icons::Icon::chevronRight };
    IconButton savePreset { "Guardar preset", icons::Icon::save };
    SegmentedSelector abSelector { { "A", "B" } };
    IconButton copyAToB { "Copiar A en B", icons::Icon::copy };
    IconButton undo { "Deshacer", icons::Icon::undo };
    IconButton redo { "Rehacer", icons::Icon::redo };
    IconButton themeButton { "Tema de aspecto", icons::Icon::palette };
    IconButton settings { "Ajustes", icons::Icon::settings };

    juce::Rectangle<int> separatorA, separatorB;
};

} // namespace pm::ui
