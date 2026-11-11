// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>

namespace pm::ui
{

/**
    Rotary control bound to a plugin parameter (docs/design/html/06, "Knob · estados").

    - Drag up/down: adjusts the value (full range in 200 px). With Ctrl: ten times finer.
    - Double click: back to the default value.
    - Alt + click: type the value. Enter applies, Esc cancels.
    - Wheel: small steps (finer with Ctrl).
    - While dragging, a bubble above the knob shows the value.
    - States: normal, hover, dragging, typing.

    Each drag is a single gesture for the host (and, in phase 5, a single undo step).
*/
class Knob final : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Style
    {
        standard, // 46 px dial with pointer, value and label underneath
        compact,  // 32 px dial without pointer (footer); text is drawn by the parent
        valueOnly // just the value text, with the same interactions (settings panel)
    };

    using Formatter = std::function<juce::String (float value)>;

    Knob (juce::RangedAudioParameter& parameter, juce::String labelText, Style style = Style::standard);
    ~Knob() override;

    void setAccentColour (juce::Colour colour);
    juce::Colour getAccentColour() const noexcept { return accent; }
    void setFormatter (Formatter newFormatter);

    /** Current value formatted for display ("−1,5 dB"). */
    juce::String getValueText() const;

    juce::RangedAudioParameter& getParameter() const noexcept { return parameter; }

    /** Preferred size for the style. */
    juce::Rectangle<int> getPreferredBounds() const;

    // Interaction helpers, public so they can be unit tested without synthesising mouse events.
    static constexpr float pixelsForFullRange = 200.0f;
    static constexpr float fineFactor = 0.1f;
    static constexpr float wheelStep = 0.01f;

    /** Normalised value after dragging `deltaPixels` upwards from `startValue`. */
    static float valueAfterDrag (float startValue, float deltaPixels, bool fine) noexcept;

    void resetToDefault();
    void beginTyping();
    bool isTyping() const noexcept { return editor != nullptr; }
    bool isDragging() const noexcept { return dragging; }

    // Component
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void lookAndFeelChanged() override { repaint(); }
    void enablementChanged() override { setAlpha (isEnabled() ? 1.0f : 0.4f); }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    class ValueBubble;

    float getNormalisedValue() const noexcept { return parameter.getValue(); }
    void setNormalisedValueAsPartOfGesture (float normalised);
    void setNormalisedValueAsCompleteGesture (float normalised);
    juce::Rectangle<float> getDialBounds() const;
    void showBubble();
    void hideBubble();
    void finishTyping (bool apply);

    juce::RangedAudioParameter& parameter;
    juce::ParameterAttachment attachment;
    const juce::String label;
    const Style style;
    juce::Colour accent;
    Formatter formatter;

    bool hovering = false;
    bool dragging = false;
    float lastDragY = 0.0f;
    float dragValue = 0.0f;

    std::unique_ptr<juce::TextEditor> editor;
    std::unique_ptr<ValueBubble> bubble;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

} // namespace pm::ui
