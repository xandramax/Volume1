#pragma once
#include "Algomorph.hpp"
#include <rack.hpp>
using rack::window::mm2px;


struct AlgomorphSmall : Algomorph<> {
    enum ParamIds {
        ENUMS(OPERATOR_BUTTONS, 4),
        ENUMS(MODULATOR_BUTTONS, 4),
        ENUMS(SCENE_BUTTONS, 3),
        MORPH_KNOB,
        MORPH_ATTEN_KNOB,
        EDIT_BUTTON,
        SCREEN_BUTTON,
        NUM_PARAMS
    };
    enum InputIds {
        WILDCARD_INPUT,
        ENUMS(OPERATOR_INPUTS, 4),
        ENUMS(MORPH_INPUTS, 2),
        NUM_INPUTS
    };
    enum OutputIds {
        ENUMS(MODULATOR_OUTPUTS, 4),
        CARRIER_SUM_OUTPUT,
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
        MORPH_ATTEN_KNOB_LIGHT,
        GLOWING_INK,
        ONE_LIGHT,
        TWO_LIGHT,
        THREE_LIGHT,
        SCREEN_BUTTON_LIGHT,
        ENUMS(SCREEN_BUTTON_RING_LIGHT, 3),     // 3 colors
        NUM_LIGHTS
    };

    float gain = 1.f;
    float morphMult[2] = {1.f, 2.f};
    
    AlgomorphSmall();
    void onReset() override;
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
    void updateSceneBrightnesses();
    float getInputBrightness(int portID);
    float getOutputBrightness(int portID);
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;
};

// /// Glowing Ink

// struct AlgomorphSmallGlowingInk : SvgLight {
// 	AlgomorphSmallGlowingInk() {
// 		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/AlgomorphSmall_GlowingInk.svg")));
// 	}

//     void draw(const DrawArgs& args) override {
//         nvgGlobalTint(args.vg, color::WHITE);
//         SvgLight::draw(args);
//     }
// };

/// Panel Widget

struct AlgomorphSmallWidget : AlgomorphWidget<> {
    // AlgomorphSmallGlowingInk* ink;
    bool morphKnobShown = true;

    std::vector<Vec> SceneButtonCenters =   {   {mm2px(17.290), mm2px(52.345)},
                                                {mm2px(25.468), mm2px(52.345)},
                                                {mm2px(33.645), mm2px(52.345)}  };
    std::vector<Vec> OpButtonCenters    =   {   {mm2px(17.714), mm2px(96.854)},
                                                {mm2px(17.714), mm2px(86.612)},
                                                {mm2px(17.714), mm2px(76.591)},
                                                {mm2px(17.714), mm2px(66.570)}  };
    std::vector<Vec> ModButtonCenters   =   {   {mm2px(33.168), mm2px(96.854)},
                                                {mm2px(33.168), mm2px(86.612)},
                                                {mm2px(33.168), mm2px(76.591)},
                                                {mm2px(33.168), mm2px(66.570)}  };

    struct SetGainLevelAction : rack::history::ModuleAction {
        float oldGain, newGain;

        SetGainLevelAction();
        void undo() override;
        void redo() override;
    };

    struct SetMorphMultAction : rack::history::ModuleAction {
        float oldMult, newMult;
        int inputId;

        SetMorphMultAction();
        void undo() override;
        void redo() override;
    };

    struct AlgomorphSmallMenuItem : rack::ui::MenuItem {
        AlgomorphSmall* module;
    };
    struct SetGainLevelItem : AlgomorphSmallMenuItem {
        float gain;

        void onAction(const rack::event::Action &e) override;
    };
    struct GainLevelMenuItem : AlgomorphSmallMenuItem {
        Menu* createChildMenu() override;
        void createGainLevelMenu(rack::ui::Menu* menu);
    };
    struct SetMorphMultItem : AlgomorphSmallMenuItem {
        float morphMult;
        int inputId;

        void onAction(const rack::event::Action &e) override;
    };
    struct MorphMultMenuItem : AlgomorphSmallMenuItem {
        int inputId;

        Menu* createChildMenu() override;
        void createMorphMultMenu(rack::ui::Menu* menu, int inputId);
    };

    AlgomorphSmallWidget(AlgomorphSmall* module);
    void appendContextMenu(Menu* menu) override;
    void step() override;
};
