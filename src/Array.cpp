#include "plugin.hpp"
#include "window.hpp" // windowIsModPressed
#include "osdialog.h"
#include "settings.hpp" // settings::*
#include "random.hpp"
#include <GLFW/glfw3.h> // key codes
#include <algorithm> // std::min, std::swap
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h" // for reading wav files

#include "Widgets.hpp"

#include <iostream>

//TODO: load buffer from text/csv file?
//TODO: prevent audio clicking at the last sample
//TODO: undo history? hard? memory intensive?
//TODO: visual representation choice right-click submenu (stairs (current), lines, points, bars)
//TODO: reinterpolate array on resize (+right-click menu option for that)

struct RangeParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		if(!module) return "";
		float v = getDisplayValue();
		if(v > 1.5f) {
			return "0..10";
		} else if (v > 0.5f) {
			return "-5..5";
		} else {
			return "-10..10";
		}
	}
};

struct Array : Module {
	enum ParamIds {
		PHASE_RANGE_PARAM,
		OUTPUT_RANGE_PARAM,
		REC_ENABLE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PHASE_INPUT,
		REC_SIGNAL_INPUT,
		REC_PHASE_INPUT,
		REC_ENABLE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		STEP_OUTPUT,
		INTERP_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		REC_LIGHT,
		NUM_LIGHTS
	};

	enum InterpBoundaryMode {
		INTERP_CONSTANT,
		INTERP_MIRROR,
		INTERP_PERIODIC,
		NUM_INTERP_MODES
	};

	enum RecordingMode {
		GATE,
		TOGGLE,
		NUM_REC_MODES
	};

	enum DataSaveMode {
		SAVE_FULL_DATA,
		SAVE_PATH_TO_SAMPLE,
		DONT_SAVE_DATA,
		NUM_DATA_SAVING_MODES,
	};

	float phases[MAX_POLY_CHANNELS];
	int nChannels = 1;
	float recPhase = 0.f;
	float sampleRate; // so that it can be read by the UI
	RecordingMode recMode = GATE;
	dsp::SchmittTrigger recTrigger;
	dsp::SchmittTrigger recClickTrigger;
	bool isRecording = false;
	std::vector<float> buffer;
	std::string lastLoadedPath;
	bool enableEditing = true;
	DataSaveMode saveMode = SAVE_FULL_DATA;
	InterpBoundaryMode boundaryMode = INTERP_PERIODIC;

	void initBuffer() {
		buffer.clear();
		int default_steps = 10;
		for(int i = 0; i < default_steps; i++) {
			buffer.push_back(i / (default_steps - 1.f));
		}
	}

