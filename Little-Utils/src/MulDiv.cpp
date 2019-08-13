#include "plugin.hpp"
#include "Widgets.hpp"

#include <algorithm> // std::replace

struct MulDiv : Module {
	enum ParamIds {
		A_SCALE_PARAM,
		B_SCALE_PARAM,
		OUT_SCALE_PARAM,
		CLIP_ENABLE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		A_INPUT,
		B_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MUL_OUTPUT,
		DIV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		CLIP_ENABLE_LIGHT,
		NUM_LIGHTS
	};

	// output this instead of NaN (when e.g. dividing by zero)
	float valid_div_value = 0.f;

	MulDiv() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	}

	void process(const ProcessArgs &args) override;


	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

};

void MulDiv::process(const ProcessArgs &args) {
	bool clip = params[CLIP_ENABLE_PARAM].value > 0.5f;
	auto a_in = inputs[A_INPUT];
	auto b_in = inputs[B_INPUT];

	float as = int(params[A_SCALE_PARAM].value) == 0 ? 1.0 : 1./(params[A_SCALE_PARAM].value * 5.0);
	float bs = int(params[B_SCALE_PARAM].value) == 0 ? 1.0 : 1./(params[B_SCALE_PARAM].value * 5.0);
	float os = int(params[OUT_SCALE_PARAM].value) == 0 ? 1.0 : params[OUT_SCALE_PARAM].value * 5.0;

	float m = (a_in.active ? a_in.value * as : 1.f) * (b_in.active ? b_in.value * bs : 1.f);
	m *= os;

	outputs[MUL_OUTPUT].value = clip ? clamp(m, -10.f, 10.f) : m;

	if(b_in.active) {
		float d = (a_in.active ? a_in.value : 1.f) / b_in.value * os;
		valid_div_value = std::isfinite(d) ? d : valid_div_value;
		if(clip) valid_div_value = clamp(valid_div_value, -10.f, 10.f);
		outputs[DIV_OUTPUT].value = valid_div_value;
	} else {
		outputs[DIV_OUTPUT].value = clip ? clamp(a_in.value * os, -10.f, 10.f) : a_in.value * os;
	}

	lights[CLIP_ENABLE_LIGHT].setBrightnessSmooth(clip);
}

struct MulDivWidget : ModuleWidget {
	MulDiv *module;

	MulDivWidget(MulDiv *module) {
		setModule(module);
		this->module = module;
		setPanel(APP->window->loadSvg(assetPlugin(pluginInstance, "res/MulDiv.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(Vec(22.5,  46), module, MulDiv::A_INPUT));

		// this has to be done manually instead of createParam to add plugin assets before setDefaultvalue()
		//TODO: fix / reimplement this
		/*
		auto scaleSwitch = new CKSSThreeH();
		scaleSwitch->box.pos = Vec(7.5, 63);
		scaleSwitch->module = module;
		scaleSwitch->paramId = MulDiv::A_SCALE_PARAM;
		scaleSwitch->addFrames(pluginInstance);
		scaleSwitch->setLimits(0.f, 2.f);
		scaleSwitch->setDefaultValue(0.f);
		addParam(scaleSwitch);

		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 119), module, MulDiv::B_INPUT));

		scaleSwitch = new CKSSThreeH();
		scaleSwitch->setPosition(Vec(7.5, 136));
		scaleSwitch->setModule(module);
		scaleSwitch->paramId = MulDiv::B_SCALE_PARAM;
		scaleSwitch->addFrames(pluginInstance);
		scaleSwitch->setLimits(0.f, 2.f);
		scaleSwitch->setDefaultValue(0.f);
		addParam(scaleSwitch);

		scaleSwitch = new CKSSThreeH();
		scaleSwitch->box.pos = Vec(7.5, 177);
		scaleSwitch->module = module;
		scaleSwitch->paramId = MulDiv::OUT_SCALE_PARAM;
		scaleSwitch->addFrames(pluginInstance);
		scaleSwitch->setLimits(0.f, 2.f);
		scaleSwitch->setDefaultValue(0.f);
		addParam(scaleSwitch);
		*/

		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 236), module, MulDiv::MUL_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 286), module, MulDiv::DIV_OUTPUT));

		addParam(createParamCentered<ToggleLEDButton>(Vec(22.5, 315), module, MulDiv::CLIP_ENABLE_PARAM, 0.f, 1.f, 0.f));

		addChild(createLightCentered<MediumLight<GreenLight>>(Vec(22.5, 315), module, MulDiv::CLIP_ENABLE_LIGHT));
	}

};


Model *modelMulDiv = createModel<MulDiv, MulDivWidget>("MultiplyDivide");
