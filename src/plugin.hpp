#pragma once
#include <rack.hpp>
#include <bitset>
#include "pluginsettings.hpp"
#include "GraphData.hpp"

using namespace rack;

extern Plugin* pluginInstance;

extern DelexanderVol1Settings pluginSettings;

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
static constexpr float DEF_RED_BRIGHTNESS = 0.6475f;
static constexpr float INDICATOR_BRIGHTNESS = 1.f;
static constexpr float SVG_LIGHT_MIN_ALPHA = 2.f/3.f;
static constexpr float RING_RADIUS = 8.752f;
static constexpr float RING_LIGHT_STROKEWIDTH = 0.75f;
static constexpr float RING_BG_STROKEWIDTH = 1.55f;
static constexpr float RING_BORDER_STROKEWIDTH = 0.5825f;
static constexpr float RING_STROKEWIDTH = RING_LIGHT_STROKEWIDTH + RING_BG_STROKEWIDTH + RING_BORDER_STROKEWIDTH;
static constexpr float LINE_LIGHT_STROKEWIDTH = .975f;

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
struct ToggleWildModSumAction : history::ModuleAction {
	ToggleWildModSumAction() {
		name = "Delexander Algomorph toggle wildcard mod summing";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->wildModIsSummed ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->wildModIsSummed ^= true;
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

template <class MODULE>
struct RandomizeCurrentAlgorithmItem : MenuItem {
	MODULE* module;

	void onAction(const event::Action &e) override {
		module->randomizeAlgorithm(module->configMode ? module->configScene : module->centerMorphScene[0]);
		module->graphDirty = true;
	}
};

template <class MODULE>
struct RandomizeAllAlgorithmsItem : MenuItem {
	MODULE* module;

	void onAction(const event::Action &e) override {
		for (int scene = 0; scene < 3; scene++)
			module->randomizeAlgorithm(scene);
		module->graphDirty = true;
	}
};


// Component Library

struct DLXPJ301MPort : PJ301MPort {
	void draw(const DrawArgs &args) override {
		PJ301MPort::draw(args);
		nvgBeginPath(args.vg);
		nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_SRC_COLOR, NVG_ONE);
		nvgCircle(args.vg, this->getBox().size.x / 2.f, this->getBox().size.x / 2.f, this->getBox().size.x / 2.f);
		nvgFillColor(args.vg, nvgRGB(0x0D, 0x00, 0x16));
		nvgFill(args.vg);
	}
};

struct DLXPurpleButton : TL1105 {
	void draw(const DrawArgs &args) override {
		TL1105::draw(args);
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
typedef TDlxMultiLight<> DLXScreenMultiLight;

struct Line {
	Vec left, right;
	Vec size;
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
		if (flipped) {
			left = Vec(RING_RADIUS * std::cos(startAngle) + a.x, RING_RADIUS * std::sin(startAngle) + a.y);
			right = Vec(b.x - RING_RADIUS * std::cos(startAngle),  b.y - RING_RADIUS * std::sin(startAngle));
		}
		else {
			left = Vec(RING_RADIUS * std::cos(endAngle) + a.x, RING_RADIUS * std::sin(endAngle) + a.y);
			right = Vec(b.x - RING_RADIUS * std::cos(endAngle),  b.y - RING_RADIUS * std::sin(endAngle));
		}
	}
};

template <class MODULE>
struct ConnectionBgWidget : OpaqueWidget {
	MODULE* module;
	std::vector<Line> lines;
	ConnectionBgWidget(std::vector<Vec> left, std::vector<Vec> right, MODULE* module) {
		this->module = module;	
		for (unsigned i = 0; i < left.size(); i++) {
			for (unsigned j = 0; j < right.size(); j++) {
				lines.push_back(Line(left[i], right[j]));
			}
		}
	}

	void draw(const Widget::DrawArgs& args) override {
        //Colors from rack::GrayModuleLightWidget
		for (Line line : lines) {
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, line.left.x - box.pos.x, line.left.y - box.pos.y);
			nvgLineTo(args.vg, line.right.x - box.pos.x, line.right.y - box.pos.y);
			nvgStrokeWidth(args.vg, 1.1f);
			// Background
			nvgStrokeColor(args.vg, nvgRGB(0x5a, 0x5a, 0x5a));
			nvgStroke(args.vg);
			// Border
			nvgStrokeWidth(args.vg, 0.6);
			nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 0x60));
			nvgStroke(args.vg);
        }

