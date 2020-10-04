#pragma once
#include <rack.hpp>
#include <bitset>
using namespace rack;
extern Plugin* pluginInstance;
extern Model* modelAlgomorph4;

struct bitsetCompare {
    bool operator() (const std::bitset<16> &b1, const std::bitset<16> &b2) const {
        return b1.to_ulong() < b2.to_ulong();
	}
};

static const NVGcolor DLXDarkPurple = nvgRGB(0x14, 0x0d, 0x13);
static const NVGcolor DLXLightPurple = nvgRGB(139, 112, 162);
static const NVGcolor DLXRed = nvgRGB(0xae, 0x34, 0x58);
static const NVGcolor DLXYellow = nvgRGB(0xa9, 0xa9, 0x83);


/// Params

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

struct DLXPurpleButton : rack::app::SvgSwitch {
	int state = 0;

	DLXPurpleButton() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_Button_0c.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_Button_1c.svg")));
	}
};

struct DLXTL1105B : rack::app::SvgSwitch {
	int state = 0;

	DLXTL1105B() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_TL1105B_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_TL1105B_1.svg")));
	}
};


/// Lights

template <typename TBase = GrayModuleLightWidget>
struct TDlxPurpleLight : TBase {
	TDlxPurpleLight() {
		this->addBaseColor(DLXLightPurple);
	}
};
typedef TDlxPurpleLight<> DLXPurpleLight;

template <typename TBase = GrayModuleLightWidget>
struct TDlxRedLight : TBase {
	TDlxRedLight() {
		this->addBaseColor(DLXRed);
	}
};
typedef TDlxRedLight<> DLXRedLight;

template <typename TBase = GrayModuleLightWidget>
struct TDlxYellowLight : TBase {
	TDlxYellowLight() {
		this->addBaseColor(DLXYellow);
	}
};
typedef TDlxYellowLight<> DLXYellowLight;

template <typename TBase = GrayModuleLightWidget>
struct TDlxMultiLight : TBase {
	TDlxMultiLight() {
		this->addBaseColor(DLXLightPurple);
		this->addBaseColor(DLXYellow);
		this->addBaseColor(DLXRed);
	}
};
typedef TDlxMultiLight<> DLXMultiLight;

template <typename TBase = GrayModuleLightWidget>
struct TDlxScreenMultiLight : TBase {
	TDlxScreenMultiLight() {
		this->addBaseColor(DLXDarkPurple);
		this->addBaseColor(DLXYellow);
	}
};
typedef TDlxMultiLight<> DLXScreenMultiLight;

struct Line {
	Vec left, right;
	Vec pos, size;
	bool flipped;
	Line(Vec a, Vec b) {
		flipped = a.y > b.y;
		size = Vec(b.x - a.x, std::fabs(b.y - a.y));
		float startAngle, endAngle;
		if (flipped) {
			startAngle = std::atan2(-size.y, size.x);
			endAngle = std::atan2(size.x, -size.y);
		}
		else {
			startAngle = std::atan2(size.x, size.y);
			endAngle = std::atan2(size.y, size.x);
		}
		float radius = 8.462 + 1.7;		//ringLight radius + strokeWidth
		if (flipped) {
			left = Vec(radius * std::cos(startAngle) + a.x, radius * std::sin(startAngle) + a.y);
			right = Vec(b.x - radius * std::cos(startAngle),  b.y - radius * std::sin(startAngle));
		}
		else {
			left = Vec(radius * std::cos(endAngle) + a.x, radius * std::sin(endAngle) + a.y);
			right = Vec(b.x - radius * std::cos(endAngle),  b.y - radius * std::sin(endAngle));
		}
	}
};

struct ConnectionBgWidget : TransparentWidget {
	std::vector<Line> lines;
	ConnectionBgWidget(std::vector<Vec> left, std::vector<Vec> right) {
		for (unsigned i = 0; i < left.size(); i++) {
			for (unsigned j = 0; j < right.size(); j++) {
				lines.push_back(Line(left[i], right[j]));
			}
		}
	}

