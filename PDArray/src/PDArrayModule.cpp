#include "PDArray.hpp"
#include "window.hpp" // windowIsModPressed
#include <GLFW/glfw3.h> // key codes

#include <iostream>

//TODO: input range right click menu
//TODO: output range right click menu
//TODO: preiodic interp right click menu
//TODO: reinitialize buffer with onInitialize()

struct PDArrayModule : Module {
	enum ParamIds {
		PITCH_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PHASE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		DIRECT_OUTPUT, //TODO: rename
		INTERP_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT,
		NUM_LIGHTS
	};

	float phase = 0.f;
	unsigned int size = 10;
	bool periodicInterpolation = true; // TODO: implement in GUI
	//float miny, maxy; // output value scale TODO
	std::vector<float> buffer;

	PDArrayModule() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		resizeBuffer(size);

		//TODO: remove
		buffer.clear();
		for(int i = 0; i < size; i++) {
			//buffer.push_back(10.f * sin(i*0.2f*3.14159f));
			buffer.push_back(20.f * i / (size - 1) - 10.f);
		}
		//buffer.push_back(10.f);
		//buffer.push_back(10.f);
		//buffer.push_back(10.f);
		//buffer.push_back(10.f);
		//buffer.push_back(10.f);
		//buffer.push_back(-10.f);
		//buffer.push_back(-10.f);
		//buffer.push_back(-10.f);
		//buffer.push_back(-10.f);
		//buffer.push_back(-10.f);
		//std::cout << "PDArrayModule(): " << this << std::endl;
	}

	void step() override;
	void resizeBuffer(unsigned int newSize) {
		buffer.resize(newSize, 0.f);
		size = newSize;
	}

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

	// TODO: store array data, if array is large enough (how large?) encode as base64?
	//void fromJson(json_t *json) override {
	//}
};

void PDArrayModule::step() {
	// Implement a simple sine oscillator
	float deltaTime = engineGetSampleTime();

	// if phase input is inactive, keep phase (TODO: good idea?).
	//if(inputs[PHASE_INPUT].active)
		phase = rescale(clamp(inputs[PHASE_INPUT].value, 0.f, 10.f), 0.f, 10.f, 0.f, 1.f);

	int i = int(phase * size); //TODO: clamp?

	outputs[DIRECT_OUTPUT].value = buffer[i];


	// interpolation based on tabread4_tilde_perform() in
	// https://github.com/pure-data/pure-data/blob/master/src/d_array.c
	float a, b, c, d;
	// TODO: add right-click menu option to toggle periodic interpolation
	if(periodicInterpolation) {
		a = buffer[(i - 1 + size) % size];
		b = buffer[i];
		c = buffer[(i + 1) % size];
		d = buffer[(i + 2) % size];
	} else {
		a = buffer[clamp(i - 1, 0, size)];
		b = buffer[clamp(i - 0, 0, size)];
		c = buffer[clamp(i + 1, 0, size)];
		d = buffer[clamp(i + 2, 0, size)];
	}

	float frac = phase * size - i; // fractional part of phase
	outputs[INTERP_OUTPUT].value = b + frac * (
			c - b - 0.1666667f * (1.f - frac) * (
				(d - a - 3.f * (c - b)) * frac + (d + 2.f * a - 3.f * b)
			)
		);

}

struct ArrayDisplay : OpaqueWidget {
	PDArrayModule *module;
	Vec dragPosition;
	bool dragging = false;

	ArrayDisplay(PDArrayModule *module): OpaqueWidget() {
		this->module = module;
		box.size = Vec(150, 100);
	}

	void draw(NVGcontext *vg) override {
		OpaqueWidget::draw(vg);

		// show phase
		float px =  module->phase * box.size.x;
		nvgBeginPath(vg);
		nvgStrokeWidth(vg, 1.f);
		nvgStrokeColor(vg, nvgRGB(0x23, 0x23, 0x23));
		nvgMoveTo(vg, px, 0);
		nvgLineTo(vg, px, box.size.y);
		nvgStroke(vg);

		int s = module->size;
		float w = box.size.x * 1.f / s;
		nvgBeginPath(vg);
		for(int i = 0; i < s; i++) {
			float x1 = i * w;
			float x2 = (i + 1) * w;
			//float y = (rescale(module->buffer[i], -10.f, 10.f, 1.f, 0.f) * 0.9f  + 0.05f) * box.size.y;
			float y = rescale(module->buffer[i], -10.f, 10.f, 1.f, 0.f) * box.size.y;

			if(i == 0) nvgMoveTo(vg, x1, y);
			else nvgLineTo(vg, x1, y);

			nvgLineTo(vg, x2, y);
		}
		nvgStrokeWidth(vg, 2.f);
		nvgStrokeColor(vg, nvgRGB(0x0, 0x0, 0x0));
		nvgStroke(vg);

		nvgBeginPath(vg);
		nvgStrokeColor(vg, nvgRGB(0x0, 0x0, 0x0));
		nvgStrokeWidth(vg, 2.f);
		nvgRect(vg, 0, 0, box.size.x, box.size.y);
		nvgStroke(vg);

	}

