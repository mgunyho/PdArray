#include "plugin.hpp"


Plugin *pluginInstance;


void init(Plugin *p) {
	pluginInstance = p;

	p->addModel(modelButtonModule);
	p->addModel(modelPulseGenerator);
	p->addModel(modelBias_Semitone);
	p->addModel(modelMulDiv);
	p->addModel(modelTeleportInModule);
	p->addModel(modelTeleportOutModule);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
