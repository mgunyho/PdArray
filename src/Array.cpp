#include "plugin.hpp"
#include <osdialog.h> //NOTE: not officially part of the public Rack v2 API, but other plugins (including Fundamental) do it this way
#include <algorithm> // std::min, std::swap, std::transform
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h" // for reading wav files

#include "Widgets.hpp"

#include <iostream>

//TODO: load buffer from text/csv file?
//TODO: prevent audio clicking at the last sample
//TODO: undo history? hard? memory intensive?
//TODO: visual representation choice right-click submenu (stairs (current), lines, points, bars)
//TODO: reinterpolate array on resize (+right-click menu option for that)

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

	// If the array size is smaller than this, serialize as JSON, otherwise
	// serialize as wav in the patch storage folder. Floats are serialized in
	// json as ~20 bytes, so 5k elements will be 100 KB, which is the limit
	// recommended by the manual.
	const static unsigned int directSerializationThreshold = 5000;
	const static std::string arrayDataFileName;

	void initBuffer() {
		buffer.clear();
		int default_steps = 10;
		for(int i = 0; i < default_steps; i++) {
			buffer.push_back(i / (default_steps - 1.f));
		}
	}

	Array() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(Array::OUTPUT_RANGE_PARAM, 0, 2, 0, "Recording and output range", {
				"-10..10 V",
				"-5..5 V",
				"0..10 V",
		});
		configSwitch(Array::PHASE_RANGE_PARAM, 0, 2, 2, "Position CV range", {
				"-10..10 V",
				"-5..5 V",
				"0..10 V",
		});
		configSwitch(Array::REC_ENABLE_PARAM, 0.f, 1.f, 0.f, "Record", {"Off", "On"});

		configInput(PHASE_INPUT, "Playback position");
		configInput(REC_SIGNAL_INPUT, "Signal to record");
		configInput(REC_PHASE_INPUT, "Recording position");
		configInput(REC_ENABLE_INPUT, "Recording enable");

		configOutput(STEP_OUTPUT, "Direct (step)");
		configOutput(INTERP_OUTPUT, "Smooth (interpolated)");

		configBypass(REC_SIGNAL_INPUT, STEP_OUTPUT);
		configBypass(REC_SIGNAL_INPUT, INTERP_OUTPUT);

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

	size_t numFadeSamples() {
		// Calculate the clicking prevention fade size (in samples)
		// based on the current buffer size.
		size_t n = buffer.size();
		if(n < 5) {
			return 0;
		} else {
			return std::min<size_t>(n / 100 + 2, 200);
		}
	}

	void loadSample(std::string path, bool resizeBuf = false);
	void saveWav(std::string path);

	json_t *dataToJson() override {
		json_t *root = json_object();
		json_object_set_new(root, "enableEditing", json_boolean(enableEditing));
		json_object_set_new(root, "boundaryMode", json_integer(boundaryMode));
		json_object_set_new(root, "recMode", json_integer(recMode));
		json_object_set_new(root, "lastLoadedPath", json_string(lastLoadedPath.c_str()));

		// we want to delete the wav file created by onSave in most cases, see below
		bool deleteWavFile = true;

		if(saveMode == SAVE_FULL_DATA) {
			if(buffer.size() <= directSerializationThreshold) {

				json_t *arr = json_array();
				for(float x : buffer) {
					json_array_append_new(arr, json_real(x));
				}
				json_object_set(root, "arrayData", arr);
				json_decref(arr);

			} else {
				deleteWavFile = false;
			}
		} else if(saveMode == SAVE_PATH_TO_SAMPLE) {
			json_object_set_new(root, "arrayData", json_string(lastLoadedPath.c_str()));
		} else if(saveMode == DONT_SAVE_DATA) {
			json_object_set_new(root, "arrayData", json_integer(buffer.size()));
		}

		if(deleteWavFile) {
			std::string path = system::join(createPatchStorageDirectory(), arrayDataFileName);
			// make sure that the wav file doesn't exist, so we don't read from the file the next time we load the patch
			if(system::isFile(path)) {
				system::remove(path);
			}
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
			enableEditing = false;
			saveMode = SAVE_PATH_TO_SAMPLE;
		} else if(json_integer_value(arrayData_J) > 0) {
			buffer.clear();
			resizeBuffer(json_integer_value(arrayData_J));
			saveMode = DONT_SAVE_DATA;
		}
		// else, arrayData was missing from JSON, so we assume it's loaded from wav file in patch storage folder
	}

	void onAdd(const AddEvent& e) override {
		std::string path = system::join(createPatchStorageDirectory(), arrayDataFileName);
		if(system::isFile(path)) {
			// if the file exists, we assume that we're supposed to load the
			// data from there instead of the JSON, i.e., it was above the
			// directSerializationThreshold.
			loadSample(path, true);
		}
	}

	void onSave(const SaveEvent& e) override {
		if(buffer.size() > directSerializationThreshold) {
			std::string path = system::join(createPatchStorageDirectory(), arrayDataFileName);
			saveWav(path);
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

const std::string Array::arrayDataFileName = "arraydata.wav";

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
	}

	drwav_free(pSampleData);
}

