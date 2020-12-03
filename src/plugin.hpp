#pragma once
#include <rack.hpp>
#include "pluginsettings.hpp"
#include "GraphData.hpp"
#include <bitset>

using namespace rack;

extern Plugin* pluginInstance;

extern FMDelexanderSettings pluginSettings;

extern Model* modelAlgomorphLarge;
extern Model* modelAlgomorphSmall;

///

struct bitsetCompare {
    bool operator() (const std::bitset<16> &b1, const std::bitset<16> &b2) const {
        return b1.to_ulong() < b2.to_ulong();
	}
};

static const NVGcolor DLXDarkPurple = nvgRGB(0x14, 0x0d, 0x13);
static const NVGcolor DLXLightPurple = nvgRGB(0x8b, 0x70, 0xa2);
static const NVGcolor DLXRed = nvgRGB(0xae, 0x34, 0x58);
static const NVGcolor DLXYellow = nvgRGB(0xa9, 0xa9, 0x83);

/// Constants

static constexpr float BLINK_INTERVAL = 0.42857142857f;
static constexpr float DEF_CLICK_FILTER_SLEW = 3750.f;
static constexpr float FIVE_D_TWO = 5.f / 2.f;
static constexpr float FIVE_D_THREE = 5.f / 3.f;
static constexpr float CLOCK_IGNORE_DURATION = 0.001f;     // disable clock on powerup and reset for 1 ms (so that the first step plays)
static constexpr float DEF_RED_BRIGHTNESS = 0.4975f;
static constexpr float INDICATOR_BRIGHTNESS = 0.325f;

/// Algorithm Display Graph Data

static const GraphData GRAPH_DATA;

/// Undo/Redo History

template < typename MODULE >
struct AlgorithmDiagonalChangeAction : history::ModuleAction {
    int scene, op, mod;

	AlgorithmDiagonalChangeAction() {
		name = "Delexander Algomorph diagonal connection";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);

		m->toggleDiagonalDestination(scene, op, mod);

		m->graphDirty = true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);

		m->toggleDiagonalDestination(scene, op, mod);

		m->graphDirty = true;
	}
};

template < typename MODULE >
struct AlgorithmHorizontalChangeAction : history::ModuleAction {
    int scene, op;

	AlgorithmHorizontalChangeAction() {
		name = "Delexander Algomorph horizontal connection";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		
		m->toggleHorizontalDestination(scene, op);

		m->graphDirty = true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		
		m->toggleHorizontalDestination(scene, op);

		m->graphDirty = true;
	}
};

template < typename MODULE >
struct KnobModeAction : history::ModuleAction {
    int oldKnobMode, newKnobMode;

	KnobModeAction() {
		name = "Delexander Algomorph knob mode";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->knobMode = oldKnobMode;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->knobMode = newKnobMode;
	}
};

template < typename MODULE >
struct AlgorithmForcedCarrierChangeAction : history::ModuleAction {
    int scene, op;

	AlgorithmForcedCarrierChangeAction() {
		name = "Delexander Algomorph forced carrier";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);

		m->toggleForcedCarrier(scene, op);
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);

		m->toggleForcedCarrier(scene, op);
	}
};

template < typename MODULE >
struct AlgorithmSceneChangeAction : history::ModuleAction {
    int oldScene, newScene;

	AlgorithmSceneChangeAction() {
		name = "Delexander Algomorph base scene";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->baseScene = oldScene;
		m->graphDirty = true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->baseScene = newScene;
		m->graphDirty = true;
	}
};

template < typename MODULE >
struct AuxInputSetAction : history::ModuleAction {
    int auxIndex, mode, channels;

	AuxInputSetAction() {
		name = "Delexander Algomorph AUX In mode set";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->unsetAuxMode(auxIndex, mode);
		for (int c = 0; c < channels; c++)
			m->auxInput[auxIndex]->voltage[mode][c] = m->auxInput[auxIndex]->defVoltage[mode];
		m->rescaleVoltage(mode, channels);
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput[auxIndex]->setMode(mode);
		m->rescaleVoltage(mode, channels);
	}
};

