#include "PDArray.hpp"
#include "Widgets.hpp"
#include "Util.hpp"

#include <algorithm> // std::replace

const float MIN_EXPONENT = -3.0f;
const float MAX_EXPONENT = 1.0f;

// quick and dirty copy-paste of LittleUtils PulseGenerator.
// TODO: move common modules to common folder
// TODO: tweak gate/ramp port vertical positions
// TODO: show sample count corresponding to duration (needs more space - remove CV?)
// TODO: add trigger buttton (another reason to remove cv)
// TODO: fix capitalization in file and module name

// based on PulseGeneraotr in include/util/digital.hpp
struct CustomPulseGenerator {
	float time;
	float triggerDuration;
	bool finished; // the output is the inverse of this

	CustomPulseGenerator() {
		reset();
	}
	/** Immediately resets the state to LOW */
	void reset() {
		time = 0.f;
		triggerDuration = 0.f;
		finished = true;
	}
	/** Advances the state by `deltaTime`. Returns whether the pulse is in the HIGH state. */
	bool process(float deltaTime) {
		time += deltaTime;
		if(!finished) finished = time >= triggerDuration;
		return !finished;
	}
	/** Begins a trigger with the given `triggerDuration`. */
	void trigger(float triggerDuration) {
		// retrigger even with a shorter duration
		time = 0.f;
		finished = false;
		this->triggerDuration = triggerDuration;
	}
};

struct MiniRamp : Module {
	enum ParamIds {
		RAMP_LENGTH_PARAM,
		CV_AMT_PARAM,
		LIN_LOG_MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		TRIG_INPUT,
		RAMP_LENGTH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		RAMP_OUTPUT,
		GATE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		RAMP_LIGHT,
		GATE_LIGHT,
		NUM_LIGHTS
	};

	dsp::SchmittTrigger inputTrigger;
	CustomPulseGenerator gateGen;
	float ramp_duration = 0.5f;
	float cv_scale = 0.f; // cv_scale = +- 1 -> 10V CV changes duration by +-10s

	MiniRamp() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		// TODO: tooltips
		configParam(MiniRamp::RAMP_LENGTH_PARAM, 0.f, 10.f,
					// 0.5s in log scale
					//rescale(-0.30103f, MIN_EXPONENT, MAX_EXPONENT, 0.f,10.f)
					5.f // 0.1s in log mode, 5s in lin mode
					);
		configParam(MiniRamp::CV_AMT_PARAM, -1.f, 1.f, 0.f, "");
		configParam(MiniRamp::LIN_LOG_MODE_PARAM, 0.f, 1.f, 1.f, "");
	}

	void process(const ProcessArgs &args) override;

};

void MiniRamp::process(const ProcessArgs &args) {
	float deltaTime = args.sampleTime;

	bool triggered = inputTrigger.process(rescale(inputs[TRIG_INPUT].getVoltage(),
												  0.1f, 2.f, 0.f, 1.f));

	// handle duration knob and CV
	float knob_value = params[RAMP_LENGTH_PARAM].getValue();
	float cv_amt = params[CV_AMT_PARAM].getValue();
	float cv_voltage = inputs[RAMP_LENGTH_INPUT].getVoltage();

	if(params[LIN_LOG_MODE_PARAM].getValue() < 0.5f) {
		// linear mode
		cv_scale = cv_amt;
		ramp_duration = clamp(knob_value + cv_voltage * cv_amt, 0.f, 10.f);
	} else {
		// logarithmic mode
		float exponent = rescale(knob_value,
				0.f, 10.f, MIN_EXPONENT, MAX_EXPONENT);

		float cv_exponent = rescale(fabs(cv_amt), 0.f, 1.f,
				MIN_EXPONENT, MAX_EXPONENT);

		// decrease exponent by one so that 10V maps to 1.0 (100%) CV.
		cv_scale = powf(10.0f, cv_exponent - 1.f) * signum(cv_amt); // take sign into account

		ramp_duration = clamp(
				powf(10.0f, exponent) + cv_voltage * cv_scale,
				0.f, 10.f);
	}

	if(triggered && ramp_duration > 0.f) {
		gateGen.trigger(ramp_duration);
	}

	// update trigger duration even in the middle of a trigger
	gateGen.triggerDuration = ramp_duration;

	bool gate = gateGen.process(deltaTime);

	outputs[RAMP_OUTPUT].value = gate ?
		clamp(gateGen.time / gateGen.triggerDuration * 10.f, 0.f, 10.f) : 0.f;
	outputs[GATE_OUTPUT].setVoltage(gate ? 10.0f : 0.0f);

	lights[RAMP_LIGHT].setSmoothBrightness(outputs[RAMP_OUTPUT].value * 1e-1f, deltaTime);
	lights[GATE_LIGHT].setSmoothBrightness(outputs[GATE_OUTPUT].value, deltaTime);

}

