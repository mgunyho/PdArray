#include "LittleUtils.hpp"
#include "Teleport.hpp"
#include "Widgets.hpp"
#include "Util.hpp"

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
		buffer[label] = 0.f;
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
			buffer[label] = 0.f;
		} else {
			//std::cout << "error reading label" << std::endl;
			label = getLabel();
			buffer[label] = 0.f;
		}
		//std::cout << "fromJson(): keys (" << buffer.size() << "):" << std::endl;
		//for(auto it = buffer.begin(); it != buffer.end(); it++) {
		//	std::cout << it->first << std::endl;
		//}
	}

};


void TeleportInModule::step() {
	//float deltaTime = engineGetSampleTime();
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
		label = buffer.begin()->first;
	}

	void step() override {
		//TODO: pick label from dropdown menu (done in widget?)

		if(buffer.find(label) != buffer.end()) {
			//outputs[OUTPUT_1].value = buffer[label];
		} else {
			//outputs[OUTPUT_1].value = 0.f;
			if(buffer.size() > 0) {
				//TODO: set label to empty, don't do this
				label = buffer.begin()->first;
			} else {
				label = "";
			}
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

struct TeleportModuleWidget : ModuleWidget {
	TextBox *labelDisplay;
	Teleport *module;
	//const std::string panelFile; // = "res/TeleportIn.svg";
	//virtual std::string panelFilename() = 0; // { return "res/ButtonModule.svg"; }

	std::string panelFilename;

	TeleportModuleWidget(Teleport *module, std::string panelFilename) : ModuleWidget(module) {
	//TeleportModuleWidget(Teleport *module) : ModuleWidget(module) {
		this->module = module;
		setPanel(SVG::load(assetPlugin(plugin, panelFilename)));

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		labelDisplay = new TextBox();
		labelDisplay->font_size = 14;
		labelDisplay->box.size = Vec(30, 14);
		labelDisplay->textOffset.x = labelDisplay->box.size.x * 0.5f;
		labelDisplay->box.pos = Vec(7.5f, RACK_GRID_WIDTH + 7.5f);
		labelDisplay->setText(module->label);
		addChild(labelDisplay);

	}
	void step() override {
		//TODO: just for debugging, don't do this on every step (?)
		labelDisplay->setText(module->label);
	}
};

struct TeleportInModuleWidget : TeleportModuleWidget {

	//TODO: replace with editable text box
	//TextBox *labelDisplay;
	//TeleportInModule *module;

	//std::string panelFilename() override { return "res/TeleportIn.svg"; }
	//std::string panelFilename = "res/TeleportIn.svg";

	TeleportInModuleWidget(TeleportInModule *module) : TeleportModuleWidget(module, "res/TeleportIn.svg") {
	//TeleportInModuleWidget(TeleportInModule *module) : TeleportModuleWidget(module) {
		//setPanel(SVG::load(assetPlugin(plugin, "res/TeleportIn.svg")));
		//panelFilename = "res/TeleportIn.svg";
		//TeleportModuleWidget
		//this->module = module;
		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 135), module, TeleportInModule::INPUT_1));
	}

};


struct TeleportOutModuleWidget : TeleportModuleWidget {
	//TextBox *labelDisplay;
	//TeleportOutModule *module;
	//std::string panelFilename() override { return "res/Gate.svg"; }
	//std::string panelFilename = "res/Gate.svg";

	TeleportOutModuleWidget(TeleportOutModule *module) : TeleportModuleWidget(module, "res/PulseGenerator.svg") {
	//TeleportOutModuleWidget(TeleportOutModule *module) : TeleportModuleWidget(module) {
		//setPanel(SVG::load(assetPlugin(plugin, "res/PulseGenerator.svg")));
		//this->module = module;
		//setPanel(SVG::load(assetPlugin(plugin, "res/TeleportIn.svg"))); //TODO

		//addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		//addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		////TODO: unify module widgets in one superclass to prevent repetition
		//labelDisplay = new TextBox();
		//labelDisplay->font_size = 14;
		//labelDisplay->box.size = Vec(30, 14);
		//labelDisplay->textOffset.x = labelDisplay->box.size.x * 0.5f;
		//labelDisplay->box.pos = Vec(7.5f, RACK_GRID_WIDTH + 7.5f);
		//labelDisplay->setText(module->label);
		//addChild(labelDisplay);

		//TODO: add LED which indicates active inputs on the other end
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 135), module, TeleportOutModule::OUTPUT_1));
	}
};

// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelTeleportInModule = Model::create<TeleportInModule, TeleportInModuleWidget>("Little Utils", "TeleportIn", "Teleport In", UTILITY_TAG);

Model *modelTeleportOutModule = Model::create<TeleportOutModule, TeleportOutModuleWidget>("Little Utils", "TeleportOut", "Teleport Out", UTILITY_TAG);
