#pragma once
#include "Algomorph.hpp"
#include "AuxSources.hpp"
#include <rack.hpp>
using rack::history::ModuleAction;
using rack::event::Action;
using rack::ui::Menu;
using rack::window::mm2px;


struct AlgomorphLarge : Algomorph<> {
    static constexpr int NUM_AUX_INPUTS = 5;

    enum ParamIds {
        ENUMS(OPERATOR_BUTTONS, 4),
        ENUMS(MODULATOR_BUTTONS, 4),
        ENUMS(SCENE_BUTTONS, 3),
        MORPH_KNOB,
        EDIT_BUTTON,
        SCREEN_BUTTON,
        ENUMS(AUX_KNOBS, AuxKnobModes::NUM_MODES),
        NUM_PARAMS
    };
    enum InputIds {
        ENUMS(OPERATOR_INPUTS, 4),
        ENUMS(AUX_INPUTS, NUM_AUX_INPUTS),
        NUM_INPUTS
    };
    enum OutputIds {
        ENUMS(MODULATOR_OUTPUTS, 4),
        CARRIER_SUM_OUTPUT,
        MODULATOR_SUM_OUTPUT,
        PHASE_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(DISPLAY_BACKLIGHT, 3),            // 3 colors
        ENUMS(SCENE_LIGHTS, 9),                 // 3 colors per light
        ENUMS(SCENE_INDICATORS, 9),             // 3 colors per light
        ENUMS(H_CONNECTION_LIGHTS, 12),         // 3 colors per light
        ENUMS(D_DISABLE_LIGHTS, 12),
        ENUMS(CONNECTION_LIGHTS, 36),           // 3 colors per light
        ENUMS(OPERATOR_LIGHTS, 12),             // 3 colors per light
        ENUMS(CARRIER_INDICATORS, 12),          // 3 colors per light
        ENUMS(MODULATOR_LIGHTS, 12),            // 3 colors per light
        EDIT_LIGHT,
        MORPH_KNOB_LIGHT,
        GLOWING_INK,
        ONE_LIGHT,
        TWO_LIGHT,
        THREE_LIGHT,
        SCREEN_BUTTON_LIGHT,
        ENUMS(SCREEN_BUTTON_RING_LIGHT, 3),     // 3 colors
        ENUMS(AUX_KNOB_LIGHTS, AuxKnobModes::NUM_MODES),
        NUM_LIGHTS
    };

    AuxInput* auxInput[NUM_AUX_INPUTS];
    float scaledAuxVoltage[AuxInputModes::NUM_MODES][16] = {{0.f}};    // store processed (ready-to-use) values from auxInput[]->voltage[][], so they can be remembered if necessary
    bool auxModeFlags[AuxInputModes::NUM_MODES] = {false};             // a mode's flag is set to true when any aux input has that mode active
    int knobMode = AuxKnobModes::MORPH_ATTEN;

    float morphPhase[16] = {0.f};                               // Range -5.f -> 5.f or 0.f -> 10.f

    rack::dsp::SchmittTrigger sceneAdvCVTrigger;
    rack::dsp::SchmittTrigger resetCVTrigger;
    long clockIgnoreOnReset = (long) (CLOCK_IGNORE_DURATION * APP->engine->getSampleRate());

    rack::dsp::SlewLimiter runClickFilter;
    bool running = true;

    float phaseMin = 0.f;
    float phaseMax = 10.f;
    bool runSilencer = true;
    bool resetOnRun = false;
    int resetScene = 1;
    bool ccwSceneSelection = true;      // Default true to interface with rising ramp LFO at Morph CV input
    bool wildModIsSummed = false;
    
    bool auxPanelDirty = true;

    AlgomorphLarge();
    void onReset() override;
    void unsetAuxMode(int auxIndex, int mode);
    void process(const ProcessArgs& args) override;
    float routeHorizontal(float sampleTime, float inputVoltage, int op, int c);
    float routeHorizontalRing(float sampleTime, float inputVoltage, int op, int c);
    float routeDiagonal(float sampleTime, float inputVoltage, int op, int mod, int c);
    float routeDiagonalB(float sampleTime, float inputVoltage, int op, int mod, int c);
    float routeDiagonalRing(float sampleTime, float inputVoltage, int op, int mod, int c);
    float routeDiagonalRingB(float sampleTime, float inputVoltage, int op, int mod, int c);
    float routeSum(float sampleTime, float inputVoltage, int op, int c);
    float routeSumB(float sampleTime, float inputVoltage, int op, int c);
    float routeSumRing(float sampleTime, float inputVoltage, int op, int c);
    float routeSumRingB(float sampleTime, float inputVoltage, int op, int c);
    void scaleAuxSumAttenCV(int channels);
    void scaleAuxModAttenCV(int channels);
    void scaleAuxClickFilterCV(int channels);
    void scaleAuxMorphCV(int channels);
    void scaleAuxDoubleMorphCV(int channels);
    void scaleAuxTripleMorphCV(int channels);
    void scaleAuxMorphAttenCV(int channels);
    void scaleAuxMorphDoubleAttenCV(int channels);
    void scaleAuxMorphTripleAttenCV(int channels);
    void scaleAuxShadow(float sampleTime, int op, int channels);
    void initRun();
    void rescaleVoltage(int mode, int channels);
    void rescaleVoltages(int channels);
    void updateSceneBrightnesses();
    float getInputBrightness(int portID);
    float getOutputBrightness(int portID);
    bool auxInputsAreDefault();
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;
};

