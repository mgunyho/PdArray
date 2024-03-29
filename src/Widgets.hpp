#include "plugin.hpp"

// TODO: same as in LittleUtils, move to common folder?
struct TextBox : TransparentWidget {
	// Kinda like TextField except not editable. Using Roboto Mono Bold font,
	// numbers look okay.
	// based on LedDisplayChoice
	std::string text;
	float fontSize;
	float letterSpacing;
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
		fontSize = 20;
		letterSpacing = 0.f;
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
 * example the box size is determined by TextBox::box.size, so that should be
 * the box to manipulate if necessary. TextField::text holds the string that is
 * being edited and TextBox::text holds the string being displayed, which
 * should always be a valid number.
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
		hoverColor = nvgRGB(0x90, 0x90, 0x90);
		maxTextLength = defaultMaxTextLength;

		// Default alignment and offset. Caret breaks with left-align, so let's
		// go with center for now. textOffset should be overridden by
		// subclasses if they change the box size.
		textAlign = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE;
		textOffset = Vec(TextBox::box.size.x / 2, TextBox::box.size.y / 2);
	}

	bool isNumber(const std::string& s) {
		// shamelessly copypasted from https://stackoverflow.com/questions/4654636/how-to-determine-if-a-string-is-a-number-with-c
		std::string::const_iterator it = s.begin();
		while (it != s.end() && std::isdigit(*it)) ++it;
		return !s.empty() && it == s.end();
	}

	void onHover(const event::Hover &e) override {
		TextField::onHover(e);
		e.consume(static_cast<TextBox *>(this)); // to catch onEnter and onLeave
	}

	void onDragHover(const event::DragHover &e) override {
		TextField::onDragHover(e);
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
		ButtonEvent ee(e);

		// Hacky way to make right-click behave the same as left click. We have
		// to do this, because TextField::pasteClipboard() is not virtual, so
		// we can't override it, so this is the only way to prevent pasting
		// text into the number field.
		// Note that in the case of a normal left click, setSelectedWidget(),
		// which calls onSelect() is called by the event loop (?) already
		// before handling the onButton event, but we can't intercept the
		// left/right click there. so we have to manually do it again here.
		if(e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ee.button = GLFW_MOUSE_BUTTON_LEFT;
		}

		if(ee.action == GLFW_PRESS && ee.button == GLFW_MOUSE_BUTTON_LEFT) {
			// hack, see above
			APP->event->setSelectedWidget(static_cast<TextField*>(this));
		}

		TextField::onButton(ee); // this handles consuming the event

		if(ee.isConsumed()) {
			e.consume(ee.getTarget());
		}
	}

	void onSelect(const event::Select &e) override {
		isFocused = true;
		e.consume(static_cast<TextField*>(this)); //TODO
	}

	void onDeselect(const event::Deselect &e) override {
		isFocused = false;
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

// For slight DRY
struct MenuItemWithRightArrow : MenuItem {
	MenuItemWithRightArrow(): MenuItem() {
		rightText = RIGHT_ARROW;
	};
};
