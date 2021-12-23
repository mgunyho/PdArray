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
	backgroundColor = state == BND_HOVER ? hoverColor : defaultColor;
	TextBox::draw(args);
}
