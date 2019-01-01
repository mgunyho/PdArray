#include "rack.hpp"


using namespace rack;

//TODO: createParamCentered() etc are deprecated (I think)...

// Forward-declare the Plugin, defined in LittleUtils.cpp
extern Plugin *plugin;

// Forward-declare each Model, defined in each module source file
extern Model *modelButtonModule;
extern Model *modelPulseGenerator;
extern Model *modelBias_Semitone;
extern Model *modelArithmetic;
extern Model *modelTeleportInModule;
extern Model *modelTeleportOutModule;
