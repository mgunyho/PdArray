#include "Widgets.hpp"

void EditableTextBox::draw(NVGcontext *vg) {
	bool focus = this == gFocusedWidget;
	std::string tmp = HoverableTextBox::text;
	if(focus) {
		HoverableTextBox::setText("");
	} else {
		//HoverableTextBox::setText(text1);
	}
	HoverableTextBox::draw(vg);
	HoverableTextBox::setText(tmp);
	//TextField::setText(tmp);

	// copied from LedDisplayTextField
	//TODO: set multiline = false
	if(focus) {
		NVGcolor highlightColor = nvgRGB(0x0, 0x0, 0xd8);
		highlightColor.a = 0.5;
		int begin = min(cursor, selection);
		int end = focus ? max(cursor, selection) : -1; //TODO: use this to figure out defocus

//		void bndIconLabelCaret(NVGcontext *ctx, float x, float y, float w, float h,
//		    int iconid, NVGcolor color, float fontsize, const char *label,
//		    NVGcolor caretcolor, int cbegin, int cend);

		nvgFontFaceId(vg, HoverableTextBox::font->handle);
		nvgFontSize(vg, font_size);
		nvgTextLetterSpacing(vg, letter_spacing);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
		bndIconLabelCaret(vg, 0.f, textOffset.y,
				box.size.x, //box.size.x - 2*textOffset.x,
				box.size.y, //box.size.y - 2*textOffset.y, //TODO: this incorrectly limits line
				-1, textColor, font_size, TextField::text.c_str(), highlightColor, begin, end);
	}
}
