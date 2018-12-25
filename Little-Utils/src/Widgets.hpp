#include "rack.hpp"

struct TextBox : TransparentWidget {
	// Kinda like TextField except not editable. Using Roboto Mono Bold font,
	// numbers look okay.
	// based on LedDisplayChoice
	std::string text;
	std::shared_ptr<Font> font;
	Vec textOffset;
	NVGcolor color;

	//TODO: create<...>() thing with position as argument?
	TextBox() {
		font = Font::load(assetPlugin(plugin, "res/RobotoMono-Bold.ttf"));
		color = nvgRGB(0x23, 0x23, 0x23);
		box.size = Vec(30, 18);
		textOffset = Vec(box.size.x * 0.5f, 0.f);
	}

	void setText(std::string s) { text = s; }

	void draw(NVGcontext *vg) override {
		// based on LedDisplayChoice::draw() in Rack/src/app/LedDisplay.cpp
		nvgScissor(vg, 0, 0, box.size.x, box.size.y);

		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 3.0);
		nvgFillColor(vg, nvgRGB(0xc8, 0xc8, 0xc8));
		nvgFill(vg);

		if (font->handle >= 0) {
			nvgFillColor(vg, color);
			nvgFontFaceId(vg, font->handle);

			nvgFontSize(vg, 20);
			nvgTextLetterSpacing(vg, -2.0);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			nvgText(vg, textOffset.x, textOffset.y, text.c_str(), NULL);
		}

		nvgResetScissor(vg);
	}
};
