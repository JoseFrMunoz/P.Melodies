// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "ui/components/Buttons.h"
#include "ui/eq/EqBandPanel.h"
#include "ui/eq/EqController.h"

#include <functional>
#include <memory>

namespace pm::ui
{

class EqGraph;

/**
    Mini panel of the selected band, floating next to its point (P.P.'s review of phase 1, §12).

        [bypass] [3 ⌒ Campana ⌄]        [solo] [×]
        400 Hz                            G5 +35
        −4,0 dB                           Q 2,50

    - Bypass, delete, and the shape button, which opens a row with the eight shapes.
    - Solo while the headphones are held (the graph moves the frequency while they are dragged).
    - Values: frequency with its note (FL Studio numbering), gain (slope in the cuts) and Q.
      Double click: type them. The wheel over the panel works as over the point.

    It sits above the point with a small arrow pointing at it, or below when there is no room,
    sliding briefly when it changes side. It never leaves the graph.

    The graph decides which band it shows (P.P., 2026-11-12): the one under the pointer, or being
    dragged, or held in solo. The pointer can travel from the point to the panel (getCorridor())
    and stay on it (isHovered()) without the panel going away.
*/
class EqBandPopup final : public juce::Component
{
public:
    EqBandPopup (EqGraph& graph, EqController& controller);
    ~EqBandPopup() override;

    /** Shows `band` (hidden for -1) next to `point`, inside `area` (graph coordinates). */
    void update (int band, juce::Point<float> point, juce::Rectangle<int> area);

    int getBand() const noexcept { return band; }
    bool isAbove() const noexcept { return above; }

    /** The card, without the arrow and the shape row, in the graph's coordinates. */
    juce::Rectangle<int> getCardBoundsInParent() const { return cardArea.translated (getX(), getY()); }

    /** The pointer is over the panel (or its row of shapes). Kept up to date by a mouse listener. */
    bool isHovered() const noexcept { return hovered; }
    /** Called by the mouse listener; public so tests can move the pointer in and out. */
    void setHovered (bool isOver, juce::Point<float> positionInParent);
    /** Tells the graph the pointer entered or left the panel (and where, in the graph's coordinates). */
    std::function<void (bool isOver, juce::Point<float> positionInParent)> onHoverChanged;

    /** Area between the point and the card (graph coordinates): crossing it keeps the panel open. */
    juce::Rectangle<float> getCorridor() const noexcept { return corridor; }

    /** Text of the two value lines, for tests: {frequency, note, gain or slope, Q}. */
    juce::StringArray getValueTexts() const { return { frequencyText, noteText, leftText, rightText }; }

    void setShapeRowOpen (bool shouldBeOpen);
    bool isShapeRowOpen() const noexcept { return shapeRowOpen; }

    // Slide when the panel changes side (public so tests can run it frame by frame).
    void advanceAnimation (double seconds);
    bool isAnimating() const noexcept { return animating; }
    static constexpr double slideSeconds = 0.04;

    // Controls, for tests.
    IconButton& getBypassButton() noexcept { return bypass; }
    juce::Button& getShapeButton() noexcept;
    juce::Component& getSoloButton() noexcept;
    IconButton& getDeleteButton() noexcept { return remove; }
    ShapeSelector& getShapeRow() noexcept { return shapeRow; }

    static constexpr int minCardWidth = 196;
    static constexpr int cardHeight = 76;
    static constexpr int arrowSize = 7;
    static constexpr float pointGap = 16.0f; // from the point's centre to the tip of the arrow

    // Component
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    class ShapeButton;
    class SoloButton;
    class HoverTracker;

    void refreshContent();
    void layout();
    void frame (double timestampSeconds);

    EqGraph& graph;
    EqController& controller;
    int band = -1;

    IconButton bypass;
    std::unique_ptr<ShapeButton> shape;
    std::unique_ptr<SoloButton> solo;
    IconButton remove;
    ShapeSelector shapeRow;
    bool shapeRowOpen = false;

    // Placement, in graph coordinates.
    juce::Point<float> point;
    juce::Rectangle<int> area;
    bool above = true;
    float cardX = 0.0f;
    float cardY = 0.0f;       // where it is drawn (slides towards targetY)
    float targetY = 0.0f;
    bool animating = false;
    bool placed = false;

    bool hovered = false;
    std::unique_ptr<HoverTracker> hoverTracker;
    juce::Rectangle<float> corridor;

    // Local geometry.
    juce::Rectangle<int> cardArea;
    float arrowX = 0.0f;

    // Shown values.
    juce::Colour colour;
    bool bypassed = false;
    bool usesSlope = false;
    juce::String frequencyText, noteText, leftText, rightText;

    juce::VBlankAttachment vblank;
    double lastFrameTime = 0.0;
};

} // namespace pm::ui
