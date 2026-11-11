// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/eq/EqBandPopup.h"

#include "PluginProcessor.h"
#include "core/ValueFormat.h"
#include "ui/eq/EqGraph.h"
#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

using dsp::eq::FilterType;

namespace
{
juce::String utf8Text (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); }

constexpr int padding = 7;
constexpr int rowHeight = 24;
constexpr int valuesTop = padding + rowHeight + 6; // separator line
constexpr int lineHeight = 15;
constexpr int shadowMargin = 10;
constexpr int shapeRowGap = 6;

void dontTakeFocus (juce::Component& c)
{
    // The panel never takes the keyboard: the space bar must keep reaching FL Studio.
    c.setWantsKeyboardFocus (false);
    c.setMouseClickGrabsKeyboardFocus (false);
}
} // namespace

//==============================================================================
/** Number, icon and name of the band's shape, with a chevron: opens the row of shapes. */
class EqBandPopup::ShapeButton final : public juce::Button
{
public:
    ShapeButton() : juce::Button ("Tipo de banda") { dontTakeFocus (*this); }

    void set (int newNumber, int newType, juce::Colour newColour, bool isBypassed, bool isOpen)
    {
        if (newNumber != number || newType != type || newColour != colour || isBypassed != bypassed || isOpen != open)
        {
            number = newNumber;
            type = newType;
            colour = newColour;
            bypassed = isBypassed;
            open = isOpen;
            setTitle (utf8Text ("Tipo de banda: ") + param::eqTypeNames()[type]);
            repaint();
        }
    }

    int preferredWidth (const Theme& theme) const
    {
        return 3 + 16 + 5 + 15 + 5 + (int) std::ceil (textWidth (theme.ui (11.5f), param::eqTypeNames()[type])) + 4 + 12 + 6;
    }

    void paintButton (juce::Graphics& g, bool isMouseOver, bool) override
    {
        const auto& theme = themeOf (*this);
        const auto& c = theme.colours;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (isMouseOver ? c.elevatedHigh : c.background);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (open ? colour : c.border);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        const auto alpha = bypassed ? 0.45f : 1.0f;
        auto area = getLocalBounds().toFloat().withTrimmedLeft (3.0f);
        const auto badge = area.removeFromLeft (16.0f).withSizeKeepingCentre (16.0f, 16.0f);
        g.setColour ((bypassed ? c.textDim : colour).withMultipliedAlpha (alpha));
        g.fillEllipse (badge);
        g.setColour (c.onAccent);
        g.setFont (theme.ui (9.5f, Weight::semiBold));
        g.drawText (juce::String (number), badge, juce::Justification::centred, false);
        area.removeFromLeft (5.0f);
        icons::draw (g, icons::eqShape (type), area.removeFromLeft (15.0f), 15.0f,
                     (bypassed ? c.textMuted : colour).withMultipliedAlpha (alpha), 1.6f);
        area.removeFromLeft (5.0f);
        const auto chevron = area.removeFromRight (18.0f);
        g.setColour (c.text.withMultipliedAlpha (alpha));
        g.setFont (theme.ui (11.5f));
        g.drawText (param::eqTypeNames()[type], area, juce::Justification::centredLeft, false);
        icons::draw (g, icons::Icon::chevronDown, chevron.withTrimmedRight (6.0f), 12.0f, c.textMuted, 1.4f);
    }

private:
    int number = 1;
    int type = 0;
    juce::Colour colour;
    bool bypassed = false;
    bool open = false;
};

//==============================================================================
/** Headphones: solo while held. Dragging it is handled by the graph (frequency, Ctrl: Q). */
class EqBandPopup::SoloButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SoloButton (EqGraph& g) : graph (g)
    {
        setTitle ("Solo");
        dontTakeFocus (*this);
        setTooltip (utf8Text ("Solo\nMant\xc3\xa9n pulsado para o\xc3\xadr solo la zona de la banda. Arrastra a los lados "
                          "para moverla por el espectro (con Ctrl, cambia la Q). Al soltar, se apaga."));
    }

    void set (int newBand, juce::Colour newColour, bool isActive)
    {
        if (newBand != band || newColour != colour || isActive != active)
        {
            band = newBand;
            colour = newColour;
            active = isActive;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = themeOf (*this);
        const auto& c = theme.colours;
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        if (active)
        {
            g.setColour (colour);
            g.fillRoundedRectangle (r, 6.0f);
        }
        else if (isMouseOver())
        {
            g.setColour (c.elevatedHigh);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (c.border);
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
        }
        icons::draw (g, icons::Icon::headphones, getLocalBounds().toFloat(), 15.0f, active ? c.onAccent : c.textSecondary, 1.4f);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (band < 0 || e.mods.isPopupMenu())
            return;
        start = e.getScreenPosition().toFloat();
        graph.beginSolo (band);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        // Screen coordinates: the panel (and this button) follows the point while dragging.
        graph.soloDragTo (e.getScreenPosition().toFloat() - start, e.mods.isCtrlDown() || e.mods.isCommandDown());
    }

    void mouseUp (const juce::MouseEvent&) override { graph.endSolo(); }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::button);
    }

