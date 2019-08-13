#include "plugin.hpp"
#include "Widgets.hpp"
#include "Util.hpp"

#include <algorithm> // std::replace

//TODO: when cv has been recently adjusted, tweaking the main knob should switch the display to the non-cv view.

const float MIN_EXPONENT = -3.0f;
const float MAX_EXPONENT = 1.0f;

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


// the module is called PulseGenModule because PulseGenerator is already taken //TODO: not true anymore, rename
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
		FINISH_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		GATE_LIGHT,
		FINISH_LIGHT,
		NUM_LIGHTS
	};

	dsp::SchmittTrigger inputTrigger, finishTrigger;
	CustomPulseGenerator gateGenerator, finishTriggerGenerator;
	float gate_base_duration = 0.5f; // gate duration without CV
	float gate_duration;
	bool realtimeUpdate = true; // whether to display gate_duration or gate_base_duration
	float cv_scale = 0.f; // cv_scale = +- 1 -> 10V CV changes duration by +-10s

	PulseGenModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PulseGenModule::LIN_LOG_MODE_PARAM, 0.f, 1.f, 1.f, "");
		gate_duration = gate_base_duration;
	}

	void process(const ProcessArgs &args) override;


	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

	json_t *dataToJson() override {
		json_t *root = json_object();
		json_object_set_new(root, "realtimeUpdate", json_boolean(realtimeUpdate));
		return root;
	}

	void dataFromJson(json_t *root) override {
		json_t *realtimeUpdate_J = json_object_get(root, "realtimeUpdate");
		if(realtimeUpdate_J) {
			realtimeUpdate = json_boolean_value(realtimeUpdate_J);
		}
	}

};

void PulseGenModule::process(const ProcessArgs &args) {
	float deltaTime = args.sampleTime;

	bool triggered = inputTrigger.process(rescale(inputs[TRIG_INPUT].value,
												  0.1f, 2.f, 0.f, 1.f));

	// handle duration knob and CV
	float knob_value = params[GATE_LENGTH_PARAM].value;
	float cv_amt = params[CV_AMT_PARAM].value;
	float cv_voltage = inputs[GATE_LENGTH_INPUT].value;

	if(params[LIN_LOG_MODE_PARAM].value < 0.5f) {
		// linear mode
		cv_scale = cv_amt;
		gate_base_duration = knob_value;
	} else {
		// logarithmic mode
		float exponent = rescale(knob_value,
				0.f, 10.f, MIN_EXPONENT, MAX_EXPONENT);

		float cv_exponent = rescale(fabs(cv_amt), 0.f, 1.f,
				MIN_EXPONENT, MAX_EXPONENT);

		// decrease exponent by one so that 10V maps to 1.0 (100%) CV.
		cv_scale = powf(10.0f, cv_exponent - 1.f) * signum(cv_amt); // take sign into account

		gate_base_duration = powf(10.0f, exponent);
	}
	gate_duration = clamp(gate_base_duration + cv_voltage * cv_scale, 0.f, 10.f);

	if(triggered && gate_duration > 0.f) {
		gateGenerator.trigger(gate_duration);
	}

	// update trigger duration even in the middle of a trigger
	gateGenerator.triggerDuration = gate_duration;

	bool gate = gateGenerator.process(deltaTime);

	if(finishTrigger.process(gate ? 0.f : 1.f)) {
		finishTriggerGenerator.trigger(1.e-3f);
	}

	outputs[GATE_OUTPUT].value = gate ? 10.0f : 0.0f;
	outputs[FINISH_OUTPUT].value = finishTriggerGenerator.process(deltaTime) ? 10.f : 0.f;

	lights[GATE_LIGHT].setBrightnessSmooth(outputs[GATE_OUTPUT].value);
	lights[FINISH_LIGHT].setBrightnessSmooth(outputs[FINISH_OUTPUT].value);

}

// TextBox defined in ./Widgets.hpp
struct MsDisplayWidget : TextBox {
	PulseGenModule *module;
	bool msLabelStatus = false; // 0 = 'ms', 1 = 's'
	bool cvLabelStatus = false; // whether to show 'cv'
	float previous_displayed_value = 0.f;
	float cvDisplayTime = 2.f;

	GUITimer cvDisplayTimer;

