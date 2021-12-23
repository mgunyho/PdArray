#include "plugin.hpp"

#include "Widgets.hpp"
#include "Util.hpp"

#include <algorithm> // std::replace

constexpr int DEFAULT_NSTEPS = 10;

struct Ministep : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InpuIds {
		RESET_INPUT,
		INCREMENT_INPUT,
		DECREMENT_INPUT,
		SCALE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		STEP_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	enum StepScaleMode {
		SCALE_ABSOLUTE,
		SCALE_RELATIVE
	};

	enum OutputScaleMode {
		SCALE_10V_PER_NSTEPS,
		SCALE_1V_PER_STEP
	};

	dsp::SchmittTrigger rstTrigger[MAX_POLY_CHANNELS];
	dsp::SchmittTrigger incTrigger[MAX_POLY_CHANNELS];
	dsp::SchmittTrigger decTrigger[MAX_POLY_CHANNELS];
	int nSteps = DEFAULT_NSTEPS;
	int currentStep[MAX_POLY_CHANNELS];
	int currentScale[MAX_POLY_CHANNELS];
	int nChannels = 1;
	bool offsetByHalfStep = false;
	StepScaleMode stepScaleMode = SCALE_RELATIVE;
	OutputScaleMode outputScaleMode = SCALE_10V_PER_NSTEPS;

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
		json_object_set_new(root, "stepScaleMode", json_integer(stepScaleMode));
		json_object_set_new(root, "outputScaleMode", json_integer(outputScaleMode));

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
		json_t *stepScaleMode_J = json_object_get(root, "stepScaleMode");
		json_t *outputScaleMode_J = json_object_get(root, "outputScaleMode");

		if(nSteps_J) {
			nSteps = json_integer_value(nSteps_J);
			if(nSteps < 1) {
				nSteps = DEFAULT_NSTEPS;
			}
		}

		if(offsetByHalfStep_J) {
			offsetByHalfStep = json_boolean_value(offsetByHalfStep_J);
		}

		if(stepScaleMode_J) {
			int sm = json_integer_value(stepScaleMode_J);
			stepScaleMode = static_cast<StepScaleMode>(sm);
		}

		if(outputScaleMode_J) {
			int sm = json_integer_value(outputScaleMode_J);
			outputScaleMode = static_cast<OutputScaleMode>(sm);
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
	nChannels = std::max(
			std::max(
				inputs[INCREMENT_INPUT].getChannels(),
				inputs[DECREMENT_INPUT].getChannels()
			),
			std::max(inputs[RESET_INPUT].getChannels(), 1)
			);


	for(int c = 0; c < nChannels; c++) {
		bool rstTriggered = rstTrigger[c].process(
				rescale(inputs[RESET_INPUT].getPolyVoltage(c), 0.1f, 2.f, 0.f, 1.f)
			);
		bool incTriggered = incTrigger[c].process(
				rescale(inputs[INCREMENT_INPUT].getPolyVoltage(c), 0.1f, 2.f, 0.f, 1.f)
			);
		bool decTriggered = decTrigger[c].process(
				rescale(inputs[DECREMENT_INPUT].getPolyVoltage(c), 0.1f, 2.f, 0.f, 1.f)
			);

		if(inputs[SCALE_INPUT].isConnected()) {
			float s = inputs[SCALE_INPUT].getPolyVoltage(c);
			if(stepScaleMode == SCALE_RELATIVE) {
				s *= nSteps / 10.0f;
			}
			currentScale[c] = int(s); // round towards zero
		} else {
			currentScale[c] = 1;
		}

		int step = currentStep[c];
		if(rstTriggered) {
			step = 0;
		} else {
			if(incTriggered && !decTriggered) {
				step += currentScale[c];
			} else if (decTriggered && !incTriggered) {
				step -= currentScale[c];
			} // if both are triggered, do nothing

			step = (step + nSteps) % nSteps;
		}
		currentStep[c] = step;

		float phase = (currentStep[c] + (offsetByHalfStep ? 0.5f : 0.0f));
		float v = outputScaleMode == SCALE_10V_PER_NSTEPS ? phase * 10.f / nSteps : phase;
		outputs[STEP_OUTPUT].setVoltage(v, c);
	}
	outputs[STEP_OUTPUT].setChannels(nChannels);
}

struct PolyIntDisplayWidget : TextBox {
	Ministep *module;
	int *polyValueToDisplay;
	int previousDisplayValue = 1;

