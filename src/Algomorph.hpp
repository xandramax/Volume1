#include "plugin.hpp"

template < typename MODULE >
struct AuxInput {
    MODULE* module;
    int id = -1;
    bool connected = false;
    int channels = 0;
    
    float voltage[AuxInputModes::NUM_MODES][16];
    float defVoltage[AuxInputModes::NUM_MODES] = { 0.f };

    bool mode[AuxInputModes::NUM_MODES] = {false};
    bool allowMultipleModes = false;
    int activeModes = 0;
    int lastSetMode;

    dsp::SchmittTrigger runCVTrigger;
    dsp::SchmittTrigger sceneAdvCVTrigger;
    dsp::SchmittTrigger reverseSceneAdvCVTrigger;

    dsp::SlewLimiter shadowClickFilter[4];                  // [op]
    
    dsp::SlewLimiter wildcardModClickFilter[16];
    float wildcardModClickGain = 0.f;
    dsp::SlewLimiter wildcardSumClickFilter[16];
    float wildcardSumClickGain = 0.f;

    AuxInput(int id, MODULE* module) {
        this->id = id;
        this->module = module;
        defVoltage[AuxInputModes::MOD_ATTEN] = 5.f;
        defVoltage[AuxInputModes::SUM_ATTEN] = 5.f;
        defVoltage[AuxInputModes::MORPH_ATTEN] = 5.f;
        resetVoltages();
        for (int i = 0; i < 4; i++)
            shadowClickFilter[i].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
        for (int c = 0; c < 16; c++) {
            wildcardModClickFilter[c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
            wildcardSumClickFilter[c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
        }
    }

    void resetVoltages() {
        for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
            for (int j = 0; j < 16; j++) {
                voltage[i][j] = defVoltage[i];
            }
        }
    }

    void setMode(int newMode) {
        activeModes++;
        if (module->debug)
            int x = 0;

        if (activeModes > 1 && !allowMultipleModes)
            unsetMode(lastSetMode);

        mode[newMode] = true;
        lastSetMode = newMode;
        module->auxModeFlags[newMode] = true;
    }

    void unsetMode(int oldMode) {
        if (mode[oldMode]) {
            activeModes--;
            if (module->debug)
                int x = 0;
            
            mode[oldMode] = false;

            module->auxModeFlags[oldMode] = false;
            for (int auxIndex = 0; auxIndex < 4; auxIndex++) {
                if (auxIndex != id)
                    if (module->auxInput[auxIndex]->mode[oldMode]) {
                        module->auxModeFlags[oldMode] = true;
                        break;
                    }
            }
        }
    }

    void clearModes() {
        for (int i = 0; i < AuxInputModes::NUM_MODES; i++)
            unsetMode(i);

        activeModes = 0;
    }

    void updateVoltage() {
        for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
            if (mode[i]) {
                for (int c = 0; c < channels; c++)
                    voltage[i][c] = module->inputs[MODULE::AUX_INPUTS + id].getPolyVoltage(c);
            }
        }
    }
};

