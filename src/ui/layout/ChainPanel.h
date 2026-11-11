// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/ChainOrder.h"
#include "ui/components/Buttons.h"

#include <array>
#include <functional>
#include <memory>

namespace pm::ui
{

class ChainPanel;

/** One module in the chain list: drag handle, colour, name, status and power button. */
class ModuleRow final : public juce::Component
{
public:
    ModuleRow (ChainPanel& owner, ModuleId id, juce::RangedAudioParameter& enabledParameter);

    ModuleId getId() const noexcept { return id; }
    bool isModuleEnabled() const noexcept { return enabled; }

    void setSelected (bool shouldBeSelected);
    void setDragging (bool shouldBeDragging);

    /** Area of the drag handle, in row coordinates. */
    juce::Rectangle<int> getHandleArea() const { return { 0, 0, 22, getHeight() }; }

    void togglePower();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void lookAndFeelChanged() override { refreshPowerColour(); }

    static constexpr int height = 46;

private:
    void refreshPowerColour();

    ChainPanel& owner;
    const ModuleId id;
    juce::ParameterAttachment enabledAttachment;
    IconButton power;
    bool enabled = true;
    bool selected = false;
    bool dragging = false;
    bool dragStartedOnHandle = false;
};

/**
    Left panel "Cadena de señal": the four modules in their real processing order between IN and
    OUT. Click a row to open the module; the power button bypasses it.

    Reordering is live: while a row is dragged by its handle, the other rows slide out of the way
    to the places they will take, so the gap always shows where it will land. On release the row
    glides into that gap and the new order is sent to the plugin (onOrderChanged). Rows also glide
    when the order changes from outside (setOrder(): projects, and presets and undo in phase 5).
*/
class ChainPanel final : public juce::Component
{
public:
    explicit ChainPanel (const std::array<juce::RangedAudioParameter*, numModules>& enabledParameters);
    ~ChainPanel() override;

    std::function<void (ModuleId)> onModuleSelected;

    /** The user dropped a module somewhere new. */
    std::function<void (const chain::Order&)> onOrderChanged;

    /** Order from the plugin. The rows glide to it (they jump before the first layout). */
    void setOrder (const chain::Order& newOrder);

    void setSelectedModule (ModuleId id);
    ModuleId getSelectedModule() const noexcept { return selectedModule; }

    const chain::Order& getOrder() const noexcept { return order; }
    ModuleRow& getRow (ModuleId id) const { return *rows[(size_t) toIndex (id)]; }

    /** Bounds of the row slot at a chain position. */
    juce::Rectangle<int> getSlotBounds (int position) const;

    /** Chain position whose slot is nearest to a row whose top edge is at y. */
    int positionForRowTop (float y) const;

    /** Order on screen: while dragging, the dragged module already sits where it would land. */
    chain::Order getDisplayedOrder() const;

    /** Slot a row is heading to; getBounds() is where it is now, mid-animation. */
    juce::Rectangle<int> getRowTargetBounds (ModuleId id) const;

    // Called by the rows. y is in panel coordinates: the pointer while dragging.
    void rowClicked (ModuleRow& row);
    void beginDrag (ModuleRow& row, float y);
    void continueDrag (float y);
    void endDrag();
    bool isDragging() const noexcept { return draggedRow != nullptr; }
    ModuleRow* getDraggedRow() const noexcept { return draggedRow; }

    /** Applies a move and reports it through onOrderChanged (used on release and by tests). */
    void moveModule (int fromPosition, int toPosition);

    /**
        Moves every row a step closer to its slot (exponential approach, time constant
        glideSeconds). Called on every screen refresh; public so tests can run it frame by frame.
    */
    void advanceAnimation (double seconds);
    bool isAnimating() const noexcept;

    static constexpr double glideSeconds = 0.045;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** Sets each row's target slot; jumps there when animate is false (first layout, resize). */
    void updateTargets (bool animate);
    void placeRow (ModuleRow& row);
    void frame (double timestampSeconds);

    chain::Order order = chain::defaultOrder();
    std::array<std::unique_ptr<ModuleRow>, numModules> rows;
    ModuleId selectedModule = ModuleId::eq;

    // Animation state per module index: where the row's top edge is and where it is heading.
    std::array<float, numModules> rowTop {};
    std::array<float, numModules> rowTargetTop {};
    bool rowsPlaced = false;

    ModuleRow* draggedRow = nullptr;
    ModuleRow* landingRow = nullptr; // released, still gliding into its slot (keeps the raised look)
    int dragFromPosition = 0;
    int dragPreviewPosition = 0;
    float dragGrabOffset = 0.0f;

    juce::VBlankAttachment vblank;
    double lastFrameTime = 0.0;
};

} // namespace pm::ui