struct MsDisplayWidget : TextBox {
	MiniRamp *module;
	bool msLabelStatus = false; // 0 = 'ms', 1 = 's'
	bool cvLabelStatus = false; // whether to show 'cv'
	float previous_displayed_value = 0.f;
	float cvDisplayTime = 2.f;

	GUITimer cvDisplayTimer;

	MsDisplayWidget(MiniRamp *m) : TextBox() {
		module = m;
		box.size = Vec(30, 27);
		letter_spacing = -2.0f;
	}

	void updateDisplayValue(float v) {
		std::string s;
		// only update/do stringf if value is changed
		if(v != previous_displayed_value) {
			previous_displayed_value = v;
			if(v <= 0.0995) {
				v *= 1e3f;
				s = string::f("%#.2g", v < 1.f ? 0.f : v);
				msLabelStatus = false;
			} else {
				s = string::f("%#.2g", v);
				msLabelStatus = true;
				if(s.at(0) == '0') s.erase(0, 1);
			}
			// hacky way to make monospace fonts prettier
			std::replace(s.begin(), s.end(), '0', 'O');
			setText(s);
		}
	}

	void draw(const DrawArgs &args) override {
		TextBox::draw(args);
		const auto vg = args.vg;
		nvgScissor(vg, 0, 0, box.size.x, box.size.y);

		if(font->handle >= 0) {
			nvgFillColor(vg, textColor);
			nvgFontFaceId(vg, font->handle);

			// draw 'ms' or 's' on bottom, depending on msLabelStatus
			nvgFontSize(vg, 12);
			nvgTextLetterSpacing(vg, 0.f);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			nvgText(vg, textOffset.x + 2, textOffset.y + 14,
					msLabelStatus ? " s" : "ms", NULL);

			if(cvLabelStatus) {
				nvgText(vg, 3, textOffset.y + 14, "cv", NULL);
			}
		}

		nvgResetScissor(vg);
	}

	void triggerCVDisplay() {
		cvDisplayTimer.trigger(cvDisplayTime);
	}

	void step() override {
		TextBox::step();
		cvLabelStatus = cvDisplayTimer.process();
		if(module) {
			updateDisplayValue(cvLabelStatus ? fabs(module->cv_scale) * 10.f : module->ramp_duration);
		}
	}

};

struct CustomTrimpot : Trimpot {
	MsDisplayWidget *display;
	CustomTrimpot(): Trimpot() {};

	void onDragMove(const event::DragMove &e) override {
		Trimpot::onDragMove(e);
		display->triggerCVDisplay();
	}
};

struct MiniRampWidget : ModuleWidget {
	MiniRamp *module;
	MsDisplayWidget *msDisplay;

	MiniRampWidget(MiniRamp *module) {
		setModule(module);
		this->module = module;
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/MiniRamp.svg"))); //TODO

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(Vec(22.5, 37.5), module,
					MiniRamp::RAMP_LENGTH_PARAM));

		addParam(createParam<CKSS>(Vec(7.5, 60), module, MiniRamp::LIN_LOG_MODE_PARAM));

		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 151), module, MiniRamp::RAMP_LENGTH_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 192), module, MiniRamp::TRIG_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 240), module, MiniRamp::RAMP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 288), module, MiniRamp::GATE_OUTPUT));

		addChild(createTinyLightForPort<GreenLight>(Vec(22.5, 240), module, MiniRamp::RAMP_LIGHT));
		addChild(createTinyLightForPort<GreenLight>(Vec(22.5, 288), module, MiniRamp::GATE_LIGHT));

		msDisplay = new MsDisplayWidget(module);
		msDisplay->box.pos = Vec(7.5, 308);
		addChild(msDisplay);

		auto cvKnob = createParamCentered<CustomTrimpot>(Vec(22.5, 110), module,
				MiniRamp::CV_AMT_PARAM);
		cvKnob->display = msDisplay;
		addParam(cvKnob);

	}

};


Model *modelMiniRamp = createModel<MiniRamp, MiniRampWidget>("Miniramp");
