// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/components/Knob.h"

#include "core/ValueFormat.h"
#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

namespace
{
// Arc geometry from the design (44 × 44 viewBox): 270° sweep starting at 7:30.
constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;
constexpr float sweepAngle = juce::MathConstants<float>::pi * 1.5f;

constexpr int standardDial = 46;
constexpr int compactDial = 32;
constexpr int valueHeight = 14;
constexpr int labelHeight = 12;
} // namespace

//==============================================================================
class Knob::ValueBubble final : public juce::Component
{
public:
    ValueBubble() { setInterceptsMouseClicks (false, false); }

    void setText (const juce::String& newText, const Theme& theme)
    {
        text = newText;
        font = theme.mono (11.0f);
        colours = { theme.colours.accent, theme.colours.onAccent };
        setSize ((int) std::ceil (textWidth (font, text)) + 14, 18);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (colours.first);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        g.setColour (colours.second);
        g.setFont (font);
        g.drawText (text, getLocalBounds(), juce::Justification::centred, false);
    }

private:
    juce::String text;
    juce::Font font { juce::FontOptions {} };
    std::pair<juce::Colour, juce::Colour> colours;
};

//==============================================================================
Knob::Knob (juce::RangedAudioParameter& p, juce::String labelText, Style knobStyle)
    : parameter (p),
      attachment (p, [this] (float) { repaint(); }, nullptr),
      label (std::move (labelText)),
      style (knobStyle)
{
    setTitle (label.isNotEmpty() ? label : p.getName (64));
    setRepaintsOnMouseActivity (false);
    attachment.sendInitialUpdate();
}

Knob::~Knob()
{
    hideBubble();
}

void Knob::setAccentColour (juce::Colour colour)
{
    accent = colour;
    repaint();
}

void Knob::setFormatter (Formatter newFormatter)
{
    formatter = std::move (newFormatter);
    repaint();
}

juce::String Knob::getValueText() const
{
    const auto value = parameter.convertFrom0to1 (parameter.getValue());

    if (formatter)
        return formatter (value);

    auto text = parameter.getCurrentValueAsText();
    if (text.startsWithChar ('-'))
        text = format::unicodeMinus() + text.substring (1);

    const auto unit = parameter.getLabel();
    return unit.isNotEmpty() ? text + " " + unit : text;
}

juce::Rectangle<int> Knob::getPreferredBounds() const
{
    if (style == Style::compact)
        return { compactDial, compactDial };

    if (style == Style::valueOnly)
        return { (int) std::ceil (textWidth (themeOf (*this).mono (11.0f), getValueText())) + 8, 18 };

    const auto& theme = themeOf (*this);
    const auto labelWidth = (int) std::ceil (textWidth (theme.label(), label.toUpperCase()));
    return { juce::jmax (60, labelWidth + 4), standardDial + 4 + valueHeight + 3 + labelHeight };
}

float Knob::valueAfterDrag (float startValue, float deltaPixels, bool fine) noexcept
{
    const auto delta = deltaPixels / pixelsForFullRange * (fine ? fineFactor : 1.0f);
    return juce::jlimit (0.0f, 1.0f, startValue + delta);
}

juce::Rectangle<float> Knob::getDialBounds() const
{
    const auto size = (float) (style == Style::compact ? compactDial : standardDial);
    const auto bounds = getLocalBounds().toFloat();
    return { bounds.getCentreX() - size * 0.5f, style == Style::compact ? bounds.getCentreY() - size * 0.5f : 0.0f,
             size, size };
}

