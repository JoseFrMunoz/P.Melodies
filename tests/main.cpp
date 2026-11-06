// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include <catch2/catch_session.hpp>
#include <juce_events/juce_events.h>

int main (int argc, char* argv[])
{
    // Some JUCE classes (timers, the message manager used by parameter attachments and the
    // editor) need JUCE to be initialised, exactly like inside a host.
    juce::ScopedJuceInitialiser_GUI juceInit;
    return Catch::Session().run (argc, argv);
}