// /// Glowing Ink

// struct AlgomorphLargeGlowingInk : SvgLight {
// 	AlgomorphLargeGlowingInk() {
// 		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/AlgomorphLarge_GlowingInk.svg")));
// 	}

//     void draw(const DrawArgs& args) override {
//         nvgAlpha(args.vg, 0.f);
//         SvgLight::draw(args);
//     }
// };

/// Panel Widget
struct AlgomorphLargeWidget : AlgomorphWidget<> {
    // AlgomorphLargeGlowingInk* ink;

    std::vector<Vec> SceneButtonCenters =   {   {mm2px(63.842), mm2px(17.924)},
                                                {mm2px(63.842), mm2px(26.717)},
                                                {mm2px(63.842), mm2px(35.510)}  };
    std::vector<Vec> OpButtonCenters    =   {   {mm2px(25.578), mm2px(86.926)},
                                                {mm2px(25.578), mm2px(75.904)},
                                                {mm2px(25.578), mm2px(64.883)},
                                                {mm2px(25.578), mm2px(53.863)}  };
    std::vector<Vec> ModButtonCenters   =   {   {mm2px(45.278), mm2px(86.926)},
                                                {mm2px(45.278), mm2px(75.904)},
                                                {mm2px(45.278), mm2px(64.883)},
                                                {mm2px(45.278), mm2px(53.863)}  };
    int activeKnob = 0;

    struct DisallowMultipleModesAction : ModuleAction {
        int auxIndex, channels;
        bool multipleActive = false, enabled[AuxInputModes::NUM_MODES] = {false};

        DisallowMultipleModesAction();
        void undo() override;
        void redo() override;
    };
    struct ResetKnobsAction : ModuleAction {
        float knobValue[AuxKnobModes::NUM_MODES] = {0.f};

        ResetKnobsAction();
        void undo() override;
        void redo() override;
    };
    struct TogglePhaseOutRangeAction : ModuleAction {
        TogglePhaseOutRangeAction();
        void undo() override;
        void redo() override;
    };

    struct AlgomorphLargeMenuItem : rack::ui::MenuItem {
        AlgomorphLarge* module;
        int auxIndex = -1, mode = -1;
    };
    struct AuxModeItem : AlgomorphLargeMenuItem {
        void onAction(const Action &e) override;
    };
    struct ResetOnRunItem : AlgomorphLargeMenuItem {
        void onAction(const Action &e) override;
    };
    struct RunSilencerItem : AlgomorphLargeMenuItem {
        void onAction(const Action &e) override;
    };
    struct CCWScenesItem : AlgomorphLargeMenuItem {
        void onAction(const Action &e) override;
    };
    struct WildModSumItem : AlgomorphLargeMenuItem {
        void onAction(const Action &e) override;
    };
    struct AllowMultipleModesItem : AlgomorphLargeMenuItem {
        void onAction(const Action &e) override;
    };
    struct SaveAuxInputSettingsItem : AlgomorphLargeMenuItem {
        void onAction(const Action &e) override;
    };
    struct KnobModeItem : AlgomorphLargeMenuItem {
        void onAction(const Action &e) override;
    };
    struct ResetKnobsItem : AlgomorphLargeMenuItem {
        void onAction(const Action &e) override;
    };
    struct PhaseOutRangeItem : AlgomorphLargeMenuItem {
        void onAction(const Action &e) override;
    };
    struct ResetSceneItem : AlgomorphLargeMenuItem {
        int scene;

        void onAction(const Action &e) override;
    };

    struct WildcardInputMenuItem : AlgomorphLargeMenuItem {
        Menu* createChildMenu() override;
        void createWildcardInputMenu(AlgomorphLarge* module, Menu* menu, int auxIndex);
    };
    struct ShadowInputMenuItem : AlgomorphLargeMenuItem {
        Menu* createChildMenu() override;
        void createShadowInputMenu(AlgomorphLarge* module, Menu* menu, int auxIndex);
    };
    struct AuxInputModeMenuItem : AlgomorphLargeMenuItem {
        Menu* createChildMenu() override;
        void createAuxInputModeMenu(AlgomorphLarge* module, Menu* menu, int auxIndex);
    };
    struct AllowMultipleModesMenuItem : AlgomorphLargeMenuItem {
        Menu* createChildMenu() override;
        void createAllowMultipleModesMenu(AlgomorphLarge* module, Menu* menu);
    };
    struct ResetSceneMenuItem : AlgomorphLargeMenuItem {
        Menu* createChildMenu() override;
        void createResetSceneMenu(AlgomorphLarge* module, Menu* menu);
    };
    struct AudioSettingsMenuItem : AlgomorphLargeMenuItem {
        Menu* createChildMenu() override;
        void createAudioSettingsMenu(AlgomorphLarge* module, Menu* menu);
    };
    struct InteractionSettingsMenuItem : AlgomorphLargeMenuItem {
        Menu* createChildMenu() override;
        void createInteractionSettingsMenu(AlgomorphLarge* module, Menu* menu);
    };
    struct VisualSettingsMenuItem : AlgomorphLargeMenuItem {
        void createVisualSettingsMenu(AlgomorphLarge* module, Menu* menu);
        Menu* createChildMenu() override;
    };
    struct KnobModeMenuItem : AlgomorphLargeMenuItem {
        Menu* createChildMenu() override;
    };

