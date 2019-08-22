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
		font = APP->window->loadFont(assetPlugin(pluginInstance, "res/RobotoMono-Bold.ttf")); // TODO: move fonts to subfolder, fix paths...
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
		nvgScissor(args.vg, 0, 0, box.size.x, box.size.y); //TODO: replace args.vg with just vg
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
	};

};