void Array::saveWav(std::string path) {
	// use drwav to save the buffer as a wav file.
	// based on VCV Fundamental wavetable.save();
	drwav_data_format format;
	format.container = drwav_container_riff;
	format.format = DR_WAVE_FORMAT_PCM;
	format.channels = 1;
	format.sampleRate = sampleRate; // note: this doesn't really matter, because we're not using the info when reading the sample back
	format.bitsPerSample = 16;

	drwav wav;
	if(!drwav_init_file_write(&wav, path.c_str(), &format))
		return;

	// Rescale from the range 0..1 to -1..1
	std::vector<float> buffer_rescaled = buffer;
	std::transform(buffer_rescaled.begin(), buffer_rescaled.end(), buffer_rescaled.begin(),
			[](float y) -> float { return (y - 0.5f) * 2.f; });

	size_t len = buffer_rescaled.size();
	int16_t* buf = new int16_t[len];
	drwav_f32_to_s16(buf, buffer_rescaled.data(), len);
	drwav_write_pcm_frames(&wav, len, buf);
	delete[] buf;

	drwav_uninit(&wav);
}

void Array::process(const ProcessArgs &args) {
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
	lights[REC_LIGHT].setBrightness(isRecording);

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

		}

		nvgBeginPath(vg);
		nvgStrokeColor(vg, nvgRGB(0x0, 0x0, 0x0));
		nvgStrokeWidth(vg, 2.f);
		nvgRect(vg, 0, 0, box.size.x, box.size.y);
		nvgStroke(vg);

	}

	void drawLayer(const DrawArgs &args, int layer) override {
		const auto vg = args.vg;

		if(layer == 1) {
			// draw playback & recording position
			if(module) {

				// draw playback position
				int nc = module->nChannels;
				int alpha = int(0xff * rescale(1.0f/nc, 0.f, 1.f, 0.5f, 1.0f));
				for(int c = 0; c < nc; c++) {
					// Offset by the thickness of the box border so we don't draw over it. Same for y.
					// (could/should also use nvgScissor, but this is ok)
					float px =  module->phases[c] * (box.size.x - 4) + 2;
					nvgBeginPath(vg);
					nvgStrokeWidth(vg, 2.f);
					nvgStrokeColor(vg, nvgRGBA(0x26, 0x8b, 0xd2, alpha));
					nvgMoveTo(vg, px, 1);
					nvgLineTo(vg, px, box.size.y - 1);
					nvgStroke(vg);
				}

				// draw recording position
				if(module->inputs[Array::REC_PHASE_INPUT].isConnected()) {
					float rpx = module->recPhase * (box.size.x - 4) + 2;
					nvgBeginPath(vg);
					nvgStrokeWidth(vg, 2.f);
					nvgStrokeColor(vg, nvgRGB(0xdc, 0x32, 0x2f));
					nvgMoveTo(vg, rpx, 1);
					nvgLineTo(vg, rpx, box.size.y - 1);
					nvgStroke(vg);
				}
			}
		}
		OpaqueWidget::drawLayer(args, layer);
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
		float zoom = getAbsoluteZoom();
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


struct ArraySizeSelector : NumberTextBox {
	Array *module;

	ArraySizeSelector(Array *m) : NumberTextBox() {
		module = m;
		TextBox::text = string::f("%lu", module ? module->buffer.size() : 1);
		TextField::text = TextBox::text;
		TextBox::box.size.x = 54;
		textOffset = Vec(TextBox::box.size.x / 2, TextBox::box.size.y / 2);
		letterSpacing = -1.5f; // tighten text to fit in six characters at this width
	};

	void onNumberSet(const int n) override {
		if(module) {
			module->resizeBuffer(n);
		}
	}

};

struct ArrayResetBufferItem : MenuItem {
	Array *module;
	void onAction(const event::Action &e) override {
		module->initBuffer();
	}
};

struct ArraySetBufferToZeroItem : MenuItem {
	Array *module;
	void onAction(const event::Action &e) override {
		auto& buf = module->buffer;
		std::fill(buf.begin(), buf.end(), module->getZeroValue());
	}
};

struct ArraySortBufferItem : MenuItem {
	Array *module;
	void onAction(const event::Action &e) override {
		std::sort(module->buffer.begin(), module->buffer.end());
	}
};

struct ArrayAddFadesMenuItem : MenuItem {
	Array *module;
	ArrayAddFadesMenuItem(Array *pModule) {
		module = pModule;
		rightText = string::f("%lu samples", module->numFadeSamples());
	}

	void onAction(const event::Action &e) override {
		size_t nFade = module->numFadeSamples();
		auto& buf = module->buffer;
		size_t bufSize = buf.size();
		float zero = module->getZeroValue();
		if(nFade > 1) {
			for(unsigned int i = 0; i < nFade; i++) {
				float fac = i * 1.f / (nFade - 1);
				buf[i] = crossfade(zero, buf[i], fac);
				buf[bufSize - 1 - i] = crossfade(zero, buf[bufSize - 1 - i], fac);
			}
		}
	}
};

// file selection dialog, based on PLAYERItem in cf
// https://github.com/cfoulc/cf/blob/master/src/PLAYER.cpp
struct ArrayFileSelectItem : MenuItem {
	Array *module;
	bool resizeBuffer;

	void onAction(const event::Action &e) override {
		std::string dir = module->lastLoadedPath.empty() ? asset::user("") : rack::system::getDirectory(module->lastLoadedPath);
#ifdef USING_CARDINAL_NOT_RACK
		Array *module = this->module;
		bool resizeBuffer = this->resizeBuffer;
		async_dialog_filebrowser(false, nullptr, dir.c_str(), "Load Wav file", [module, resizeBuffer](char* path) {
			pathSelected(module, resizeBuffer, path);
		});
#else
		osdialog_filters* filters = osdialog_filters_parse(".wav files:wav");
		char *path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, filters);
		osdialog_filters_free(filters);
		pathSelected(module, resizeBuffer, path);
#endif
	}

	static void pathSelected(Array *module, bool resizeBuffer, char *path) {
		if(path) {
			module->loadSample(path, resizeBuffer);
			module->lastLoadedPath = path;
			module->enableEditing = false; // disable editing for loaded wav files
			free(path);
		}
	}
};

