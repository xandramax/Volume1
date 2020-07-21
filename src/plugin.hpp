#pragma once
#include <rack.hpp>
using namespace rack;
// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;
// Declare each Model, defined in each module source file
// extern Model* modelMyModule;
extern Model* modelAlgomorph4;

struct DLXKnob : RoundKnob {
	DLXKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXKnob.svg")));
		// shadow->opacity = 0.f;
	}
};

struct DLXPortPoly : SvgPort {
	DLXPortPoly() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXPort16.svg")));
		// shadow->opacity = 0.f;
	}
};

struct DLXPortPolyOut : SvgPort {
	DLXPortPolyOut() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXPort16b.svg")));
		// shadow->opacity = 0.f;
	}
};

struct DLXPortG : SvgPort {
	DLXPortG() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXPortG.svg")));
		// shadow->opacity = 0.f;
	}
};

struct TPurpleLight : GrayModuleLightWidget {
	TPurpleLight() {
		this->addBaseColor(nvgRGB(139, 112, 162));
	}
};

/* struct TWhitePurpleLight : GrayModuleLightWidget {
	TWhitePurpleLight() {
		this->addBaseColor(nvgRGB(231, 225, 236));
	}
}; */

struct LineLight : TPurpleLight {
	LineLight(Vec a, Vec b) {
		this->box.size = Vec(b.x - a.x, b.y - a.y);
	}
	void drawLight(const widget::Widget::DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0, 0);
		nvgLineTo(args.vg, this->box.size.x, this->box.size.y);
		if (this->color.a > 0.0) {
			nvgStrokeWidth(args.vg, 1.f);
			nvgStrokeColor(args.vg, this->color);
			nvgStroke(args.vg);
		}
	}
};