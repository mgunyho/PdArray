#include "plugin.hpp"
#include "Widgets.hpp"

#include <algorithm> // std::replace

const int MAX_POLY_CHANNELS = 16;

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
	float valid_div_value[MAX_POLY_CHANNELS];

	MulDiv() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(MulDiv::A_SCALE_PARAM,   0.f, 2.f, 0.f, "A scale");
		configParam(MulDiv::B_SCALE_PARAM,   0.f, 2.f, 0.f, "B scale");
		configParam(MulDiv::OUT_SCALE_PARAM, 0.f, 2.f, 0.f, "Output scale");
		configParam(MulDiv::CLIP_ENABLE_PARAM, 0.f, 1.f, 0.f, "Clip to +/-10V");
		for(int i = 0; i < MAX_POLY_CHANNELS; i++) {
			valid_div_value[i] = 0.f;
		}
	}

	void process(const ProcessArgs &args) override;

};

void MulDiv::process(const ProcessArgs &args) {
	float deltaTime = args.sampleTime;
	bool clip = params[CLIP_ENABLE_PARAM].getValue() > 0.5f;
	auto a_in = inputs[A_INPUT];
	auto b_in = inputs[B_INPUT];
	int ac = a_in.getChannels();
	int bc = b_in.getChannels();
	const int channels = std::max(ac, bc);

	outputs[MUL_OUTPUT].setChannels(channels);
	outputs[DIV_OUTPUT].setChannels(channels);

	float as = int(params[A_SCALE_PARAM].getValue()) == 0 ? 1.0 : 1./(params[A_SCALE_PARAM].getValue() * 5.0);
	float bs = int(params[B_SCALE_PARAM].getValue()) == 0 ? 1.0 : 1./(params[B_SCALE_PARAM].getValue() * 5.0);
	float os = int(params[OUT_SCALE_PARAM].getValue()) == 0 ? 1.0 : params[OUT_SCALE_PARAM].getValue() * 5.0;

	for(int c = 0; c < channels; c++) {

	float a_cond = (c < ac) || ac == 1;
	float b_cond = (c < bc) || bc == 1;
	float m = (a_cond ? a_in.getPolyVoltage(c) * as : 1.f) * (b_cond ? b_in.getPolyVoltage(c) * bs : 1.f);
	m *= os;

	outputs[MUL_OUTPUT].setVoltage(clip ? clamp(m, -10.f, 10.f) : m, c);

	if(b_cond) {
		float d = (a_cond ? a_in.getPolyVoltage(c) : 1.f) / b_in.getPolyVoltage(c) * os;
		valid_div_value[c] = std::isfinite(d) ? d : valid_div_value[c];
		if(clip) valid_div_value[c] = clamp(valid_div_value[c], -10.f, 10.f);
		outputs[DIV_OUTPUT].setVoltage(valid_div_value[c], c);
	} else {
		outputs[DIV_OUTPUT].setVoltage(clip ? clamp(a_in.getPolyVoltage(c) * os, -10.f, 10.f) : a_in.getPolyVoltage(c) * os, c);
	}

	}

	lights[CLIP_ENABLE_LIGHT].setSmoothBrightness(clip, deltaTime);
}

struct MulDivWidget : ModuleWidget {
	MulDiv *module;

	MulDivWidget(MulDiv *module) {
		setModule(module);
		this->module = module;
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/MulDiv.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(Vec(22.5,  46), module, MulDiv::A_INPUT));

		addChild(createParam<CKSSThreeH>(Vec(7.5,  63), module, MulDiv::A_SCALE_PARAM));

		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 119), module, MulDiv::B_INPUT));

		addChild(createParam<CKSSThreeH>(Vec(7.5, 136), module, MulDiv::B_SCALE_PARAM));
		addChild(createParam<CKSSThreeH>(Vec(7.5, 177), module, MulDiv::OUT_SCALE_PARAM));

		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 236), module, MulDiv::MUL_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 286), module, MulDiv::DIV_OUTPUT));

		addParam(createParamCentered<ToggleLEDButton>(Vec(22.5, 315), module, MulDiv::CLIP_ENABLE_PARAM));

		addChild(createLightCentered<MediumLight<GreenLight>>(Vec(22.5, 315), module, MulDiv::CLIP_ENABLE_LIGHT));
	}

};


Model *modelMulDiv = createModel<MulDiv, MulDivWidget>("MultiplyDivide");
