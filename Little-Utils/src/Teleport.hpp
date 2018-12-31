#include "LittleUtils.hpp"
#include <vector>

#define NUM_TELEPORT_INPUTS 1

struct Teleport : Module {
	// this buffer holds the values oof
	//static std::vector<float> buffer;
	Teleport(int numParams, int numInputs, int numOutputs, int numLights = 0):
		Module(numParams, numInputs, numOutputs, numLights) {};
	static float buffer[NUM_TELEPORT_INPUTS];
};

float Teleport::buffer[] = { 0.f };
