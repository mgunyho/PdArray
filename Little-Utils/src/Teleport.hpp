#include "LittleUtils.hpp"
#include <vector>
#include <map>

#define NUM_TELEPORT_INPUTS 8
#define LABEL_LENGTH 4

struct Teleport : Module {
	bool status; //TODO: necessary?
	std::string label;
	Teleport(int numParams, int numInputs, int numOutputs, int numLights = 0):
		Module(numParams, numInputs, numOutputs, numLights) {
			status = false;
		}

	//static unsigned int sync //TODO: allow multiple inputs with same name?

	// this static buffer is used for transferring the signal from one endpoint to another
	// TODO: rename
	// TODO: static std::map<std::string, Input> buffer; <-- access inputs directly in map, good idea? does this prevent mixing? do we want multiple inputs with the same label?
	static std::map<std::string, std::vector<std::reference_wrapper<Input>>> buffer;
	static std::string lastInsertedKey; // this is used to assign the label of an output initially

	void addInputsToBuffer(Teleport *module) {
		//buffer.insert(std::pair<std::string, float>(key, std::vector<float>()));
		std::string key = module->label;
		for(int i = 0; i < NUM_TELEPORT_INPUTS; i++) {
			buffer[key].push_back(module->inputs[i]);
		}
		lastInsertedKey = key;
	}
};

std::map<std::string, std::vector<std::reference_wrapper<Input>>> Teleport::buffer = {};
std::string Teleport::lastInsertedKey = "";
