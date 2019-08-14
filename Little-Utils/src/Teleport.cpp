#include "Teleport.hpp"
#include "Widgets.hpp"
#include "Util.hpp"

/////////////
// modules //
/////////////

//TODO: when editing the label of a teleport in and then clicking on a teleport out, the edited label doesn't appear in the list

struct TeleportInModule : Teleport {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		INPUT_1,
		INPUT_2,
		INPUT_3,
		INPUT_4,
		INPUT_5,
		INPUT_6,
		INPUT_7,
		INPUT_8,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	// Generate random, unique label for this teleport endpoint. Don't modify the sources map.
	std::string getLabel() {
		std::string l;
		do {
			l = randomString(EditableTextBox::defaultMaxTextLength);
		} while(sourceExists(l)); // if the label exists, regenerate
		return l;
	}

	// Change the label of this input, if the label doesn't exist already.
	// Return whether the label was updated.
	bool updateLabel(std::string lbl) {
		if(lbl.empty() || sourceExists(lbl)) {
			return false;
		}
		sources.erase(label);
		label = lbl;
		addSource(this);
		return true;
	}

	TeleportInModule() : Teleport(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		assert(NUM_INPUTS == NUM_TELEPORT_INPUTS);
		label = getLabel();
		addSource(this);
	}

	~TeleportInModule() {
		sources.erase(label);
	}


	// step() is not needed for a teleport source, values are read directly from map

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
	json_t* dataToJson() override {
		json_t *data = json_object();
		json_object_set_new(data, "label", json_string(label.c_str()));
		return data;
	}

	void dataFromJson(json_t* root) override {
		json_t *label_json = json_object_get(root, "label");
		if(json_is_string(label_json)) {
			// remove previous label randomly generated in constructor
			sources.erase(label);
			label = std::string(json_string_value(label_json));
			if(sourceExists(label)) {
				// Label already exists in sources, this means that dataFromJson()
				// was called due to duplication instead of loading from file.
				// Generate new label.
				label = getLabel();
			}
		} else {
			// label couldn't be read from json for some reason, generate new one
			label = getLabel();
		}

		addSource(this);

	}

};

struct TeleportOutModule : Teleport {

	bool sourceIsValid;

	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_1,
		OUTPUT_2,
		OUTPUT_3,
		OUTPUT_4,
		OUTPUT_5,
		OUTPUT_6,
		OUTPUT_7,
		OUTPUT_8,
		NUM_OUTPUTS
	};
	enum LightIds {
		OUTPUT_1_LIGHTG,
		OUTPUT_1_LIGHTR,
		OUTPUT_2_LIGHTG,
		OUTPUT_2_LIGHTR,
		OUTPUT_3_LIGHTG,
		OUTPUT_3_LIGHTR,
		OUTPUT_4_LIGHTG,
		OUTPUT_4_LIGHTR,
		OUTPUT_5_LIGHTG,
		OUTPUT_5_LIGHTR,
		OUTPUT_6_LIGHTG,
		OUTPUT_6_LIGHTR,
		OUTPUT_7_LIGHTG,
		OUTPUT_7_LIGHTR,
		OUTPUT_8_LIGHTG,
		OUTPUT_8_LIGHTR,

		NUM_LIGHTS
	};

	TeleportOutModule() : Teleport(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		assert(NUM_OUTPUTS == NUM_TELEPORT_INPUTS);
		if(sources.size() > 0) {
			if(sourceExists(lastInsertedKey)) {
				label = lastInsertedKey;
			} else {
				// the lastly added input doesn't exist anymore,
				// pick first input in alphabetical order
				label = sources.begin()->first;
			}
			sourceIsValid = true;
		} else {
			label = "";
			sourceIsValid = false;
		}
	}

	void process(const ProcessArgs &args) override {

		if(sourceExists(label)){
			TeleportInModule *src = sources[label];
			for(int i = 0; i < NUM_TELEPORT_INPUTS; i++) {
				Input input = src->inputs[TeleportInModule::INPUT_1 + i];
				outputs[OUTPUT_1 + i].setVoltage(input.value);
				lights[OUTPUT_1_LIGHTG + 2*i].setBrightness( input.active);
				lights[OUTPUT_1_LIGHTR + 2*i].setBrightness(!input.active);
			}
			sourceIsValid = true;
		} else {
			for(int i = 0; i < NUM_TELEPORT_INPUTS; i++) {
				outputs[OUTPUT_1 + i].setVoltage(0.f);
				lights[OUTPUT_1_LIGHTG + 2*i].setBrightness(0.f);
				lights[OUTPUT_1_LIGHTR + 2*i].setBrightness(0.f);
			}
			sourceIsValid = false;
		}
	};

	json_t* dataToJson() override {
		json_t *data = json_object();
		json_object_set_new(data, "label", json_string(label.c_str()));
		return data;
	}

	void dataFromJson(json_t* root) override {
		json_t *label_json = json_object_get(root, "label");
		if(json_is_string(label_json)) {
			label = json_string_value(label_json);
		}
	}
};

void Teleport::addSource(TeleportInModule *t) {
	std::string key = t->label;
	sources[key] = t;
	lastInsertedKey = key;
}


////////////////////////////////////
// some teleport-specific widgets //
////////////////////////////////////

struct TeleportLabelDisplay {
	NVGcolor errorTextColor = nvgRGB(0xd8, 0x0, 0x0);
};

struct EditableTeleportLabelTextbox : EditableTextBox, TeleportLabelDisplay {
	TeleportInModule *module;
	std::string errorText = "!err";
	GUITimer errorDisplayTimer;
	float errorDuration = 3.f;