	void onMouseDown(EventMouseDown &e) override {
		OpaqueWidget::onMouseDown(e);
		dragPosition = e.pos;
	}

	void onDragStart(EventDragStart &e) override {
		OpaqueWidget::onDragStart(e);
		dragging = true;
	}

	void onDragEnd(EventDragEnd &e) override {
		OpaqueWidget::onDragEnd(e);
		dragging = false;
	}

	void onDragMove(EventDragMove &e) override {
		OpaqueWidget::onDragMove(e);
		//TODO: this results in erroneous behavior, use gRackWidget.lastMousePos? (then needs conversion to local coordinate frame). See e.g.
		// https://github.com/jeremywen/JW-Modules/blob/master/src/XYPad.cpp
		dragPosition = dragPosition.plus(e.mouseRel);

		// int() rounds down, so the upper limit of rescale will be module->size without -1.
		int i = clamp(int(rescale(dragPosition.x, 0, box.size.x, 0, module->size)), 0, module->size - 1);
		float y = clamp(rescale(dragPosition.y, 0, box.size.y, 10.f, -10.f), -10.f, 10.f);
		module->buffer[i] = y;
	}

	void step() override {
		OpaqueWidget::step();
	}
};


// TextField that only allows inputting numbers
struct NumberTextField : TextField {
	int maxCharacters = 6;
	PDArrayModule *module;
	std::string validText = "1";

	NumberTextField(PDArrayModule *m) : TextField() {
		module = m;
		validText = stringf("%u", module->size);
		text = validText;
	};

	bool isNumber(const std::string& s) {
		// shamelessly copypasted from https://stackoverflow.com/questions/4654636/how-to-determine-if-a-string-is-a-number-with-c
		std::string::const_iterator it = s.begin();
		while (it != s.end() && std::isdigit(*it)) ++it;
		return !s.empty() && it == s.end();
	}

	void onAction(EventAction &e) {
		if(text.size() > 0) {
			int n = stoi(text); // text should always contain only digits
			if(n > 0) {
				validText = stringf("%u", n);
				module->resizeBuffer(n);
			}
		}
		text = validText;
		if(gFocusedWidget == this) gFocusedWidget = NULL;
		e.consumed = true;
	}

	void onKey(EventKey &e) override {
		if(e.key == GLFW_KEY_V && windowIsModPressed()) {
			// prevent pasting too long text
			int pasteLength = maxCharacters - TextField::text.size();
			if(pasteLength > 0) {
				std::string newText(glfwGetClipboardString(gWindow));
				if(newText.size() > pasteLength) newText.erase(pasteLength);
				if(isNumber(newText)) insertText(newText);
			}
			e.consumed = true;

		} else if(e.key == GLFW_KEY_ESCAPE && (gFocusedWidget == this)) {
			// same as pressing enter
			EventAction ee;
			onAction(ee);

		} else {
			TextField::onKey(e);
		}
	}

	void onText(EventText &e) override {
		// assuming GLFW number keys are contiguous
		if(text.size() < maxCharacters
				&& GLFW_KEY_0  <= e.codepoint
				&& e.codepoint <= GLFW_KEY_9) {
			TextField::onText(e);
		}
	}

};

struct PDArrayModuleWidget : ModuleWidget {
	ArrayDisplay *display;
	NumberTextField *sizeSelector;

	PDArrayModuleWidget(PDArrayModule *module) : ModuleWidget(module) {

		setPanel(SVG::load(assetPlugin(plugin, "res/Array.svg"))); //TODO

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		//addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		//addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(Port::create<PJ301MPort>(Vec(10.f, 50), Port::INPUT, module, PDArrayModule::PHASE_INPUT));
		addOutput(Port::create<PJ301MPort>(Vec(10.f, 100), Port::OUTPUT, module, PDArrayModule::DIRECT_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(10.f, 150), Port::OUTPUT, module, PDArrayModule::INTERP_OUTPUT));

		display = new ArrayDisplay(module);
		display->box.pos = Vec(50, 10);
		addChild(display);

		sizeSelector = new NumberTextField(module);
		sizeSelector->box.pos = Vec(50, 120);
		sizeSelector->box.size.x = 50;
		addChild(sizeSelector);
	}

	//void draw(NVGcontext *vg) override {
	//	ModuleWidget::draw(vg);
	//}
};

Model *modelPDArray = Model::create<PDArrayModule, PDArrayModuleWidget>("PDArray", "PDArray", "Array", VISUAL_TAG, ENVELOPE_GENERATOR_TAG);