	Array() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<RangeParamQuantity>(Array::OUTPUT_RANGE_PARAM, 0, 2, 0, "Recording and output range", "V");
		configParam<RangeParamQuantity>(Array::PHASE_RANGE_PARAM, 0, 2, 2, "Position CV range", "V");
		configParam(Array::REC_ENABLE_PARAM, 0.f, 1.f, 0.f, "Record");
		for(int i = 0; i < MAX_POLY_CHANNELS; i++) phases[i] = 0.f;
		initBuffer();
	}

	void process(const ProcessArgs &args) override;

	float getZeroValue() {
		// The buffer internal values are always 0..1. Depending on the
		// signedness of the output, the buffer value corresponding to 0V
		// output is either 0.0 or 0.5. This function returns that value.
		bool outputIsSigned = params[OUTPUT_RANGE_PARAM].getValue() < 1.5f;
		return outputIsSigned ? 0.5f : 0.f;
	}

	void resizeBuffer(unsigned int newSize) {
		buffer.resize(newSize, getZeroValue());
	}

	void loadSample(std::string path, bool resizeBuf = false);

	// TODO: if array is large enough (how large?) encode as base64?
	// see https://stackoverflow.com/questions/45508360/quickest-way-to-encode-vector-of-floats-into-hex-or-base64binary
	// also: rack::string::to/fromBase64

	json_t *dataToJson() override {
		json_t *root = json_object();
		json_object_set_new(root, "enableEditing", json_boolean(enableEditing));
		json_object_set_new(root, "boundaryMode", json_integer(boundaryMode));
		json_object_set_new(root, "recMode", json_integer(recMode));
		json_object_set_new(root, "lastLoadedPath", json_string(lastLoadedPath.c_str()));
		if(saveMode == SAVE_FULL_DATA) {
			json_t *arr = json_array();
			for(float x : buffer) {
				json_array_append_new(arr, json_real(x));
			}
			json_object_set(root, "arrayData", arr);
			json_decref(arr);
		} else if(saveMode == SAVE_PATH_TO_SAMPLE) {
			json_object_set_new(root, "arrayData", json_string(lastLoadedPath.c_str()));
		} else if(saveMode == DONT_SAVE_DATA) {
			json_object_set_new(root, "arrayData", json_integer(buffer.size()));
		}
		return root;
	}

	void dataFromJson(json_t *root) override {
		json_t *enableEditing_J = json_object_get(root, "enableEditing");
		json_t *boundaryMode_J = json_object_get(root, "boundaryMode");
		json_t *recMode_J = json_object_get(root, "recMode");
		json_t *arrayData_J = json_object_get(root, "arrayData");
		json_t *lastLoadedPath_J = json_object_get(root, "lastLoadedPath");

		if(enableEditing_J) {
			enableEditing = json_boolean_value(enableEditing_J);
		}
		if(boundaryMode_J) {
			int bm = int(json_integer_value(boundaryMode_J));
			if(bm < NUM_INTERP_MODES) {
				boundaryMode = static_cast<InterpBoundaryMode>(bm);
			}
		}
		if(recMode_J) {
			int rm = int(json_integer_value(recMode_J));
			if(rm < NUM_REC_MODES) {
				recMode = static_cast<RecordingMode>(rm);
			}
		}
		if(lastLoadedPath_J) {
			lastLoadedPath = std::string(json_string_value(lastLoadedPath_J));
		}

		if(json_array_size(arrayData_J) > 0) {
			buffer.clear();
			size_t i;
			json_t *val;
			json_array_foreach(arrayData_J, i, val) {
				buffer.push_back(json_real_value(val));
			}
			saveMode = SAVE_FULL_DATA;
		} else if(json_string_value(arrayData_J) != NULL) {
			lastLoadedPath = std::string(json_string_value(arrayData_J));
			loadSample(lastLoadedPath, true);
			saveMode = SAVE_PATH_TO_SAMPLE;
		} else if(json_integer_value(arrayData_J) > 0) {
			buffer.clear();
			resizeBuffer(json_integer_value(arrayData_J));
			saveMode = DONT_SAVE_DATA;
		}
	}

	void onReset() override {
		boundaryMode = INTERP_PERIODIC;
		enableEditing = true;
		initBuffer();
	}

	void onRandomize() override {
		Module::onRandomize();
		for(unsigned int i = 0; i < buffer.size(); i++) {
			buffer[i] = random::uniform();
		}
	}
};

void Array::loadSample(std::string path, bool resizeBuf) {
	unsigned int channels, sampleRate;
	drwav_uint64 totalPCMFrameCount;
	float* pSampleData = drwav_open_file_and_read_pcm_frames_f32(path.c_str(), &channels, &sampleRate, &totalPCMFrameCount);

	if (pSampleData != NULL) {
		unsigned long nSamplesToRead = std::min((unsigned long) totalPCMFrameCount, 999999UL);
		unsigned long newSize = resizeBuf ? nSamplesToRead : buffer.size();
		buffer.resize(newSize, 0);
		unsigned long max_i = std::min(newSize, nSamplesToRead);
		for(unsigned long i = 0; i < max_i; i++) {
			int ii = i * channels;
			float s = pSampleData[ii];
			if(channels == 2) {
				s = (s + pSampleData[ii + 1]) * 0.5f; // mix stereo channels, good idea?
			}
			buffer[i] = (s + 1.f) * 0.5f;
		}
		enableEditing = false;
	}

	drwav_free(pSampleData);
}

