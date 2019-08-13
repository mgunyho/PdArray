#include "plugin.hpp"
#include "Util.hpp"

struct ButtonModule : Module {
	enum ParamIds {
		BUTTON_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		TRIG_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		TRIG_OUTPUT,
		GATE_OUTPUT,
		TOGGLE_OUTPUT,
		CONST_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		TRIG_LIGHT,
		GATE_LIGHT,
		TOGGLE_LIGHT,
		CONST_1_LIGHTP,
		CONST_1_LIGHTM,
		CONST_5_LIGHTP,
		CONST_5_LIGHTM,
		CONST_10_LIGHTP,
		CONST_10_LIGHTM,
		NUM_LIGHTS
	};

	bool toggle = false;
	int const_choice = 0;
	dsp::SchmittTrigger inputTrigger;
	dsp::PulseGenerator triggerGenerator;

	ButtonModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ButtonModule::BUTTON_PARAM, 0.0f, 1.0f, 0.0f, "");
		onReset();
	}

	void onReset() override {
		// turn off all lights
		for(int i = 0; i < NUM_LIGHTS; i++) {
			lights[i].setBrightness(0.f);
		}
		toggle = false;
		const_choice = 0;
	}

	void process(const ProcessArgs &args) override;


	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
	json_t* dataToJson() override {
		//TODO: this or dataFromJson causes segfault sometimes (I think), consider using json_object_set_new instead of decref (or should reference even be decremented?)
		json_t *data, *toggle_value, *const_choice_value;
		data = json_object();
		toggle_value = json_boolean(toggle);
		const_choice_value = json_integer(const_choice);

		json_object_set(data, "toggle", toggle_value);
		json_object_set(data, "const_choice", const_choice_value);

		json_decref(toggle_value);
		json_decref(const_choice_value);
		return data;
	}

	void dataFromJson(json_t* root) override {
		json_t *toggle_value, *const_choice_value;
		toggle_value = json_object_get(root, "toggle");
		if(json_is_boolean(toggle_value)) {
			toggle = json_boolean_value(toggle_value);
		}
		const_choice_value = json_object_get(root, "const_choice");
		if(json_is_integer(const_choice_value)) {
			const_choice = int(json_integer_value(const_choice_value));
		}
	}

};


void ButtonModule::process(const ProcessArgs &args) {
	float deltaTime = args.sampleTime;

	float gateVoltage = rescale(inputs[TRIG_INPUT].value, 0.1f, 2.f, 0.f, 1.f);

	bool gate = (bool(params[BUTTON_PARAM].value)
	             || gateVoltage >= 1.f);

	bool triggered = inputTrigger.process(gate ? 1.0f : 0.0f);

	bool trigger = triggerGenerator.process(deltaTime);

	if(triggered) {
		triggerGenerator.trigger(1e-3f);
		toggle = !toggle;
		const_choice += 1;
		const_choice %= 6;
	}

	outputs[TRIG_OUTPUT].value = trigger ? 10.0f : 0.0f;
	lights[TRIG_LIGHT].setBrightnessSmooth(trigger);

	outputs[GATE_OUTPUT].value = gate ? 10.0f : 0.0f;
	lights[GATE_LIGHT].setBrightnessSmooth(gate);

	outputs[TOGGLE_OUTPUT].value = toggle ? 10.0f : 0.0f;
	lights[TOGGLE_LIGHT].setBrightnessSmooth(toggle);

	bool sign = const_choice >= 3;
	switch(const_choice % 3) {
		case 0: {
			lights[CONST_10_LIGHTP + !sign].setBrightnessSmooth(0.0f);
			lights[CONST_1_LIGHTP + sign].setBrightnessSmooth(1.0f);
			outputs[CONST_OUTPUT].value = 1.f;
			break;
		};
		case 1: {
			lights[CONST_1_LIGHTP + sign].setBrightnessSmooth(0.f);
			lights[CONST_5_LIGHTP + sign].setBrightnessSmooth(1.f);
			outputs[CONST_OUTPUT].value = 5.f;
			break;
		};
		case 2:
		default: {
			lights[CONST_5_LIGHTP + sign].setBrightnessSmooth(0.f);
			lights[CONST_10_LIGHTP + sign].setBrightnessSmooth(1.f);
			outputs[CONST_OUTPUT].value = 10.f;
			break;
		}
	}

	if(const_choice >= 3) {
		outputs[CONST_OUTPUT].value *= -1;
	}

}

struct ButtonWidget : SVGSwitch {
	ButtonWidget() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Button_button_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Button_button_1.svg")));
	}
};

struct ButtonModuleWidget : ModuleWidget {
	ButtonModuleWidget(ButtonModule *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ButtonModule.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(createParam<ButtonWidget>(Vec(7.5, 7.5 + RACK_GRID_WIDTH), module, ButtonModule::BUTTON_PARAM));

		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 87), module, ButtonModule::TRIG_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 142), module, ButtonModule::TRIG_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 192), module, ButtonModule::GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 242), module, ButtonModule::TOGGLE_OUTPUT));

		addChild(createTinyLightForPort<GreenLight>(Vec(22.5, 142), module, ButtonModule::TRIG_LIGHT));
		addChild(createTinyLightForPort<GreenLight>(Vec(22.5, 192), module, ButtonModule::GATE_LIGHT));
		addChild(createTinyLightForPort<GreenLight>(Vec(22.5, 242), module, ButtonModule::TOGGLE_LIGHT));

		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 320), module, ButtonModule::CONST_OUTPUT));

		addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(15, 281), module, ButtonModule::CONST_1_LIGHTP));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(15, 291), module, ButtonModule::CONST_5_LIGHTP));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(15, 301), module, ButtonModule::CONST_10_LIGHTP));
	}
};


Model *modelButtonModule = createModel<ButtonModule, ButtonModuleWidget>("Button");
