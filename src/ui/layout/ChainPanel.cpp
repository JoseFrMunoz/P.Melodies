// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/layout/ChainPanel.h"

#include "ui/ModuleInfo.h"
#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

namespace
{
constexpr int paddingX = 10;
constexpr int paddingTop = 12;
constexpr int rowGap = 5;
constexpr int markerHeight = 18;
constexpr int firstRowY = paddingTop + 18 + rowGap + markerHeight + rowGap; // label, IN marker
constexpr float dragTiltRadians = -1.5f * juce::MathConstants<float>::pi / 180.0f;

juce::String text (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); }
} // namespace

//==============================================================================
ModuleRow::ModuleRow (ChainPanel& panel, ModuleId moduleId, juce::RangedAudioParameter& enabledParameter)
    : owner (panel),
      id (moduleId),
      enabledAttachment (enabledParameter,
                         [this] (float v)
                         {
                             enabled = v >= 0.5f;
                             refreshPowerColour();
                             repaint();
                         },
                         nullptr),
      power (text ("Encender o apagar ") + moduleInfo (moduleId).shortName, icons::Icon::power, 14.0f)
{
    setTitle (moduleInfo (id).shortName);
    power.setTooltip (text ("Encender / apagar\nApagado, el m\xc3\xb3" "dulo deja pasar el audio sin tocarlo."));
    power.onClick = [this] { togglePower(); };
    addAndMakeVisible (power);
    enabledAttachment.sendInitialUpdate();
}

void ModuleRow::togglePower()
{
    enabledAttachment.setValueAsCompleteGesture (enabled ? 0.0f : 1.0f);
}

void ModuleRow::refreshPowerColour()
{
    const auto& c = themeOf (*this).colours;
    power.setIconColour (enabled ? c.accent : c.textDim);
    power.setTitle ((enabled ? text ("Apagar ") : text ("Encender ")) + moduleInfo (id).shortName);
}

void ModuleRow::setSelected (bool shouldBeSelected)
{
    selected = shouldBeSelected;
    repaint();
}

void ModuleRow::setDragging (bool shouldBeDragging)
{
    dragging = shouldBeDragging;
    repaint();
}

void ModuleRow::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto radius = theme.metrics.radiusRow;

    if (dragging)
    {
        juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 12, { 0, 4 }).drawForRectangle (g, getLocalBounds().reduced (4));
        g.setColour (c.elevatedHigh);
        g.fillRoundedRectangle (bounds, radius);
        g.setColour (c.accent);
        g.drawRoundedRectangle (bounds, radius, 1.0f);
    }
    else if (selected)
    {
        g.setColour (c.elevated);
        g.fillRoundedRectangle (bounds, radius);
        g.setColour (c.accent.withAlpha (0.4f));
        g.drawRoundedRectangle (bounds, radius, 1.0f);
    }
    else if (! enabled)
    {
        juce::Path outline;
        outline.addRoundedRectangle (bounds, radius);
        juce::Path dashed;
        const float dashes[] = { 3.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (c.borderHover);
        g.fillPath (dashed);
    }

    icons::drawDragHandle (g, getHandleArea().toFloat().withTrimmedLeft (6.0f).withWidth (10.0f), c.textDim);

    const auto info = moduleInfo (id);
    g.setColour (theme.moduleColour (id));
    g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ 30.0f, getHeight() * 0.5f }), 2.0f);

    auto textArea = getLocalBounds().withTrimmedLeft (42).withTrimmedRight (32);
    const auto lineTop = getHeight() / 2 - 15;
    g.setColour (enabled ? c.text : c.textMuted);
    g.setFont (theme.ui (13.0f, Weight::medium));
    g.drawText (info.shortName, textArea.withY (lineTop).withHeight (17), juce::Justification::centredLeft, true);
    g.setColour (c.textMuted);
    g.setFont (theme.ui (10.0f));
    g.drawText (enabled ? owner.getStatusText (id) : juce::String ("Apagado"), textArea.withY (lineTop + 17).withHeight (13),
                juce::Justification::centredLeft, true);
}

void ModuleRow::resized()
{
    power.setBounds (juce::Rectangle<int> (24, 24).withCentre ({ getWidth() - 6 - 12, getHeight() / 2 }));
}

void ModuleRow::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (getHandleArea().contains (e.getPosition()) ? juce::MouseCursor::DraggingHandCursor
                                                               : juce::MouseCursor::PointingHandCursor);
}

void ModuleRow::mouseDown (const juce::MouseEvent& e)
{
    dragStartedOnHandle = getHandleArea().contains (e.getPosition());
}

void ModuleRow::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragStartedOnHandle)
        return;

    const auto y = e.getEventRelativeTo (&owner).position.y;
    if (! owner.isDragging() && e.getDistanceFromDragStart() > 2)
        owner.beginDrag (*this, y);

    if (owner.isDragging())
        owner.continueDrag (y);
}

