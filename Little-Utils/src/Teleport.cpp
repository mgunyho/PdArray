#include "LittleUtils.hpp"
#include "Teleport.hpp"
#include "Widgets.hpp"
#include "Util.hpp"

#include <iostream>

//TODO: why don't LED's on wires get updated?

struct TeleportInModule : Teleport {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		INPUT_1,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		//INPUT_1_LIGHT,
		NUM_LIGHTS
	};

	// Generate random, unique label for this teleport endpoint. Don't modify the buffer.
	std::string getLabel() {
		std::string l;
		do {
			l = randomString(LABEL_LENGTH);
		} while(buffer.find(l) != buffer.end()); // if the label exists, regenerate
		return l;
	}

	TeleportInModule() : Teleport(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		label = getLabel();
		//buffer.insert(std::pair<std::string, float>(label, 0.f));
		addToBuffer(label, 0.f);
	}

	~TeleportInModule() {
		//std::cout << "~TeleportInModule(): " << label << std::endl;
		buffer.erase(label);
	}


	void step() override;

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
			buffer.erase(label);
			label = std::string(json_string_value(label_json));
			if(buffer.find(label) != buffer.end()) {
				// Label already exists in buffer, this means that fromJson()
				// was called due to duplication instead of loading from file.
				// Generate new label.
				label = getLabel();
			}
		} else {
			//std::cout << "error reading label" << std::endl;
			// label couldn't be read from json for some reason, generate new one
			label = getLabel();
		}

		addToBuffer(label, 0.f);

		//std::cout << "fromJson(): keys (" << buffer.size() << "):" << std::endl;
		//for(auto it = buffer.begin(); it != buffer.end(); it++) {
		//	std::cout << it->first << std::endl;
		//}
	}

};


void TeleportInModule::step() {
	//float deltaTime = engineGetSampleTime();
	// buffer[label] should exist
	buffer[label] = inputs[INPUT_1].value;
}


struct TeleportOutModule : Teleport {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_1,
		NUM_OUTPUTS
	};
	enum LightIds {
		//INPUT_1_LIGHT,
		NUM_LIGHTS
	};

	TeleportOutModule() : Teleport(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		if(buffer.size() > 0) {
			if(buffer.find(lastInsertedKey) != buffer.end()) {
				label = lastInsertedKey;
			} else {
				// the lastly added input doesn't exist anymore,
				// pick first input in alphabetical order
				label = buffer.begin()->first;
			}
		} else {
			label = "";
		}
	}

	void step() override {
		//TODO: pick label from dropdown menu (done in widget?)

		if(buffer.find(label) != buffer.end()) {
			//outputs[OUTPUT_1].value = buffer[label];
		} else {
			//TODO: don't set label to empty, but indicate somehow that no input exists (gray out text? make text red?)
			label = "";
		}
		outputs[OUTPUT_1].value = label.empty() ? 0.f : buffer[label];
	};


	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
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

template <typename LabelDisplayType = TextBox>
struct TeleportModuleWidget : ModuleWidget {
	TextBox *labelDisplay;
	Teleport *module;

	TeleportModuleWidget(Teleport *module, std::string panelFilename) : ModuleWidget(module) {
	//TeleportModuleWidget(Teleport *module) : ModuleWidget(module) {
		this->module = module;
		setPanel(SVG::load(assetPlugin(plugin, panelFilename)));

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		labelDisplay = new LabelDisplayType();
		labelDisplay->font_size = 14;
		labelDisplay->box.size = Vec(30, 14);
		labelDisplay->textOffset.x = labelDisplay->box.size.x * 0.5f;
		labelDisplay->box.pos = Vec(7.5f, RACK_GRID_WIDTH + 7.5f);
		labelDisplay->setText(module->label);
		addChild(labelDisplay);

	}
	virtual void step() override {
		//TODO: just for debugging, don't do this on every step (?)
		labelDisplay->setText(module->label);
	}
};

struct TeleportInModuleWidget : TeleportModuleWidget<> {

	//TODO: editable text box ?

	TeleportInModuleWidget(TeleportInModule *module) : TeleportModuleWidget(module, "res/TeleportIn.svg") {
		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 135), module, TeleportInModule::INPUT_1));
	}

};

struct TeleportLabelMenuItem : MenuItem {
	TeleportOutModule *module;
	std::string label;
	void onAction(EventAction &e) override {
		module->label = label;
	}
};

struct TeleportLabelSelectorTextBox : TextBox {
	TeleportOutModule *module;

	TeleportLabelSelectorTextBox() : TextBox() {}

	//TODO: SEGFAULTS
	void onAction(EventAction &e) override {
		// based on AudioDeviceChoice::onAction in src/app/AudioWidget.cpp
		Menu *menu = gScene->createMenu();
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Select input"));

		std::cout << module << std::endl;
		auto buf = module->buffer;
		for(auto it = buf.begin(); it != buf.end(); it++) {
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

struct TeleportOutModuleWidget : TeleportModuleWidget<TeleportLabelSelectorTextBox> {
	//TextBox *labelDisplay;
	TeleportLabelSelectorTextBox *labelDisplay;
	//TeleportOutModule *module;

	//TODO: svg file
	TeleportOutModuleWidget(TeleportOutModule *module) : TeleportModuleWidget(module, "res/TeleportIn.svg") {
		// TODO: is this hacky?
		// if not casting like this, labelDisplay will not have ->module for some reason
		this->labelDisplay = (TeleportLabelSelectorTextBox *) TeleportModuleWidget::labelDisplay;
		this->labelDisplay->module = module;

		//TODO: add LED which indicates active inputs on the other end
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 135), module, TeleportOutModule::OUTPUT_1));
	}

	//void step() override {
	//	labelDisplay->setText(module->label);
	//}
};

// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelTeleportInModule = Model::create<TeleportInModule, TeleportInModuleWidget>("Little Utils", "TeleportIn", "Teleport In", UTILITY_TAG);

Model *modelTeleportOutModule = Model::create<TeleportOutModule, TeleportOutModuleWidget>("Little Utils", "TeleportOut", "Teleport Out", UTILITY_TAG);
