#include "plugin.hpp"
#include "Algomorph.hpp"

struct AlgomorphSmall : Algomorph {
    static constexpr int NUM_AUX_INPUTS = 5;

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
    
    AlgomorphSmall();
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

/// Glowing Ink

struct AlgomorphSmallGlowingInk : SvgLight {
	AlgomorphSmallGlowingInk() {
		sw->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/AlgomorphSmall_GlowingInk.svg")));
	}
};

/// Panel Widget

struct AlgomorphSmallWidget : AlgomorphWidget {
    AlgomorphSmallGlowingInk* ink;
    bool morphKnobShown = true;

    std::vector<Vec> SceneButtonCenters =   {   {mm2px(17.090), mm2px(52.345)},
                                                {mm2px(25.268), mm2px(52.345)},
                                                {mm2px(33.445), mm2px(52.345)}  };
    std::vector<Vec> OpButtonCenters    =   {   {mm2px(17.714), mm2px(96.854)},
                                                {mm2px(17.714), mm2px(86.612)},
                                                {mm2px(17.714), mm2px(76.591)},
                                                {mm2px(17.714), mm2px(66.570)}  };
    std::vector<Vec> ModButtonCenters   =   {   {mm2px(32.968), mm2px(96.854)},
                                                {mm2px(32.968), mm2px(86.612)},
                                                {mm2px(32.968), mm2px(76.591)},
                                                {mm2px(32.968), mm2px(66.570)}  };

    struct AlgomorphSmallMenuItem : MenuItem {
        AlgomorphSmall* module;
    };

    AlgomorphSmallWidget(AlgomorphSmall* module);
    void appendContextMenu(Menu* menu) override;
    void step() override;
};
