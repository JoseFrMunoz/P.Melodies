// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <memory>

namespace pm
{
class PMelodiesProcessor;
}

namespace pm::ui
{

/**
    Small display of the compressor's sidechain EQ (design 02, §12): the curve of the high-pass,
    the bell and the low-pass, with a point for each.

    - Drag a point: its frequency (and the bell's gain, vertically).
    - Wheel over a point: the slope of a cut (one step per notch) or the bell's Q.
    - Alt+click a point: turns that filter off or on.
    - Double click a point: type its frequency (also as a note, like the EQ).

    A line of text shows the point under the pointer, or the last one touched.
*/
class SidechainEqView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Filter
    {
        highPass = 0,
        bell = 1,
        lowPass = 2
    };
    static constexpr int numFilters = 3;

    explicit SidechainEqView (PMelodiesProcessor& processor);
    ~SidechainEqView() override;

    /** 30 times a second: repaints when a value changed (automation, undo, another control). */
    void refresh();

    void setAccentColour (juce::Colour colour);

    /** Where a filter's point is drawn. */
    juce::Point<float> pointOf (Filter filter) const;

    /** Filter whose point is within reach of a position, or -1. */
    int filterAt (juce::Point<float> position) const;

    /** "HP 80 Hz · 24 dB/oct", "Campana 1 kHz · +3,0 dB · Q 1", "LP abierto"… */
    juce::String describe (Filter filter) const;
    juce::String getInfoText() const { return describe (focus); }
    Filter getFocus() const noexcept { return focus; }

    bool isEnabled (Filter filter) const;

    // Mapping of the display: 20 Hz to 20 kHz, +12 dB at the top to −30 dB at the bottom.
    static constexpr float minHz = 20.0f;
    static constexpr float maxHz = 20000.0f;
    static constexpr float topDb = 12.0f;
    static constexpr float bottomDb = -30.0f;
    static constexpr float grabRadius = 8.0f;

    float xForFrequency (float hz) const noexcept;
    float frequencyForX (float x) const noexcept;
    float yForDb (float db) const noexcept;
    float dbForY (float y) const noexcept;

    // Actions, public so tests can drive them without synthetic mouse events.
    void dragTo (Filter filter, juce::Point<float> position);
    void stepWheel (Filter filter, int steps);
    void toggle (Filter filter);
    void beginTyping (Filter filter);
    bool isTyping() const noexcept { return editor != nullptr; }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    struct Parameters
    {
        juce::RangedAudioParameter* on = nullptr;
        juce::RangedAudioParameter* frequency = nullptr;
        juce::RangedAudioParameter* slope = nullptr; // cuts
        juce::RangedAudioParameter* gain = nullptr;  // bell
        juce::RangedAudioParameter* q = nullptr;     // bell
    };

    const Parameters& parametersOf (Filter filter) const { return parameters[(size_t) filter]; }
    float value (const juce::RangedAudioParameter* p) const;
    void set (juce::RangedAudioParameter* p, float realValue);
    void finishTyping (bool apply);
    juce::Path makeCurve() const;
    juce::String snapshot() const;

    std::array<Parameters, numFilters> parameters;
    juce::Colour accent;
    Filter focus = Filter::highPass;
    int hovered = -1;
    int dragging = -1;
    float wheelAccumulator = 0.0f;
    juce::String lastSnapshot;
    std::unique_ptr<juce::TextEditor> editor;
    Filter typing = Filter::highPass;
};

} // namespace pm::ui