	void draw(const widget::Widget::DrawArgs& args) override {
        //Colors from GrayModuleLightWidget
		for (Line line : lines) {
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, line.left.x, line.left.y);
			nvgLineTo(args.vg, line.right.x, line.right.y);
			nvgStrokeWidth(args.vg, 1.1f);
			// Background
			nvgStrokeColor(args.vg, nvgRGB(0x5a, 0x5a, 0x5a));
			nvgStroke(args.vg);
			// Border
			nvgStrokeWidth(args.vg, 0.6);
			nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 0x60));
			nvgStroke(args.vg);
        }
    }
};

template < typename TBase = GrayModuleLightWidget >
struct TBacklight : TBase {
    void drawLight(const widget::Widget::DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, this->box.size.x, this->box.size.y, 3.675f);
		if (this->color.a > 0.0) {
			nvgFillColor(args.vg, this->color);
			nvgFill(args.vg);
		}
	}
	void drawHalo(const widget::Widget::DrawArgs& args) override {
		// float owidth = 1.2275f * this->box.size.x;
		// float oheight = 1.2275f * this->box.size.y;
		float xradius = 56.537f, yradius = 46.638f;
		float oxradius = xradius * 3.65f, oyradius = yradius * 3.65f;

		nvgBeginPath(args.vg);
		nvgRect(args.vg, xradius - oxradius, yradius - oyradius, 2.f * oxradius, 2.f * oyradius);
		
		NVGpaint p;
		float rx = (xradius + oxradius)*0.5f;
		float ry = (yradius + oyradius)*0.5f;
		float fx = (oxradius - xradius);
		NVG_NOTUSED(args.vg);
		memset(&p, 0, sizeof(p));
		nvgTransformIdentity(p.xform);
		p.xform[4] = xradius;
		p.xform[5] = yradius;
		p.extent[0] = rx;
		p.extent[1] = ry;
		p.radius = rx;
		p.feather = std::max(1.0f, fx);
		p.innerColor = color::mult(this->color, 0.07);
		p.outerColor = nvgRGB(0, 0, 0);
		
		nvgFillPaint(args.vg, p);
		nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
		nvgFill(args.vg);
	}
};
typedef TBacklight<> Backlight;

template <typename TBase = DLXScreenMultiLight>
TBacklight<TBase>* createBacklight(Vec pos, Vec size, engine::Module* module, int firstLightId) {
	TBacklight<TBase>* o = new TBacklight<TBase>();
	o->box.pos = pos;
	o->box.size = size;
	o->module = module;
	o->firstLightId = firstLightId;
	return o;
}

template <typename TBase = GrayModuleLightWidget>
struct TLineLight : TBase {
	bool flipped = false;
	TLineLight(Vec a, Vec b) {
		flipped = a.y > b.y;
		/* if (!flipped)
			a.y *= -1; */
		this->box.size = Vec(b.x - a.x, std::fabs(b.y - a.y));
		float angle = std::atan2(this->box.size.y, this->box.size.x);
		float radius = 8.462 + 1.7;		//ringLight radius + strokeWidth
		Vec start, end;
		if (flipped) {
			start = Vec(a.x + radius * std::cos(angle), a.y - radius * std::sin(angle));
			end = Vec(b.x - radius * std::cos(angle),  b.y + radius * std::sin(angle));
		}
		else {
			start = Vec(a.x + radius * std::cos(angle), a.y + radius * std::sin(angle));
			end = Vec(b.x - radius * std::cos(angle),  b.y - radius * std::sin(angle));
		}
		this->box.pos = start;
		if (flipped)
			this->box.pos.y = end.y;
		this->box.size = Vec(end.x - start.x, std::fabs(end.y - start.y));
		// this->box.pos.y *= -1;
	}
	void drawLight(const widget::Widget::DrawArgs& args) override {
		nvgBeginPath(args.vg);
		if (flipped) {
			nvgMoveTo(args.vg, 0.f, this->box.size.y);
			nvgLineTo(args.vg, this->box.size.x, 0.f);
		}
		else {
			nvgMoveTo(args.vg, 0.f, 0.f);
			nvgLineTo(args.vg, this->box.size.x, this->box.size.y);
		}
		nvgStrokeWidth(args.vg, .975f);
		// Foreground
		if (this->color.a > 0.0) {
			nvgStrokeColor(args.vg, this->color);
			nvgStroke(args.vg);
		}
	}
	void drawHalo(const widget::Widget::DrawArgs& args) override {
		/* float radius = std::min(this->box.size.x, this->box.size.y) / 2.0;
		float oradius = 3.0 * radius;

		nvgBeginPath(args.vg);
		if (flipped)
			nvgRect(args.vg, radius - oradius, radius - oradius - this->box.size.y, 2 * oradius, 2 * oradius);
		else
			nvgRect(args.vg, radius - oradius, radius - oradius, 2 * oradius, 2 * oradius);

		NVGpaint paint;
		NVGcolor icol = color::mult(this->color, 0.07);
		NVGcolor ocol = nvgRGB(0, 0, 0);
		if (flipped)
			paint = nvgRadialGradient(args.vg, radius, radius - this->box.size.y, radius, oradius, icol, ocol);
		else
			paint = nvgRadialGradient(args.vg, radius, radius, radius, oradius, icol, ocol);
		nvgFillPaint(args.vg, paint);
		nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
		nvgFill(args.vg); */
	}
};
typedef TLineLight<> LineLight;