void ModuleRow::mouseUp (const juce::MouseEvent& e)
{
    if (owner.isDragging())
        owner.endDrag();
    else if (! e.mouseWasDraggedSinceMouseDown())
        owner.rowClicked (*this);

    dragStartedOnHandle = false;
}

//==============================================================================
ChainPanel::ChainPanel (juce::RangedAudioParameter& orderParameter,
                        const std::array<juce::RangedAudioParameter*, numModules>& enabledParameters)
    : orderAttachment (orderParameter,
                       [this] (float v)
                       {
                           order = chain::decode (juce::roundToInt (v));
                           if (draggedRow != nullptr) // the host moved things mid-drag: keep holding the same module
                               dragFromPosition = chain::positionOf (order, draggedRow->getId());
                           updateTargets (rowsPlaced);
                       },
                       nullptr),
      order (chain::decode (chain::defaultOrderIndex)),
      vblank (this, [this] (double timestampSeconds) { frame (timestampSeconds); })
{
    for (int i = 0; i < numModules; ++i)
    {
        rows[(size_t) i] = std::make_unique<ModuleRow> (*this, moduleAt (i), *enabledParameters[(size_t) i]);
        addAndMakeVisible (*rows[(size_t) i]);
    }

    setTitle (text ("Cadena de se\xc3\xb1" "al"));
    orderAttachment.sendInitialUpdate();
    setSelectedModule (ModuleId::eq);
}

ChainPanel::~ChainPanel() = default;

juce::String ChainPanel::getStatusText (ModuleId id) const
{
    return statusText ? statusText (id) : juce::String ("Activo");
}

void ChainPanel::refreshStatus()
{
    for (auto& row : rows)
    {
        auto& shown = shownStatus[(size_t) toIndex (row->getId())];
        if (const auto status = getStatusText (row->getId()); status != shown)
        {
            shown = status;
            row->repaint();
        }
    }
}

void ChainPanel::setSelectedModule (ModuleId id)
{
    selectedModule = id;
    for (auto& row : rows)
        row->setSelected (row->getId() == id);
}

juce::Rectangle<int> ChainPanel::getSlotBounds (int position) const
{
    return { paddingX, firstRowY + position * (ModuleRow::height + rowGap), getWidth() - 2 * paddingX, ModuleRow::height };
}

int ChainPanel::positionForRowTop (float y) const
{
    const auto pitch = (float) (ModuleRow::height + rowGap);
    return juce::jlimit (0, numModules - 1, juce::roundToInt ((y - (float) getSlotBounds (0).getY()) / pitch));
}

chain::Order ChainPanel::getDisplayedOrder() const
{
    return draggedRow != nullptr ? chain::move (order, dragFromPosition, dragPreviewPosition) : order;
}

juce::Rectangle<int> ChainPanel::getRowTargetBounds (ModuleId id) const
{
    return getSlotBounds (chain::positionOf (getDisplayedOrder(), id));
}

void ChainPanel::updateTargets (bool animate)
{
    const auto displayed = getDisplayedOrder();
    for (int p = 0; p < numModules; ++p)
    {
        const auto index = (size_t) toIndex (displayed[(size_t) p]);
        rowTargetTop[index] = (float) getSlotBounds (p).getY();
        if (! animate)
            rowTop[index] = rowTargetTop[index];
    }

    for (auto& row : rows)
        if (row.get() != draggedRow)
            placeRow (*row);

    if (! animate)
    {
        if (landingRow != nullptr)
            landingRow->setDragging (false);
        landingRow = nullptr;
    }
    rowsPlaced = true;
}

void ChainPanel::placeRow (ModuleRow& row)
{
    const auto slot = getSlotBounds (0);
    row.setBounds (slot.getX(), juce::roundToInt (rowTop[(size_t) toIndex (row.getId())]), slot.getWidth(), slot.getHeight());
}

void ChainPanel::advanceAnimation (double seconds)
{
    const auto step = (float) (1.0 - std::exp (-juce::jmax (0.0, seconds) / glideSeconds));
    for (auto& row : rows)
    {
        if (row.get() == draggedRow)
            continue; // it follows the pointer

        const auto index = (size_t) toIndex (row->getId());
        auto& top = rowTop[index];
        const auto target = rowTargetTop[index];
        top += (target - top) * step;
        if (std::abs (target - top) < 0.5f)
            top = target;
        placeRow (*row);

        if (row.get() == landingRow && juce::exactlyEqual (top, target))
        {
            landingRow->setDragging (false);
            landingRow = nullptr;
            repaint(); // removes the gap outline
        }
    }
}

bool ChainPanel::isAnimating() const noexcept
{
    for (auto& row : rows)
        if (row.get() != draggedRow && ! juce::exactlyEqual (rowTop[(size_t) toIndex (row->getId())],
                                                             rowTargetTop[(size_t) toIndex (row->getId())]))
            return true;
    return landingRow != nullptr;
}

