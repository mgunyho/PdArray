#include "plugin.hpp"

// TODO: same as in LittleUtils, move to common folder?
struct TextBox : TransparentWidget {
	// Kinda like TextField except not editable. Using Roboto Mono Bold font,
	// numbers look okay.
	// based on LedDisplayChoice
	std::string text;
	float font_size;
	float letter_spacing;
	Vec textOffset;
	NVGcolor defaultTextColor;
	NVGcolor textColor; // This can be used to temporarily override text color
	NVGcolor backgroundColor;
	int textAlign;

	//TODO: create<...>() thing with position as argument?
	TextBox() {
		defaultTextColor = nvgRGB(0x23, 0x23, 0x23);
		textColor = defaultTextColor;
		backgroundColor = nvgRGB(0x78, 0x78, 0x78);
		box.size = Vec(30, 18);
		// size 20 with spacing -2 will fit 3 characters on a 30px box with Roboto mono
		font_size = 20;
		letter_spacing = 0.f;
		textOffset = Vec(box.size.x * 0.5f, 0.f);
		textAlign = NVG_ALIGN_CENTER | NVG_ALIGN_TOP;
	}

	virtual void setText(std::string s) { text = s; }

	virtual void draw(const DrawArgs &args) override;

};

/*
 * Editable TextBox that only allows inputting positive numbers.
 *
 * Based on EditableTextbox and HoverableTextbox in LittleUtils. TextField is
 * inherited so that the default behavior of onSelectKey and some click events
 * can be handled by it. Otherwise everything should come from TextBox. For
 * example the box size is * determined by TextBox::box.size, so manipulate
 * that if necessary. TextField::text holds the string being edited and
 * TextBox::text holds the string being displayed, which should always be a
 * valid number.
 *
 * TODO: copy-paste docstring from LittleUtils EditableTextbox here and check that it's valid
 */
struct NumberTextBox : TextBox, TextField {

	// attributes from HoverableTextBox
	BNDwidgetState state = BND_DEFAULT;
	NVGcolor defaultColor;
	NVGcolor hoverColor;

	// attributes from EditableTextBox
	bool isFocused = false;
	const static unsigned int defaultMaxTextLength = 6;
	unsigned int maxTextLength;

	NumberTextBox(): TextBox(), TextField() {
		defaultColor = backgroundColor;
		hoverColor = nvgRGB(0x88, 0x88, 0x88);
		maxTextLength = defaultMaxTextLength;

		// Default alignment and offset. Caret breaks with left-align, so let's
		// go with center for now. textOffset should be overridden by
		// subclasses if they change the box size.
		textAlign = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE;
		textOffset = Vec(TextBox::box.size.x / 2, TextBox::box.size.y / 2);
	}

	void onHover(const event::Hover &e) override {
		TextField::onHover(e);
		e.consume(static_cast<TextBox *>(this)); // to catch onEnter and onLeave
	}

	void onHoverScroll(const event::HoverScroll &e) override {
		TextField::onHoverScroll(e);
	}

	void onEnter(const event::Enter &e) override {
		state = BND_HOVER;
	}

	void onLeave(const event::Leave &e) override {
		state = BND_DEFAULT;
	}

	void onButton(const event::Button &e) override {
		TextField::onButton(e); // this handles consuming the event
	}

	void onSelect(const event::Select &e) override {
		isFocused = true;
		e.consume(static_cast<TextField*>(this)); //TODO
	}

	void onDeselect(const event::Deselect &e) override {
		isFocused = false;
		TextBox::setText(TextField::text);
		e.consume(NULL);
	}

	void onAction(const event::Action &e) override;

	void onSelectText(const event::SelectText &e) override;
	void onSelectKey(const event::SelectKey &e) override;

	void step() override {
		TextField::step();
	}

	void draw(const DrawArgs &args) override;

	// custom event, called from within onAction
	virtual void onNumberSet(const int n) {};

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
			APP->event->setSelectedWidget(this);
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

// For slight DRY
struct MenuItemWithRightArrow : MenuItem {
	MenuItemWithRightArrow(): MenuItem() {
		rightText = RIGHT_ARROW;
	};
};
