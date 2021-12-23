#include "Widgets.hpp"

void TextBox::draw(const DrawArgs &args) {
	// based on LedDisplayChoice::draw() in Rack/src/app/LedDisplay.cpp
	const auto vg = args.vg;
	nvgScissor(vg, 0, 0, box.size.x, box.size.y);
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 3.0);
	nvgFillColor(vg, backgroundColor);
	nvgFill(vg);

	std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/RobotoMono-Bold.ttf"));
	if (font->handle >= 0) {

		nvgFillColor(vg, textColor);
		nvgFontFaceId(vg, font->handle);

		nvgFontSize(vg, font_size);
		nvgTextLetterSpacing(vg, letter_spacing);
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
	TextBox::draw(args);
	TextBox::setText(originalText);

	if(isFocused) {
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

		nvgRect(vg,
				textOffset.x + (begin - 0.5f * TextField::text.size()) * char_width - 1,
				ymargin,
				(len > 0 ? char_width * len : 1) + 1,
				TextBox::box.size.y - 2.f * ymargin);
		nvgFill(vg);

	}
}
