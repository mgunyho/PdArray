#include "PDArray.hpp"

#include <iostream>

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

	//std::cout
	//	<< "phase: " << phase
	//	<< ", i: " << i
	//	<< ", frac: " << frac
	//	<< std::endl;

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

struct PDArrayModuleWidget : ModuleWidget {
	ArrayDisplay *display;

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
	}

	//void draw(NVGcontext *vg) override {
	//	ModuleWidget::draw(vg);
	//}
};

Model *modelPDArray = Model::create<PDArrayModule, PDArrayModuleWidget>("PDArray", "PDArray", "Array", VISUAL_TAG, ENVELOPE_GENERATOR_TAG);
