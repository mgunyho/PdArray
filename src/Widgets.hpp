#include "rack.hpp"

// TODO: same as in LittleUtils, move to common folder?
struct TextBox : TransparentWidget {
	// Kinda like TextField except not editable. Using Roboto Mono Bold font,
	// numbers look okay.
	// based on LedDisplayChoice
	std::string text;
	std::shared_ptr<Font> font;
	float font_size;
	float letter_spacing;
	Vec textOffset;
	NVGcolor defaultTextColor;
	NVGcolor textColor; // This can be used to temporarily override text color
	NVGcolor backgroundColor;

	//TODO: create<...>() thing with position as argument?
	TextBox() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/RobotoMono-Bold.ttf"));
		defaultTextColor = nvgRGB(0x23, 0x23, 0x23);
		textColor = defaultTextColor;
		backgroundColor = nvgRGB(0xc8, 0xc8, 0xc8);
		box.size = Vec(30, 18);
		// size 20 with spacing -2 will fit 3 characters on a 30px box with Roboto mono
		font_size = 20;
		letter_spacing = 0.f;
		textOffset = Vec(box.size.x * 0.5f, 0.f);
	}

	virtual void setText(std::string s) { text = s; }

	virtual void draw(const DrawArgs &args) override {
		// based on LedDisplayChoice::draw() in Rack/src/app/LedDisplay.cpp
		const auto vg = args.vg;
		nvgScissor(vg, 0, 0, box.size.x, box.size.y);
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 3.0);
		nvgFillColor(vg, backgroundColor);
		nvgFill(vg);

		if (font->handle >= 0) {

			nvgFillColor(vg, textColor);
			nvgFontFaceId(vg, font->handle);

			nvgFontSize(vg, font_size);
			nvgTextLetterSpacing(vg, letter_spacing);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			nvgText(vg, textOffset.x, textOffset.y, text.c_str(), NULL);
		}

		nvgResetScissor(vg);
	};

};

// TextField that only allows inputting numbers
struct NumberTextField : TextField {
	int maxCharacters = 6;
	std::string validText = "1";

	NumberTextField() : TextField() {
		text = validText;
	};

	bool isNumber(const std::string& s) {
		// shamelessly copypasted from https://stackoverflow.com/questions/4654636/how-to-determine-if-a-string-is-a-number-with-c
		std::string::const_iterator it = s.begin();
		while (it != s.end() && std::isdigit(*it)) ++it;
		return !s.empty() && it == s.end();
	}

	void onButton(const event::Button& e) override {

		event::Button ee = e;

		// make right-click behave the same as left click
		if(e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ee.button = GLFW_MOUSE_BUTTON_LEFT;
		}

		TextField::onButton(ee);

		if(ee.action == GLFW_PRESS && ee.button == GLFW_MOUSE_BUTTON_LEFT) {
			// HACK
			APP->event->setSelected(this);
		}

		if(ee.isConsumed()) {
			e.consume(ee.getTarget());
		}

	}

	void onSelectKey(const event::SelectKey &e) override {
		//TODO: compare this to onSelectKey in Little-Utils/src/Widgets.cpp
		if(e.key == GLFW_KEY_V && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
			// prevent pasting too long text
			int pasteLength = maxCharacters - TextField::text.size();
			if(pasteLength > 0) {
				std::string newText(glfwGetClipboardString(APP->window->win));
				if(newText.size() > pasteLength) newText.erase(pasteLength);
				if(isNumber(newText)) insertText(newText);
			}
			e.consume(this);

		} else if(e.key == GLFW_KEY_ESCAPE) {
			// same as pressing enter
			event::Action eAction;
			onAction(eAction);
			event::Deselect eDeselect;
			onDeselect(eDeselect);
			APP->event->selectedWidget = NULL;
			e.consume(this);

		} else {
			TextField::onSelectKey(e);
		}
	}

	void onSelectText(const event::SelectText &e) override {
		// assuming GLFW number keys are contiguous
		if(text.size() < maxCharacters
				&& GLFW_KEY_0  <= e.codepoint
				&& e.codepoint <= GLFW_KEY_9) {
			TextField::onSelectText(e);
		}
	}

	// custom event, called from within onAction
	virtual void onNumberSet(const int n) {};

	void onAction(const event::Action &e) override {
		if(text.size() > 0) {
			int n = stoi(text); // text should always contain only digits
			if(n > 0) {
				validText = string::f("%u", n);
				onNumberSet(n);
			}
		}
		text = validText;
		//if(gFocusedWidget == this) gFocusedWidget = NULL;
		if(APP->event->selectedWidget == this) APP->event->selectedWidget = NULL; //TODO: replace with onSelect / onDeselect (?) -- at least emit onDeselect, compare to TextField (?)
		e.consume(this);
	}

};