	PolyIntDisplayWidget(Ministep *m, int *valueToDisplay) : TextBox() {
		module = m;
		polyValueToDisplay = valueToDisplay;
		box.size = Vec(30, 21);
		textAlign = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE;
		textOffset = Vec(box.size.x / 2 + 0.5f, box.size.y / 2);
		letter_spacing = -1.5f; // tighten text to fit in three characters at this width

		if(module) {
			previousDisplayValue = polyValueToDisplay[0];
		}

		setText(string::f("%i", previousDisplayValue));
	}

	// determine the value to display in monophonic mode
	virtual int getSingleValue() { return polyValueToDisplay[0]; }
	// this function set s the vertical position of the bar for each value in polyValueToDisplay
	virtual void getBarVPos(int i, float *h, float *y) { *h = 0; *y = box.size.y; }

	void draw(const DrawArgs &args) override {
		if(!module || module->nChannels == 1) {
			if(previousDisplayValue < -99) {
				font_size = 16;
			} else {
				font_size = 20;
			}
			TextBox::draw(args);
		} else {
			setText("");
			TextBox::draw(args);

			const auto vg = args.vg;
			int n = module->nChannels;
			float xrange = box.size.x - 4;
			float w = xrange / n;
			for(int i = 0; i < n; i++) {
				float h, y;
				getBarVPos(i, &h, &y);
				float x = i * 1.f / n * xrange + 2;
				nvgFillColor(vg, textColor);
				nvgBeginPath(vg);
				nvgRect(vg, x, y, w, h);
				nvgFill(vg);
			}
		}
	}

	void step() override {
		TextBox::step();
		if(module) {
			int v = getSingleValue();
			if(v != previousDisplayValue) {
				std::string s = string::f("%i", v);
				std::replace(s.begin(), s.end(), '0', 'O');
				setText(s);
			}
			previousDisplayValue = v;
		}
	}
};

struct ScaleDisplayWidget : PolyIntDisplayWidget {
	int maxValue = 1;
	ScaleDisplayWidget(Ministep *m, int *valueToDisplay)
		: PolyIntDisplayWidget(m, valueToDisplay) {};

	void getBarVPos(int i, float *h, float *y) override {
		if(i == 0) {
			// hacky way to avoid repeating this loop for all i
			maxValue = 1;
			for(int j = 0; j < module->nChannels; j++) {
				maxValue = std::max(std::abs(polyValueToDisplay[j]), maxValue);
			}
		}
		//return polyValueToDisplay[i] * 0.5f / maxVal + 0.5;
		int val = polyValueToDisplay[i];
		if(val == 0) {
			*h = 1.0f;
			*y = 0.5f * box.size.y - 0.5f;
		} else {
			*h = val * 0.5f / maxValue * box.size.y;
			*y = 0.5f * box.size.y - *h + 0.5f * signum(1.0f * val);
		}
	}
};

struct CurrentStepDisplayWidget : PolyIntDisplayWidget {
	CurrentStepDisplayWidget(Ministep *m, int *valueToDisplay)
		: PolyIntDisplayWidget(m, valueToDisplay) {};

	// display step 0 as '1'
	int getSingleValue() override { return polyValueToDisplay[0] + 1; }

	void getBarVPos(int i, float *h, float *y) override {
		*h = (polyValueToDisplay[i] + 1) * 1.f / module->nSteps * box.size.y;
		*y = box.size.y - *h;
	}
};




//TODO: replace with something like EditableTextBox in LittleUtils?
struct NStepsSelector : NumberTextBox {
	Ministep *module;

	NStepsSelector(Ministep *m) : NumberTextBox() {
		module = m;
		TextBox::text = string::f("%u", module ? module->nSteps : DEFAULT_NSTEPS);
		TextField::text = TextBox::text;
		maxTextLength = 3;
		TextBox::box.size = Vec(30, 21);
		textOffset = Vec(TextBox::box.size.x / 2 + 0.5f, TextBox::box.size.y / 2);
		letter_spacing = -1.5f; // tighten text to fit in three characters at this width
	};

