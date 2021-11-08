#pragma once
#include "plugin.hpp" // For constants
#include <rack.hpp>
using rack::math::Vec;
using rack::engine::Module;
using rack::widget::Widget;
using rack::window::Svg;
using rack::asset::plugin;


// Component Library

struct DLXPJ301MPort : rack::componentlibrary::PJ301MPort {
	void draw(const DrawArgs &args) override {
		rack::componentlibrary::PJ301MPort::draw(args);
		nvgBeginPath(args.vg);
		nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_SRC_COLOR, NVG_ONE);
		nvgCircle(args.vg, this->getBox().size.x / 2.f, this->getBox().size.x / 2.f, this->getBox().size.x / 2.f);
		nvgFillColor(args.vg, nvgRGB(0x0D, 0x00, 0x16));
		nvgFill(args.vg);
	}
};

struct DLXPurpleButton : rack::componentlibrary::TL1105 {
	void draw(const DrawArgs &args) override {
		rack::componentlibrary::TL1105::draw(args);
		nvgBeginPath(args.vg);
		nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
		nvgCircle(args.vg, this->getBox().size.x / 2.f, this->getBox().size.x / 2.f, this->getBox().size.x / 2.f);
		nvgFillColor(args.vg, nvgRGB(0x0D, 0x00, 0x16));
		nvgFill(args.vg);
	}
};


/// Lights

template <typename TBase = rack::GrayModuleLightWidget>
struct TDlxPurpleLight : TBase {
	TDlxPurpleLight() {
		this->addBaseColor(DLXLightPurple);
	}
};
typedef TDlxPurpleLight<> DLXPurpleLight;

template <typename TBase = rack::GrayModuleLightWidget>
struct TDlxRedLight : TBase {
	TDlxRedLight() {
		this->addBaseColor(DLXRed);
	}
};
typedef TDlxRedLight<> DLXRedLight;

template <typename TBase = rack::GrayModuleLightWidget>
struct TDlxYellowLight : TBase {
	TDlxYellowLight() {
		this->addBaseColor(DLXYellow);
	}
};
typedef TDlxYellowLight<> DLXYellowLight;

template <typename TBase = rack::GrayModuleLightWidget>
struct TDlxMultiLight : TBase {
	TDlxMultiLight() {
		this->addBaseColor(DLXLightPurple);
		this->addBaseColor(DLXYellow);
		this->addBaseColor(DLXRed);
        this->bgColor = SCHEME_DARK_GRAY;
        this->borderColor = SCHEME_LIGHT_GRAY;
	}
};
typedef TDlxMultiLight<> DLXMultiLight;

template <typename TBase = rack::GrayModuleLightWidget>
struct TDlxScreenMultiLight : TBase {
	TDlxScreenMultiLight() {
		this->addBaseColor(DLXDarkPurple);
		this->addBaseColor(DLXYellow);
		this->addBaseColor(DLXRed);
	}
};
typedef TDlxScreenMultiLight<> DLXScreenMultiLight;

template < typename TBase = rack::GrayModuleLightWidget >
struct TBacklight : TBase {
	void drawBackground(const Widget::DrawArgs& args) override {};

    void drawLight(const Widget::DrawArgs& args) override {
		// Don't draw backlight if rendering in a framebuffer, e.g. screenshots or Module Browser
		// From LightWidget::drawHalo()
		if (args.fb)
			return;

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, this->box.size.x, this->box.size.y, 3.675f);
		if (this->color.a > 0.0) {
			nvgFillColor(args.vg, this->color);
			nvgFill(args.vg);
		}
	}

	void drawHalo(const Widget::DrawArgs& args) override {
		// Don't draw halo if rendering in a framebuffer, e.g. screenshots or Module Browser
		if (args.fb)
			return;

		const float halo = rack::settings::haloBrightness;
		if (halo == 0.f)
			return;

		// If light is off, rendering the halo gives no effect.
		if (this->color.r == 0.f && this->color.g == 0.f && this->color.b == 0.f)
			return;

		float ixradius = this->box.size.x / 8.f;
		float iyradius = this->box.size.y / 8.f;
		float oxradius = ixradius * 1.125f;
		float oyradius = iyradius * 1.125f;
		float x =  -oxradius;
		float y = -oyradius;
		float w = this->box.size.x + oxradius * 2.f;
		float h = this->box.size.y + oyradius * 2.f;

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, x - w, y - h, w * 3.f, h * 3.f, h * 1.5f);

		NVGcolor icol = rack::color::mult(this->color, halo);
		NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
		NVGpaint paint = nvgBoxGradient(args.vg, x, y, w, h, h / 2.f, h, icol, ocol);
		nvgFillPaint(args.vg, paint);
		nvgFill(args.vg);
	}
};
typedef TBacklight<> Backlight;