private:
    EqGraph& graph;
    int band = -1;
    juce::Colour colour;
    bool active = false;
    juce::Point<float> start;
};

//==============================================================================
EqBandPopup::EqBandPopup (EqGraph& g, EqController& c)
    : graph (g),
      controller (c),
      bypass ("Bypass de la banda", icons::Icon::power, 15.0f),
      shape (std::make_unique<ShapeButton>()),
      solo (std::make_unique<SoloButton> (g)),
      remove ("Eliminar banda", icons::Icon::close, 14.0f),
      vblank (this, [this] (double t) { frame (t); })
{
    setTitle ("Banda");
    setInterceptsMouseClicks (true, true);

    bypass.setTooltip (utf8Text ("Bypass de la banda\nLa deja sin efecto sin borrarla (tambi\xc3\xa9n Alt+clic en su punto)."));
    bypass.onClick = [this]
    {
        if (band >= 0)
            controller.setBypassed (band, ! controller.getBand (band).bypassed);
        graph.bandsChanged();
    };

    shape->setTooltip (utf8Text ("Tipo de curva\nDespliega las 8 formas."));
    shape->onClick = [this] { setShapeRowOpen (! shapeRowOpen); };

    remove.setTooltip (utf8Text ("Eliminar banda\nTambi\xc3\xa9n con la tecla Supr."));
    remove.onClick = [this]
    {
        if (band >= 0)
            controller.deleteBand (band);
        graph.bandsChanged();
    };

    shapeRow.onChange = [this] (int type)
    {
        if (band >= 0)
            controller.setType (band, (FilterType) type);
        setShapeRowOpen (false);
        graph.bandsChanged();
    };

    for (auto* comp : std::initializer_list<juce::Component*> { &bypass, shape.get(), solo.get(), &remove, &shapeRow })
    {
        dontTakeFocus (*comp);
        addAndMakeVisible (comp);
    }
    shapeRow.setVisible (false);
    setVisible (false);
}

EqBandPopup::~EqBandPopup() = default;

juce::Button& EqBandPopup::getShapeButton() noexcept { return *shape; }
juce::Component& EqBandPopup::getSoloButton() noexcept { return *solo; }

void EqBandPopup::setShapeRowOpen (bool shouldBeOpen)
{
    if (shouldBeOpen == shapeRowOpen)
        return;
    shapeRowOpen = shouldBeOpen;
    shapeRow.setVisible (shapeRowOpen);
    refreshContent();
    layout();
}

void EqBandPopup::update (int newBand, juce::Point<float> newPoint, juce::Rectangle<int> newArea)
{
    if (newBand < 0)
    {
        band = -1;
        placed = false;
        animating = false;
        if (shapeRowOpen)
            setShapeRowOpen (false);
        setVisible (false);
        return;
    }

    if (newBand != band)
    {
        band = newBand;
        placed = false; // a new band: appear in place, no slide
        if (shapeRowOpen)
            setShapeRowOpen (false);
    }

    point = newPoint;
    area = newArea;
    refreshContent();

    // Above the point when it fits, else below.
    const auto cardWidth = (float) cardArea.getWidth();
    const auto aboveTop = point.y - pointGap - (float) arrowSize - (float) cardHeight;
    const auto nowAbove = aboveTop >= (float) area.getY() + 4.0f;
    const auto newTarget = nowAbove ? aboveTop : point.y + pointGap + (float) arrowSize;

    cardX = juce::jlimit ((float) area.getX() + 6.0f, juce::jmax ((float) area.getX() + 6.0f, (float) area.getRight() - 6.0f - cardWidth),
                          point.x - cardWidth * 0.5f);
    if (placed && nowAbove != above)
        animating = true; // changed side: slide there
    above = nowAbove;
    targetY = newTarget;
    if (! animating || ! placed)
    {
        cardY = targetY;
        animating = false;
    }
    placed = true;

    layout();
    setVisible (true);
}