void Array::process(const ProcessArgs &args) {
	float deltaTime = args.sampleTime;
	sampleRate = args.sampleRate;

	float phaseMin, phaseMax;
	float prange = params[PHASE_RANGE_PARAM].getValue();
	if(prange > 1.5f) {
		phaseMin = 0.f;
		phaseMax = 10.f;
	} else if(prange > 0.5f) {
		phaseMin = -5.f;
		phaseMax =  5.f;
	} else {
		phaseMin = -10.f;
		phaseMax =  10.f;
	}

	int size = buffer.size();


	float inOutMin, inOutMax;
	float orange = params[OUTPUT_RANGE_PARAM].getValue();
	if(orange > 1.5f) {
		inOutMin = 0.f;
		inOutMax = 10.f;
	} else if(orange > 0.5f) {
		inOutMin = -5.f;
		inOutMax =  5.f;
	} else {
		inOutMin = -10.f;
		inOutMax =  10.f;
	}

	// recording
	recPhase = clamp(rescale(inputs[REC_PHASE_INPUT].getVoltage(), phaseMin, phaseMax, 0.f, 1.f), 0.f, 1.f);
	int ri = int(recPhase * size);
	bool recWasTriggered = recTrigger.process(rescale(inputs[REC_ENABLE_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f));
	bool recWasClicked = recClickTrigger.process(params[REC_ENABLE_PARAM].getValue());

	if(recMode == GATE) {
		isRecording = recTrigger.isHigh() || recClickTrigger.isHigh();
	} else if(recMode == TOGGLE && (recWasTriggered || recWasClicked)) {
		isRecording = !isRecording;
	}
	if(isRecording) {
		buffer[ri] = clamp(rescale(inputs[REC_SIGNAL_INPUT].getVoltage(), inOutMin, inOutMax, 0.f, 1.f), 0.f, 1.f);
	}
	lights[REC_LIGHT].setSmoothBrightness(isRecording, deltaTime);

	nChannels = inputs[PHASE_INPUT].getChannels();
	outputs[STEP_OUTPUT].setChannels(nChannels);
	outputs[INTERP_OUTPUT].setChannels(nChannels);
	for(int chan = 0; chan < nChannels; chan++) {
		float phase = clamp(rescale(inputs[PHASE_INPUT].getVoltage(chan), phaseMin, phaseMax, 0.f, 1.f), 0.f, 1.f);
		phases[chan] = phase;
		// direct output
		int i_step = clamp((int) std::floor(phase * size), 0, size - 1);
		outputs[STEP_OUTPUT].setVoltage(rescale(buffer[i_step], 0.f, 1.f, inOutMin, inOutMax), chan);

		// interpolated output, based on tabread4_tilde_perform() in
		// https://github.com/pure-data/pure-data/blob/master/src/d_array.c
		//TODO: adjust symmetry of surrounding indices (based on range polarity)?
		int i = i_step;
		int ia, ib, ic, id;
		switch(boundaryMode) {
			case INTERP_CONSTANT:
				{
					ia = clamp(i - 1, 0, size - 1);
					ib = clamp(i + 0, 0, size - 1);
					ic = clamp(i + 1, 0, size - 1);
					id = clamp(i + 2, 0, size - 1);
					break;
				}
			case INTERP_MIRROR:
				{
					ia = i < 1 ? 1 : i - 1;
					ib = i;
					ic = i + 1 < size ? i + 1 : size - 1;
					id = i + 2 < size ? i + 2 : 2*size - (i + 3);
					break;
				}
			case INTERP_PERIODIC:
			default:
				{
					ia = (i - 1 + size) % size;
					ib = (i + 0) % size;
					ic = (i + 1) % size;
					id = (i + 2) % size;
					break;
				}
		}
		float a = buffer[ia];
		float b = buffer[ib];
		float c = buffer[ic];
		float d = buffer[id];

		// Pd algorithm magic
		float frac = phase * size - i; // fractional part of phase
		float y = b + frac * (
				c - b - 0.1666667f * (1.f - frac) * (
					(d - a - 3.f * (c - b)) * frac + (d + 2.f * a - 3.f * b)
					)
				);
		outputs[INTERP_OUTPUT].setVoltage(rescale(y, 0.f, 1.f, inOutMin, inOutMax), chan);
	}

}

struct ArrayDisplay : OpaqueWidget {
	Array *module;
	Vec dragPosition;
	bool dragging = false;

	ArrayDisplay(Array *module): OpaqueWidget() {
		this->module = module;
		box.size = Vec(230, 205);
	}

	void draw(const DrawArgs &args) override {
		OpaqueWidget::draw(args);
		const auto vg = args.vg;

		if(module) {

			// draw the array contents
			int s = module->buffer.size();
			float w = box.size.x * 1.f / s;
			nvgBeginPath(vg);
			if(s < box.size.x) {
				for(int i = 0; i < s; i++) {
					float x1 = i * w;
					float x2 = (i + 1) * w;
					float y = (1.f - module->buffer[i]) * box.size.y;

					if(i == 0) nvgMoveTo(vg, x1, y);
					else nvgLineTo(vg, x1, y);

					nvgLineTo(vg, x2, y);
				}
			} else {
				for(int i = 0; i < box.size.x; i++) {
					//int i1 = clamp(int(rescale(i, 0, box.size.x - 1, 0, s - 1)), 0, s - 1);
					// just use the left edge (should really use average over i1..i2 instead...
					int ii = clamp(int(rescale(i, 0, box.size.x - 1, 0, s - 1)), 0, s - 1);
					float y = (1.f - module->buffer[ii]) * box.size.y;
					if(i == 0) nvgMoveTo(vg, 0, y);
					else nvgLineTo(vg, i, y);
				}
			}
			nvgStrokeWidth(vg, 2.f);
			nvgStrokeColor(vg, nvgRGB(0x0, 0x0, 0x0));
			nvgStroke(vg);

			// show phase
			int nc = module->nChannels;
			int alpha = int(0xff * rescale(1.0f/nc, 0.f, 1.f, 0.5f, 1.0f));
			for(int c = 0; c < nc; c++) {
				float px =  module->phases[c] * box.size.x;
				nvgBeginPath(vg);
				nvgStrokeWidth(vg, 2.f);
				nvgStrokeColor(vg, nvgRGBA(0x26, 0x8b, 0xd2, alpha));
				nvgMoveTo(vg, px, 0);
				nvgLineTo(vg, px, box.size.y);
				nvgStroke(vg);
			}

			// show phase of recording
			if(module->inputs[Array::REC_PHASE_INPUT].isConnected()) {
				float rpx = module->recPhase * box.size.x;
				nvgBeginPath(vg);
				nvgStrokeWidth(vg, 2.f);
				nvgStrokeColor(vg, nvgRGB(0xdc, 0x32, 0x2f));
				nvgMoveTo(vg, rpx, 0);
				nvgLineTo(vg, rpx, box.size.y);
				nvgStroke(vg);
			}

		}

		nvgBeginPath(vg);
		nvgStrokeColor(vg, nvgRGB(0x0, 0x0, 0x0));
		nvgStrokeWidth(vg, 2.f);
		nvgRect(vg, 0, 0, box.size.x, box.size.y);
		nvgStroke(vg);

	}

	void onButton(const event::Button &e) override {
		bool ctrl = (APP->window->getMods() & RACK_MOD_MASK) == RACK_MOD_CTRL;
		if(e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS
				&& module->enableEditing && !ctrl) {
			e.consume(this);
			dragPosition = e.pos;
		}
	}

	void onDragStart(const event::DragStart &e) override {
		OpaqueWidget::onDragStart(e);
		dragging = true;
	}

	void onDragEnd(const event::DragEnd &e) override {
		OpaqueWidget::onDragEnd(e);
		dragging = false;
	}

	void onDragMove(const event::DragMove &e) override {
		OpaqueWidget::onDragMove(e);
		if(!module->enableEditing) return;
		Vec dragPosition_old = dragPosition;
		float zoom = std::pow(2.f, settings::zoom);
		dragPosition = dragPosition.plus(e.mouseDelta.div(zoom)); // take zoom into account

		// int() rounds down, so the upper limit of rescale is buffer.size() without -1.
		int s = module->buffer.size();
		math::Vec bs = box.size;
		int i1 = clamp(int(rescale(dragPosition_old.x, 0, bs.x, 0, s)), 0, s - 1);
		int i2 = clamp(int(rescale(dragPosition.x,     0, bs.x, 0, s)), 0, s - 1);

		if(abs(i1 - i2) < 2) {
			float y = clamp(rescale(dragPosition.y, 0, bs.y, 1.f, 0.f), 0.f, 1.f);
			module->buffer[i2] = y;
		} else {
			// mouse moved more than one index, interpolate
			float y1 = clamp(rescale(dragPosition_old.y, 0, bs.y, 1.f, 0.f), 0.f, 1.f);
			float y2 = clamp(rescale(dragPosition.y,     0, bs.y, 1.f, 0.f), 0.f, 1.f);
			if(i2 < i1) {
				std::swap(i1, i2);
				std::swap(y1, y2);
			}
			for(int i = i1; i <= i2; i++) {
				float y = y1 + rescale(i, i1, i2, 0.f, 1.0f) * (y2 - y1);
				module->buffer[i] = y;
			}
		}
	}

	void step() override {
		OpaqueWidget::step();
	}
};


struct ArraySizeSelector : NumberTextField {
	Array *module;

	ArraySizeSelector(Array *m) : NumberTextField() {
		module = m;
		validText = string::f("%u", module ? module->buffer.size() : 1);
		text = validText;
	};

	void onNumberSet(const int n) override {
		if(module) {
			module->resizeBuffer(n);
		}
	}

	void step() override {
		NumberTextField::step();
		// eh, kinda hacky - is there any way to do this just once after the module has been initialized? after dataFromJson?
		if(module) {
			if(APP->event->selectedWidget != this) {
				validText = string::f("%u", module->buffer.size());
				text = validText;
			}
		}
	}

};

struct ArrayResetBufferItem : MenuItem {
	Array *module;
	void onAction(const event::Action &e) override {
		module->initBuffer();
	}
};

struct ArraySortBufferItem : MenuItem {
	Array *module;
	void onAction(const event::Action &e) override {
		std::sort(module->buffer.begin(), module->buffer.end());
	}
};

// file selection dialog, based on PLAYERItem in cf
// https://github.com/cfoulc/cf/blob/master/src/PLAYER.cpp
struct ArrayFileSelectItem : MenuItem {
	Array *module;
	bool resizeBuffer;

	void onAction(const event::Action &e) override {
		std::string dir = module->lastLoadedPath.empty() ? asset::user("") : rack::string::directory(module->lastLoadedPath);
		osdialog_filters* filters = osdialog_filters_parse(".wav files:wav");
		char *path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, filters);
		if(path) {
			module->loadSample(path, resizeBuffer);
			module->lastLoadedPath = path;
			free(path);
		}
		osdialog_filters_free(filters);
	}
};

