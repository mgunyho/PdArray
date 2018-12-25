#include "LittleUtils.hpp"
#include "dsp/digital.hpp"

#include <algorithm> // std::replace

const float MIN_EXPONENT = -3.0f;
const float MAX_EXPONENT = 1.0f;

// the module is called PulseGenModule because PulseGenerator is already taken
// in dsp/digital.hpp
struct PulseGenModule : Module {
	enum ParamIds {
		GATE_LENGTH_PARAM,
		CV_AMT_PARAM,
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

//TODO: consider moving to utils.hpp or something
//sgn() function based on https://stackoverflow.com/questions/1903954/is-there-a-standard-sign-function-signum-sgn-in-c-c
inline constexpr
int signum(float val) {
	return (0.f < val) - (val < 0.f);
}

void PulseGenModule::step() {
	float deltaTime = engineGetSampleTime();

	bool triggered = inputTrigger.process(rescale(inputs[TRIG_INPUT].value,
												  0.1f, 2.f, 0.f, 1.f));

	float knob_value = params[GATE_LENGTH_PARAM].value;
	float cv_amt = params[CV_AMT_PARAM].value;
	float cv_voltage = inputs[GATE_LENGTH_INPUT].value;
	if(params[LIN_LOG_MODE_PARAM].value < 0.5f) {

		gate_duration = clamp(knob_value + cv_voltage * cv_amt, 0.f, 10.f);

	} else {
		float exponent = rescale(knob_value,
				0.f, 10.f, MIN_EXPONENT, MAX_EXPONENT);

		if(cv_amt != 0 && cv_voltage != 0) {
			// logarithmic CV
			float cv_exponent = rescale(fabs(cv_amt), 0.f, 1.f,
					MIN_EXPONENT, MAX_EXPONENT);
			// decrease exponent by one so that 10V maps to 1.0 (100%) CV.
			float cv_scale = powf(10.0f, cv_exponent - 1.f);
			gate_duration = clamp(
					powf(10.0f, exponent) + cv_scale * cv_voltage * signum(cv_amt),
					0.f, 10.f);
		} else {
			gate_duration = powf(10.0f, exponent);
		}
	}

	if(triggered) {
		gateGenerator.trigger(gate_duration);
	}
	//TODO: adjusting trigger time once triggered has no effect
	//gateGenerator.setDuration(gate_duration) ???

	outputs[GATE_OUTPUT].value = gateGenerator.process(deltaTime) ? 10.0f : 0.0f;

	lights[GATE_LIGHT].setBrightnessSmooth(outputs[GATE_OUTPUT].value);

}

struct MsDisplayWidget : TransparentWidget {
	// based on LedDisplayChoice
	std::string text;
	bool msLabelStatus = false; // 0 = 'ms', 1 = 's'
	std::shared_ptr<Font> font;
	Vec textOffset;
	NVGcolor color;

	//TODO: create<...>() thing with position as argument?
	MsDisplayWidget() {
		font = Font::load(assetPlugin(plugin, "res/RobotoMono-Bold.ttf"));
		color = nvgRGB(0x23, 0x23, 0x23);
		box.size = Vec(30, 35);
		textOffset = Vec(box.size.x * 0.5f, 0.f);
	}

	void updateDisplayValue(float v) {
		std::string s;
		if(v <= 0.0995) {
			v *= 1e3f;
			s = stringf("%#.2g", v < 1.f ? 0.f : v);
			msLabelStatus = false;
		} else {
			s = stringf("%#.2g", v);
			msLabelStatus = true;
			if(s.at(0) == '0') s.erase(0, 1);
		}
		// hacky way to make monospace fonts prettier
		std::replace(s.begin(), s.end(), '0', 'O');
		text = s;
	}

	void draw(NVGcontext *vg) override {
		// based on LedDisplayChoice::draw() in Rack/src/app/LedDisplay.cpp
		nvgScissor(vg, 0, 0, box.size.x, box.size.y);

		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 3.0);
		nvgFillColor(vg, nvgRGB(0xc8, 0xc8, 0xc8));
		nvgFill(vg);

		if (font->handle >= 0) {
			nvgFillColor(vg, color);
			nvgFontFaceId(vg, font->handle);

			nvgFontSize(vg, 20);
			nvgTextLetterSpacing(vg, -2.0);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			nvgText(vg, textOffset.x, textOffset.y, text.c_str(), NULL);

			nvgFontSize(vg, 20);
			nvgTextLetterSpacing(vg, 0.f);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			nvgText(vg, textOffset.x, textOffset.y + 14,
					msLabelStatus ? " s" : "ms", NULL);
		}

		nvgResetScissor(vg);
	}
};

struct PulseGeneratorWidget : ModuleWidget {
	PulseGenModule *module;
	MsDisplayWidget *msDisplay;

	PulseGeneratorWidget(PulseGenModule *module) : ModuleWidget(module) {
		this->module = module;
		setPanel(SVG::load(assetPlugin(plugin, "res/PulseGenerator.svg")));

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(
					Vec(22.5, 37.5), module,
					PulseGenModule::GATE_LENGTH_PARAM, 0.f, 10.f,
					// 0.5s in log scale
					rescale(-0.30103f, MIN_EXPONENT, MAX_EXPONENT, 0.f,10.f)
					));
		addParam(createParamCentered<Trimpot>(
					Vec(22.5, 133), module, PulseGenModule::CV_AMT_PARAM,
					-1.f, 1.f, 0.f));
		addParam(createParam<CKSS>(Vec(7.5, 69), module, PulseGenModule::LIN_LOG_MODE_PARAM, 0.f, 1.f, 1.f));

		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 182), module, PulseGenModule::GATE_LENGTH_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 228), module, PulseGenModule::TRIG_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 278), module, PulseGenModule::GATE_OUTPUT));

		double offset = 3.6;
		addChild(createLightCentered<TinyLight<GreenLight>>(Vec(37.5 - offset, 263 + offset), module, PulseGenModule::GATE_LIGHT));

		//TODO: is 'new' good? does it get freed somewhere?
		msDisplay = new MsDisplayWidget();
		msDisplay->box.pos = Vec(7.5, 300);
		addChild(msDisplay);

	}

	void step() override {
		ModuleWidget::step();
		msDisplay->updateDisplayValue(module->gate_duration);
	}
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelPulseGenerator = Model::create<PulseGenModule, PulseGeneratorWidget>("Little Utils", "PulseGenerator", "Pulse Generator", UTILITY_TAG, ENVELOPE_GENERATOR_TAG);
