#include "LittleUtils.hpp"
#include "dsp/digital.hpp"

#include <iostream> //TODO

const float MIN_EXPONENT = -3.0f;
const float MAX_EXPONENT = 1.0f;

// the module is called PulseGenModule because PulseGenerator is already taken
// in dsp/digital.hpp
struct PulseGenModule : Module {
	enum ParamIds {
		GATE_LENGTH_PARAM,
		LIN_LOG_MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		TRIG_INPUT,
		GATE_LENGTH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		GATE_LIGHT,
		NUM_LIGHTS
	};

	SchmittTrigger inputTrigger;
	PulseGenerator gateGenerator;
	float gate_duration = 0.5f;

	PulseGenModule() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}

	void step() override;


	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
	json_t* toJson() override {
		// TODO
		return NULL;
	}

	void fromJson(json_t* root) override {
		// TODO
	}

};


void PulseGenModule::step() {
	float deltaTime = engineGetSampleTime();

	bool triggered = inputTrigger.process(rescale(inputs[TRIG_INPUT].value,
												  0.1f, 2.f, 0.f, 1.f));

	//float gate_voltage = clamp(params[GATE_LENGTH_PARAM].value + inputs[ ... ]);
	if(params[LIN_LOG_MODE_PARAM].value < 0.5f) {
		gate_duration = params[GATE_LENGTH_PARAM].value;
	} else {
		float exponent = rescale(params[GATE_LENGTH_PARAM].value, // gate_voltage //TODO
							 		0.f, 10.f, MIN_EXPONENT, MAX_EXPONENT);
		gate_duration = powf(10.0f, exponent);
	}
	//TODO
	//exponent = clamp(params[GATE_LENGTH_PARAM].value +
	//					rescale(inputs[GATE_LENGTH_INPUT].value,
	//							0.0f, 10.0f, MIN_EXPONENT, MAX_EXPONENT),
	//					MIN_EXPONENT, MAX_EXPONENT);
	//gate_duration = powf(10.0f, exponent);

	if(triggered) {
		//TODO: making trigger time shorter once this has been triggered has no effect
		//gateGenerator.setDuration(...) ???
		gateGenerator.trigger(gate_duration);
	}

	outputs[GATE_OUTPUT].value = gateGenerator.process(deltaTime) ? 10.0f : 0.0f;

	//lights[TRIG_LIGHT].setBrightnessSmooth(trigger); //TODO: lights?

}

struct MsDisplayWidget : TextField {
	PulseGenModule* module;
	//std::shared_ptr<Font> font; //TODO
	MsDisplayWidget(PulseGenModule* m) {
		module = m;
		//font = Font::load(assetPlugin(plugin, "res/???.ttf")); //TODO
	}

	//TODO: implement typing into box (?)
	// suppress all mouse events TODO: is this correct?
	void onMouseDown(EventMouseDown &e) override {
		//if(module->inputs[PulseGenModule::GATE_LENGTH_INPUT].active) {
		//} else {
		//	TextField::onMouseDown(e);
		//}
		//OpaqueWidget::onMouseDown(e);
	};
	void onMouseUp(EventMouseUp &e) override {
		//e.consumed = true;
		//OpaqueWidget::onMouseUp(e);
	}
	void onMouseMove(EventMouseMove &e) override {};
	void onMouseEnter(EventMouseEnter &e) override {};
	void onMouseLeave(EventMouseLeave &e) override {};
};

struct PulseGeneratorWidget : ModuleWidget {
	PulseGenModule *module;
	TextField* msDisplay;
	//MsDisplayWidget* msDisplay;

	PulseGeneratorWidget(PulseGenModule *module) : ModuleWidget(module) {
		this->module = module;
		setPanel(SVG::load(assetPlugin(plugin, "res/PulseGenerator.svg")));

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 87), module, PulseGenModule::TRIG_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 147), module, PulseGenModule::GATE_LENGTH_INPUT));

		addParam(createParamCentered<RoundBlackKnob>(
					Vec(22.5, RACK_GRID_WIDTH + 25), module,
					PulseGenModule::GATE_LENGTH_PARAM, 0.f, 10.f,
					// 0.5s in log scale
					rescale(-0.30103f, MIN_EXPONENT, MAX_EXPONENT, 0.f,10.f)
					));
		addParam(createParamCentered<CKSS>(Vec(22.5, RACK_GRID_WIDTH + 55), module, PulseGenModule::LIN_LOG_MODE_PARAM, 0.f, 1.f, 1.f));

		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 192), module, PulseGenModule::GATE_OUTPUT));

		//TODO: consider
		//addChild(createLightCentered<TinyLight<GreenLight>>(Vec(37.5 - offset, 177 + offset), module, PulseGenModule::GATE_LIGHT));
		msDisplay = new MsDisplayWidget(module);
		msDisplay->box.pos = Vec(7.5, 230);
		msDisplay->box.size = Vec(30, 40);
		addChild(msDisplay);

	}

	void step() override {
		ModuleWidget::step();

		//if(inputs[GATE_LENGTH_INPUT].active) {
		//}

		auto input = module->inputs[PulseGenModule::GATE_LENGTH_INPUT];
		if(input.active || true) { //TODO: remove
			//TODO: check formatting (currently just copied from https://github.com/alikins/Alikins-rack-plugins/blob/master/src/HoveredValue.cpp)
			float v = module->gate_duration;
			std::string s;
			if(v < 0.1f) {
				v *= 1e3f;
				s = stringf("%#.2g ms", v < 1.f ? 0.f : v);
			} else {
				s = stringf("%#.2g s", v);
			}
			msDisplay->setText(s);
		}
	}
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelPulseGenerator = Model::create<PulseGenModule, PulseGeneratorWidget>("Little Utils", "PulseGenerator", "Pulse Generator", UTILITY_TAG, ENVELOPE_GENERATOR_TAG);
