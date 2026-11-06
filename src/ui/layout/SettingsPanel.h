// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "ui/components/Buttons.h"
#include "ui/components/Knob.h"
#include "ui/components/SegmentedSelector.h"
#include "ui/components/ToggleSwitch.h"

#include <functional>
#include <memory>

namespace pm
{
class PMelodiesProcessor;
}

namespace pm::ui
{

/**
    "Ajustes" panel (docs/design/html/05). Phase 0 shows the global quality (oversampling when
    playing and when exporting) and the safety clipper with its ceiling; window size, theme and
    tooltips arrive in phase 5.

    It is an overlay: a dimmed backdrop over the window and the panel on the right. Clicking the
    backdrop, the close button or pressing Esc closes it.
*/
class SettingsOverlay final : public juce::Component
{
public:
    explicit SettingsOverlay (PMelodiesProcessor& processor);
    ~SettingsOverlay() override;

    std::function<void()> onClose;

    juce::Rectangle<int> getPanelBounds() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void close();

    IconButton closeButton { "Cerrar ajustes", icons::Icon::close, 14.0f };
    SegmentedSelector realtimeQuality;
    SegmentedSelector offlineQuality;
    ToggleSwitch clipperSwitch;
    Knob ceiling;
    juce::HyperlinkButton repositoryLink;

    juce::Rectangle<int> panel;
};

} // namespace pm::ui