		OpaqueWidget::draw(args);
    }

	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			createContextMenu();
			e.consume(this);
		}
		OpaqueWidget::onButton(e);
	}

	void createContextMenu() {
		ui::Menu* menu = createMenu();
		menu->addChild(construct<RandomizeCurrentAlgorithmItem<MODULE>>(&MenuItem::text, "Randomize Algorithm " +  std::to_string((module->configMode ? module->configScene : module->centerMorphScene[0]) + 1), &RandomizeCurrentAlgorithmItem<MODULE>::module, module));
		menu->addChild(construct<RandomizeAllAlgorithmsItem<MODULE>>(&MenuItem::text, "Randomize All Algorithms", &RandomizeAllAlgorithmsItem<MODULE>::module, module));
	}
};

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
		return;
		// TODO: Design backlight halo

		// Don't draw halo if rendering in a framebuffer, e.g. screenshots or Module Browser
		if (args.fb)
			return;

		const float halo = settings::haloBrightness;
		if (halo == 0.f)
			return;

		// If light is off, rendering the halo gives no effect.
		if (this->color.r == 0.f && this->color.g == 0.f && this->color.b == 0.f)
			return;

		math::Vec c = this->box.size.div(2);
		float radius = std::min(this->box.size.x, this->box.size.y) / 2.0;
		float oradius = radius + std::min(radius * 4.f, 15.f);

		nvgBeginPath(args.vg);
		nvgRect(args.vg, c.x - oradius, c.y - oradius, 2 * oradius, 2 * oradius);

		NVGcolor icol = color::mult(this->color, halo);
		NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
		NVGpaint paint = nvgRadialGradient(args.vg, c.x, c.y, radius, oradius, icol, ocol);
		nvgFillPaint(args.vg, paint);
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

		const float halo = settings::haloBrightness;
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
		nvgRoundedRect(args.vg, x - w, y - h, w * 3.f, h * 3.f, h);

		NVGcolor icol = color::mult(this->color, halo);
		NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
		NVGpaint paint = nvgBoxGradient(args.vg, x, y, w, h, h * 0.5f, h, icol, ocol);
		nvgFillPaint(args.vg, paint);
		nvgFill(args.vg);
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

		const float halo = settings::haloBrightness;
		if (halo == 0.f)
			return;

		// If light is off, rendering the halo gives no effect.
		if (this->color.r == 0.f && this->color.g == 0.f && this->color.b == 0.f)
			return;

		math::Vec c = this->box.size.div(2);

		// Outer halo
		float iradius = RING_LIGHT_STROKEWIDTH + this->radius;
		float oradius = RING_LIGHT_STROKEWIDTH * 9.125f + this->radius;
		nvgBeginPath(args.vg);
		nvgRect(args.vg, c.x - oradius, c.y - oradius, 2 * (oradius), 2 * (oradius));
		nvgPathWinding(args.vg, NVG_HOLE);
		nvgCircle(args.vg, c.x, c.y, this->radius);
		NVGcolor icol = color::mult(this->color, halo);
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
TRingLight<TBase>* createRingLight(Vec pos, engine::Module* module, int firstLightId, float r = RING_RADIUS) {
	TRingLight<TBase>* o;
	o = new TRingLight<TBase>(r);
	o->box.size = Vec(r * 2, r * 2);
	o->box.pos = pos;
	o->module = module;
	o->firstLightId = firstLightId;
	return o;
}

template <typename TBase = DLXPurpleLight>
TRingLight<TBase>* createRingLightCentered(Vec pos, engine::Module* module, int firstLightId, float r = RING_RADIUS) {
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
		nvgFillColor(args.vg, bgColor);
		nvgFill(args.vg);
	}

	void drawLight(const Widget::DrawArgs& args) override {
		if (!module)
			return;

		if (this->color.a > 0.0) {
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, this->radius, this->radius, RING_LIGHT_STROKEWIDTH);
			nvgFillColor(args.vg, SCHEME_WHITE);
			nvgFill(args.vg);
		}
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			rotate(args);
		}

		DLXMultiLight::drawLayer(args, layer);
	}

	inline void rotate(const DrawArgs& args) {
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
	}
};

template < typename MODULE >
DLXRingIndicator<MODULE>* createRingIndicator(Vec pos, engine::Module* module, int firstLightId, float r = RING_RADIUS) {
    DLXRingIndicator<MODULE>* o = new DLXRingIndicator<MODULE>(r);
    o->box.pos = pos;
    o->module = module;
    o->firstLightId = firstLightId;
    return o;
}

template < typename MODULE >
DLXRingIndicator<MODULE>* createRingIndicatorCentered(Vec pos, engine::Module* module, int firstLightId, float r = RING_RADIUS) {
    DLXRingIndicator<MODULE>* o = createRingIndicator<MODULE>(pos, module, firstLightId, r);
    o->box.pos.x -= r;
    o->box.pos.y -= r;
    return o;
}

