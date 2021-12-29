#pragma once
#include "rack.hpp"

static const int MAX_POLY_CHANNELS = 16;

using namespace rack;

// Forward-declare the Plugin, defined in Array.cpp
extern Plugin *pluginInstance;

// Forward-declare each Model, defined in each module source file
extern Model *modelArray;
extern Model *modelMiniramp;
extern Model *modelMinistep;