void EqBandPopup::refreshContent()
{
    if (band < 0)
        return;

    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto b = controller.getBand (band);
    const auto& s = b.settings;

    colour = theme.eqBandColour (band);
    bypassed = b.bypassed;
    usesSlope = dsp::eq::usesSlope (s.type);

    frequencyText = format::frequency (s.frequency);
    noteText = format::note (s.frequency);
    if (dsp::eq::usesGain (s.type))
        leftText = format::decibels (s.gainDb, 1, true);
    else if (usesSlope)
        leftText = juce::String (dsp::eq::slopesDbPerOctave[(size_t) s.slopeIndex]) + " dB/oct";
    else
        leftText = utf8Text ("\xe2\x80\x94");
    rightText = dsp::eq::usesQ (s.type, s.slopeIndex) ? "Q " + format::number (s.q, 2) : utf8Text ("Q \xe2\x80\x94");

    bypass.setIconColour (bypassed ? c.textDim : colour);
    bypass.setTitle (bypassed ? utf8Text ("Activar la banda") : utf8Text ("Bypass de la banda"));
    shape->set (band + 1, (int) s.type, colour, bypassed, shapeRowOpen);
    solo->set (band, colour, controller.getSoloBand() == band);
    shapeRow.setSelected ((int) s.type);
    shapeRow.setAccentColour (colour);

    // Card width: wide enough for the longest shape name.
    const auto row = padding + rowHeight + 4 + shape->preferredWidth (theme) + 10 + rowHeight + 4 + rowHeight + padding;
    cardArea.setSize (juce::jmax (minCardWidth, row), cardHeight);
    repaint();
}

void EqBandPopup::layout()
{
    if (band < 0)
        return;

    const auto cardW = cardArea.getWidth();
    auto card = juce::Rectangle<float> (cardX, cardY, (float) cardW, (float) cardHeight);

    // The arrow points at the point, kept away from the rounded corners.
    const auto tipX = juce::jlimit (card.getX() + 14.0f, card.getRight() - 14.0f, point.x);
    const auto arrowRect = above ? juce::Rectangle<float> (tipX - arrowSize, card.getBottom(), 2.0f * arrowSize, (float) arrowSize)
                                 : juce::Rectangle<float> (tipX - arrowSize, card.getY() - arrowSize, 2.0f * arrowSize, (float) arrowSize);

    auto all = card.getUnion (arrowRect);
    juce::Rectangle<float> row;
    if (shapeRowOpen)
    {
        const auto size = shapeRow.getPreferredBounds();
        const auto rowX = juce::jlimit ((float) area.getX() + 6.0f, juce::jmax ((float) area.getX() + 6.0f, (float) area.getRight() - 6.0f - (float) size.getWidth()),
                                        card.getX());
        // Under the card (past the arrow), or over it when there is no room below.
        auto rowY = card.getBottom() + (above ? (float) arrowSize : 0.0f) + (float) shapeRowGap;
        if (rowY + (float) size.getHeight() > (float) area.getBottom() - 4.0f)
            rowY = card.getY() - (above ? 0.0f : (float) arrowSize) - (float) shapeRowGap - (float) size.getHeight();
        row = { rowX, rowY, (float) size.getWidth(), (float) size.getHeight() };
        all = all.getUnion (row);
    }

    const auto bounds = all.getSmallestIntegerContainer().expanded (shadowMargin);
    setBounds (bounds);
    const auto origin = bounds.getPosition().toFloat();
    cardArea = card.translated (-origin.x, -origin.y).getSmallestIntegerContainer().withSize (cardW, cardHeight);
    arrowX = tipX - origin.x;

    // Row 1: bypass, shape … solo, delete.
    auto top = cardArea.reduced (padding, 0).withTrimmedTop (padding).withHeight (rowHeight);
    bypass.setBounds (top.removeFromLeft (rowHeight));
    top.removeFromLeft (4);
    remove.setBounds (top.removeFromRight (rowHeight));
    top.removeFromRight (4);
    solo->setBounds (top.removeFromRight (rowHeight));
    shape->setBounds (top.removeFromLeft (shape->preferredWidth (themeOf (*this))));

    if (shapeRowOpen)
        shapeRow.setBounds (row.translated (-origin.x, -origin.y).toNearestInt());
}

void EqBandPopup::advanceAnimation (double seconds)
{
    if (! animating)
        return;
    const auto k = (float) (1.0 - std::exp (-seconds / slideSeconds));
    cardY += (targetY - cardY) * k;
    if (std::abs (targetY - cardY) < 0.5f)
    {
        cardY = targetY;
        animating = false;
    }
    layout();
}

void EqBandPopup::frame (double timestampSeconds)
{
    const auto elapsed = lastFrameTime > 0.0 ? juce::jlimit (0.0, 0.1, timestampSeconds - lastFrameTime) : 0.0;
    lastFrameTime = timestampSeconds;
    advanceAnimation (elapsed);
}