template < typename MODULE >
struct AuxInputSwitchAction : history::ModuleAction {
	int auxIndex, oldMode, newMode, channels;

	AuxInputSwitchAction() {
		name = "Delexander Algomorph AUX In mode switch";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->unsetAuxMode(auxIndex, newMode);
		for (int c = 0; c < channels; c++)
			m->auxInput[auxIndex]->voltage[newMode][c] = m->auxInput[auxIndex]->defVoltage[newMode];
		m->rescaleVoltage(newMode, channels);
		m->auxInput[auxIndex]->setMode(oldMode);
		m->rescaleVoltage(oldMode, channels);
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->unsetAuxMode(auxIndex, oldMode);
		for (int c = 0; c < channels; c++)
			m->auxInput[auxIndex]->voltage[oldMode][c] = m->auxInput[auxIndex]->defVoltage[oldMode];
		m->rescaleVoltage(oldMode, channels);
		m->auxInput[auxIndex]->setMode(newMode);
		m->rescaleVoltage(newMode, channels);
	}
};

template < typename MODULE >
struct AuxInputUnsetAction : history::ModuleAction {
    int auxIndex, mode, channels;

	AuxInputUnsetAction() {
		name = "Delexander Algomorph AUX In mode unset";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput[auxIndex]->setMode(mode);
		m->rescaleVoltage(mode, channels);
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->unsetAuxMode(auxIndex, mode);
		for (int c = 0; c < channels; c++)
			m->auxInput[auxIndex]->voltage[mode][c] = m->auxInput[auxIndex]->defVoltage[mode];
		m->rescaleVoltage(mode, channels);
	}
};

template < typename MODULE >
struct AllowMultipleModesAction : history::ModuleAction {
	int auxIndex;
	AllowMultipleModesAction() {
		name = "Delexander Algomorph allow multiple modes";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput[auxIndex]->allowMultipleModes = false;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput[auxIndex]->allowMultipleModes = true;
	}
};

template < typename MODULE >
struct ResetSceneAction : history::ModuleAction {
	int oldResetScene, newResetScene;

	ResetSceneAction() {
		name = "Delexander Algomorph change reset scene";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->resetScene = oldResetScene;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->resetScene = newResetScene;
	}
};

template < typename MODULE >
struct ToggleModeBAction : history::ModuleAction {
	ToggleModeBAction() {
		name = "Delexander Algomorph toggle mode B";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		
		m->toggleModeB();

		m->graphDirty = true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		
		m->toggleModeB();

		m->graphDirty = true;
	}
};

template < typename MODULE >
struct ToggleRingMorphAction : history::ModuleAction {
	ToggleRingMorphAction() {
		name = "Delexander Algomorph toggle ring morph";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->ringMorph ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->ringMorph ^= true;
	}
};

template < typename MODULE >
struct ToggleRandomizeRingMorphAction : history::ModuleAction {
	ToggleRandomizeRingMorphAction() {
		name = "Delexander Algomorph toggle randomize ring morph";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->randomRingMorph ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->randomRingMorph ^= true;
	}
};

template < typename MODULE >
struct ToggleExitConfigOnConnectAction : history::ModuleAction {
	ToggleExitConfigOnConnectAction() {
		name = "Delexander Algomorph toggle exit config on connect";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->exitConfigOnConnect ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->exitConfigOnConnect ^= true;
	}
};

template < typename MODULE >
struct ToggleCCWSceneSelectionAction : history::ModuleAction {
	ToggleCCWSceneSelectionAction() {
		name = "Delexander Algomorph toggle CCW scene selection";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->ccwSceneSelection ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->ccwSceneSelection ^= true;
	}
};

