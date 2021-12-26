#include "Widgets.hpp"
#include <algorithm> // std::replace

void TextBox::draw(const DrawArgs &args) {
	// based on LedDisplayChoice::draw() in Rack/src/app/LedDisplay.cpp
	const auto vg = args.vg;
	nvgScissor(vg, 0, 0, box.size.x, box.size.y);
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 3.0);
	nvgFillColor(vg, backgroundColor);
	nvgFill(vg);

	std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/RobotoMono-Bold.ttf"));
	if (font) {

		nvgFillColor(vg, textColor);
		nvgFontFaceId(vg, font->handle);

		nvgFontSize(vg, fontSize);
		nvgTextLetterSpacing(vg, letterSpacing);
		nvgTextAlign(vg, textAlign);
		nvgText(vg, textOffset.x, textOffset.y, text.c_str(), NULL);
	}

	nvgResetScissor(vg);
};


void NumberTextBox::draw(const DrawArgs &args) {
	auto vg = args.vg;
	backgroundColor = state == BND_HOVER ? hoverColor : defaultColor;

	std::string originalText = TextBox::text;

	if(isFocused) {
		// if we're editing, display TextField::text
		TextBox::setText(TextField::text);
	}
	std::string s = TextBox::text;
	std::replace(s.begin(), s.end(), '0', 'O');
	TextBox::setText(s);
	TextBox::draw(args);

	TextBox::setText(originalText);

	if(isFocused) {
		// This color is used for highlighting the selection and for the caret
		NVGcolor highlightColor = nvgRGB(0x0, 0x90, 0xd8);
		highlightColor.a = 0.5;

		int begin = std::min(cursor, selection);
		int end = std::max(cursor, selection);
		int len = end - begin;

		// font face, size, alignment etc should be the same as for TextBox after the above draw call

		// hacky way of measuring character width
		NVGglyphPosition glyphs[4];
		nvgTextGlyphPositions(vg, 0.f, 0.f, "0", NULL, glyphs, 4);
		float char_width = -2*glyphs[0].x;

		float ymargin = 2.f;
		nvgBeginPath(vg);
		nvgFillColor(vg, highlightColor);

		// draw highlight / caret. note that this assumes that the text is center-aligned.
		nvgRect(vg,
				textOffset.x + (begin - 0.5f * TextField::text.size()) * char_width - 1,
				ymargin,
				(len > 0 ? char_width * len : 1) + 1,
				TextBox::box.size.y - 2.f * ymargin);
		nvgFill(vg);

	}
}

void NumberTextBox::onAction(const event::Action &e) {
	// this gets fired when the user confirms the text input

	bool success = false;
	std::string inp = TextField::text;
	inp.erase(0, inp.find_first_not_of("0")); // trim leading zeros
	if(inp.size() > 0) {
		// here we assume that the text contains only digits
		int n = stoi(inp);
		if(n > 0) {
			// the number was valid, call onNumberSet and update display text
			onNumberSet(n);
			TextBox::setText(inp);
			TextField::setText(inp); // update trimmed leading zeros to TextField also
			success = true;
		}
	}

	if(!success) {
		// the input number was invalid, revert TextField to the text we were displaying before
		cursor = 0;
		selection = 0;
		TextField::setText(TextBox::text);
	}

	event::Deselect eDeselect;
	onDeselect(eDeselect);
	APP->event->setSelectedWidget(NULL);
	e.consume(NULL);
}

void NumberTextBox::onSelectText(const event::SelectText &e) {
	// only accpet numeric input

	// cursor != selection means that something is selected, so we may input
	// even if the textbox is full.
	if((TextField::text.size() < maxTextLength || cursor != selection)
			// assuming GLFW number keys are contiguous
			&& GLFW_KEY_0 <= e.codepoint
			&& e.codepoint <= GLFW_KEY_9) {
		TextField::onSelectText(e);
	} else {
		e.consume(NULL);
	}
}

void NumberTextBox::onSelectKey(const event::SelectKey &e) {
	// handle actual typing

	bool act = e.action == GLFW_PRESS || e.action == GLFW_REPEAT;
	if(act && e.key == GLFW_KEY_V && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
		// prevent pasting too long text
		int pasteLength = maxTextLength - TextField::text.size() + abs(selection - cursor);
		if(pasteLength > 0) {
			std::string newText(glfwGetClipboardString(APP->window->win));
			if(newText.size() > pasteLength) newText.erase(pasteLength);

			if(isNumber(newText)) {
				insertText(newText);
			}
		}
		// e is consumed below

	} else if(act && (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT && e.key == GLFW_KEY_HOME) {
		// don't move selection
		cursor = 0;
	} else if(act && (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT && e.key == GLFW_KEY_END) {
		cursor = TextField::text.size();
	} else if(act && e.key == GLFW_KEY_ESCAPE) {
		// escape is the same as pressing enter. onAction calls onDeselect().
		event::Action eAction;
		onAction(eAction);

		// e is consumed below

	} else {
		TextField::onSelectKey(e);
	}

	if(!e.isConsumed())
		e.consume(static_cast<TextField*>(this));
}