template <typename TBase = DLXScreenMultiLight>
TBacklight<TBase>* createBacklight(Vec pos, Vec size, Module* module, int firstLightId) {
	TBacklight<TBase>* o = new TBacklight<TBase>();
	o->box.pos = pos;
	o->box.size = size;
	o->module = module;
	o->firstLightId = firstLightId;
	return o;
}

template <typename TBase = rack::GrayModuleLightWidget>
struct TLineLight : TBase {
	Vec start, end;
	bool flipped = false;
	float angle = 0.f;
	float length = 0.f;

	TLineLight(Vec a, Vec b) {
		flipped = a.y > b.y;

		this->box.size = Vec(b.x - a.x, std::fabs(b.y - a.y));
		angle = std::atan2(this->box.size.y, this->box.size.x);
		if (flipped) {
			start = Vec(a.x + RING_RADIUS * std::cos(angle), a.y - RING_RADIUS * std::sin(angle));
			end = Vec(b.x - RING_RADIUS * std::cos(angle),  b.y + RING_RADIUS * std::sin(angle));
		}
		else {
			start = Vec(a.x + RING_RADIUS * std::cos(angle), a.y + RING_RADIUS * std::sin(angle));
			end = Vec(b.x - RING_RADIUS * std::cos(angle),  b.y - RING_RADIUS * std::sin(angle));
		}
		
		this->box.pos = start;
		if (flipped)
			this->box.pos.y = end.y;
		this->box.size = Vec(end.x - start.x, std::fabs(end.y - start.y));

		//Convert to relative coordinates
		if (flipped) {
			start = Vec(0.f, this->box.size.y);
			end = Vec(this->box.size.x, 0.f);
		}
		else {
			start = Vec(0.f, 0.f);
			end = Vec(this->box.size.x, this->box.size.y);
		}
		
		length = sqrtf(powf(end.x - start.x, 2) + powf(end.y - start.y, 2)) + LINE_LIGHT_STROKEWIDTH * 2.f;
	}

	void drawBackground(const Widget::DrawArgs& args) override {
		return;
	};

	void drawLight(const Widget::DrawArgs& args) override {
		// Don't draw lineLight if rendering in a framebuffer, e.g. screenshots or Module Browser
		// From LightWidget::drawHalo()
		if (args.fb)
			return;

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, start.x, start.y);
		nvgLineTo(args.vg, end.x, end.y);

		// Foreground
		if (this->color.a > 0.0) {
			nvgStrokeWidth(args.vg, LINE_LIGHT_STROKEWIDTH);
			nvgStrokeColor(args.vg, this->color);
			nvgStroke(args.vg);
		}
	}

	void drawHalo(const Widget::DrawArgs& args) override {
		// Don't draw halo if rendering in a framebuffer, e.g. screenshots or Module Browser
		// From LightWidget::drawHalo()
		if (args.fb)
			return;

		const float halo = rack::settings::haloBrightness;
		if (halo == 0.f)
			return;

		// If light is off, rendering the halo gives no effect.
		if (this->color.r == 0.f && this->color.g == 0.f && this->color.b == 0.f)
			return;

		float radius = LINE_LIGHT_STROKEWIDTH;
		float oradius = radius * 5.f;
		float x = start.x - oradius - radius;
		float y = start.y - oradius - radius * 0.5f;
		float w = length + oradius * 2.f;
		float h = radius + oradius * 2.f;

		nvgBeginPath(args.vg);
		if (flipped) {
			nvgTranslate(args.vg, 0.f, this->box.size.y);
			nvgRotate(args.vg, -angle);
			nvgTranslate(args.vg, 0.f, -this->box.size.y);
			// angle += .003f;	// Weeee!
		}
		else
			nvgRotate(args.vg, angle);
		nvgRoundedRect(args.vg, x - w, y - h, w * 3.f, h * 3.f, h * 1.5f);

		NVGcolor icol = rack::color::mult(this->color, halo);
		NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
		NVGpaint paint = nvgBoxGradient(args.vg, x, y, w, h, h * 0.5f, h, icol, ocol);
		nvgFillPaint(args.vg, paint);
		nvgFill(args.vg);
	}
};
typedef TLineLight<> LineLight;

