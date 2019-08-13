#include "plugin.hpp"
#include "Util.hpp"
#include "Widgets.hpp"

#include <algorithm> // std::replace

// TODO: add 'quantize output' option, quantize semitones to 1st and volts to 0.1V (?)

const int N_KNOBS = 5;
constexpr float KNOB_COLORS[N_KNOBS][3] = {
	{0.0f, 1.0f, 0.0f},
	{1.0f, 0.5f, 0.0f},
	{1.0f, 0.0f, 0.0f},
	{1.0f, 0.0f, 1.0f},
	{0.0f, 0.5f, 1.0f}
};

const int MAX_SEMITONES = 36;

struct Bias_Semitone : Module {
	enum ParamIds {
		BIAS_1_PARAM,
		BIAS_2_PARAM,
		BIAS_3_PARAM,
		BIAS_4_PARAM,
		BIAS_5_PARAM,
		MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		INPUT_1,
		INPUT_2,
		INPUT_3,
		INPUT_4,
		INPUT_5,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_1,
		OUTPUT_2,
		OUTPUT_3,
		OUTPUT_4,
		OUTPUT_5,
		NUM_OUTPUTS
	};
	enum LightIds {
		INPUT_1_LIGHTR,
		INPUT_1_LIGHTG,
		INPUT_1_LIGHTB,
		INPUT_2_LIGHTR,
		INPUT_2_LIGHTG,
		INPUT_2_LIGHTB,
		INPUT_3_LIGHTR,
		INPUT_3_LIGHTG,
		INPUT_3_LIGHTB,
		INPUT_4_LIGHTR,
		INPUT_4_LIGHTG,
		INPUT_4_LIGHTB,
		INPUT_5_LIGHTR,
		INPUT_5_LIGHTG,
		INPUT_5_LIGHTB,

		OUTPUT_1_LIGHTR,
		OUTPUT_1_LIGHTG,
		OUTPUT_1_LIGHTB,
		OUTPUT_2_LIGHTR,
		OUTPUT_2_LIGHTG,
		OUTPUT_2_LIGHTB,
		OUTPUT_3_LIGHTR,
		OUTPUT_3_LIGHTG,
		OUTPUT_3_LIGHTB,
		OUTPUT_4_LIGHTR,
		OUTPUT_4_LIGHTG,
		OUTPUT_4_LIGHTB,
		OUTPUT_5_LIGHTR,
		OUTPUT_5_LIGHTG,
		OUTPUT_5_LIGHTB,
		NUM_LIGHTS
	};

	//SchmittTrigger oneToManyTrigger;
	//bool oneToManyState;
	Bias_Semitone() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	}

	void process(const ProcessArgs &args) override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

};

void Bias_Semitone::process(const ProcessArgs &args) {
	//float deltaTime = engineGetSampleTime();

	int li = 0; // index of the latest encountered active input
	for(int i = 0; i < N_KNOBS; i++) {
		float bias = params[BIAS_1_PARAM + i].value;
		li = inputs[INPUT_1 + i].active ? i : li;
		if(params[MODE_PARAM].value < 0.5f) {
			// shift input CV by semitones
			bias = int(bias * MAX_SEMITONES) / 12.f;
		} else {
			// output volts
			bias *= 10.f;
		}
		outputs[OUTPUT_1 + i].value = inputs[INPUT_1 + li].value + bias;

		// use setBrigthness instead of setBrightnessSmooth to reduce power usage
		lights[INPUT_1_LIGHTR + 3*i].setBrightness(KNOB_COLORS[i][0]);
		lights[INPUT_1_LIGHTG + 3*i].setBrightness(KNOB_COLORS[i][1]);
		lights[INPUT_1_LIGHTB + 3*i].setBrightness(KNOB_COLORS[i][2]);

		lights[OUTPUT_1_LIGHTR + 3*i].setBrightness(KNOB_COLORS[li][0]);
		lights[OUTPUT_1_LIGHTG + 3*i].setBrightness(KNOB_COLORS[li][1]);
		lights[OUTPUT_1_LIGHTB + 3*i].setBrightness(KNOB_COLORS[li][2]);
	}
}

struct Bias_SemitoneWidget : ModuleWidget {
	Bias_Semitone *module;
	TextBox *displays[N_KNOBS];

	Bias_SemitoneWidget(Bias_Semitone *module) {
		setModule(module);
		this->module = module;
		setPanel(SVG::load(assetPlugin(pluginInstance, "res/Bias_Semitone.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float elem_height = 58.f;
		for(int i = 0; i < N_KNOBS; i++) {
			float top_items_center_y = 30.f + i * elem_height;
			float bot_items_center_y = 55.f + i * elem_height;
			addParam(createParamCentered<Trimpot>(Vec(20., top_items_center_y),
						module, Bias_Semitone::BIAS_1_PARAM + i, -1.f, 1.f, 0.f));

			Vec input_pos = Vec(20., bot_items_center_y);
			addInput(createInputCentered<PJ301MPort>(input_pos,
						module, Bias_Semitone::INPUT_1 + i));

			Vec output_pos = Vec(75 - 20., bot_items_center_y);
			addOutput(createOutputCentered<PJ301MPort>(output_pos,
						module, Bias_Semitone::OUTPUT_1 + i));

			addChild(createTinyLightForPort<RedGreenBlueLight>(input_pos,  module, Bias_Semitone::INPUT_1_LIGHTR  + 3*i));
			addChild(createTinyLightForPort<RedGreenBlueLight>(output_pos, module, Bias_Semitone::OUTPUT_1_LIGHTR + 3*i));

			TextBox *display = new TextBox();
			//display->font_size = 18;
			//display->box.size.x = 45;
			display->font_size = 14;
			display->box.size.x = 35;
			display->box.size.y = 14;
			display->textOffset.x = display->box.size.x * 0.5f;
			display->box.pos = Vec(35.0, top_items_center_y - display->box.size.y * 0.5f);
			displays[i] = display;
			addChild(displays[i]);
		}

		addParam(createParam<CKSS>(Vec(15, 311), module, Bias_Semitone::MODE_PARAM,
				0.f, 1.f, 1.f
				));
	}

	void step() override {
		ModuleWidget::step();
		for(int i = 0; i < N_KNOBS; i++) {
			float bias = module->params[Bias_Semitone::BIAS_1_PARAM + i].value;
			std::string s;
			if(module->params[Bias_Semitone::MODE_PARAM].value < 0.5f) {
				int st = bias * MAX_SEMITONES;
				s = stringf("%+3dst", st);
			} else {
				s = stringf(fabs(bias) < 0.995f ? "%+.1fV" : "%+.0f.V", bias * 10.f);
			}
			std::replace(s.begin(), s.end(), '0', 'O');
			displays[i]->setText(s);
		}
	}
};


Model *modelBias_Semitone = createModel<Bias_Semitone, Bias_SemitoneWidget>("BiasSemitone");