	MsDisplayWidget(PulseGenModule *m) : TextBox() {
		module = m;
		box.size = Vec(30, 27);
		letter_spacing = -2.0f;
	}

	void updateDisplayValue(float v) {
		// only update/do stringf if value is changed
		if(v != previous_displayed_value) {
			std::string s;
			previous_displayed_value = v;
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
			setText(s);
		}
	}

	void draw(const DrawArgs &args) override {
		TextBox::draw(args);
		nvgScissor(args.vg, 0, 0, box.size.x, box.size.y); //TODO: use just vg instead of args.vg

		if(font->handle >= 0) {
			nvgFillColor(args.vg, textColor);
			nvgFontFaceId(args.vg, font->handle);

			// draw 'ms' or 's' on bottom, depending on msLabelStatus
			nvgFontSize(args.vg, 12);
			nvgTextLetterSpacing(args.vg, 0.f);
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			nvgText(args.vg, textOffset.x + 2, textOffset.y + 14,
					msLabelStatus ? " s" : "ms", NULL);

			if(cvLabelStatus) {
				nvgText(args.vg, 3, textOffset.y + 14, "cv", NULL);
			}
		}

		nvgResetScissor(args.vg);
	}

	void triggerCVDisplay() {
		cvDisplayTimer.trigger(cvDisplayTime);
	}

	void step() override {
		TextBox::step();
		cvLabelStatus = cvDisplayTimer.process();
		if(cvLabelStatus){
			updateDisplayValue(fabs(module->cv_scale * 10.f));
		}else{
			updateDisplayValue(module->realtimeUpdate ? module->gate_duration : module->gate_base_duration);
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

struct PulseGeneratorToggleRealtimeMenuItem : MenuItem {
	PulseGenModule *module;
	PulseGeneratorToggleRealtimeMenuItem(): MenuItem() {}
	void onAction(const event::Action &e) override {
		module->realtimeUpdate = !module->realtimeUpdate;
	}
};

struct PulseGeneratorWidget : ModuleWidget {
	PulseGenModule *module;
	MsDisplayWidget *msDisplay;

	PulseGeneratorWidget(PulseGenModule *module) {
		setModule(module);
		this->module = module;
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PulseGenerator.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(
					Vec(22.5, 37.5), module,
					PulseGenModule::GATE_LENGTH_PARAM, 0.f, 10.f,
					// 0.5s in log scale
					//rescale(-0.30103f, MIN_EXPONENT, MAX_EXPONENT, 0.f,10.f)
					5.f // 0.1s in log mode, 5s in lin mode
					));

		//addParam(createParamCentered<CustomTrimpot>(
		//			Vec(22.5, 133), module, PulseGenModule::CV_AMT_PARAM,
		//			-1.f, 1.f, 0.f));

		addParam(createParam<CKSS>(Vec(7.5, 60), module, PulseGenModule::LIN_LOG_MODE_PARAM));

		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 151), module, PulseGenModule::GATE_LENGTH_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 192), module, PulseGenModule::TRIG_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 240), module, PulseGenModule::GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 288), module, PulseGenModule::FINISH_OUTPUT));

		addChild(createTinyLightForPort<GreenLight>(Vec(22.5, 240), module, PulseGenModule::GATE_LIGHT));
		addChild(createTinyLightForPort<GreenLight>(Vec(22.5, 288), module, PulseGenModule::FINISH_LIGHT));

		msDisplay = new MsDisplayWidget(module);
		msDisplay->box.pos = Vec(7.5, 308);
		addChild(msDisplay);

		auto cvKnob = createParamCentered<CustomTrimpot>(
					Vec(22.5, 110), module, PulseGenModule::CV_AMT_PARAM,
					-1.f, 1.f, 0.f);
		cvKnob->display = msDisplay;
		addParam(cvKnob);

	}

	void appendContextMenu(ui::Menu* menu) override {

		menu->addChild(new MenuLabel());

		auto *toggleItem = new PulseGeneratorToggleRealtimeMenuItem();
		toggleItem->text = "Update display in real time";
		toggleItem->rightText = CHECKMARK(this->module->realtimeUpdate);
		toggleItem->module = this->module;
		menu->addChild(toggleItem);
	}

};


Model *modelPulseGenerator = createModel<PulseGenModule, PulseGeneratorWidget>("PulseGenerator");