	void onNumberSet(const int n) override {
		if(module) {
			module->nSteps = n;
		}
	}

};

template <typename T>
struct ScaleModeChildMenuItem : MenuItem {
	Ministep *module;
	//T is either Ministep::OutputScaleMode or Ministep::StepScaleMode
	T mode;
	// modeParam is either Ministep->outputScaleMode or Ministep->stepScaleMode
	T *modeParam;
	ScaleModeChildMenuItem(Ministep *m, T pMode, T *pModeParam,
	                       std::string label) : MenuItem() {
		module = m;
		mode = pMode;
		modeParam = pModeParam;
		text = label;
		rightText = CHECKMARK(*modeParam == mode);
	}

	void onAction(const event::Action &e) override {
		//module->outputScaleMode = mode;
		*modeParam = mode;
	}
};

struct StepScaleModeChildMenuItem : ScaleModeChildMenuItem<Ministep::StepScaleMode> {
	StepScaleModeChildMenuItem(Ministep *m, Ministep::StepScaleMode pMode,
	                           std::string label)
		:  ScaleModeChildMenuItem(m, pMode, &m->stepScaleMode, label) {};
};

struct StepScaleModeMenuItem : MenuItem {
	Ministep *module;
	Menu *createChildMenu() override {
		Menu *menu = new Menu();
		menu->addChild(new StepScaleModeChildMenuItem(module,
		                                              Ministep::SCALE_RELATIVE,
		                                              "10V = max"));
		menu->addChild(new StepScaleModeChildMenuItem(module,
		                                              Ministep::SCALE_ABSOLUTE,
		                                              "1V per step"));
		return menu;
	}
};

struct OutputScaleModeChildMenuItem : ScaleModeChildMenuItem<Ministep::OutputScaleMode> {
	OutputScaleModeChildMenuItem(Ministep *m, Ministep::OutputScaleMode pMode,
	                             std::string label)
		:  ScaleModeChildMenuItem(m, pMode, &m->outputScaleMode, label) {};
};

struct OutputScaleModeMenuItem : MenuItem {
	Ministep *module;
	Menu *createChildMenu() override {
		Menu *menu = new Menu();

		menu->addChild(new OutputScaleModeChildMenuItem(module,
		                                                Ministep::SCALE_10V_PER_NSTEPS,
		                                                "max = 10V"));
		menu->addChild(new OutputScaleModeChildMenuItem(module,
		                                                Ministep::SCALE_1V_PER_STEP,
		                                                "1V per step"));
		return menu;
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
	ScaleDisplayWidget *scaleDisplay;
	CurrentStepDisplayWidget *currentStepDisplay;
	NStepsSelector *nStepsSelector;

	MinistepWidget(Ministep *module) {
		setModule(module);
		this->module = module;
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Ministep.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 1*47), module, Ministep::INCREMENT_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 2*47), module, Ministep::DECREMENT_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 3*47), module, Ministep::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 4*47), module, Ministep::SCALE_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 261), module, Ministep::STEP_OUTPUT));

		scaleDisplay = new ScaleDisplayWidget(module, module->currentScale);
		scaleDisplay->box.pos = Vec(7.5, 208);
		addChild(scaleDisplay);

		currentStepDisplay = new CurrentStepDisplayWidget(module, module->currentStep);
		currentStepDisplay->box.pos = Vec(7.5, 280);
		addChild(currentStepDisplay);

		nStepsSelector = new NStepsSelector(module);
		nStepsSelector->TextBox::box.pos = Vec(7.5, 317);
		addChild(static_cast<TextBox*>(nStepsSelector));

	}

	void appendContextMenu(ui::Menu *menu) override {
		if(module) {
			menu->addChild(new MenuLabel());

			auto *stepSMMenuItem = new StepScaleModeMenuItem();
			stepSMMenuItem->text = "Scale mode";
			stepSMMenuItem->rightText = RIGHT_ARROW;
			stepSMMenuItem->module = module;
			menu->addChild(stepSMMenuItem);

			auto *outSMMenuItem = new OutputScaleModeMenuItem();
			outSMMenuItem->text = "Output mode";
			outSMMenuItem->rightText = RIGHT_ARROW;
			outSMMenuItem->module = module;
			menu->addChild(outSMMenuItem);

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