template <typename TBase = DLXPurpleLight>
TLineLight<TBase>* createLineLight(Vec a, Vec b, engine::Module* module, int firstLightId) {
	TLineLight<TBase>* o = new TLineLight<TBase>(a, b);
	o->module = module;
	o->firstLightId = firstLightId;
	return o;
}

template <typename TBase = GrayModuleLightWidget>
struct TRingLight : TBase {
	float radius = 1.f;
	float strokeSize = 0.f;
	TRingLight(float r, float s = 0) {
		radius = r;
		strokeSize = s;
	}
	void drawLight(const widget::Widget::DrawArgs& args) override {
			// Adapted from LightWidget::drawLight, with no fill
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, radius, radius, radius);
			nvgStrokeWidth(args.vg, 1.2 + strokeSize);

			// Background
			if (this->bgColor.a > 0.0) {
				nvgStrokeColor(args.vg, this->bgColor);
				nvgStroke(args.vg);
				// nvgFillColor(args.vg, this->color);
				// nvgFill(args.vg);
			}

			// Foreground
			if (this->color.a > 0.0) {
				nvgStrokeWidth(args.vg, 1.3 + strokeSize);
				nvgStrokeColor(args.vg, this->color);
				nvgStroke(args.vg);
			}

			// Border
			if (this->borderColor.a > 0.0) {
				nvgStrokeWidth(args.vg, 0.5);
				nvgStrokeColor(args.vg, this->borderColor);
				nvgStroke(args.vg);
			}
	}
};
typedef TRingLight<> RingLight;

template <typename TBase = DLXPurpleLight>
TRingLight<TBase>* createRingLight(Vec pos, float r, engine::Module* module, int firstLightId, float s = 0) {
	TRingLight<TBase>* o = new TRingLight<TBase>(r, s);
	o->box.pos = pos;
	o->module = module;
	o->firstLightId = firstLightId;
	return o;
}

template <typename TBase = DLXPurpleLight>
TRingLight<TBase>* createRingLightCentered(Vec pos, float r, engine::Module* module, int firstLightId, float s = 0) {
	TRingLight<TBase>* o = createRingLight<TBase>(pos, r, module, firstLightId, s);
	o->box.pos.x -= r;
	o->box.pos.y -= r;
	return o;
}

struct SvgLight : ModuleLightWidget {
	widget::SvgWidget* sw;

	SvgLight() {
		sw = new widget::SvgWidget;
		this->addChild(sw);
	};

	void draw(const DrawArgs& args) override {
		sw->draw(args);
		Widget::draw(args);
	}
};

template <class TSvgLight>
TSvgLight* createSvgLight(Vec pos, engine::Module* module, int firstLightId) {
	TSvgLight* o = new TSvgLight;
	o->box.pos = pos;
	o->module = module;
	if (module)
		o->firstLightId = firstLightId;
	return o;
}