	EditableTeleportLabelTextbox(TeleportInModule *m): EditableTextBox() {
		assert(errorText.size() <= maxTextLength);
		module = m;
	}

	void onDeselect(const event::Deselect &e) override {
		if(module->updateLabel(TextField::text) || module->label.compare(TextField::text) == 0) {
			errorDisplayTimer.reset();
		} else {
			errorDisplayTimer.trigger(errorDuration);
		}

		isFocused = false;
		e.consume(NULL);
	}

	void step() override {
		EditableTextBox::step();
		if(!module) return; //TODO: add dummy display
		if(errorDisplayTimer.process()) {
			textColor = isFocused ? defaultTextColor : errorTextColor;
			HoverableTextBox::setText(errorText);
		} else {
			textColor = defaultTextColor;
			HoverableTextBox::setText(module->label);
			if(!isFocused) {
				TextField::setText(module->label);
			}
		}
	}

};

struct TeleportLabelMenuItem : MenuItem {
	TeleportOutModule *module;
	std::string label;
	void onAction(const event::Action &e) override {
		module->label = label;
	}
};

struct TeleportSourceSelectorTextBox : HoverableTextBox, TeleportLabelDisplay {
	TeleportOutModule *module;

	TeleportSourceSelectorTextBox() : HoverableTextBox() {}

	void onAction(const event::Action &e) override {
		// based on AudioDeviceChoice::onAction in src/app/AudioWidget.cpp
		Menu *menu = createMenu();
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Select source"));

		{
			TeleportLabelMenuItem *item = new TeleportLabelMenuItem();
			item->module = module;
			item->label = "";
			item->text = "(none)";
			item->rightText = CHECKMARK(module->label.empty());
			menu->addChild(item);
		}

		if(!module->sourceIsValid && !module->label.empty()) {
			// the source of the module doesn't exist, it shouldn't appear in sources, so display it as unavailable
			TeleportLabelMenuItem *item = new TeleportLabelMenuItem();
			item->module = module;
			item->label = module->label;
			item->text = module->label;
			item->text += " (missing)";
			item->rightText = CHECKMARK("true");
			menu->addChild(item);
		}

		auto src = module->sources;
		for(auto it = src.begin(); it != src.end(); it++) {
			TeleportLabelMenuItem *item = new TeleportLabelMenuItem();
			item->module = module;
			item->label = it->first;
			item->text = it->first;
			item->rightText = CHECKMARK(item->label == module->label);
			menu->addChild(item);
		}
	}

	void onButton(const event::Button &e) override {
		HoverableTextBox::onButton(e);
		bool l = e.button == GLFW_MOUSE_BUTTON_LEFT;
		bool r = e.button == GLFW_MOUSE_BUTTON_RIGHT;
		if(e.action == GLFW_PRESS && (l || r)) {
			event::Action eAction;
			onAction(eAction);
			e.consume(this);
		}
	}

	void step() override {
		HoverableTextBox::step();
		if(!module) return; //TODO: dummy value
		setText(module->label);
		textColor = module->sourceIsValid ? defaultTextColor : errorTextColor;
	}

};


////////////////////
// module widgets //
////////////////////

struct TeleportModuleWidget : ModuleWidget {
	HoverableTextBox *labelDisplay;
	Teleport *module;

	virtual void addLabelDisplay(HoverableTextBox *disp) {
		disp->font_size = 14;
		disp->box.size = Vec(30, 14);
		disp->textOffset.x = disp->box.size.x * 0.5f;
		disp->box.pos = Vec(7.5f, RACK_GRID_WIDTH + 7.0f);
		labelDisplay = disp;
		addChild(labelDisplay);
	}

	float getPortYCoord(int i) {
		return 57.f + 37.f * i;
	}

	TeleportModuleWidget(Teleport *module, std::string panelFilename) {
		setModule(module);
		this->module = module;
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, panelFilename)));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	}
};


struct TeleportInModuleWidget : TeleportModuleWidget {

	TeleportInModuleWidget(TeleportInModule *module) : TeleportModuleWidget(module, "res/TeleportIn.svg") {
		addLabelDisplay(new EditableTeleportLabelTextbox(module));
		for(int i = 0; i < NUM_TELEPORT_INPUTS; i++) {
			addInput(createInputCentered<PJ301MPort>(Vec(22.5, getPortYCoord(i)), module, TeleportInModule::INPUT_1 + i));
		}
	}

};


struct TeleportOutModuleWidget : TeleportModuleWidget {
	TeleportSourceSelectorTextBox *labelDisplay;

	TeleportOutModuleWidget(TeleportOutModule *module) : TeleportModuleWidget(module, "res/TeleportOut.svg") {
		labelDisplay = new TeleportSourceSelectorTextBox();
		labelDisplay->module = module;
		addLabelDisplay(labelDisplay);

		for(int i = 0; i < NUM_TELEPORT_INPUTS; i++) {
			float y = getPortYCoord(i);
			addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, y), module, TeleportOutModule::OUTPUT_1 + i));
			addChild(createTinyLightForPort<GreenRedLight>(Vec(22.5, y), module, TeleportOutModule::OUTPUT_1_LIGHTG + 2*i));
		}
		//addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 135), module, TeleportOutModule::OUTPUT_1));
	}

};


Model *modelTeleportInModule = createModel<TeleportInModule, TeleportInModuleWidget>("TeleportIn");
Model *modelTeleportOutModule = createModel<TeleportOutModule, TeleportOutModuleWidget>("TeleportOut");