struct DLXSvgFakeLight : SvgWidget {
	void draw(const DrawArgs& args) override {
		// Do not call SvgWidget::draw, as it will draw on the wrong layer
		Widget::draw(args);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {	
			//From SvgWidget::draw()
			if (!svg)
				return;

			// Scale from max brightness to min brightness, as rack brightness is reduced from one to zero
			nvgAlpha(args.vg, (1.f - SVG_LIGHT_MIN_ALPHA) * rack::settings::rackBrightness + SVG_LIGHT_MIN_ALPHA);
			window::svgDraw(args.vg, svg->handle);
		}

		SvgWidget::drawLayer(args, layer);
	}
};

struct DLXLargeKnobLight : DLXSvgFakeLight {
	DLXLargeKnobLight() {
		setSvg(Svg::load(asset::system("res/ComponentLibrary/RoundHugeBlackKnob.svg")));
	}
};

struct DLXMediumKnobLight : DLXSvgFakeLight {
	DLXMediumKnobLight() {
		setSvg(Svg::load(asset::system("res/ComponentLibrary/RoundLargeBlackKnob.svg")));
	}
};

struct DLXSmallKnobLight : DLXSvgFakeLight {
	DLXSmallKnobLight() {
		setSvg(Svg::load(asset::system("res/ComponentLibrary/RoundSmallBlackKnob.svg")));
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

struct DLXSvgSwitchLight : SvgSwitch {
	//Subclassing SvgSwitch here even though it comes with baggage (sw = SvgWidget, shadow),
	//as it would be a PITA to reimplement SvgSwitch::addFrame() and SvgSwitch::onChange().
	DLXSvgSwitchLight() {
		SvgSwitch();

		//We are repurposing the SvgSwitch's sw to hold our SvgFakeLight.
		//So we remove it from the scene (fb), request deletion of the previous widget,
		//assign our FakeLight to the sw, and add it back to the scene.
		this->fb->removeChild(this->sw);
		this->sw->requestDelete();

		this->sw = new DLXSvgFakeLight;
		this->fb->addChild(this->sw);

		//We don't want a shadow, because we're visually masquerading as part of the switch below.
		this->fb->removeChild(this->shadow);
		this->shadow->requestDelete();
	}
};

struct DLXPencilButtonLight : DLXSvgSwitchLight {
	DLXPencilButtonLight() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_PencilButtonLight_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_PencilButtonLight_1.svg")));
	}
};

struct DLXScreenButtonLight : DLXSvgSwitchLight {
	DLXScreenButtonLight() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_ScreenButtonLight_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_ScreenButtonLight_1.svg")));
	}
};

struct DLX1ButtonLight : DLXSvgSwitchLight {
	DLX1ButtonLight() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_1b_light_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_1b_light_1.svg")));
	}
};

struct DLX2ButtonLight : DLXSvgSwitchLight {
	DLX2ButtonLight() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_2b_light_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_2b_light_1.svg")));
	}
};

struct DLX3ButtonLight : DLXSvgSwitchLight {
	DLX3ButtonLight() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_3b_light_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_3b_light_1.svg")));
	}
};

template < typename DLXKnobLight >
struct DLXLightKnob : SvgKnob {
	// "sw" is used for the bg svg widget. light is the fg svg widget.
	// This allows use of SvgKnob::setSvg() for the background svg.
	DLXKnobLight* light;
	widget::FramebufferWidget* bg_fb;

	DLXLightKnob() {
		SvgKnob();
	
		//From RoundKnob::RoundKnob()
		minAngle = -0.83 * M_PI;
		maxAngle = 0.83 * M_PI;

		bg_fb = new widget::FramebufferWidget;
		addChildBottom(bg_fb);

		this->tw->removeChild(this->sw);
		bg_fb->addChild(this->sw);

		light = new DLXKnobLight();
		this->tw->addChild(light);
	};

	void setSvg(std::shared_ptr<window::Svg> svg) {
		SvgKnob::setSvg(svg);
		bg_fb->box.size = this->sw->box.size;
		light->box.size = this->sw->box.size;
	}

	void draw(const Widget::DrawArgs& args) override {
		SvgKnob::draw(args);
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
		this->setSvg(Svg::load(asset::system("res/ComponentLibrary/RoundHugeBlackKnob-bg.svg")));
	}
};

struct DLXMediumLightKnob : DLXLightKnob<DLXMediumKnobLight> {
	DLXMediumLightKnob() {
		this->setSvg(Svg::load(asset::system("res/ComponentLibrary/RoundLargeBlackKnob-bg.svg")));
	}
};

struct DLXSmallLightKnob : DLXLightKnob<DLXSmallKnobLight> {
	DLXSmallLightKnob() {
		this->setSvg(Svg::load(asset::system("res/ComponentLibrary/RoundSmallBlackKnob-bg.svg")));
	}
};