template <typename TBase = DLXPurpleLight>
TLineLight<TBase>* createLineLight(Vec a, Vec b, Module* module, int firstLightId) {
	TLineLight<TBase>* o = new TLineLight<TBase>(a, b);
	o->module = module;
	o->firstLightId = firstLightId;
	return o;
}

template <typename TBase = rack::GrayModuleLightWidget>
struct TRingLight : TBase {
	float radius = 0.f;

	TRingLight(float r) {
		this->radius = r;
	}
	
	void drawBackground(const Widget::DrawArgs& args) override {
		// Adapted from LightWidget::drawBackground, with no fill
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, this->radius, this->radius, this->radius);

		// Background
		if (this->bgColor.a > 0.0) {
			nvgStrokeWidth(args.vg, RING_BG_STROKEWIDTH);
			nvgStrokeColor(args.vg, this->bgColor);
			nvgStroke(args.vg);
		}

		// Border
		if (this->borderColor.a > 0.0) {
			nvgStrokeWidth(args.vg, RING_BORDER_STROKEWIDTH);
			nvgStrokeColor(args.vg, this->borderColor);
			nvgStroke(args.vg);
		}
	}

	void drawLight(const Widget::DrawArgs& args) override {
		// Don't draw ringLight if rendering in a framebuffer, e.g. screenshots or Module Browser
		// From LightWidget::drawHalo()
		if (args.fb)
			return;

		// Adapted from LightWidget::drawLight, with no fill
		if (this->color.a > 0.0) {
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, this->radius, this->radius, this->radius);
			nvgStrokeWidth(args.vg, RING_LIGHT_STROKEWIDTH);
			nvgStrokeColor(args.vg, this->color);
			nvgStroke(args.vg);
		}
	}

	void drawHalo(const Widget::DrawArgs& args) override {
		// Don't draw halo if rendering in a framebuffer, e.g. screenshots or Module Browser
		if (args.fb)
			return;

		const float halo = rack::settings::haloBrightness;
		if (halo == 0.f)
			return;

		// If light is off, rendering the halo gives no effect.
		if (this->color.r == 0.f && this->color.g == 0.f && this->color.b == 0.f)
			return;

		Vec c = this->box.size.div(2);

		// Outer halo
		float iradius = RING_LIGHT_STROKEWIDTH + this->radius;
		float oradius = RING_LIGHT_STROKEWIDTH * 9.125f + this->radius;
		nvgBeginPath(args.vg);
		nvgRect(args.vg, c.x - oradius, c.y - oradius, 2 * (oradius), 2 * (oradius));
		nvgPathWinding(args.vg, NVG_HOLE);
		nvgCircle(args.vg, c.x, c.y, this->radius);
		NVGcolor icol = rack::color::mult(this->color, halo);
		NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
		NVGpaint paint = nvgRadialGradient(args.vg, c.x, c.y, iradius, oradius, icol, ocol);
		nvgFillPaint(args.vg, paint);
		nvgFill(args.vg);

		// Inner halo
		iradius = -RING_LIGHT_STROKEWIDTH * 9.125f + this->radius;
		oradius = this->radius;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, this->radius);
		paint = nvgRadialGradient(args.vg, c.x, c.y, iradius, oradius, ocol, icol);
		nvgFillPaint(args.vg, paint);
		nvgFill(args.vg);
	}
};
typedef TRingLight<> RingLight;

template <typename TBase = DLXPurpleLight>
TRingLight<TBase>* createRingLight(Vec pos, Module* module, int firstLightId, float r = RING_RADIUS) {
	TRingLight<TBase>* o;
	o = new TRingLight<TBase>(r);
	o->box.size = Vec(r * 2, r * 2);
	o->box.pos = pos;
	o->module = module;
	o->firstLightId = firstLightId;
	return o;
}

template <typename TBase = DLXPurpleLight>
TRingLight<TBase>* createRingLightCentered(Vec pos, Module* module, int firstLightId, float r = RING_RADIUS) {
	TRingLight<TBase>* o = createRingLight<TBase>(pos, module, firstLightId, r);
	o->box.pos.x -= r;
	o->box.pos.y -= r;
	return o;
}

struct RingIndicatorRotor {
	float angle = 0.f;