void ChainPanel::frame (double timestampSeconds)
{
    // Frames can be irregular (or stop while the window is hidden): cap the step so a late frame
    // does not jump.
    const auto elapsed = lastFrameTime > 0.0 ? juce::jlimit (0.0, 0.05, timestampSeconds - lastFrameTime) : 0.0;
    lastFrameTime = timestampSeconds;
    if (isAnimating())
        advanceAnimation (elapsed);
}

void ChainPanel::resized()
{
    updateTargets (false);
}

void ChainPanel::rowClicked (ModuleRow& row)
{
    setSelectedModule (row.getId());
    if (onModuleSelected)
        onModuleSelected (row.getId());
}

void ChainPanel::beginDrag (ModuleRow& row, float y)
{
    if (landingRow != nullptr)
        landingRow->setDragging (false);
    landingRow = nullptr;

    draggedRow = &row;
    dragFromPosition = chain::positionOf (order, row.getId());
    dragPreviewPosition = dragFromPosition;
    dragGrabOffset = y - (float) row.getY();
    row.setDragging (true);
    row.toFront (false);
    repaint();
}

void ChainPanel::continueDrag (float y)
{
    if (draggedRow == nullptr)
        return;

    const auto top = juce::jlimit ((float) getSlotBounds (0).getY() - 12.0f,
                                   (float) getSlotBounds (numModules - 1).getY() + 12.0f, y - dragGrabOffset);
    rowTop[(size_t) toIndex (draggedRow->getId())] = top;
    placeRow (*draggedRow);
    draggedRow->setTransform (juce::AffineTransform::rotation (dragTiltRadians, (float) draggedRow->getBounds().getCentreX(),
                                                               (float) draggedRow->getBounds().getCentreY()));

    // The others make room as soon as the dragged row is nearer to another slot than to its own.
    if (const auto preview = positionForRowTop (top); preview != dragPreviewPosition)
    {
        dragPreviewPosition = preview;
        updateTargets (true);
        repaint();
    }
}

void ChainPanel::endDrag()
{
    if (draggedRow == nullptr)
        return;

    auto& row = *draggedRow;
    row.setTransform ({});
    moveModule (dragFromPosition, dragPreviewPosition); // one gesture for the host (and one undo step)

    draggedRow = nullptr;
    landingRow = &row; // glides from where it was dropped into its slot, still raised
    updateTargets (true);
    repaint();
}

void ChainPanel::moveModule (int fromPosition, int toPosition)
{
    if (fromPosition == toPosition)
        return;

    const auto newOrder = chain::move (order, fromPosition, toPosition);
    orderAttachment.setValueAsCompleteGesture ((float) chain::encode (newOrder));
}

void ChainPanel::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;

    g.fillAll (c.panel);
    g.setColour (c.borderSubtle);
    g.fillRect (getLocalBounds().removeFromRight (1));

    g.setColour (c.textMuted);
    g.setFont (theme.label());
    g.drawText (text ("CADENA DE SE\xc3\x91" "AL"), juce::Rectangle<int> (paddingX + 6, paddingTop, getWidth(), 14),
                juce::Justification::centredLeft, false);

    const auto drawMarker = [&] (const juce::String& name, int y)
    {
        const auto font = theme.mono (11.0f);
        const auto x = paddingX + 6;
        const auto w = (int) std::ceil (textWidth (font, name));
        g.setColour (c.textMuted);
        g.setFont (font);
        g.drawText (name, juce::Rectangle<int> (x, y, w, markerHeight), juce::Justification::centredLeft, false);
        g.setColour (c.border);
        g.fillRect (x + w + 6, y + markerHeight / 2, getWidth() - paddingX - 6 - (x + w + 6), 1);
    };

    drawMarker ("IN", firstRowY - rowGap - markerHeight);
    drawMarker ("OUT", getSlotBounds (numModules - 1).getBottom() + rowGap);

    // The gap the dragged row will drop into, under the rows.
    if (const auto* held = draggedRow != nullptr ? draggedRow : landingRow)
    {
        juce::Path outline;
        outline.addRoundedRectangle (getRowTargetBounds (held->getId()).toFloat().reduced (0.5f), theme.metrics.radiusRow);
        juce::Path dashed;
        const float dashes[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (c.accent.withAlpha (0.55f));
        g.fillPath (dashed);
    }

    // Help text at the bottom, above a separator.
    const auto help = text ("Arrastra desde el asa para cambiar el orden. Clic para abrir el m\xc3\xb3" "dulo.");
    auto helpArea = getLocalBounds().reduced (paddingX, 0).withTrimmedBottom (12);
    helpArea = helpArea.removeFromBottom (58);
    g.setColour (c.borderSubtle);
    g.fillRect (helpArea.removeFromTop (1));
    helpArea.removeFromTop (10);

    juce::AttributedString s;
    s.append (help, theme.ui (11.0f), c.textMuted);
    s.setLineSpacing (4.0f);
    s.setWordWrap (juce::AttributedString::byWord);
    juce::TextLayout layout;
    layout.createLayout (s, (float) helpArea.getWidth() - 12.0f);
    layout.draw (g, helpArea.withTrimmedLeft (6).toFloat());
}

} // namespace pm::ui