//==============================================================================
void Knob::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto dial = getDialBounds();
    const auto scale = dial.getWidth() / 44.0f;
    const auto centre = dial.getCentre();
    const auto arcColour = accent.isTransparent() ? c.accent : accent;
    if (style == Style::valueOnly)
    {
        if (! isTyping())
        {
            g.setColour (dragging || hovering ? arcColour : c.text);
            g.setFont (theme.mono (11.0f));
            g.drawText (getValueText(), getLocalBounds(), juce::Justification::centredRight, false);
        }
        return;
    }

    const auto trackColour = hovering && ! dragging ? c.borderHover : c.border;
    const auto capColour = hovering && ! dragging ? c.knobCapHover : c.elevatedHigh;
    const auto capOutline = dragging ? arcColour : hovering ? c.knobCapBorderHover : c.borderStrong;
    const auto pointerColour = hovering && ! dragging ? c.textBright : c.text;

    const auto strokeWidth = (style == Style::compact ? 4.0f : 3.5f) * scale;
    const auto radius = 17.0f * scale;
    const juce::PathStrokeType stroke (strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, startAngle + sweepAngle, true);
    g.setColour (trackColour);
    g.strokePath (track, stroke);

    const auto value = getNormalisedValue();
    const auto valueAngle = startAngle + sweepAngle * value;

    if (value > 0.0005f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, valueAngle, true);
        g.setColour (arcColour);
        g.strokePath (arc, stroke);
    }

    if (style == Style::compact)
    {
        const auto capRadius = 11.0f * scale;
        g.setColour (capColour);
        g.fillEllipse (juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre));
        return;
    }

    const auto capRadius = 11.5f * scale;
    const auto cap = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre);
    g.setColour (capColour);
    g.fillEllipse (cap);
    g.setColour (capOutline);
    g.drawEllipse (cap, 1.0f);

    const auto pointerLength = 9.0f * scale;
    const auto tip = centre.getPointOnCircumference (pointerLength, valueAngle);
    g.setColour (pointerColour);
    g.drawLine ({ centre, tip }, 2.0f * scale);
    g.fillEllipse (juce::Rectangle<float> (2.0f * scale, 2.0f * scale).withCentre (centre));
    g.fillEllipse (juce::Rectangle<float> (2.0f * scale, 2.0f * scale).withCentre (tip));

    // Value and label underneath.
    auto text = getLocalBounds().withTrimmedTop (standardDial + 4);
    auto valueArea = text.removeFromTop (valueHeight);
    text.removeFromTop (3);

    if (! isTyping())
    {
        g.setColour (c.text);
        g.setFont (theme.mono (11.0f));
        g.drawText (getValueText(), valueArea, juce::Justification::centred, false);
    }

    g.setColour (c.textMuted);
    g.setFont (theme.label());
    g.drawText (label.toUpperCase(), text.removeFromTop (labelHeight), juce::Justification::centred, false);
}

void Knob::resized()
{
    if (editor != nullptr && style == Style::valueOnly)
        editor->setBounds (getLocalBounds());
    else if (editor != nullptr)
        editor->setBounds (juce::Rectangle<int> (54, 18).withCentre ({ getWidth() / 2, standardDial + 4 + valueHeight / 2 }));
}

//==============================================================================
void Knob::mouseEnter (const juce::MouseEvent&)
{
    hovering = true;
    repaint();
}

void Knob::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
    repaint();
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled())
        return;

    if (e.mods.isAltDown())
    {
        beginTyping();
        return;
    }

    if (isTyping())
        finishTyping (true);

    dragging = true;
    lastDragY = e.position.y;
    dragValue = getNormalisedValue();
    attachment.beginGesture();
    showBubble();
    repaint();
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    const auto delta = lastDragY - e.position.y;
    lastDragY = e.position.y;
    dragValue = valueAfterDrag (dragValue, delta, e.mods.isCtrlDown() || e.mods.isCommandDown());
    setNormalisedValueAsPartOfGesture (dragValue);
    showBubble();
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;

    dragging = false;
    attachment.endGesture();
    hideBubble();
    repaint();
}

void Knob::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (isEnabled() && ! e.mods.isAltDown())
        resetToDefault();
}

void Knob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (! isEnabled() || isTyping())
        return;

    const auto direction = (wheel.deltaY >= 0.0f) != wheel.isReversed ? 1.0f : -1.0f;
    if (std::abs (wheel.deltaY) < 1.0e-4f)
        return;

    const auto fine = e.mods.isCtrlDown() || e.mods.isCommandDown();
    const auto step = wheelStep * (fine ? fineFactor : 1.0f) * direction;

    const auto& range = parameter.getNormalisableRange();
    const auto current = parameter.convertFrom0to1 (getNormalisedValue());
    auto target = parameter.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, getNormalisedValue() + step));

    // Always move at least one step of the parameter, even with very fine wheels.
    if (range.interval > 0.0f && std::abs (target - current) < range.interval * 0.5f)
        target = juce::jlimit (range.start, range.end, current + range.interval * direction);

    attachment.setValueAsCompleteGesture (target);
}