	void step(float deltaTime) {
		if (angle >= 2.f * M_PI)
			angle = 0.f;
		else
			angle += 4.f * deltaTime;
	}
};

template < typename MODULE >
struct DLXRingIndicator : DLXMultiLight {
	float angle = 0.f;
	float transform[6];
	float radius = 0.f;
	
	DLXRingIndicator(float r) {
		this->radius = r;
	}

	void drawBackground(const Widget::DrawArgs& args) override {
		if (!module)
			return;

		rotate(args);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, this->radius, this->radius, RING_BG_STROKEWIDTH);
		nvgFillColor(args.vg, this->bgColor);
		nvgFill(args.vg);
	}

	void drawLight(const Widget::DrawArgs& args) override {
		if (!module)
			return;

		if (this->color.a > 0.0) {
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, this->radius, this->radius, RING_LIGHT_STROKEWIDTH);
			nvgFillColor(args.vg, this->color);
			nvgFill(args.vg);
		}
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && module) {
			rotate(args);
		}

		DLXMultiLight::drawLayer(args, layer);
	}

	inline void rotate(const DrawArgs& args) {
		angle = dynamic_cast<MODULE*>(module)->rotor.angle;
		Vec center = Vec(radius, radius);
		nvgTransformIdentity(transform);
		float t[6];
		nvgTransformTranslate(t, center.x, center.y);
		nvgTransformPremultiply(transform, t);
		nvgTransformRotate(t, angle);
		nvgTransformPremultiply(transform, t);
		nvgTransformTranslate(t, center.neg().x, center.neg().y);
		nvgTransformPremultiply(transform, t);
		nvgTransform(args.vg, transform[0], transform[1], transform[2], transform[3], transform[4], transform[5]);
	}
};

template < typename MODULE >
DLXRingIndicator<MODULE>* createRingIndicator(Vec pos, Module* module, int firstLightId, float r = RING_RADIUS) {
    DLXRingIndicator<MODULE>* o = new DLXRingIndicator<MODULE>(r);
    o->box.pos = pos;
    o->module = module;
    o->firstLightId = firstLightId;
    return o;
}

template < typename MODULE >
DLXRingIndicator<MODULE>* createRingIndicatorCentered(Vec pos, Module* module, int firstLightId, float r = RING_RADIUS) {
    DLXRingIndicator<MODULE>* o = createRingIndicator<MODULE>(pos, module, firstLightId, r);
    o->box.pos.x -= r;
    o->box.pos.y -= r;
    return o;
}

struct DLXSvgLight : rack::widget::SvgWidget {
	void draw(const DrawArgs& args) override {
		// Do not call SvgWidget::draw, as it will draw on the wrong layer
		Widget::draw(args);
	}

	virtual void drawHalo(const DrawArgs& args) {
		return;
	};

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {	
			//From SvgWidget::draw()
			if (!svg)
				return;

			// Scale from max brightness to min brightness, as rack brightness is reduced from one to zero
			nvgAlpha(args.vg, (1.f - SVG_LIGHT_MIN_ALPHA) * rack::settings::rackBrightness + SVG_LIGHT_MIN_ALPHA);
			
			// From LightWidget::drawLayer()
			// Use the formula `lightColor * (1 - dest) + dest` for blending
			nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
			rack::window::svgDraw(args.vg, svg->handle);
			drawHalo(args);
		}

		rack::widget::SvgWidget::drawLayer(args, layer);
	}
};

struct DLXKnobLight : DLXSvgLight {
	void drawHalo(const DrawArgs& args) override {
		const float halo = rack::settings::haloBrightness;
		if (halo == 0.f)
			return;

		Vec c = this->box.size.div(2);

		nvgGlobalAlpha(args.vg, 1.f/3.f);

		// Indicator halo
		float radius = LINE_LIGHT_STROKEWIDTH * 1.5f;
		float oradius = radius * 1.5f;
		float x = c.x - oradius - radius * 0.5f;
		float y = -oradius - radius;
		float w = radius + oradius * 2.f;
		float h = c.y + oradius * 2.f;
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, x - w, y - h, w * 3.f, h * 3.f, w * 1.5f);
		NVGcolor icol = rack::color::mult(rack::componentlibrary::SCHEME_LIGHT_GRAY, halo);
		NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
		NVGpaint paint = nvgBoxGradient(args.vg, x, y, w, h, w * 0.5f, w, icol, ocol);
		nvgFillPaint(args.vg, paint);
		nvgFill(args.vg);