struct ArrayEnableEditingMenuItem : MenuItem {
	Array *module;
	bool valueToSet;
	void onAction(const event::Action &e) override {
		module->enableEditing = valueToSet;
	}
};

// For slight DRY
struct MenuItemWithRightArrow : MenuItem {
	MenuItemWithRightArrow(): MenuItem() {
		rightText = RIGHT_ARROW;
	};
};

// Generic child menu item for selecting one of many enum values
// E is the enum, memberToSet is a pointer to the member variable which is a variant of said enum
template <typename E>
struct ArrayEnumSettingChildMenuItem : MenuItem {
	Array *module;
	E mode;
	E *memberToSet;
	ArrayEnumSettingChildMenuItem(Array *pModule,
			E pMode,
			std::string label,
			E *pMemberToSet):
		MenuItem() {
			module = pModule;
			mode = pMode;
			text = label;
			memberToSet = pMemberToSet;
			rightText = CHECKMARK(*memberToSet == mode);
		}
	void onAction(const event::Action &e) override {
		*memberToSet = mode;
	}
};

struct ArrayRecordingModeMenuItem : MenuItemWithRightArrow {
	Array *module;
	Menu* createChildMenu() override {
		Menu *menu = new Menu();
		menu->addChild(new ArrayEnumSettingChildMenuItem<Array::RecordingMode>(this->module, Array::GATE, "Gate", &module->recMode));
		menu->addChild(new ArrayEnumSettingChildMenuItem<Array::RecordingMode>(this->module, Array::TOGGLE, "Toggle", &module->recMode));

		return menu;
	}
};

