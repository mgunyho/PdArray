#include "plugin.hpp"

#include "Widgets.hpp"

//TODO: right-click menu to toggle output mode: 10V / nSteps <-> 1V / step
//TODO: scale input: allow incrementing multiple steps with a single trigger

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
	bool offsetByHalfStep = false;

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
		json_object_set_new(root, "offsetByHalfStep", json_boolean(offsetByHalfStep));

		json_t *currentStep_J = json_array();
		for(int i = 0; i < MAX_POLY_CHANNELS; i++) {
			json_array_append_new(currentStep_J, json_integer(currentStep[i]));
		}

		json_object_set(root, "currentStep", currentStep_J);
		json_decref(currentStep_J);

		return root;
	}

	void dataFromJson(json_t *root) override {
		json_t *nSteps_J = json_object_get(root, "nSteps");
		json_t *offsetByHalfStep_J = json_object_get(root, "offsetByHalfStep");
		json_t *currentStep_J = json_object_get(root, "currentStep");

		if(nSteps_J) {
			nSteps = json_integer_value(nSteps_J);
			if(nSteps < 1) {
				nSteps = DEFAULT_NSTEPS;
			}
		}

		if(offsetByHalfStep_J) {
			offsetByHalfStep = json_boolean_value(offsetByHalfStep_J);
		}

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

struct CurrentStepDisplayWidget : TextBox {
	Ministep *module;
	int previousDisplayValue = 1;

	CurrentStepDisplayWidget(Ministep *m) : TextBox() {
		font = APP->window->uiFont; // default blendish font (same as TextField)
		module = m;
		box.size = Vec(30, 21);
		font_size = 14;
		textAlign = NVG_ALIGN_RIGHT | NVG_ALIGN_TOP;
		textOffset = Vec(box.size.x - 4, 3.5);

		//backgroundColor = nvgRGB(0xef, 0xe8, 0xd5); // solarized base2
		backgroundColor = nvgRGB(0x78, 0x78, 0x78);

		if(module) {
			previousDisplayValue = module->currentStep[0];
		}

		setText(string::f("%u", previousDisplayValue));
	}

	//TODO: override draw(), draw progress bars for all counters if polyphonic

	void step() override {
		TextBox::step();
		if(module) {
			int v = module->currentStep[0] + 1;
			if(v != previousDisplayValue) {
				setText(string::f("%u", v));
			}
			previousDisplayValue = v;
		}
	}
};

//TODO: replace with something like EditableTextBox in LittleUtils?
struct NStepsSelector : NumberTextField {
	Ministep *module;

	NStepsSelector(Ministep *m) : NumberTextField() {
		module = m;
		validText = string::f("%u", module ? module->nSteps : DEFAULT_NSTEPS);
		text = validText;
		maxCharacters = 3;
	};

	void onNumberSet(const int n) override {
		if(module) {
			module->nSteps = n;
		}
	}

	void step() override {
		NumberTextField::step();
		// is this CPU intensive to do on every step?
		if(module) {
			if(APP->event->selectedWidget != this) {
				validText = string::f("%u", module->nSteps);
				text = validText;
			}
		}
	}
};

struct OffsetByHalfStepMenuItem : MenuItem {
	Ministep *module;
	bool valueToSet;
	void onAction(const event::Action &e) override {
		module->offsetByHalfStep = valueToSet;
	}
};

struct MinistepWidget : ModuleWidget {
	Ministep *module;
	CurrentStepDisplayWidget *currentStepDisplay;
	NStepsSelector *nStepsSelector;

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
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 249), module, Ministep::STEP_OUTPUT));

		//addChild(createTinyLightForPort<GreenLight>(Vec(22.5, 240), module, Miniramp::RAMP_LIGHT));
		//addChild(createTinyLightForPort<GreenLight>(Vec(22.5, 288), module, Miniramp::GATE_LIGHT));

		currentStepDisplay = new CurrentStepDisplayWidget(module);
		currentStepDisplay->box.pos = Vec(7.5, 280);
		addChild(currentStepDisplay);

		nStepsSelector = new NStepsSelector(module);
		nStepsSelector->box.pos = Vec(7.5, 317);
		nStepsSelector->box.size.x = 30;
		addChild(nStepsSelector);

	}

	void appendContextMenu(ui::Menu *menu) override {
		if(module) {
			menu->addChild(new MenuLabel());

			auto *offsetMenuItem = new OffsetByHalfStepMenuItem();
			offsetMenuItem->text = "Offset output by half step";
			offsetMenuItem->module = module;
			offsetMenuItem->rightText = CHECKMARK(module->offsetByHalfStep);
			offsetMenuItem->valueToSet = !module->offsetByHalfStep;
			menu->addChild(offsetMenuItem);
		}
	}
};

Model *modelMinistep = createModel<Ministep, MinistepWidget>("Ministep");