    AlgomorphLargeWidget(AlgomorphLarge* module);
    void appendContextMenu(Menu* menu) override;
    void setKnobMode(int mode);
    void step() override;
};


// Undo/Redo

template < int OPS = 4, int SCENES = 3 >
struct ResetSceneAction : ModuleAction {
	int oldResetScene, newResetScene;

	ResetSceneAction()  {
		name = "Delexander Algomorph change reset scene";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->resetScene = oldResetScene;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->resetScene = newResetScene;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct ToggleCCWSceneSelectionAction : ModuleAction {
	ToggleCCWSceneSelectionAction() {
		name = "Delexander Algomorph toggle CCW scene selection";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->ccwSceneSelection ^= true;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->ccwSceneSelection ^= true;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct ToggleWildModSumAction : ModuleAction {
	ToggleWildModSumAction() {
		name = "Delexander Algomorph toggle wildcard mod summing";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->wildModIsSummed ^= true;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->wildModIsSummed ^= true;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct ToggleResetOnRunAction : ModuleAction {
	ToggleResetOnRunAction() {
		name = "Delexander Algomorph toggle reset on run";
	};
    void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->resetOnRun ^= true;
	};
    void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->resetOnRun ^= true;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct ToggleRunSilencerAction : ModuleAction {
	ToggleRunSilencerAction() {
		name = "Delexander Algomorph toggle run silencer";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->runSilencer ^= true;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->runSilencer ^= true;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct KnobModeAction : ModuleAction {
    int oldKnobMode, newKnobMode;

	KnobModeAction() {
		name = "Delexander Algomorph knob mode";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->knobMode = oldKnobMode;
	};
    void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->knobMode = newKnobMode;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct AuxInputSetAction : ModuleAction {
    int auxIndex, mode, channels;

	AuxInputSetAction() {
		name = "Delexander Algomorph AUX In mode set";
	}
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->unsetAuxMode(auxIndex, mode);
		for (int c = 0; c < channels; c++)
			m->auxInput[auxIndex]->voltage[mode][c] = m->auxInput[auxIndex]->defVoltage[mode];
		m->rescaleVoltage(mode, channels);
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->auxInput[auxIndex]->setMode(mode);
		m->rescaleVoltage(mode, channels);
	};
};

template < int OPS = 4, int SCENES = 3 >
struct AuxInputSwitchAction : ModuleAction {
	int auxIndex, oldMode, newMode, channels;

	AuxInputSwitchAction()  {
		name = "Delexander Algomorph AUX In mode switch";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->unsetAuxMode(auxIndex, newMode);
		for (int c = 0; c < channels; c++)
			m->auxInput[auxIndex]->voltage[newMode][c] = m->auxInput[auxIndex]->defVoltage[newMode];
		m->rescaleVoltage(newMode, channels);
		m->auxInput[auxIndex]->setMode(oldMode);
		m->rescaleVoltage(oldMode, channels);
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->unsetAuxMode(auxIndex, oldMode);
		for (int c = 0; c < channels; c++)
			m->auxInput[auxIndex]->voltage[oldMode][c] = m->auxInput[auxIndex]->defVoltage[oldMode];
		m->rescaleVoltage(oldMode, channels);
		m->auxInput[auxIndex]->setMode(newMode);
		m->rescaleVoltage(newMode, channels);
	};
};

template < int OPS = 4, int SCENES = 3 >
struct AuxInputUnsetAction : ModuleAction {
    int auxIndex, mode, channels;

	AuxInputUnsetAction() {
		name = "Delexander Algomorph AUX In mode unset";
	}
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->auxInput[auxIndex]->setMode(mode);
		m->rescaleVoltage(mode, channels);
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->unsetAuxMode(auxIndex, mode);
		for (int c = 0; c < channels; c++)
			m->auxInput[auxIndex]->voltage[mode][c] = m->auxInput[auxIndex]->defVoltage[mode];
		m->rescaleVoltage(mode, channels);
	};
};

template < int OPS = 4, int SCENES = 3 >
struct AllowMultipleModesAction : ModuleAction {
	int auxIndex;
	AllowMultipleModesAction() {
		name = "Delexander Algomorph allow multiple modes";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->auxInput[auxIndex]->allowMultipleModes = false;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
		assert(m);
		m->auxInput[auxIndex]->allowMultipleModes = true;
	};
};