struct ArrayDataSaveModeMenuItem : MenuItemWithRightArrow {
	Array *module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu();

		menu->addChild(new ArrayEnumSettingChildMenuItem<Array::DataSaveMode>(this->module, Array::SAVE_FULL_DATA, "Save full array data to patch file", &module->saveMode));
		menu->addChild(new ArrayEnumSettingChildMenuItem<Array::DataSaveMode>(this->module, Array::SAVE_PATH_TO_SAMPLE, "Save path to loaded sample", &module->saveMode));
		menu->addChild(new ArrayEnumSettingChildMenuItem<Array::DataSaveMode>(this->module, Array::DONT_SAVE_DATA, "Don't save array data", &module->saveMode));

		return menu;
	}
};

struct ArrayInterpModeMenuItem : MenuItemWithRightArrow {
	Array* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu();

		menu->addChild(new ArrayEnumSettingChildMenuItem<Array::InterpBoundaryMode>(this->module, Array::INTERP_CONSTANT, "Constant", &module->boundaryMode));
		menu->addChild(new ArrayEnumSettingChildMenuItem<Array::InterpBoundaryMode>(this->module, Array::INTERP_MIRROR, "Mirror", &module->boundaryMode));
		menu->addChild(new ArrayEnumSettingChildMenuItem<Array::InterpBoundaryMode>(this->module, Array::INTERP_PERIODIC, "Periodic", &module->boundaryMode));

