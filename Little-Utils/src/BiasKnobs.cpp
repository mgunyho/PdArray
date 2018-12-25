#include "LittleUtils.hpp"
#include "dsp/digital.hpp"
#include "Widgets.hpp"

#include <algorithm> // std::replace

const int N_KNOBS = 5;

//TODO: good name?
struct BiasKnobs : Module {
	enum ParamIds {
		BIAS_1_PARAM,
		BIAS_2_PARAM,
		BIAS_3_PARAM,
		BIAS_4_PARAM,
		BIAS_5_PARAM,
		//BIAS_6_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		INPUT_1,
		INPUT_2,
		INPUT_3,
		INPUT_4,
		INPUT_5,
		//INPUT_6,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_1,
		OUTPUT_2,
		OUTPUT_3,
		OUTPUT_4,
		OUTPUT_5,
		//OUTPUT_6,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	BiasKnobs() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}

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

void BiasKnobs::step() {

	for(int i = 0; i < N_KNOBS; i++) {
		float bias = params[BIAS_1_PARAM + i].value;
		outputs[OUTPUT_1 + i].value = inputs[INPUT_1 + i].value + bias;
	}

	//lights[GATE_LIGHT].setBrightnessSmooth(outputs[GATE_OUTPUT].value);

}

struct BiasKnobsWidget : ModuleWidget {
	BiasKnobs *module;
	TextBox *displays[N_KNOBS];

	BiasKnobsWidget(BiasKnobs *module) : ModuleWidget(module) {
		this->module = module;
		setPanel(SVG::load(assetPlugin(plugin, "res/BiasKnobs.svg")));

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float elem_height = 65.f;
		for(int i = 0; i < N_KNOBS; i++) {
			float top_items_center_y = 30.f + i * elem_height;
			float bot_items_center_y = 60.f + i * elem_height;
			addParam(createParamCentered<Trimpot>(Vec(20., top_items_center_y),
						module, BiasKnobs::BIAS_1_PARAM + i, -10.f, 10.f, 0.f));
			addInput(createInputCentered<PJ301MPort>(Vec(20., bot_items_center_y),
						module, BiasKnobs::INPUT_1 + i));
			//addOutput(createOutputCentered<PJ301MPort>(Vec(120 - 22.5, bot_items_center_y),
			//			module, BiasKnobs::OUTPUT_1 + i));
			addOutput(createOutputCentered<PJ301MPort>(Vec(75 - 20., bot_items_center_y),
						module, BiasKnobs::OUTPUT_1 + i));

			//TODO: is 'new' good? does it get freed somewhere?
			TextBox *display = new TextBox();
			display->box.pos = Vec(35.0, top_items_center_y - display->box.size.y * 0.5f);
			//display->font_size = 18;
			//display->box.size.x = 45;
			display->font_size = 14;
			display->box.size.x = 35;
			display->textOffset.x = display->box.size.x * 0.5f;
			displays[i] = display;
			addChild(displays[i]);
		}

	}

	void step() override {
		ModuleWidget::step();
		for(int i = 0; i < N_KNOBS; i++) {
			float bias = module->params[BiasKnobs::BIAS_1_PARAM + i].value;
			std::string s = stringf(fabs(bias) < 9.95f ? "%+.1fV" : "%+.0f.V", bias);
			//std::string s = stringf(fabs(bias) < 9.95f ? "%+.1f" : "%+.0f.", bias);
			std::replace(s.begin(), s.end(), '0', 'O');
			displays[i]->setText(s);
		}
	}
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelBiasKnobs = Model::create<BiasKnobs, BiasKnobsWidget>("Little Utils", "BiasKnobs", "Bias Knobs", UTILITY_TAG);
