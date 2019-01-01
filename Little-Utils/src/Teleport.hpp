#include "LittleUtils.hpp"
#include <vector>
#include <map>

#define NUM_TELEPORT_INPUTS 1
#define LABEL_LENGTH 4

struct Teleport : Module {
	std::string label;
	Teleport(int numParams, int numInputs, int numOutputs, int numLights = 0):
		Module(numParams, numInputs, numOutputs, numLights) {}

	// this static buffer is used for transferring the signal from one endpoint to another
	// TODO: rename
	// TODO: static std::map<std::string, Input> buffer; <-- access inputs directly in map, good idea? does this prevent mixing? do we want multiple inputs with the same label?
	static std::map<std::string, float> buffer;
};

std::map<std::string, float> Teleport::buffer = {};