		return menu;
	}
};

struct ArrayModuleWidget : ModuleWidget {
	ArrayDisplay *display;
	ArraySizeSelector *sizeSelector;
	Array *module;

	ArrayModuleWidget(Array *module) {
		setModule(module);
		this->module = module;

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Array.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		//TPort *createInputCentered(Vec pos, Module *module, int inputId) {
		addInput(createInputCentered<PJ301MPort>(Vec(27.5f, 247.5f), module, Array::PHASE_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(97.5f, 247.5f), module, Array::STEP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(175.f, 247.5f), module, Array::INTERP_OUTPUT));

		addInput(createInputCentered<PJ301MPort>(Vec(27.5f, 347.5f), module, Array::REC_PHASE_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(97.5f, 347.5f), module, Array::REC_SIGNAL_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(175.f, 347.5f), module, Array::REC_ENABLE_INPUT));

		addParam(createParam<CKSSThree>(Vec(107.5f, 290), module, Array::OUTPUT_RANGE_PARAM));
		addParam(createParam<CKSSThree>(Vec(27.5f, 290), module, Array::PHASE_RANGE_PARAM));

		auto recButtonLight = createLightCentered<MediumLight<RedLight>>(Vec(207.5f, 355), module, Array::REC_LIGHT);
		addChild(recButtonLight);

		auto recButton = createParam<Switch>(Vec(207.5f, 355), module, Array::REC_ENABLE_PARAM);
		recButton->box.size = recButtonLight->box.size;
		recButton->box.pos = recButtonLight->box.pos;
		recButton->momentary = true;
		addParam(recButton);

		display = new ArrayDisplay(module);
		display->box.pos = Vec(5, 20);
		addChild(display);

		sizeSelector = new ArraySizeSelector(module);
		sizeSelector->box.pos = Vec(174, 295);
		sizeSelector->box.size.x = 51; // additional pixel, otherwise last digit gets put on next line on some zoom levels
		addChild(sizeSelector);
	}

	void appendContextMenu(ui::Menu *menu) override {

		Array *arr = dynamic_cast<Array*>(module);
		if(arr){
			menu->addChild(new MenuLabel()); // spacer

			auto *bufResetItem = new ArrayResetBufferItem();
			bufResetItem->text = "Reset array contents";
			bufResetItem->module = arr;
			menu->addChild(bufResetItem);

			auto *bufSortItem = new ArraySortBufferItem();
			bufSortItem->text = "Sort array contents";
			bufSortItem->module = arr;
			menu->addChild(bufSortItem);

			auto *edItem = new ArrayEnableEditingMenuItem();
			edItem->text = "Disable drawing";
			edItem->module = arr;
			edItem->rightText = CHECKMARK(!arr->enableEditing);
			edItem->valueToSet = !arr->enableEditing;
			menu->addChild(edItem);

			auto *rmItem = new ArrayRecordingModeMenuItem();
			rmItem->text = "Recording mode";
			rmItem->module = this->module;
			menu->addChild(rmItem);

			{
			auto *fsItem = new ArrayFileSelectItem();
			float duration = arr->buffer.size() * 1.f / arr->sampleRate;
			fsItem->resizeBuffer = false;
			fsItem->text = "Load .wav file...";
			fsItem->rightText = string::f("(%.2f s)", duration);
			fsItem->module = arr;
			menu->addChild(fsItem);
			}

			{
			auto *fsItem = new ArrayFileSelectItem();
			fsItem->resizeBuffer = true;
			fsItem->text = "Load .wav file and resize array...";
			fsItem->module = arr;
			menu->addChild(fsItem);
			}

			auto *saveModeSubMenu = new ArrayDataSaveModeMenuItem();
			saveModeSubMenu->text = "Data persistence";
			saveModeSubMenu->module = this->module;
			menu->addChild(saveModeSubMenu);

			auto *interpModeSubMenu = new ArrayInterpModeMenuItem();
			interpModeSubMenu->text = "Interpolation at boundary";
			interpModeSubMenu->module = this->module;
			menu->addChild(interpModeSubMenu);
		}

	}

};


Model *modelArray = createModel<Array, ArrayModuleWidget>("Array");