template < typename MODULE >
struct ToggleClickFilterAction : history::ModuleAction {
	ToggleClickFilterAction() {
		name = "Delexander Algomorph toggle click filter";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->clickFilterEnabled ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->clickFilterEnabled ^= true;
	}
};

template < typename MODULE >
struct ToggleGlowingInkAction : history::ModuleAction {
	ToggleGlowingInkAction() {
		name = "Delexander Algomorph toggle glowing ink";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->glowingInk ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->glowingInk ^= true;
	}
};

template < typename MODULE >
struct ToggleVULightsAction : history::ModuleAction {
	ToggleVULightsAction() {
		name = "Delexander Algomorph toggle VU lights";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->vuLights ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->vuLights ^= true;
	}
};

template < typename MODULE >
struct ToggleResetOnRunAction : history::ModuleAction {
	ToggleResetOnRunAction() {
		name = "Delexander Algomorph toggle reset on run";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->resetOnRun ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->resetOnRun ^= true;
	}
};

template < typename MODULE >
struct ToggleRunSilencerAction : history::ModuleAction {
	ToggleRunSilencerAction() {
		name = "Delexander Algomorph toggle run silencer";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->runSilencer ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->runSilencer ^= true;
	}
};

template < typename MODULE >
struct SetClickFilterAction : history::ModuleAction {
	float oldSlew, newSlew;

	SetClickFilterAction() {
		name = "Delexander Algomorph set click filter slew";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->clickFilterSlew = oldSlew;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->clickFilterSlew = newSlew;
	}
};


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
		this->addBaseColor(DLXRed);
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
	TRingLight<TBase>* o;
	if (s == 0)
		o = new TRingLight<TBase>(r);
	else
		o = new TRingLight<TBase>(r, s);
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
	float radius = 1.f;
	float strokeSize = 0.f;
	
	DLXRingIndicator(float r, float s = 1.3) {
		radius = r;
		strokeSize = s;
	}

	void draw(const DrawArgs& args) override {
		if (!module)
			return;
		angle = dynamic_cast<MODULE*>(module)->rotor.angle;
		math::Vec center = Vec(radius, radius);
		nvgTransformIdentity(transform);
		float t[6];
		nvgTransformTranslate(t, center.x, center.y);
		nvgTransformPremultiply(transform, t);
		nvgTransformRotate(t, angle);
		nvgTransformPremultiply(transform, t);
		nvgTransformTranslate(t, center.neg().x, center.neg().y);
		nvgTransformPremultiply(transform, t);
		nvgTransform(args.vg, transform[0], transform[1], transform[2], transform[3], transform[4], transform[5]);

		nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, 2.6f, 2.6f, strokeSize);
		nvgFillColor(args.vg, this->color);
		nvgFill(args.vg);

		Widget::draw(args);
	}
};

template < typename MODULE >
DLXRingIndicator<MODULE>* createRingIndicator(Vec pos, float r, engine::Module* module, int firstLightId, float s = 0) {
    DLXRingIndicator<MODULE>* o = new DLXRingIndicator<MODULE>(r, s);
    o->box.pos = pos;
    o->module = module;
    o->firstLightId = firstLightId;
    return o;
}

template < typename MODULE >
DLXRingIndicator<MODULE>* createRingIndicatorCentered(Vec pos, float r, engine::Module* module, int firstLightId, float s = 0) {
    DLXRingIndicator<MODULE>* o = createRingIndicator<MODULE>(pos, r, module, firstLightId, s);
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

struct DLXScreenButtonLight : SvgSwitchLight {
	int state = 0;

	DLXScreenButtonLight() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_ScreenButton_light_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_ScreenButton_light_1.svg")));
	}
};

struct DLX1Light : SvgSwitchLight {
	int state = 0;

	DLX1Light() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_1b_light_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_1b_light_1.svg")));
	}
};

struct DLX2Light : SvgSwitchLight {
	int state = 0;

	DLX2Light() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_2b_light_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_2b_light_1.svg")));
	}
};