struct ArrayEnableEditingMenuItem : MenuItem {
	Array *module;
	bool valueToSet;
	void onAction(const event::Action &e) override {
		module->enableEditing = valueToSet;
	}
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

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<RedLight>>>(Vec(207.5f, 347.5f), module, Array::REC_ENABLE_PARAM, Array::REC_LIGHT));

		display = new ArrayDisplay(module);
		display->box.pos = Vec(5, 20);
		addChild(display);

		sizeSelector = new ArraySizeSelector(module);
		sizeSelector->TextBox::box.pos = Vec(174, 295);
		addChild(static_cast<TextBox*>(sizeSelector));
	}

	void appendContextMenu(ui::Menu *menu) override {

		Array *arr = dynamic_cast<Array*>(module);
		if(arr){
			menu->addChild(new MenuLabel()); // spacer

			auto *bufResetItem = new ArrayResetBufferItem();
			bufResetItem->text = "Reset array contents";
			bufResetItem->module = arr;
			menu->addChild(bufResetItem);

			auto *bufSetToZeroItem = new ArraySetBufferToZeroItem();
			bufSetToZeroItem->text = "Set array contents to zero";
			bufSetToZeroItem->module = arr;
			menu->addChild(bufSetToZeroItem);

			auto *bufSortItem = new ArraySortBufferItem();
			bufSortItem->text = "Sort array contents";
			bufSortItem->module = arr;
			menu->addChild(bufSortItem);

			auto *addFadesItem = new ArrayAddFadesMenuItem(arr);
			addFadesItem->text = "Add fade in/out to prevent clicks";
			menu->addChild(addFadesItem);

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