struct Algomorph4 : Module {
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
        ENUMS(AUX_INPUTS, 4),   // For legacy support, aux inputs are out-of-order on the panel:
        // Fourth Aux Input (Default Morph input)
        // Second Aux Input (Default Clk input)
        // First Aux Input  (Default Res input)
        // Third Aux Input  (Default Morph input)
        NUM_INPUTS
    };
    enum OutputIds {
        ENUMS(MODULATOR_OUTPUTS, 4),
        SUM_OUTPUT,
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
        KNOB_LIGHT,
        GLOWING_INK,
        ONE_LIGHT,
        TWO_LIGHT,
        THREE_LIGHT,
        SCREEN_BUTTON_LIGHT,
        ENUMS(SCREEN_BUTTON_RING_LIGHT, 3),     // 3 colors
        ENUMS(AUX_KNOB_LIGHTS, AuxKnobModes::NUM_MODES),
        NUM_LIGHTS
    };

    AuxInput<Algomorph4>* auxInput[4];
    float scaledAuxVoltage[AuxInputModes::NUM_MODES][16] = {{0.f}};    // store processed (ready-to-use) values from auxInput[]->voltage[][], so they can be remembered if necessary
    bool auxModeFlags[AuxInputModes::NUM_MODES] = {false};                // a mode's flag is set to true when any aux input has that mode active
    int knobMode = 1;

    float morph[16] = {0.f};                                    // Range -1.f -> 1.f
    float relativeMorphMagnitude[16] = { morph[0] };
    int baseScene = 1;                                          // Center the Morph knob on saved algorithm 0, 1, or 2
    int centerMorphScene[16] = { baseScene }, forwardMorphScene[16] = { (baseScene + 1) % 3 }, backwardMorphScene[16] = { (baseScene + 2) % 3 };

    bool horizontalDestinations[3][4] = {{false}};                          // [scene][op]
    bool opDestinations[3][4][3] = {{{false}}};                               // [scene][op][legal mod]
    std::bitset<16> algoName[3];                                // 16-bit IDs of the three stored algorithms
    std::bitset<16> displayAlgoName[3];                         // When operators are disabled, remove their mod destinations from here
                                                                // If a disabled operator is a mod destination, set it to enabled here
    bool forcedCarrier[3][4] = {{false}};                                   // [scene][op]
    bool carrier[3][4] = {  {true, true, true, true},           // Per-algorithm operator carriership status
                            {true, true, true, true},           // [scene][op]
                            {true, true, true, true}};
    bool opDisabled[3][4] = {{false}};

    dsp::SchmittTrigger sceneAdvCVTrigger;
    dsp::SchmittTrigger resetCVTrigger;
    long clockIgnoreOnReset = (long) (CLOCK_IGNORE_DURATION * APP->engine->getSampleRate());

    dsp::ClockDivider cvDivider;
    dsp::BooleanTrigger sceneButtonTrigger[3];
    dsp::BooleanTrigger editTrigger;
    dsp::BooleanTrigger operatorTrigger[4];
    dsp::BooleanTrigger modulatorTrigger[4];

    dsp::SlewLimiter modClickFilters[4][4][16];    // [noRing/ring][op][legal mod][channel], [op][3] = Sum output
    float modClickGain[4][4][16] = {{{0.f}}};             // Click filter gains:  [noRing/ring][op][legal mod][channel], clickGain[x][y][3][z] = sum output
    dsp::SlewLimiter modRingClickFilters[4][4][16];    // [noRing/ring][op][legal mod][channel], [op][3] = Sum output
    float modRingClickGain[4][4][16] = {{{0.f}}};             // Click filter gains:  [noRing/ring][op][legal mod][channel], clickGain[x][y][3][z] = sum output

    dsp::SlewLimiter sumClickFilters[4][16];    // [noRing/ring][op][legal mod][channel], [op][3] = Sum output
    float sumClickGain[4][16] = {{0.f}};             // Click filter gains:  [noRing/ring][op][legal mod][channel], clickGain[x][y][3][z] = sum output
    dsp::SlewLimiter sumRingClickFilters[4][16];    // [noRing/ring][op][legal mod][channel], [op][3] = Sum output
    float sumRingClickGain[4][16] = {{0.f}};             // Click filter gains:  [noRing/ring][op][legal mod][channel], clickGain[x][y][3][z] = sum output

    dsp::SlewLimiter runClickFilter;

    dsp::ClockDivider clickFilterDivider;

    dsp::ClockDivider lightDivider;
    float sceneBrightnesses[3][12][3] = {{{}}};
    float blinkTimer = BLINK_INTERVAL;
    bool blinkStatus = true;

    bool configMode = true;
    int configOp = -1;                                          // Set to 0-3 when configuring mod destinations for operators 1-4
    int configScene = 1;
    bool running = true;

    bool graphDirty = true;
    bool debug = false;

    int graphAddressTranslation[0xFFFF];                        // Graph ID conversion
                                                                // The algorithm graph data are stored with IDs in 12-bit space:
                                                                //       0000 000 000 000 000 -> 1111 000 000 000 000
                                                                // The first 4 bits mark which operators are disabled (1) vs enabled (0).
                                                                // Each set of 3 bits corresponds to an operator.
                                                                // Each bit represents one of an oscillator's "legal" mod destinations.
                                                                // At least one operator is a carrier (having no mod destinations, i.e. all bits zero).
                                                                // However, the algorithms are accessed via 20-bit IDs:
                                                                //       0000 0000 0000 0000 0000 -> 1111 0000 0000 0000 0000
                                                                // In 16-bit space, the feedback destinations are included but never equal 1.  
                                                                // graphAddressTranslation is indexed by 20-bit ID and returns equivalent 12-bit ID.
    int threeToFour[4][3] = {{0}};                                      // Modulator ID conversion ([op][x] = y, where x is 0..2 and y is 0..3)
    int fourToThree[4][4] = {{0}};

    //User settings
    bool clickFilterEnabled = true;
    bool ringMorph = false;
    bool randomRingMorph = false;
    bool exitConfigOnConnect = false;
    bool ccwSceneSelection = true;      // Default true to interface with rising ramp LFO at Morph CV input
    bool glowingInk = false;
    bool vuLights = true;
    bool runSilencer = true;
    bool resetOnRun = false;
    bool modeB = false;
    int resetScene = 1;
    float clickFilterSlew = DEF_CLICK_FILTER_SLEW;

    Algomorph4();
    void onReset() override;
    void onRandomize() override;
    void process(const ProcessArgs& args) override;
    inline float routeHorizontal(float sampleTime, float inputVoltage, int op, int c);
    inline float routeHorizontalRing(float sampleTime, float inputVoltage, int op, int c);
    inline float routeDiagonal(float sampleTime, float inputVoltage, int op, int mod, int c);
    inline float routeDiagonalB(float sampleTime, float inputVoltage, int op, int mod, int c);
    inline float routeDiagonalRing(float sampleTime, float inputVoltage, int op, int mod, int c);
    inline float routeDiagonalRingB(float sampleTime, float inputVoltage, int op, int mod, int c);
    inline float routeSum(float sampleTime, float inputVoltage, int op, int c);
    inline float routeSumB(float sampleTime, float inputVoltage, int op, int c);
    inline float routeSumRing(float sampleTime, float inputVoltage, int op, int c);
    inline float routeSumRingB(float sampleTime, float inputVoltage, int op, int c);
    inline void scaleAuxSumAttenCV(int channels);
    inline void scaleAuxModAttenCV(int channels);
    inline void scaleAuxClickFilterCV(int channels);
    inline void scaleAuxMorphCV(int channels);
    inline void scaleAuxDoubleMorphCV(int channels);
    inline void scaleAuxTripleMorphCV(int channels);
    inline void scaleAuxMorphAttenCV(int channels);
    inline void scaleAuxShadow(float sampleTime, int op, int channels);
    void rescaleVoltage(int mode, int channels);
    void rescaleVoltages(int channels);
    void initRun();
    void toggleHorizontalDestination(int scene, int op);
    void toggleDiagonalDestination(int scene, int op, int mod);
    bool isCarrier(int scene, int op);
    bool isDisabled(int scene, int op);
    void toggleDisabled(int scene, int op);
    void updateDisplayAlgo(int scene);
    void toggleModeB();
    void toggleForcedCarrier(int scene, int op);
    inline float getPortBrightness(Port port);
    void updateSceneBrightnesses();
    void swapAlgorithms(int a, int b);
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;
};

struct Algomorph4Widget : ModuleWidget {
    std::vector<Vec> SceneButtonCenters = {  {mm2px(53.550), mm2px(48.050)},
                                            {mm2px(53.550), mm2px(56.227)},
                                            {mm2px(53.550), mm2px(64.405)} };
    std::vector<Vec> OpButtonCenters = { {mm2px(22.389), mm2px(50.974)},
                                        {mm2px(22.389), mm2px(61.994)},
                                        {mm2px(22.389), mm2px(73.015)},
                                        {mm2px(22.389), mm2px(84.037)} };
    std::vector<Vec> ModButtonCenters = {{mm2px(39.003), mm2px(50.974)},
                                        {mm2px(39.003), mm2px(61.994)},
                                        {mm2px(39.003), mm2px(73.015)},
                                        {mm2px(39.003), mm2px(84.037)} };
    DLXGlowingInk* ink;

    Algomorph4Widget(Algomorph4* module);
    void appendContextMenu(Menu* menu) override;
    void setKnobMode(int knobMode);
    void toggleInkVisibility();
};