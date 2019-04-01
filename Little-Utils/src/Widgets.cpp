#include "Widgets.hpp"
#include "window.hpp" // windowIsModPressed
#include <GLFW/glfw3.h> // key codes

void TextBox::draw(NVGcontext *vg) {
	// based on LedDisplayChoice::draw() in Rack/src/app/LedDisplay.cpp
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
}

void EditableTextBox::draw(NVGcontext *vg) {

	std::string tmp = HoverableTextBox::text;
	if(isFocused) {
		HoverableTextBox::setText(TextField::text);
	}

	HoverableTextBox::draw(vg);
	HoverableTextBox::setText(tmp);

	if(isFocused) {
		NVGcolor highlightColor = nvgRGB(0x0, 0x90, 0xd8);
		highlightColor.a = 0.5;

		int begin = min(cursor, selection);
		int end = max(cursor, selection);
		int len = end - begin;

		// font face, size, alignment etc should be the same as for TextBox after the above draw call

		// hacky way of measuring character width
		NVGglyphPosition glyphs[4];
		nvgTextGlyphPositions(vg, 0.f, 0.f, "a", NULL, glyphs, 4);
		float char_width = -2*glyphs[0].x;

		float ymargin = 2.f;
		nvgBeginPath(vg);
		nvgFillColor(vg, highlightColor);
		nvgRect(vg,
				textOffset.x + (begin - 0.5f * TextField::text.size()) * char_width - 1,
				ymargin,
				(len > 0 ? char_width * len : 1) + 1,
				box.size.y - 2.f * ymargin);
		nvgFill(vg);
	}
}

void EditableTextBox::onAction(EventAction &e) {
	// this should only be called by TextField when enter is pressed

	if(this == gFocusedWidget) {
		gFocusedWidget = NULL;
		EventAction e;
		//onDefocus(); // gets called anyway
	}
	e.consumed = true;
}

void EditableTextBox::onKey(EventKey &e) {
	//TODO: shift+home/end to select until beginning / end
	//std::cout << "windowIsShiftPressed():" << windowIsShiftPressed() << std::endl;
	//std::cout << "e.key == GLFW_KEY_END:" << (e.key == GLFW_KEY_END) << std::endl;
	//std::cout << "e.key == GLFW_KEY_HOME:" << (e.key == GLFW_KEY_HOME) << std::endl;
	if(e.key == GLFW_KEY_V && windowIsModPressed()) {
		// prevent pasting too long text
		int pasteLength = maxTextLength - TextField::text.size();
		if(pasteLength > 0) {
			std::string newText(glfwGetClipboardString(gWindow));
			if(newText.size() > pasteLength) newText.erase(pasteLength);
			insertText(newText);
		}
		e.consumed = true;

	} else if(e.key == GLFW_KEY_ESCAPE && isFocused) {
		// defocus on escape
		gFocusedWidget = NULL;
		e.consumed = true;

	} else {
		TextField::onKey(e);
	}
}
