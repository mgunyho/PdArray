#pragma once
#include <rack.hpp>

using namespace rack;

static const int MAX_POLY_CHANNELS = 16;

// Declare the Plugin, defined in plugin.cpp
extern Plugin *pluginInstance;

// Declare each Model, defined in each module source file
extern Model *modelButtonModule;
extern Model *modelPulseGenerator;
extern Model *modelBias_Semitone;
extern Model *modelMulDiv;
extern Model *modelTeleportInModule;
extern Model *modelTeleportOutModule;
