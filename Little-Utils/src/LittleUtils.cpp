#include "LittleUtils.hpp"


Plugin *plugin;


void init(Plugin *p) {
	plugin = p;
	p->slug = TOSTRING(SLUG);
	p->version = TOSTRING(VERSION);

	// Add all Models defined throughout the plugin
	p->addModel(modelButtonModule);
	p->addModel(modelPulseGenerator);
	p->addModel(modelBias_Semitone);
	p->addModel(modelArithmetic);
	p->addModel(modelTeleportInModule);
	p->addModel(modelTeleportOutModule);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