struct SvgSwitchLight : SvgLight {
	std::vector<std::shared_ptr<Svg>> frames;
	engine::ParamQuantity* paramQuantity = NULL;
	float dirtyValue = NAN;
	/** Return to original position when released */
	bool momentary = false;
	/** Hysteresis state for momentary switch */
	bool momentaryPressed = false;
	bool momentaryReleased = false;

	/** Adds an SVG file to represent the next switch position */
	void addFrame(std::shared_ptr<Svg> svg) {
		frames.push_back(svg);
		// If this is our first frame, automatically set SVG and size
		if (!sw->svg) {
			sw->setSvg(svg);
			box.size = sw->box.size;
		}
	};
	void onChange(const event::Change& e) override {
		if (!frames.empty() && paramQuantity) {
			int index = (int) std::round(paramQuantity->getValue() - paramQuantity->getMinValue());
			index = math::clamp(index, 0, (int) frames.size() - 1);
			sw->setSvg(frames[index]);
		}
	};

	void step() override {
		if (momentaryPressed) {
			momentaryPressed = false;
			// Wait another frame.
		}
		else if (momentaryReleased) {
			momentaryReleased = false;
			if (paramQuantity) {
				// Set to minimum value
				paramQuantity->setMin();
			}
		}
		if (this->paramQuantity) {
			float value = paramQuantity->getValue();
			// Trigger change event when paramQuantity value changes
			if (value != dirtyValue) {
				dirtyValue = value;
				event::Change eChange;
				onChange(eChange);
			}
		}
		Widget::step();
	}

	void onDoubleClick(const event::DoubleClick& e) override {
		// Don't reset parameter on double-click
	}

	void onDragStart(const event::DragStart& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;

		if (momentary) {
			if (paramQuantity) {
				// Set to maximum value
				paramQuantity->setMax();
				momentaryPressed = true;
			}
		}
		else {
			if (paramQuantity) {
				float oldValue = paramQuantity->getValue();
				if (paramQuantity->isMax()) {
					// Reset value back to minimum
					paramQuantity->setMin();
				}
				else {
					// Increment value by 1
					paramQuantity->setValue(std::round(paramQuantity->getValue()) + 1.f);
				}

				float newValue = paramQuantity->getValue();
				if (oldValue != newValue) {
					// Push ParamChange history action
					history::ParamChange* h = new history::ParamChange;
					h->name = "move switch";
					h->moduleId = paramQuantity->module->id;
					h->paramId = paramQuantity->paramId;
					h->oldValue = oldValue;
					h->newValue = newValue;
					APP->history->push(h);
				}
			}
		}
	}

	void onDragEnd(const event::DragEnd& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;

		if (momentary) {
			momentaryReleased = true;
		}
	}

	void reset() {
		if (paramQuantity && !momentary) {
			paramQuantity->reset();
		}
	}
};

template <class TSvgSwitchLight>
TSvgSwitchLight* createSvgSwitchLight(Vec pos, engine::Module* module, int firstLightId, int paramId) {
	TSvgSwitchLight* o = new TSvgSwitchLight;
	o->box.pos = pos;
	o->module = module;
	if (module) {
		o->firstLightId = firstLightId;
		o->paramQuantity = module->paramQuantities[paramId];
	}
	return o;
}

template <class TSvgSwitchLight>
TSvgSwitchLight* createSvgSwitchLightCentered(Vec pos, engine::Module* module, int firstLightId, int paramId) {
	TSvgSwitchLight* o = createSvgSwitchLight<TSvgSwitchLight>(pos, module, firstLightId, paramId);
	o->box.pos = o->box.pos.minus(o->box.size.div(2));
	return o;
}

struct DLXPencilLight : SvgSwitchLight {
	int state = 0;

	DLXPencilLight() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_PencilLight_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_PencilLight_1.svg")));
	}
};

struct DLXKnobLight : ModuleLightWidget {
	float angle = 0;
	float transform[6];

	DLXKnobLight() {
		box.size = mm2px(Vec(10.322, 10.322));
	}

