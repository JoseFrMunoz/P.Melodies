// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

namespace
{
constexpr float tooltipMaxWidth = 300.0f;
constexpr float tooltipPadX = 10.0f;
constexpr float tooltipPadY = 8.0f;
} // namespace

LookAndFeel::LookAndFeel (const Theme& initialTheme) : theme (&initialTheme)
{
    applyColours();
}

void LookAndFeel::setTheme (const Theme& newTheme)
{
    theme = &newTheme;
    applyColours();
}

void LookAndFeel::applyColours()
{
    const auto& c = theme->colours;

    setDefaultSansSerifTypeface (theme->ui (12.0f).getTypefacePtr());

    setColour (juce::ResizableWindow::backgroundColourId, c.background);
    setColour (juce::TooltipWindow::backgroundColourId, c.elevatedHigh);
    setColour (juce::TooltipWindow::textColourId, c.text);
    setColour (juce::TooltipWindow::outlineColourId, c.borderHover);
    setColour (juce::PopupMenu::backgroundColourId, c.elevatedHigh);
    setColour (juce::PopupMenu::textColourId, c.textSecondary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, c.elevated);
    setColour (juce::PopupMenu::highlightedTextColourId, c.text);
    setColour (juce::TextEditor::backgroundColourId, c.window);
    setColour (juce::TextEditor::textColourId, c.text);
    setColour (juce::TextEditor::outlineColourId, c.accent);
    setColour (juce::TextEditor::focusedOutlineColourId, c.accent);
    setColour (juce::TextEditor::highlightColourId, c.accent.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, c.text);
    setColour (juce::CaretComponent::caretColourId, c.accent);
    setColour (juce::HyperlinkButton::textColourId, c.accent);
}

//==============================================================================
juce::TextLayout layoutTooltip (const Theme& theme, const juce::String& text, float maxWidth)
{
    const auto title = text.upToFirstOccurrenceOf ("\n", false, false);
    const auto body = text.fromFirstOccurrenceOf ("\n", false, false);

    juce::AttributedString s;
    s.setWordWrap (juce::AttributedString::byWord);
    s.setLineSpacing (3.0f);

    if (body.isEmpty())
    {
        s.append (title, theme.ui (12.0f), theme.colours.text);
    }
    else
    {
        s.append (title + "\n", theme.ui (12.0f, Weight::semiBold), theme.colours.text);
        s.append (body, theme.ui (12.0f), theme.colours.textSecondary);
    }

    juce::TextLayout layout;
    layout.createLayout (s, maxWidth);
    return layout;
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> screenPos,
                                                    juce::Rectangle<int> parentArea)
{
    const auto layout = layoutTooltip (*theme, text, tooltipMaxWidth);
    const auto w = (int) std::ceil (layout.getWidth() + 2.0f * tooltipPadX);
    const auto h = (int) std::ceil (layout.getHeight() + 2.0f * tooltipPadY);

    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 16,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 8) : screenPos.y + 18, w, h)
        .constrainedWithin (parentArea);
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto bounds = juce::Rectangle<float> ((float) width, (float) height);
    g.setColour (theme->colours.elevatedHigh);
    g.fillRoundedRectangle (bounds, theme->metrics.radiusControl);
    g.setColour (theme->colours.borderHover);
    g.drawRoundedRectangle (bounds.reduced (0.5f), theme->metrics.radiusControl, 1.0f);

    layoutTooltip (*theme, text, tooltipMaxWidth).draw (g, bounds.reduced (tooltipPadX, tooltipPadY));
}

//==============================================================================
void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto bounds = juce::Rectangle<float> ((float) width, (float) height);
    g.setColour (theme->colours.elevatedHigh);
    g.fillRect (bounds);
    g.setColour (theme->colours.borderHover);
    g.drawRect (bounds, 1.0f);
}

juce::Font LookAndFeel::getPopupMenuFont()
{
    return theme->ui (12.0f);
}

void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                     bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                     const juce::String& text, const juce::String& shortcutKeyText,
                                     const juce::Drawable* icon, const juce::Colour* textColour)
{
    juce::ignoreUnused (hasSubMenu, shortcutKeyText, icon);
    const auto& c = theme->colours;

    if (isSeparator)
    {
        g.setColour (c.border);
        g.fillRect (area.reduced (6, 0).withHeight (1).withCentre (area.getCentre()));
        return;
    }

    auto r = area.reduced (2, 1).toFloat();

    if (isHighlighted && isActive)
    {
        g.setColour (isTicked ? c.accent.withAlpha (0.18f) : c.elevated);
        g.fillRoundedRectangle (r, theme->metrics.radiusSmall);
    }
    else if (isTicked)
    {
        g.setColour (c.accent.withAlpha (0.12f));
        g.fillRoundedRectangle (r, theme->metrics.radiusSmall);
    }

    auto colour = textColour != nullptr ? *textColour : (isTicked ? c.accent : isHighlighted ? c.text : c.textSecondary);
    if (! isActive)
        colour = c.textDim;

    g.setColour (colour);
    g.setFont (getPopupMenuFont());
    g.drawFittedText (text, r.reduced (10.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1);
}

//==============================================================================
void LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor&)
{
    g.setColour (theme->colours.window);
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) width, (float) height), theme->metrics.radiusSmall);
}

void LookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor&)
{
    g.setColour (theme->colours.accent);
    g.drawRoundedRectangle (juce::Rectangle<float> ((float) width, (float) height).reduced (0.5f),
                            theme->metrics.radiusSmall, 1.0f);
}

//==============================================================================
const Theme& themeOf (const juce::Component& component)
{
    if (auto* lnf = dynamic_cast<const LookAndFeel*> (&component.getLookAndFeel()))
        return lnf->getTheme();

    return Theme::dark();
}

} // namespace pm::ui
