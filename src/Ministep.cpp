#include "plugin.hpp"

//TODO: right-click menu to toggle output mode: 10V / nSteps <-> 1V / step
//TODO: right-click menu to offset phase by 0.5 step

constexpr int DEFAULT_NSTEPS = 10;

struct Ministep : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InpuIds {
		RESET_INPUT,
		//SCALE_INPUT, //TODO: is there enough space?
		INCREMENT_INPUT,
		DECREMENT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		STEP_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	dsp::SchmittTrigger rstTrigger[MAX_POLY_CHANNELS];
	dsp::SchmittTrigger incTrigger[MAX_POLY_CHANNELS];
	dsp::SchmittTrigger decTrigger[MAX_POLY_CHANNELS];
	int nSteps = DEFAULT_NSTEPS;
	int currentStep[MAX_POLY_CHANNELS];

	Ministep() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		nSteps = DEFAULT_NSTEPS;
		for(int i = 0; i < MAX_POLY_CHANNELS; i++) currentStep[i] = 0;
	}

	void onReset() override {
		nSteps = DEFAULT_NSTEPS;
		for(int i = 0; i < MAX_POLY_CHANNELS; i++) currentStep[i] = 0;
	}

	json_t *dataToJson() override {
		json_t *root = json_object();

		json_object_set_new(root, "nSteps", json_integer(nSteps));
		//json_object_set_new(root, "currentStep", json_integer(currentStep));

		json_t *currentStep_J = json_array();
		for(int i = 0; i < MAX_POLY_CHANNELS; i++) {
			json_array_append_new(currentStep_J, json_integer(currentStep[i]));
		}

		json_object_set(root, "currentStep", currentStep_J);
		json_decref(currentStep_J);

		return root;
	}

	void dataFromJson(json_t *root) override {
		json_t *nSteps_J = json_object_get(root, "currentStep");
		json_t *currentStep_J = json_object_get(root, "currentStep");

		if(nSteps_J) {
			nSteps = json_integer_value(nSteps_J);
			if(nSteps < 1) {
				nSteps = DEFAULT_NSTEPS;
			}
		}

		//if(currentStep_J)
		//	currentStep = json_integer_value(currentStep_J);
		if(currentStep_J) {
			for(int i = 0; i < MAX_POLY_CHANNELS; i++) {
				json_t *step = json_array_get(currentStep_J, i);
				if(step) {
					currentStep[i] = json_integer_value(step);
				}
			}
		}
	}

	void process(const ProcessArgs &args) override;

};

void Ministep::process(const ProcessArgs &args) {
	const int channels = std::max(
			std::max(
				inputs[INCREMENT_INPUT].getChannels(),
				inputs[DECREMENT_INPUT].getChannels()
			),
			std::max(inputs[RESET_INPUT].getChannels(), 1)
			);

	for(int c = 0; c < channels; c++) {
		bool rstTriggered = rstTrigger[c].process(
				rescale(inputs[RESET_INPUT].getPolyVoltage(c), 0.1f, 2.f, 0.f, 1.f)
			);
		bool incTriggered = incTrigger[c].process(
				rescale(inputs[INCREMENT_INPUT].getPolyVoltage(c), 0.1f, 2.f, 0.f, 1.f)
			);
		bool decTriggered = decTrigger[c].process(
				rescale(inputs[DECREMENT_INPUT].getPolyVoltage(c), 0.1f, 2.f, 0.f, 1.f)
			);

		int step = currentStep[c];
		if(rstTriggered) {
			step = 0;
		} else {
			if(incTriggered && !decTriggered) {
				step += 1;
			} else if (decTriggered && !incTriggered) {
				step -= 1;
			} // if both are triggered, do nothing

			step = (step + nSteps) % nSteps;
		}
		currentStep[c] = step;

		float phase = currentStep[c] * 1.f / nSteps;
		outputs[STEP_OUTPUT].setVoltage(phase * 10.f, c);
	}
	outputs[STEP_OUTPUT].setChannels(channels);
}

struct MinistepWidget : ModuleWidget {
	Ministep *module;
	//MsDisplayWidget *msDisplay;

	MinistepWidget(Ministep *module) {
		setModule(module);
		this->module = module;
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Ministep.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		//addParam(createParam<CKSS>(Vec(7.5, 60), module, Miniramp::LIN_LOG_MODE_PARAM));

		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 48),  module, Ministep::INCREMENT_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 96),  module, Ministep::DECREMENT_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 144), module, Ministep::RESET_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 227), module, Ministep::STEP_OUTPUT));

		//addChild(createTinyLightForPort<GreenLight>(Vec(22.5, 240), module, Miniramp::RAMP_LIGHT));
		//addChild(createTinyLightForPort<GreenLight>(Vec(22.5, 288), module, Miniramp::GATE_LIGHT));

		//TODO: displays
		//msDisplay = new MsDisplayWidget(module);
		//msDisplay->box.pos = Vec(7.5, 308);
		//addChild(msDisplay);

		//auto cvKnob = createParamCentered<CustomTrimpot>(Vec(22.5, 110), module,
		//		Miniramp::CV_AMT_PARAM);
		//cvKnob->display = msDisplay;
		//addParam(cvKnob);

	}
};

Model *modelMinistep = createModel<Ministep, MinistepWidget>("Ministep");