struct DLX3Light : SvgSwitchLight {
	int state = 0;

	DLX3Light() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_3b_light_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_3b_light_1.svg")));
	}
};

struct SvgKnobLight : SvgLight {
	float angle = 0;
	float transform[6];

	void draw(const DrawArgs& args) override {
		math::Vec center = Vec(sw->box.size.x / 2, sw->box.size.y / 2);
		nvgTransformIdentity(transform);
		float t[6];
		nvgTransformTranslate(t, center.x, center.y);
		nvgTransformPremultiply(transform, t);
		nvgTransformRotate(t, angle);
		nvgTransformPremultiply(transform, t);
		nvgTransformTranslate(t, center.neg().x, center.neg().y);
		nvgTransformPremultiply(transform, t);
		nvgTransform(args.vg, transform[0], transform[1], transform[2], transform[3], transform[4], transform[5]);
		sw->draw(args);
		Widget::draw(args);
	}
};

struct DLXLargeKnobLight : SvgKnobLight {
	DLXLargeKnobLight() {
		sw->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXKnobB_large_light.svg")));
	}
};

struct DLXMediumKnobLight : SvgKnobLight {
	DLXMediumKnobLight() {
		sw->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXKnobB_medium_light.svg")));
	}
};

struct DLXSmallKnobLight : SvgKnobLight {
	DLXSmallKnobLight() {
		sw->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXKnobB_light.svg")));
	}
};

template <class TKnobLight = DLXSmallKnobLight>
struct LightKnob : RoundKnob {
	TKnobLight* sibling;

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

struct DLXLargeLightKnob : LightKnob<DLXLargeKnobLight> {
	DLXLargeLightKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXKnobB_large.svg")));
	}
};

struct DLXMediumLightKnob : LightKnob<DLXMediumKnobLight> {
	DLXMediumLightKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXKnobB_medium.svg")));
	}
};

struct DLXSmallLightKnob : LightKnob<DLXSmallKnobLight> {
	bool randomizable = true;
	bool resettable = true;

	DLXSmallLightKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXKnobB.svg")));
	}

	void randomize() override {
		if (randomizable)
			Knob::randomize();
	}

	void reset() override {
		if (paramQuantity && resettable) {
			paramQuantity->reset();
			oldValue = snapValue = paramQuantity->getValue();
		}
	}

	void onDoubleClick(const event::DoubleClick& e) override {
		resetAction();
	}

	void resetAction() {
		if (paramQuantity && resettable) {
			float oldValue = paramQuantity->getValue();
			reset();
			// Here's another way of doing it, but either works.
			// paramQuantity->getParam()->reset();
			float newValue = paramQuantity->getValue();

			if (oldValue != newValue) {
				// Push ParamChange history action
				history::ParamChange* h = new history::ParamChange;
				h->name = "reset parameter";
				h->moduleId = paramQuantity->module->id;
				h->paramId = paramQuantity->paramId;
				h->oldValue = oldValue;
				h->newValue = newValue;
				APP->history->push(h);
			}
		}
	}
};

template <class TKnobLight = DLXSmallKnobLight, class TDLXLightKnob = DLXSmallLightKnob>
TDLXLightKnob* createLightKnob(Vec pos, engine::Module* module, int paramId, TKnobLight* kl) {
	TDLXLightKnob* o = new TDLXLightKnob();
	o->box.pos = pos;
	if (module) {
		o->paramQuantity = module->paramQuantities[paramId];
	}
	o->setSibling(kl);
	return o;
}

template <class TKnobLight = DLXSmallKnobLight, class TDLXLightKnob = DLXSmallLightKnob>
TDLXLightKnob* createLightKnobCentered(Vec pos, engine::Module* module, int paramId, TKnobLight* kl) {
	TDLXLightKnob* o = createLightKnob<TKnobLight, TDLXLightKnob>(pos, module, paramId, kl);
	o->box.pos = o->box.pos.minus(o->box.size.div(2));
	return o;
}
