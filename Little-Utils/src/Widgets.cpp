#include "Widgets.hpp"
#include "window.hpp" // WINDOW_MOD_CTRL
#include <GLFW/glfw3.h> // key codes

void TextBox::draw(const DrawArgs &args) {
	// based on LedDisplayChoice::draw() in Rack/src/app/LedDisplay.cpp
	nvgScissor(args.vg, 0, 0, box.size.x, box.size.y); //TODO: replace args.vg with just vg (?)

	nvgBeginPath(args.vg);
	nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3.0);
	nvgFillColor(args.vg, backgroundColor);
	nvgFill(args.vg);

	if (font->handle >= 0) {

		nvgFillColor(args.vg, textColor);
		nvgFontFaceId(args.vg, font->handle);

		nvgFontSize(args.vg, font_size);
		nvgTextLetterSpacing(args.vg, letter_spacing);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
		nvgText(args.vg, textOffset.x, textOffset.y, text.c_str(), NULL);
	}

	nvgResetScissor(args.vg);
}

void EditableTextBox::draw(const DrawArgs &args) {

	std::string tmp = HoverableTextBox::text;
	if(isFocused) {
		HoverableTextBox::setText(TextField::text);
	}

	HoverableTextBox::draw(args);
	HoverableTextBox::setText(tmp);

	if(isFocused) {
		NVGcolor highlightColor = nvgRGB(0x0, 0x90, 0xd8);
		highlightColor.a = 0.5;

		int begin = std::min(cursor, selection);
		int end = std::max(cursor, selection);
		int len = end - begin;

		// font face, size, alignment etc should be the same as for TextBox after the above draw call

		// hacky way of measuring character width
		NVGglyphPosition glyphs[4];
		nvgTextGlyphPositions(args.vg, 0.f, 0.f, "a", NULL, glyphs, 4); //TODO: replace args.vg with just vg
		float char_width = -2*glyphs[0].x;

		float ymargin = 2.f;
		nvgBeginPath(args.vg);
		nvgFillColor(args.vg, highlightColor);

		nvgRect(args.vg,
				textOffset.x + (begin - 0.5f * TextField::text.size()) * char_width - 1,
				ymargin,
				(len > 0 ? char_width * len : 1) + 1,
				HoverableTextBox::box.size.y - 2.f * ymargin);
		nvgFill(args.vg);

	}
}

void EditableTextBox::onAction(const event::Action &e) {
	printf("onAction()\n");
	//TextField::onAction(e);
	//e.consume(dynamic_cast<HoverableTextBox*>(this));
}

void EditableTextBox::onSelectText(const event::SelectText &e) {
	printf("onSelectText()\n");
	if(TextField::text.size() < maxTextLength) {
		TextField::onSelectText(e);
	} else {
		e.consume(NULL);
	}
}


void EditableTextBox::onButton(const event::Button &e) {
	printf("onButton()\n");
	//TextField::onButton(e); // textField consumes the event (?)
	e.consume(dynamic_cast<HoverableTextBox*>(this));
}

void EditableTextBox::onSelectKey(const event::SelectKey &e) {
	//printf("onSelectKey()\n");

	//TODO: shift+home/end to select until beginning / end
	if(false /*e.key == GLFW_KEY_V && (e.mods & WINDOW_MOD_MASK) == WINDOW_MOD_CTRL TODO: check*/) {
		// prevent pasting too long text
		//int pasteLength = maxTextLength - TextField::text.size();
		//if(pasteLength > 0) {
		//	std::string newText(glfwGetClipboardString(gWindow));
		//	if(newText.size() > pasteLength) newText.erase(pasteLength);
		//	insertText(newText);
		//}
		//e.consume(NULL); //TODO: null correct?

	} else if(false /*e.key == GLFW_KEY_ESCAPE && isFocused*/) {
		// defocus on escape
		//gFocusedWidget = NULL;
		//e.consumed = true;
		//TODO: check if this works

	} else {
		TextField::onSelectKey(e);
	}
	//e.consume(dynamic_cast<HoverableTextBox*>(this));
}

void EditableTextBox::onSelect(const event::Select &e) {
	printf("onSelect()\n");
	isFocused = true;
	//e.consume(dynamic_cast<HoverableTextBox*>(this));
	e.consume(dynamic_cast<TextField*>(this));
}

void EditableTextBox::onDeselect(const event::Deselect &e) {
	printf("onDeselect()\n");
	isFocused = false;
	//HoverableTextBox::setText(TextField::text);
	//e.consume(dynamic_cast<HoverableTextBox*>(this));
}