bool EqBandPopup::hitTest (int x, int y)
{
    if (cardArea.contains (x, y))
        return true;
    return shapeRowOpen && shapeRow.getBounds().contains (x, y);
}

void EqBandPopup::paint (juce::Graphics& g)
{
    if (band < 0)
        return;

    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto card = cardArea.toFloat();

    // Shadow, card and the arrow towards the point.
    juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 16, { 0, 5 }).drawForRectangle (g, cardArea);
    g.setColour (c.elevated);
    g.fillRoundedRectangle (card.reduced (0.5f), 9.0f);
    g.setColour (c.borderStrong);
    g.drawRoundedRectangle (card.reduced (0.5f), 9.0f, 1.0f);

    const auto edge = above ? card.getBottom() - 0.5f : card.getY() + 0.5f;
    const auto tip = above ? edge + (float) arrowSize : edge - (float) arrowSize;
    juce::Path arrow;
    arrow.startNewSubPath (arrowX - (float) arrowSize, edge);
    arrow.lineTo (arrowX, tip);
    arrow.lineTo (arrowX + (float) arrowSize, edge);
    g.setColour (c.elevated);
    g.fillPath (arrow);
    g.fillRect (juce::Rectangle<float> (arrowX - (float) arrowSize + 1.0f, above ? edge - 1.5f : edge - 0.5f, 2.0f * (float) arrowSize - 2.0f, 2.0f));
    g.setColour (c.borderStrong);
    g.strokePath (arrow, juce::PathStrokeType (1.0f));

    // Separator and values.
    g.setColour (c.border);
    g.fillRect (juce::Rectangle<float> (card.getX() + 8.0f, card.getY() + (float) valuesTop, card.getWidth() - 16.0f, 1.0f));

    const auto alpha = bypassed ? 0.45f : 1.0f;
    const auto font = theme.mono (11.0f, Weight::regular);
    const auto hintFont = theme.ui (9.0f);
    auto lines = cardArea.reduced (11, 0).withTrimmedTop (valuesTop + 4).withHeight (2 * lineHeight).toFloat();
    auto first = lines.removeFromTop ((float) lineHeight);
    auto second = lines.removeFromTop ((float) lineHeight);

    const auto drawHint = [&] (juce::Rectangle<float>& line, const juce::String& hint, bool fromRight)
    {
        const auto w = textWidth (hintFont, hint) + 8.0f;
        const auto box = (fromRight ? line.removeFromRight (w) : line.removeFromLeft (w)).withSizeKeepingCentre (w, 12.0f);
        if (fromRight)
            line.removeFromRight (4.0f);
        g.setColour (c.border.withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (c.textMuted.withMultipliedAlpha (alpha));
        g.setFont (hintFont);
        g.drawText (hint, box, juce::Justification::centred, false);
    };

    g.setFont (font);
    g.setColour (c.text.withMultipliedAlpha (alpha));
    g.drawText (frequencyText, first, juce::Justification::centredLeft, false);
    g.setColour (c.textMuted.withMultipliedAlpha (alpha));
    g.drawText (noteText, first, juce::Justification::centredRight, false);

    if (usesSlope)
    {
        // The cuts remind which gesture changes what: wheel = slope, Ctrl+wheel = Q.
        auto right = second.removeFromRight (second.getWidth() * 0.5f);
        drawHint (right, "Ctrl", true);
        g.setFont (font);
        g.setColour (c.text.withMultipliedAlpha (alpha));
        g.drawText (rightText, right, juce::Justification::centredRight, false);
        const auto w = textWidth (font, leftText);
        g.drawText (leftText, second.removeFromLeft (w + 2.0f), juce::Justification::centredLeft, false);
        second.removeFromLeft (4.0f);
        drawHint (second, "rueda", false);
    }
    else
    {
        g.setColour ((leftText == utf8Text ("\xe2\x80\x94") ? c.textMuted : c.text).withMultipliedAlpha (alpha));
        g.drawText (leftText, second, juce::Justification::centredLeft, false);
        g.setColour (c.text.withMultipliedAlpha (alpha));
        g.drawText (rightText, second, juce::Justification::centredRight, false);
    }
}

void EqBandPopup::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (band >= 0 && e.y > cardArea.getY() + valuesTop)
        graph.showValueEditor (band);
}

void EqBandPopup::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // The wheel over the panel works as over the point.
    const auto delta = wheel.isReversed ? -wheel.deltaY : wheel.deltaY;
    graph.wheel (band, delta, e.mods.isCtrlDown() || e.mods.isCommandDown(), e.mods.isShiftDown());
}

} // namespace pm::ui
