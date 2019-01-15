#include "PDArray.hpp"

#include <iostream>

struct PDArrayModule : Module {
	enum ParamIds {
		PITCH_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PHASE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		DIRECT_OUTPUT, //TODO: rename
		INTERP_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT,
		NUM_LIGHTS
	};

	float phase = 0.0;
	unsigned int size = 10;
	//float miny, maxy; // output value scale TODO
	std::vector<float> buffer;

	PDArrayModule() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		resizeBuffer(size);

		//TODO: remove
		buffer.clear();
		buffer.push_back(10.f);
		buffer.push_back(10.f);
		buffer.push_back(10.f);
		buffer.push_back(10.f);
		buffer.push_back(10.f);
		buffer.push_back(-10.f);
		buffer.push_back(-10.f);
		buffer.push_back(-10.f);
		buffer.push_back(-10.f);
		buffer.push_back(-10.f);
	}

	void step() override;
	void resizeBuffer(unsigned int newSize) {
		buffer.resize(newSize, 0.f);
	}

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};

void PDArrayModule::step() {
	// Implement a simple sine oscillator
	float deltaTime = engineGetSampleTime();
	phase = rescale(clamp(inputs[PHASE_INPUT].value, 0.f, 10.f), 0.f, 10.f, 0.f, 1.f);

	int i = int(phase * size); //TODO: clamp?

	outputs[DIRECT_OUTPUT].value = buffer[i];


	// interpolation based on tabread4_tilde_perform() in
	// https://github.com/pure-data/pure-data/blob/master/src/d_array.c
	float a, b, c, d;
	a = buffer[clamp(i - 1, 0, size)];
	b = buffer[clamp(i - 0, 0, size)];
	c = buffer[clamp(i + 1, 0, size)];
	d = buffer[clamp(i + 2, 0, size)];
	float frac = phase * size - i; // fractional part of phase
	outputs[INTERP_OUTPUT].value = b + frac * (
			c - b - 0.1666667f * (1.f - frac) * (
				(d - a - 3.f * (c - b)) * frac + (d + 2.f * a - 3.f * b)
			)
		);

	//std::cout
	//	<< "phase: " << phase
	//	<< ", i: " << i
	//	<< ", frac: " << frac
	//	<< std::endl;
}

struct PDArrayModuleWidget : ModuleWidget {
	PDArrayModuleWidget(PDArrayModule *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/Array.svg"))); //TODO

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		//addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		//addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(Port::create<PJ301MPort>(Vec(10.f, 50), Port::INPUT, module, PDArrayModule::PHASE_INPUT));
		addOutput(Port::create<PJ301MPort>(Vec(10.f, 100), Port::OUTPUT, module, PDArrayModule::DIRECT_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(10.f, 150), Port::OUTPUT, module, PDArrayModule::INTERP_OUTPUT));
	}
};

Model *modelPDArray = Model::create<PDArrayModule, PDArrayModuleWidget>("PDArray", "PDArray", "Array", VISUAL_TAG, ENVELOPE_GENERATOR_TAG);
