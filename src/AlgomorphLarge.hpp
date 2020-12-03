#include "plugin.hpp"
#include "Algomorph.hpp"
#include "AuxSources.hpp"

struct AlgomorphLarge : Algomorph {
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

    AuxInput<AlgomorphLarge>* auxInput[NUM_AUX_INPUTS];
    float scaledAuxVoltage[AuxInputModes::NUM_MODES][16] = {{0.f}};    // store processed (ready-to-use) values from auxInput[]->voltage[][], so they can be remembered if necessary
    bool auxModeFlags[AuxInputModes::NUM_MODES] = {false};             // a mode's flag is set to true when any aux input has that mode active
    int knobMode = AuxKnobModes::MORPH_ATTEN;

    float morphPhase[16] = {0.f};                               // Range -5.f -> 5.f or 0.f -> 10.f

    dsp::SchmittTrigger sceneAdvCVTrigger;
    dsp::SchmittTrigger resetCVTrigger;
    long clockIgnoreOnReset = (long) (CLOCK_IGNORE_DURATION * APP->engine->getSampleRate());

    dsp::SlewLimiter runClickFilter;
    bool running = true;

    float phaseMin = 0.f;
    float phaseMax = 10.f;
    bool runSilencer = true;
    bool resetOnRun = false;
    int resetScene = 1;
    bool ccwSceneSelection = true;      // Default true to interface with rising ramp LFO at Morph CV input
    
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

/// Glowing Ink

struct AlgomorphLargeGlowingInk : SvgLight {
	AlgomorphLargeGlowingInk() {
		sw->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/AlgomorphLarge_GlowingInk.svg")));
	}
};

/// Panel Widget

struct AlgomorphLargeWidget : AlgomorphWidget {
    AlgomorphLargeGlowingInk* ink;

    std::vector<Vec> SceneButtonCenters =   {   {mm2px(63.709), mm2px(18.341)},
                                                {mm2px(63.709), mm2px(26.519)},
                                                {mm2px(63.709), mm2px(34.697)}  };
    std::vector<Vec> OpButtonCenters    =   {   {mm2px(25.578), mm2px(86.926)},
                                                {mm2px(25.578), mm2px(75.904)},
                                                {mm2px(25.578), mm2px(64.883)},
                                                {mm2px(25.578), mm2px(53.863)}  };
    std::vector<Vec> ModButtonCenters   =   {   {mm2px(45.278), mm2px(86.926)},
                                                {mm2px(45.278), mm2px(75.904)},
                                                {mm2px(45.278), mm2px(64.883)},
                                                {mm2px(45.278), mm2px(53.863)}  };
    int activeKnob = 0;

    struct DisallowMultipleModesAction : history::ModuleAction {
        int auxIndex, channels;
        bool multipleActive = false, enabled[AuxInputModes::NUM_MODES] = {false};

        DisallowMultipleModesAction();
        void undo() override;
        void redo() override;
    };
    struct ResetKnobsAction : history::ModuleAction {
        float knobValue[AuxKnobModes::NUM_MODES] = {0.f};

        ResetKnobsAction();
        void undo() override;
        void redo() override;
    };
    struct TogglePhaseOutRangeAction : history::ModuleAction {
        TogglePhaseOutRangeAction();
        void undo() override;
        void redo() override;
    };

    struct AlgomorphLargeMenuItem : MenuItem {
        AlgomorphLarge* module;
        int auxIndex = -1, mode = -1;
    };
    struct AuxModeItem : AlgomorphLargeMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct ResetOnRunItem : AlgomorphLargeMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct RunSilencerItem : AlgomorphLargeMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct CCWScenesItem : AlgomorphLargeMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct AllowMultipleModesItem : AlgomorphLargeMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct SaveAuxInputSettingsItem : AlgomorphLargeMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct KnobModeItem : AlgomorphLargeMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct ResetKnobsItem : AlgomorphLargeMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct PhaseOutRangeItem : AlgomorphLargeMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct ResetSceneItem : AlgomorphLargeMenuItem {
        int scene;

        void onAction(const event::Action &e) override;
    };

    struct WildcardInputMenuItem : AlgomorphLargeMenuItem {
        Menu* createChildMenu() override;
        void createWildcardInputMenu(AlgomorphLarge* module, ui::Menu* menu, int auxIndex);
    };
    struct ShadowInputMenuItem : AlgomorphLargeMenuItem {
        Menu* createChildMenu() override;
        void createShadowInputMenu(AlgomorphLarge* module, ui::Menu* menu, int auxIndex);
    };
    struct AuxInputModeMenuItem : AlgomorphLargeMenuItem {
        Menu* createChildMenu() override;
        void createAuxInputModeMenu(AlgomorphLarge* module, ui::Menu* menu, int auxIndex);
    };
    struct AllowMultipleModesMenuItem : AlgomorphLargeMenuItem {
        Menu* createChildMenu() override;
        void createAllowMultipleModesMenu(AlgomorphLarge* module, ui::Menu* menu);
    };
    struct ResetSceneMenuItem : AlgomorphLargeMenuItem {
        Menu* createChildMenu() override;
        void createResetSceneMenu(AlgomorphLarge* module, ui::Menu* menu);
    };
    struct AudioSettingsMenuItem : AlgomorphLargeMenuItem {
        Menu* createChildMenu() override;
        void createAudioSettingsMenu(AlgomorphLarge* module, ui::Menu* menu);
    };
    struct InteractionSettingsMenuItem : AlgomorphLargeMenuItem {
        Menu* createChildMenu() override;
        void createInteractionSettingsMenu(AlgomorphLarge* module, ui::Menu* menu);
    };
    struct VisualSettingsMenuItem : AlgomorphLargeMenuItem {
        void createVisualSettingsMenu(AlgomorphLarge* module, ui::Menu* menu);
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
