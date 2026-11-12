// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "modules/comp/CompParameters.h"
#include "modules/eq/EqParameters.h"

namespace pm
{

/**
    Per-module settings, taken once per audio block together with the global ones. Each module
    reads its own part in ModuleBase::setParameters(). The saturation and distortion add theirs in
    their phases.
*/
struct ModuleParameters
{
    eq::Parameters eq;
    comp::Parameters comp;
};

} // namespace pm
