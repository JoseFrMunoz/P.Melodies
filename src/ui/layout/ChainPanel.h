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
    OUT. Drag a row by its handle to reorder (a line shows where it will land); click a row to
    open the module; the power button bypasses it.
*/
class ChainPanel final : public juce::Component
{
public:
    ChainPanel (juce::RangedAudioParameter& orderParameter,
                const std::array<juce::RangedAudioParameter*, numModules>& enabledParameters);
    ~ChainPanel() override;

    std::function<void (ModuleId)> onModuleSelected;

    void setSelectedModule (ModuleId id);
    ModuleId getSelectedModule() const noexcept { return selectedModule; }

    const chain::Order& getOrder() const noexcept { return order; }
    ModuleRow& getRow (ModuleId id) const { return *rows[(size_t) toIndex (id)]; }

    /** Bounds of the row slot at a chain position. */
    juce::Rectangle<int> getSlotBounds (int position) const;

    /** Drop slot (0 = before the first module … 4 = after the last) for a y coordinate. */
    int dropSlotForY (float y) const;

    /** Final position of a module dragged from `fromPosition` and dropped in `dropSlot`. */
    static int targetPosition (int fromPosition, int dropSlot) noexcept;

    // Called by the rows.
    void rowClicked (ModuleRow& row);
    void beginDrag (ModuleRow& row, const juce::MouseEvent& e);
    void continueDrag (const juce::MouseEvent& e);
    void endDrag (const juce::MouseEvent& e);
    bool isDragging() const noexcept { return draggedRow != nullptr; }

    /** Applies a drag-and-drop move (used by the mouse handlers and by tests). */
    void moveModule (int fromPosition, int toPosition);

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void layoutRows();

    juce::ParameterAttachment orderAttachment;
    chain::Order order;
    std::array<std::unique_ptr<ModuleRow>, numModules> rows;
    ModuleId selectedModule = ModuleId::eq;

    ModuleRow* draggedRow = nullptr;
    int dragFromPosition = 0;
    float dragGrabOffset = 0.0f;
    int dropSlot = -1;
};

} // namespace pm::ui
