#pragma once
#include <rack.hpp>

//TODO: polyphony support
//TODO: see phase 3 of porting
//TODO: undo stuff... (some are added automatically but not everything)
//TODO: tooltips

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin *pluginInstance;

// Declare each Model, defined in each module source file
extern Model *modelButtonModule;
extern Model *modelPulseGenerator;
extern Model *modelBias_Semitone;
extern Model *modelMulDiv;
extern Model *modelTeleportInModule;
extern Model *modelTeleportOutModule;