		// Outer halo
		radius = c.x;
		float iradius = RING_LIGHT_STROKEWIDTH + radius;
		oradius = RING_LIGHT_STROKEWIDTH * 9.125f + radius;
		nvgBeginPath(args.vg);
		nvgRect(args.vg, c.x - oradius, c.y - oradius, 2 * (oradius), 2 * (oradius));
		nvgPathWinding(args.vg, NVG_HOLE);
		nvgCircle(args.vg, c.x, c.y, radius);
		paint = nvgRadialGradient(args.vg, c.x, c.y, iradius, oradius, icol, ocol);
		nvgFillPaint(args.vg, paint);
		nvgFill(args.vg);

		// Inner halo
		iradius = -RING_LIGHT_STROKEWIDTH * 11.f + radius;
		oradius = radius;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, radius);
		paint = nvgRadialGradient(args.vg, c.x, c.y, iradius, oradius, ocol, icol);
		nvgFillPaint(args.vg, paint);
		nvgFill(args.vg);
	}
};

struct DLXLargeKnobLight : DLXKnobLight {
	DLXLargeKnobLight() {
		setSvg(Svg::load(rack::asset::system("res/ComponentLibrary/RoundHugeBlackKnob.svg")));
	}
};

struct DLXMediumKnobLight : DLXKnobLight {
	DLXMediumKnobLight() {
		setSvg(Svg::load(rack::asset::system("res/ComponentLibrary/RoundLargeBlackKnob.svg")));
	}
};

struct DLXSmallKnobLight : DLXKnobLight {
	DLXSmallKnobLight() {
		setSvg(Svg::load(rack::asset::system("res/ComponentLibrary/RoundSmallBlackKnob.svg")));
	}
};

template <typename THoleMask>
struct DLXDonutLargeKnobLight : DLXLargeKnobLight {
	float hole_radius = 0.f;

	DLXDonutLargeKnobLight() {
		//Create knob to extract radius
		THoleMask* mask = new THoleMask();
		hole_radius = mask->sw->box.size.x / 2.f;
		mask->requestDelete();
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		DLXLargeKnobLight::drawLayer(args, layer);

		//Cut hole
		nvgGlobalCompositeOperation(args.vg, NVG_DESTINATION_OUT);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, this->getBox().size.x / 2.f, this->getBox().size.x / 2.f, hole_radius);
		nvgFill(args.vg);
	}
};

template < typename DLXKnobLight >
struct DLXLightKnob : rack::app::SvgKnob {
	// "sw" is used for the bg svg widget. light is the fg svg widget.
	// This allows use of SvgKnob::setSvg() for the background svg.
	DLXKnobLight* light;
	rack::widget::FramebufferWidget* bg_fb;

	DLXLightKnob() {
		rack::app::SvgKnob();
	
		//From RoundKnob::RoundKnob()
		minAngle = -0.83 * M_PI;
		maxAngle = 0.83 * M_PI;

		bg_fb = new rack::widget::FramebufferWidget;
		addChildBottom(bg_fb);

		this->tw->removeChild(this->sw);
		bg_fb->addChild(this->sw);

		light = new DLXKnobLight();
		this->tw->addChild(light);
	};

	void setSvg(std::shared_ptr<Svg> svg) {
		rack::app::SvgKnob::setSvg(svg);
		bg_fb->box.size = this->sw->box.size;
		light->box.size = this->sw->box.size;
	}

	void draw(const Widget::DrawArgs& args) override {
		rack::app::SvgKnob::draw(args);
		nvgBeginPath(args.vg);
		nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
		nvgCircle(args.vg, this->getBox().size.x / 2.f, this->getBox().size.x / 2.f, this->getBox().size.x / 2.f);
		nvgFillColor(args.vg, nvgRGB(0x0D, 0x00, 0x16));
		nvgFill(args.vg);
	}
};

template <typename TKnobLight = DLXLargeKnobLight>
struct DLXLargeLightKnob : DLXLightKnob<TKnobLight> {
	DLXLargeLightKnob() {
		this->setSvg(Svg::load(rack::asset::system("res/ComponentLibrary/RoundHugeBlackKnob-bg.svg")));
	}
};