	void draw(const DrawArgs& args) override {
		math::Vec center = Vec(4.081, 4.081);

		nvgBeginPath(args.vg);
		nvgScale(args.vg, 3.745, 3.745);
		nvgCircle(args.vg, center.x, center.y, center.x - .4);
		nvgStrokeColor(args.vg, DLXLightPurple);
		nvgStrokeWidth(args.vg, .2785);
		nvgStroke(args.vg);

		nvgBeginPath(args.vg);
		nvgTransformIdentity(transform);
		float t[6];
		nvgTransformTranslate(t, center.x, center.y);
		nvgTransformPremultiply(transform, t);
		nvgTransformRotate(t, angle);
		nvgTransformPremultiply(transform, t);
		nvgTransformTranslate(t, center.neg().x, center.neg().y);
		nvgTransformPremultiply(transform, t);
		nvgTransform(args.vg, transform[0], transform[1], transform[2], transform[3], transform[4], transform[5]);
		nvgMoveTo(args.vg, 4.2915104,0.794608);
		nvgBezierTo(args.vg, 4.3143438,0.62833073, 4.1037866,0.52002026, 3.9662013,0.59255105);
		nvgBezierTo(args.vg, 3.8556057,0.63433429, 3.8183122,0.76099231, 3.8537918,0.86619144);
		nvgBezierTo(args.vg, 3.9560394,1.5850019, 3.8517625,2.3219891, 3.6287271,3.0087598);
		nvgBezierTo(args.vg, 3.6092158,3.080529, 3.501243,3.2593293, 3.655733,3.2154921);
		nvgBezierTo(args.vg, 3.796758,3.1799277, 3.9220276,3.0926393, 4.0670114,3.072624);
		nvgBezierTo(args.vg, 4.2265284,3.0902694, 4.3623073,3.2020904, 4.5231983,3.2115989);
		nvgBezierTo(args.vg, 4.5983946,3.15853, 4.489421,3.0422152, 4.4852236,2.9611388);
		nvgBezierTo(args.vg, 4.2754123,2.2639957, 4.179022,1.5182965, 4.2915104,0.794608);
		nvgClosePath(args.vg);

		nvgFillColor(args.vg, nvgRGB(0xc3, 0xc3, 0xc3));
		nvgFill(args.vg);
		nvgScale(args.vg, 1, 1);
		Widget::draw(args);
	}
};

template <class TKnobLight = DLXKnobLight>
struct DLXLightKnob : RoundKnob {
	TKnobLight* sibling;

	DLXLightKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXKnobB.svg")));
	}

	void setSibling(TKnobLight* kl) {
		sibling = kl;
	}

	void onChange(const event::Change& e) override {
		// Re-transform the widget::TransformWidget and pdate the sibling's transformation angle
		if (paramQuantity) {
			float angle;
			if (paramQuantity->isBounded()) {
				angle = math::rescale(paramQuantity->getScaledValue(), 0.f, 1.f, minAngle, maxAngle);
			}
			else {
				angle = math::rescale(paramQuantity->getValue(), -1.f, 1.f, minAngle, maxAngle);
			}
			angle = std::fmod(angle, 2 * M_PI);
			sibling->angle = angle;
			// Rotate SVG
			tw->identity();
			math::Vec center = sw->box.getCenter();
			tw->translate(center);
			tw->rotate(angle);
			tw->translate(center.neg());
			fb->dirty = true;
		}
		Knob::onChange(e);
	}
};

template <class TKnobLight = DLXKnobLight, class TDLXLightKnob = DLXLightKnob<TKnobLight>>
TDLXLightKnob* createLightKnob(Vec pos, engine::Module* module, int paramId, TKnobLight* kl) {
	TDLXLightKnob* o = new TDLXLightKnob();
	o->box.pos = pos;
	if (module) {
		o->paramQuantity = module->paramQuantities[paramId];
	}
	o->setSibling(kl);
	return o;
}

///Glowing Ink

struct DLXGlowingInk : SvgLight {
	DLXGlowingInk() {
		sw->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/GlowingInk.svg")));
	}
};