//==============================================================================
void Knob::setNormalisedValueAsPartOfGesture (float normalised)
{
    attachment.setValueAsPartOfGesture (parameter.convertFrom0to1 (normalised));
}

void Knob::setNormalisedValueAsCompleteGesture (float normalised)
{
    attachment.setValueAsCompleteGesture (parameter.convertFrom0to1 (normalised));
}

void Knob::resetToDefault()
{
    setNormalisedValueAsCompleteGesture (parameter.getDefaultValue());
}

void Knob::showBubble()
{
    if (style != Style::standard)
        return; // the value is already visible next to the control

    auto* host = findParentComponentOfClass<juce::AudioProcessorEditor>();
    if (host == nullptr)
        host = dynamic_cast<juce::AudioProcessorEditor*> (getTopLevelComponent());
    if (host == nullptr)
        return;

    if (bubble == nullptr)
    {
        bubble = std::make_unique<ValueBubble>();
        host->addAndMakeVisible (*bubble);
    }

    bubble->setText (getValueText(), themeOf (*this));
    const auto dial = getDialBounds();
    const auto anchor = host->getLocalPoint (this, juce::Point<float> (dial.getCentreX(), dial.getY() - 8.0f));
    bubble->setTopLeftPosition (juce::roundToInt (anchor.x - bubble->getWidth() * 0.5f), juce::roundToInt (anchor.y) - bubble->getHeight());
    bubble->toFront (false);
}

void Knob::hideBubble()
{
    if (bubble != nullptr)
    {
        if (auto* parent = bubble->getParentComponent())
            parent->removeChildComponent (bubble.get());
        bubble.reset();
    }
}

//==============================================================================
void Knob::beginTyping()
{
    if (style == Style::compact || isTyping())
        return;

    const auto& theme = themeOf (*this);
    editor = std::make_unique<juce::TextEditor>();
    editor->setFont (theme.mono (11.0f));
    editor->setJustification (juce::Justification::centred);
    editor->setIndents (2, 1);
    editor->setText (parameter.getCurrentValueAsText(), false);
    if (style == Style::valueOnly)
        editor->setJustification (juce::Justification::centredRight);
    editor->setTitle (getTitle());
    editor->onReturnKey = [this] { finishTyping (true); };
    editor->onEscapeKey = [this] { finishTyping (false); };
    editor->onFocusLost = [this] { finishTyping (true); };
    addAndMakeVisible (*editor);
    resized();
    editor->grabKeyboardFocus();
    editor->selectAll();
    repaint();
}

void Knob::finishTyping (bool apply)
{
    if (editor == nullptr)
        return;

    if (apply)
    {
        const auto text = editor->getText().trim();
        if (text.isNotEmpty())
            attachment.setValueAsCompleteGesture (parameter.convertFrom0to1 (parameter.getValueForText (text)));
    }

    // The editor may be calling us from one of its own callbacks: delete it afterwards.
    auto* finished = editor.release();
    finished->onReturnKey = nullptr;
    finished->onEscapeKey = nullptr;
    finished->onFocusLost = nullptr;
    finished->setVisible (false);
    juce::MessageManager::callAsync ([finished] { delete finished; });
    repaint();
}

//==============================================================================
std::unique_ptr<juce::AccessibilityHandler> Knob::createAccessibilityHandler()
{
    struct ValueInterface final : public juce::AccessibilityRangedNumericValueInterface
    {
        explicit ValueInterface (Knob& k) : knob (k) {}

        bool isReadOnly() const override { return false; }
        double getCurrentValue() const override { return knob.parameter.convertFrom0to1 (knob.parameter.getValue()); }
        void setValue (double newValue) override { knob.attachment.setValueAsCompleteGesture ((float) newValue); }

        AccessibleValueRange getRange() const override
        {
            const auto& r = knob.parameter.getNormalisableRange();
            return { { r.start, r.end }, r.interval };
        }

        Knob& knob;
    };

    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::slider, juce::AccessibilityActions(),
        juce::AccessibilityHandler::Interfaces { std::make_unique<ValueInterface> (*this) });
}

} // namespace pm::ui