struct DLXMediumLightKnob : DLXLightKnob<DLXMediumKnobLight> {
	DLXMediumLightKnob() {
		this->setSvg(Svg::load(rack::asset::system("res/ComponentLibrary/RoundLargeBlackKnob-bg.svg")));
	}
};

struct DLXSmallLightKnob : DLXLightKnob<DLXSmallKnobLight> {
	DLXSmallLightKnob() {
		this->setSvg(Svg::load(rack::asset::system("res/ComponentLibrary/RoundSmallBlackKnob-bg.svg")));
	}
};

struct DLXSvgBloomLight : DLXSvgLight {
	std::shared_ptr<Svg> svgHalo;

	void setHaloSvg(std::shared_ptr<Svg> svg) {
		svgHalo = svg;
	}

	void drawHalo(const DrawArgs& args) override {
		// Don't draw halo if rendering in a framebuffer, e.g. screenshots or Module Browser
		if (args.fb)
			return;

		const float halo = rack::settings::haloBrightness;
		if (halo == 0.f)
			return;

		nvgAlpha(args.vg, halo);
		rack::window::svgDraw(args.vg, svgHalo->handle);
	}
};

struct DLXSvgSwitchBloomLight : rack::app::SvgSwitch {
	std::vector<std::shared_ptr<Svg>> haloFrames;
	DLXSvgBloomLight* light;
	
	//Subclassing SvgSwitch here even though it comes with baggage (sw = SvgWidget, shadow),
	//as it would be a PITA to reimplement SvgSwitch::addFrame() and SvgSwitch::onChange().
	DLXSvgSwitchBloomLight() {
		rack::app::SvgSwitch();

		//We are repurposing the SvgSwitch's sw to hold our SvgLight.
		//So we remove it from the scene (fb), request deletion of the previous widget,
		//assign our Light to the sw, and add it back to the scene.
		this->fb->removeChild(this->sw);
		this->sw->requestDelete();

		this->sw = new DLXSvgBloomLight();
		this->fb->addChild(this->sw);
		this->light = reinterpret_cast<DLXSvgBloomLight*>(this->sw);

		//We don't want a shadow, because we're visually masquerading as part of the switch below.
		this->fb->removeChild(this->shadow);
		this->shadow->requestDelete();
	}

	void addHaloFrame(std::shared_ptr<Svg> svg) {
		// From SvgSwitch::addFrame()

		haloFrames.push_back(svg);
		// If this is our first frame, automatically set SVG and size
		if (!this->light->svgHalo) {
			this->light->setHaloSvg(svg);
		}
	}

	void onChange(const ChangeEvent& e) override {
		rack::engine::ParamQuantity* pq = getParamQuantity();
		if (!frames.empty() && !haloFrames.empty() && pq) {
			int index = (int) std::round(pq->getValue() - pq->getMinValue());
			index = rack::math::clamp(index, 0, (int) frames.size() - 1);
			light->setSvg(frames[index]);
			light->setHaloSvg(haloFrames[index]);
			fb->dirty = true;
		}
		ParamWidget::onChange(e);
	}
};

struct DLXPencilButtonLight : DLXSvgSwitchBloomLight {
	DLXPencilButtonLight() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_PencilButtonLight_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_PencilButtonLight_1.svg")));
		addHaloFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_PencilButtonLight_0.svg")));
		addHaloFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_PencilButtonLight_1.svg")));
	}
};

struct DLXScreenButtonLight : DLXSvgSwitchBloomLight {
	DLXScreenButtonLight() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_ScreenButtonLight_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_ScreenButtonLight_1.svg")));
		addHaloFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_ScreenButtonLight_0.svg")));
		addHaloFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_ScreenButtonLight_1.svg")));
	}
};

struct DLX1ButtonLight : DLXSvgSwitchBloomLight {
	DLX1ButtonLight() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_1b_light_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_1b_light_1.svg")));
		addHaloFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_1b_light_0.svg")));
		addHaloFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_1b_light_1.svg")));
	}
};

struct DLX2ButtonLight : DLXSvgSwitchBloomLight {
	DLX2ButtonLight() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_2b_light_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_2b_light_1.svg")));
		addHaloFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_2b_light_0.svg")));
		addHaloFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_2b_light_1.svg")));
	}
};

struct DLX3ButtonLight : DLXSvgSwitchBloomLight {
	DLX3ButtonLight() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_3b_light_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_3b_light_1.svg")));
		addHaloFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_3b_light_0.svg")));
		addHaloFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_3b_light_1.svg")));
	}
};
