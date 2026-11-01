#pragma once

#include "Rig.h"

#include <vector>

// Starter sounds that ship with the plugin (bank A).
std::vector<Rig> makeFactoryRigs();

// The built-in expansions' presets (ExpansionRigs.cpp), each in its expansion's own bank (Expansions.h).
std::vector<Rig> makeExpansionRigs();
