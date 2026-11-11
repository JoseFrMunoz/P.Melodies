// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "ui/eq/EqBandPopup.h"
#include "ui/eq/EqController.h"
#include "ui/eq/SpectrumAnalyser.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace pm::ui
{

/**
    The EQ display (docs/design/html/01, §4 and §12): spectrum, the selected band's contribution,
    the overall curve and one numbered point per band.

    Mouse and keyboard:
    - Double click on the background: new band; its shape depends on where (§12), its gain is the
      height of the click.
    - Over the curve, away from the points, a ghost point shows the shape a new band would have
      there; dragging the curve from that spot creates it and moves it (a click alone still just
      deselects).
    - Drag a point: frequency and gain (for shapes without gain, the vertical drag sets the Q).
      Ctrl: fine adjustment. Beyond the scale (±6 or ±12 dB) the display switches to the next one
      by itself; it never shrinks on its own.
    - Wheel over a point (or with a band selected): Q; in the low and high cuts, the slope
      (6 → 96 dB/oct, one step per notch). Ctrl+wheel: always the Q. Shift: finer Q steps.
    - Alt+click a point: bypass. Right click: menu with shape, bypass and delete.
    - Double click a point: type its frequency, gain and Q. Delete (Supr): deletes the band.
    - Clicking a point opens its mini panel (EqBandPopup): bypass, shape, solo while held,
      delete and values. A click on the background or Esc closes it (and deselects).
*/
class EqGraph final : public juce::Component
{
public:
    explicit EqGraph (EqController& controller);
    ~EqGraph() override;

    /** Pulls the analyser samples and the parameter values; repaints what changed. 30 times a second. */
    void refresh();

    // Geometry (local coordinates), public for tests.
    static constexpr double minDisplayHz = 20.0;
    static constexpr double maxDisplayHz = 20000.0;
    float xForFrequency (double hz) const noexcept;
    double frequencyForX (float x) const noexcept;
    float yForGain (double db) const noexcept;
    double gainForY (float y) const noexcept;
    double getRangeDb() const;

    juce::Point<float> pointForBand (int index) const;
    int bandAt (juce::Point<float> position) const;

    /** "400 Hz · −4,0 dB" (or "· Q 2,50" for shapes without gain): the point's accessible description. */
    juce::String labelText (int index) const;

    /** What a new band on the curve would be, at the pointer: the spot on the curve and its shape. */
    struct LinePreview
    {
        juce::Point<float> point;
        dsp::eq::FilterType type;
    };
    /** Preview for a pointer position, if it is close enough to the curve (and not on a point). */
    std::optional<LinePreview> linePreviewAt (juce::Point<float> position);
    /** Preview currently shown (the pointer is over the curve). */
    const std::optional<LinePreview>& getLinePreview() const noexcept { return linePreview; }

    /** Pixels from the curve within which it can be grabbed. */
    static constexpr float lineGrabDistance = 8.0f;
    /** Movement that turns a press on the curve into a new band (less is a click). */
    static constexpr float lineDragThreshold = 3.0f;

    // Actions, public so tests can run them without synthesising mouse events.
    int createBandAt (juce::Point<float> position);
    void beginDrag (int index, juce::Point<float> position);
    void dragTo (juce::Point<float> position, bool fine);
    void endDrag();
    void wheel (int index, float deltaY, bool qInsteadOfSlope, bool fine);

    /** Widens the display scale until gainDb fits (never narrows it). */
    void showGain (double gainDb);

    /** Wheel movement that makes one slope step: one notch of a mouse wheel (JUCE gives 0.23). */
    static constexpr float slopeWheelStep = 0.2f;

    enum MenuItem
    {
        menuBypass = 1,
        menuDelete = 2,
        menuShapeBase = 100 // + FilterType index
    };
    juce::PopupMenu buildBandMenu (int index) const;
    void handleMenuResult (int index, int result);

    /** Applies typed values (empty strings keep the current value). Returns false if nothing valid. */
    bool applyTypedValues (int index, const juce::String& frequency, const juce::String& gain, const juce::String& q);
    void showValueEditor (int index);
    bool isEditingValues() const noexcept { return valueEditor != nullptr; }

    SpectrumAnalyser& getAnalyser() noexcept { return analyser; }

    /** Mini panel of the selected band (P.P.'s review of phase 1, §12). */
    EqBandPopup& getPopup() noexcept { return *popup; }

    /** Places the mini panel on the selected band, or hides it. */
    void updatePopup();

    /** A band changed from the mini panel or the editor: redraw the curves and the panel. */
    void bandsChanged();

    /**
        Solo from the mini panel's headphones: heard while held. Dragging them moves the band
        (offset in pixels from where the press started): horizontally the frequency; with
        changeQ (Ctrl) the vertical movement sets the Q, up = narrower. One gesture each.
    */
    void beginSolo (int index);
    void soloDragTo (juce::Point<float> offset, bool changeQ);
    void endSolo();
    bool isSoloHeld() const noexcept { return soloBand >= 0; }

    /**
        Frequencies heard in solo (held or latched), as drawn on the display between two dashed
        lines; empty without solo. Matches eq::soloSettings(): the band-pass edges at −3 dB, or
        everything below/above the frequency for shelves and cuts.
    */
    juce::Range<double> soloRangeHz() const;

    // Component
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void lookAndFeelChanged() override { repaint(); }

private:
    class ValueEditor;

    void updateCurves();
    juce::Rectangle<float> plotArea() const noexcept;

    EqController& controller;
    SpectrumAnalyser analyser;

    // Cached curves: one value per two pixels, recomputed when a band or the size changes.
    std::array<eq::BandParameters, eq::numBands> cachedBands {};
    double cachedSampleRate = 0.0;
    std::vector<float> totalDb;
    std::array<std::vector<float>, eq::numBands> bandDb;
    bool curvesValid = false;

    int hovered = -1;
    std::optional<LinePreview> linePreview;
    std::optional<juce::Point<float>> linePress; // pressed on the curve; becomes a band if dragged
    std::unique_ptr<EqBandPopup> popup;
    int soloBand = -1;      // held from the mini panel
    int soloPrevious = -1;  // solo before it was held (the editor's Solo button latches)
    float soloStartX = 0.0f;
    double soloQ = 1.0;
    juce::Point<float> soloLastOffset;
    float slopeWheel = 0.0f; // wheel movement not yet turned into a slope step
    int slopeWheelBand = -1;
    int dragging = -1;
    juce::Point<float> lastDragPosition;
    bool dragUsesGain = true;
    double dragFrequency = 1000.0, dragValue = 0.0; // the value under the pointer while dragging
    bool analyserWasOn = true;

    std::unique_ptr<ValueEditor> valueEditor;
};

} // namespace pm::ui
