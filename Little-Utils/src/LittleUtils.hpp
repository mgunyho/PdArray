#include "rack0.hpp"


using namespace rack;

// Forward-declare the Plugin, defined in LittleUtils.cpp
extern Plugin *pluginInstance;

// Forward-declare each Model, defined in each module source file
extern Model *modelButtonModule;
extern Model *modelPulseGenerator;
extern Model *modelBias_Semitone;
extern Model *modelMulDiv;
extern Model *modelTeleportInModule;
extern Model *modelTeleportOutModule;
