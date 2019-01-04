#include "LittleUtils.hpp"
#include "Teleport.hpp"
#include "Widgets.hpp"
#include "Util.hpp"

#include <iostream>

/////////////
// modules //
/////////////

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
		//INPUT_1_LIGHT,
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

	TeleportInModule() : Teleport(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		assert(NUM_INPUTS == NUM_TELEPORT_INPUTS);
		label = getLabel();
		addSource(this);
	}

	~TeleportInModule() {
		//std::cout << "~TeleportInModule(): " << label << std::endl;
		sources.erase(label);
	}


	// step() is not needed for a teleport source

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
	json_t* toJson() override {
		json_t *data = json_object();
		json_object_set_new(data, "label", json_string(label.c_str()));
		return data;
	}

	void fromJson(json_t* root) override {
		json_t *label_json = json_object_get(root, "label");
		if(json_is_string(label_json)) {
			// remove previous label randomly generated in constructor
			sources.erase(label);
			label = std::string(json_string_value(label_json));
			if(sourceExists(label)) {
				// Label already exists in sources, this means that fromJson()
				// was called due to duplication instead of loading from file.
				// Generate new label.
				label = getLabel();
			}
		} else {
			//std::cout << "error reading label" << std::endl;
			// label couldn't be read from json for some reason, generate new one
			label = getLabel();
		}

		addSource(this);

	}

};

struct TeleportOutModule : Teleport {
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
		//STATUS_LIGHTG,
		//STATUS_LIGHTR,

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
		} else {
			label = "";
		}
	}

	void step() override {

		if(sourceExists(label)){
			TeleportInModule *src = sources[label];
			for(int i = 0; i < NUM_TELEPORT_INPUTS; i++) {
				Input input = src->inputs[TeleportInModule::INPUT_1 + i];
				outputs[OUTPUT_1 + i].value = input.value;
				lights[OUTPUT_1_LIGHTG + 2*i].setBrightness( input.active);
				lights[OUTPUT_1_LIGHTR + 2*i].setBrightness(!input.active);
			}
		} else {
			//TODO: don't set label to empty, but indicate somehow that no input exists (gray out text? make text red? status LED?)
			label = "";
			for(int i = 0; i < NUM_TELEPORT_INPUTS; i++) {
				outputs[OUTPUT_1 + i].value = 0.f;
				lights[OUTPUT_1_LIGHTG + 2*i].setBrightness(0.f);
				lights[OUTPUT_1_LIGHTR + 2*i].setBrightness(0.f);
			}

		}
	};

	json_t* toJson() override {
		json_t *data = json_object();
		json_object_set_new(data, "label", json_string(label.c_str()));
		return data;
	}

	void fromJson(json_t* root) override {
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

//struct EditableTeleportLabelDisplay : HoverableTextBox, TextField {
//	EditableTeleportLabelDisplay() : HoverableTextBox(), TextField() {};
//
//	void draw(NVGcontext *vg) override {
//		HoverableTextBox::draw(vg);
//
//		// copied from LedDisplayTextField
//		//TODO: set multiline = false
//		NVGcolor highlightColor = nvgRGB(0xd8, 0x0, 0x0);
//		highlightColor.a = 0.5;
//		int begin = min(cursor, selection);
//		int end = (this == gFocusedWidget) ? max(cursor, selection) : -1; //TODO: use this to figure out defocus
//		bndIconLabelCaret(vg, textOffset.x, textOffset.y,
//			box.size.x - 2*textOffset.x, box.size.y - 2*textOffset.y, //TODO: this incorrectly limits line
//			-1, textColor, 12, TextField::text.c_str(), highlightColor, begin, end);
//	}
//
//	void onMouseDown(EventMouseDown &e) override {
//		std::cout << "EditableTeleportLabelDisplay: onMouseDown()" << std::endl; //TODO: remove
//		TextField::onMouseDown(e);
//	}
//
//	void onMouseUp(EventMouseUp &e) override {
//		TextField::onMouseUp(e);
//	}
//
//	void onMouseMove(EventMouseMove &e) override {
//		TextField::onMouseMove(e);
//	}
//
//	void onScroll(EventScroll &e) override {
//		TextField::onScroll(e);
//	}
//
//	void onTextChange() override {
//		std::cout << "onTextChange() called implementation missing" << std::endl;
//	}
//
//	void onAction(EventAction &e) override {
//		std::cout << "onAction()" << std::endl;
//		TextField::onAction(e);
//	}
//
//};

////////////////////////////////////
// some teleport-specific widgets //
////////////////////////////////////

struct EditableTeleportLabelTextbox : EditableTextBox {
	EditableTeleportLabelTextbox(): EditableTextBox() {}

	void onDefocus() override {
		//TODO
		std::cout << "EditableTeleportLabelTextbox::onDefocus()" << std::endl;
		EditableTextBox::onDefocus();
	}

};

struct TeleportLabelMenuItem : MenuItem {
	TeleportOutModule *module;
	std::string label;
	void onAction(EventAction &e) override {
		module->label = label;
	}
};

struct TeleportSourceSelectorTextBox : HoverableTextBox {
	TeleportOutModule *module;

	TeleportSourceSelectorTextBox() : HoverableTextBox() {}

	void onAction(EventAction &e) override {
		// based on AudioDeviceChoice::onAction in src/app/AudioWidget.cpp
		Menu *menu = gScene->createMenu();
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Select source"));

		{
			TeleportLabelMenuItem *item = new TeleportLabelMenuItem();
			item->module = module;
			item->label = "";
			item->text = "(none)";
			item->rightText = CHECKMARK(module->label.empty());
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

	void onMouseDown(EventMouseDown &e) override {
		if(e.button == 0 || e.button == 1) {
			EventAction eAction;
			onAction(eAction);
			e.consumed = true;
			e.target = this;
		}
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
		disp->box.pos = Vec(7.5f, RACK_GRID_WIDTH + 7.5f);
		disp->setText(module->label);
		labelDisplay = disp;
		addChild(labelDisplay);
	}

	float getPortYCoord(int i) {
		return 75.f + 35.f * i;
	}

	TeleportModuleWidget(Teleport *module, std::string panelFilename) : ModuleWidget(module) {
		this->module = module;
		setPanel(SVG::load(assetPlugin(plugin, panelFilename)));

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	}
	virtual void step() override {
		ModuleWidget::step();
		//TODO: don't do this on every step (?)
		labelDisplay->setText(module->label);
	}
};


struct TeleportInModuleWidget : TeleportModuleWidget {

	//TODO: editable text box ?

	TeleportInModuleWidget(TeleportInModule *module) : TeleportModuleWidget(module, "res/TeleportIn.svg") {
		//addLabelDisplay(new HoverableTextBox());
		//addLabelDisplay(new EditableTeleportLabelDisplay());
		addLabelDisplay(new EditableTeleportLabelTextbox());
		//addInput(createInputCentered<PJ301MPort>(Vec(22.5, 135), module, TeleportInModule::INPUT_1));
		for(int i = 0; i < NUM_TELEPORT_INPUTS; i++) {
			addInput(createInputCentered<PJ301MPort>(Vec(22.5, getPortYCoord(i)), module, TeleportInModule::INPUT_1 + i));

		}
	}

};


struct TeleportOutModuleWidget : TeleportModuleWidget {
	TeleportSourceSelectorTextBox *labelDisplay;

	//TODO: svg file
	TeleportOutModuleWidget(TeleportOutModule *module) : TeleportModuleWidget(module, "res/TeleportIn.svg") {
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

// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelTeleportInModule = Model::create<TeleportInModule, TeleportInModuleWidget>("Little Utils", "TeleportIn", "Teleport In", UTILITY_TAG);

Model *modelTeleportOutModule = Model::create<TeleportOutModule, TeleportOutModuleWidget>("Little Utils", "TeleportOut", "Teleport Out", UTILITY_TAG);
