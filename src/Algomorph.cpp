#include "plugin.hpp"
#include "GraphStructure.hpp"
#include <bitset>

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
        MORPH_INPUT,
        SCENE_ADV_INPUT,    //Clk input
        RESET_INPUT,
        AUX_INPUT,
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
    template < typename MODULE >
    struct AuxInput {
        bool mode[AuxInputModes::NUM_MODES] = {false};
        bool mustForget[AuxInputModes::NUM_MODES] = {false};
        bool allowMultipleModes = false;
        int activeModes = 0;
        int lastSetMode;
        bool forgetVoltage = true;
        bool connected = false;
        MODULE* module;
        dsp::SchmittTrigger runCVTrigger;
        dsp::SchmittTrigger sceneAdvCVTrigger;
        dsp::SchmittTrigger reverseSceneAdvCVTrigger;
        dsp::SlewLimiter shadowOpClickFilters[2][4][5][16];     // [noRing/ring][shadow op][legal mod][channel], clickGain[x][y][3][z] = horizontal connection, clickGain[x][y][4][z] = sum output
        float shadowOpClickGains[2][4][5][16] = {{{0.f}}};
        dsp::SlewLimiter wildcardModClickFilter[16];
        float wildcardModClickGain = 0.f;
        dsp::SlewLimiter wildcardSumClickFilter[16];
        float wildcardSumClickGain = 0.f;
        float voltage[AuxInputModes::NUM_MODES][16];
        float defVoltage[AuxInputModes::NUM_MODES] = { 0.f };
        int channels = 0;

        AuxInput(MODULE* m) {
            module = m;
            defVoltage[AuxInputModes::MOD_ATTEN] = 5.f;
            defVoltage[AuxInputModes::SUM_ATTEN] = 5.f;
            resetVoltages();
            for (int i = AuxInputModes::CLOCK; i <= AuxInputModes::RUN; i++)
                mustForget[i] = true;
            for (int i = AuxInputModes::WILDCARD_MOD; i <= AuxInputModes::SHADOW + 3; i++)
                mustForget[i] = true;
            for (int c = 0; c < 16; c++) {
                wildcardModClickFilter[c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
                wildcardSumClickFilter[c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
                for (int i = 0; i < 2; i++) {
                    for (int j = 0; j < 4; j++) {
                        for (int k = 0; k < 5; k++)
                            shadowOpClickFilters[i][j][k][c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
                    }
                }
            }
        }

        void forgetVoltages() {
            for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
                for (int j = 0; j < 16; j++) {
                    if (!mode[i])
                        voltage[i][j] = defVoltage[i];
                }
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

            mode[newMode] = true;
            lastSetMode = newMode;
        }

        void unsetMode(int oldMode) {
            activeModes--;
            
            mode[oldMode] = false;
        }

        void clearModes() {
            activeModes = 0;

            for (int i = 0; i < AuxInputModes::NUM_MODES; i++)
                mode[i] = false;
        }

        void updateVoltage() {
            for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
                if (mode[i]) {
                    for (int c = 0; c < channels; c++)
                        voltage[i][c] = module->inputs[MODULE::AUX_INPUT].getPolyVoltage(c);
                }
            }
        }
    };
    AuxInput<Algomorph4> auxInput = AuxInput<Algomorph4>(this);
    float scaledAuxVoltage[AuxInputModes::NUM_MODES][16];      // store processed (ready-to-use) values from auxInput.voltage[][], so they can be remembered if necessary

    int knobMode = 1;

    int baseScene = 1;                                          // Center the Morph knob on saved algorithm 0, 1, or 2
    float morph[16] = {0.f};                                    // Range -1.f -> 1.f
    float relativeMorphMagnitude[16] = { morph[0] };
    int centerMorphScene[16] = { baseScene }, forwardMorphScene[16] = { (baseScene + 1) % 3 }, backwardMorphScene[16] = { (baseScene + 2) % 3 };

    bool horizontalDestinations[3][4];                          // [scene][op]
    bool opDestinations[3][4][3];                               // [scene][op][legal mod]
    bool forcedCarrier[3][4];                                   // [scene][op]
    std::bitset<16> algoName[3];                                // 16-bit IDs of the three stored algorithms
    std::bitset<16> displayAlgoName[3];                         // When operators are disabled, remove their mod destinations from here

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
    int threeToFour[4][3];                                      // Modulator ID conversion ([op][x] = y, where x is 0..2 and y is 0..3)
    int fourToThree[4][4];
    
    bool configMode = true;
    int configOp = -1;                                          // Set to 0-3 when configuring mod destinations for operators 1-4
    int configScene = 1;
    bool running = true;

    bool graphDirty = true;
    bool debug = false;

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

    dsp::SchmittTrigger sceneAdvCVTrigger;
    dsp::SchmittTrigger resetCVTrigger;
    long clockIgnoreOnReset = (long) (CLOCK_IGNORE_DURATION * APP->engine->getSampleRate());

    dsp::ClockDivider cvDivider;
    dsp::BooleanTrigger sceneButtonTrigger[3];
    dsp::BooleanTrigger editTrigger;
    dsp::BooleanTrigger operatorTrigger[4];
    dsp::BooleanTrigger modulatorTrigger[4];

    dsp::SlewLimiter runClickFilter;
    dsp::SlewLimiter clickFilters[2][4][5][16];    // [noRing/ring][op][legal mod][channel], [op][3] = Sum output
    dsp::ClockDivider clickFilterDivider;

    dsp::ClockDivider lightDivider;
    float sceneBrightnesses[3][12][3] = {{{}}};
    float blinkTimer = BLINK_INTERVAL;
    bool blinkStatus = true;

    Algomorph4() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(MORPH_KNOB, -1.f, 1.f, 0.f, "Morph", " millimorphs", 0, 1000);
        configParam(AUX_KNOBS + AuxKnobModes::MORPH_ATTEN, -1.f, 1.f, 0.f, AuxKnobModeLabels[AuxKnobModes::MORPH_ATTEN], "%", 0, 100);
        configParam(AUX_KNOBS + AuxKnobModes::MORPH, -1.f, 1.f, 0.f, "AUX " + AuxKnobModeLabels[AuxKnobModes::MORPH], " millimorphs", 0, 1000);
        configParam(AUX_KNOBS + AuxKnobModes::SUM_GAIN, 0.f, 1.f, 1.f, AuxKnobModeLabels[AuxKnobModes::SUM_GAIN], "%", 0, 100);
        configParam(AUX_KNOBS + AuxKnobModes::MOD_GAIN, 0.f, 1.f, 1.f, AuxKnobModeLabels[AuxKnobModes::MOD_GAIN], "%", 0, 100);
        configParam(AUX_KNOBS + AuxKnobModes::OP_GAIN, 0.f, 1.f, 1.f, AuxKnobModeLabels[AuxKnobModes::OP_GAIN], "%", 0, 100);
        configParam(AUX_KNOBS + AuxKnobModes::UNI_MORPH, 0.f, 3.f, 0.f, AuxKnobModeLabels[AuxKnobModes::UNI_MORPH], " millimorphs", 0, 1000);
        configParam(AUX_KNOBS + AuxKnobModes::DOUBLE_MORPH, -2.f, 2.f, 0.f, AuxKnobModeLabels[AuxKnobModes::DOUBLE_MORPH], " millimorphs", 0, 1000);
        configParam(AUX_KNOBS + AuxKnobModes::TRIPLE_MORPH, -3.f, 3.f, 0.f, AuxKnobModeLabels[AuxKnobModes::TRIPLE_MORPH], " millimorphs", 0, 1000);
        configParam(AUX_KNOBS + AuxKnobModes::ENDLESS_MORPH, -INFINITY, INFINITY, 0.f, AuxKnobModeLabels[AuxKnobModes::ENDLESS_MORPH], " millimorphs", 0, 1000);
        configParam(AUX_KNOBS + AuxKnobModes::CLICK_FILTER, 0.004, 2.004f, 1.f, AuxKnobModeLabels[AuxKnobModes::CLICK_FILTER], "x", 0, 1);
        for (int i = 0; i < 4; i++) {
            configParam(OPERATOR_BUTTONS + i, 0.f, 1.f, 0.f);
            configParam(MODULATOR_BUTTONS + i, 0.f, 1.f, 0.f);
        }
        for (int i = 0; i < 3; i++) {
            configParam(SCENE_BUTTONS + i, 0.f, 1.f, 0.f);
        }
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 5; j++) {
                for (int c = 0; c < 16; c++) {
                    clickFilters[0][i][j][c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
                    clickFilters[1][i][j][c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
                }
            }
        }
        runClickFilter.setRiseFall(400.f, 400.f);
        clickFilterDivider.setDivision(128);
        lightDivider.setDivision(64);
        cvDivider.setDivision(32);

        // Map 3-bit operator-relative mod output indices to 4-bit generalized equivalents
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (i != j) {
                    threeToFour[i][i > j ? j : j - 1] = j;
                    fourToThree[i][j] = i > j ? j : j - 1;
                }
                else
                    fourToThree[i][j] = -1;
            }
        }

        // Initialize graphAddressTranslation[] to -1, then index local ids by generalized 16-bit equivalents
        for (int i = 0; i < 0xFFFF; i++) {
            graphAddressTranslation[i] = -1;
        }
        for (int i = 0; i < 1980; i++) {
            graphAddressTranslation[(int)xNodeData[i][0]] = i;
        }

        onReset();
    }

    void onReset() override {
        for (int i = 0; i < 3; i++) {
            algoName[i].reset();
            displayAlgoName[i].reset();
            for (int j = 0; j < 4; j++) {
                horizontalDestinations[i][j] = false;
                forcedCarrier[i][j] = false;
                for (int k = 0; k < 3; k++) {
                    opDestinations[i][j][k] = false;
                }
            }
        }
        auxInput.setMode(AuxInputModes::MORPH);
        auxInput.resetVoltages();
        rescaleVoltages();
        auxInput.allowMultipleModes = false;
        auxInput.forgetVoltage = true;
        configMode = false;
        configOp = -1;
        configScene = 1;
        baseScene = 1;
        clickFilterEnabled = true;
        clickFilterSlew = DEF_CLICK_FILTER_SLEW;
        ringMorph = false;
        randomRingMorph = false;
        exitConfigOnConnect = false;
        ccwSceneSelection = true;
        running = true;
        runSilencer = true;
        resetOnRun = false;
        resetScene = 1;
        modeB = false;

        blinkStatus = true;
        blinkTimer = BLINK_INTERVAL;
        graphDirty = true;
    }

    void onRandomize() override {
        //If in config mode, only randomize the current algorithm,
        //do not change the base scene and do not enable/disable ring morph
        if (configMode) {
            bool noCarrier = true;
            algoName[configScene].reset();    //Initialize
            for (int j = 0; j < 4; j++) {
                horizontalDestinations[configScene][j] = false;   //Initialize
                if (modeB) {
                    bool disabled = true;
                    forcedCarrier[configScene][j] = false;   //Initialize
                    if (random::uniform() > .5f) {
                        forcedCarrier[configScene][j] = true;
                        noCarrier = false;
                        disabled = false;
                    }
                    if (random::uniform() > .5f) {
                        horizontalDestinations[configScene][j] = true;
                        //Do not set algoName, because the operator is not disabled
                        disabled = false;
                    }
                    for (int k = 0; k < 3; k++) {
                        opDestinations[configScene][j][k] = false;    //Initialize
                        if (random::uniform() > .5f) {
                            opDestinations[configScene][j][k] = true;
                            algoName[configScene].flip(j * 3 + k);
                            disabled = false;    
                        }
                    }
                    if (disabled)
                        algoName[configScene].flip(12 + j);
                }
                else {
                    forcedCarrier[configScene][j] = false;  //Disable
                    if (random::uniform() > .5f) {      //If true, operator is a carrier
                        noCarrier = false;
                        for (int k = 0; k < 3; k++)
                            opDestinations[configScene][j][k] = false;
                    }
                    else {
                        if (random::uniform() > .5) {    //If true, operator is disabled
                            horizontalDestinations[configScene][j] = true;
                            algoName[configScene].flip(12 + j);
                            for (int k = 0; k < 3; k++)
                                opDestinations[configScene][j][k] = false;
                        }
                        else {
                            for (int k = 0; k < 3; k++) {
                                opDestinations[configScene][j][k] = false;    //Initialize
                                if (random::uniform() > .5f) {
                                    opDestinations[configScene][j][k] = true;
                                    algoName[configScene].flip(j * 3 + k);    
                                }
                            }
                        }
                    }
                }
            }
            if (noCarrier) {
                int shortStraw = std::floor(random::uniform() * 4);
                while (shortStraw == 4)
                    shortStraw = std::floor(random::uniform() * 4);
                if (modeB) {
                    forcedCarrier[configScene][shortStraw] = true;
                    algoName[configScene].set(12 + shortStraw, false);
                }
                else {
                    horizontalDestinations[configScene][shortStraw] = false;
                    algoName[configScene].set(12 + shortStraw, false);
                    for (int k = 0; k < 3; k++) {
                        opDestinations[configScene][shortStraw][k] = false;
                        algoName[configScene].set(shortStraw * 3 + k, false);
                    }
                }
            }
            displayAlgoName[configScene] = algoName[configScene];   // Sync
            if (!modeB) {
                for (int i = 0; i < 4; i++) {
                    if (horizontalDestinations[configScene][i]) {
                        for (int j = 0; j < 3; j++) {
                            displayAlgoName[configScene].set(i * 3 + j, false);     // Set destinations to false in the display
                            // If another operator is modulating this one, enable this operator in the display
                            if (!horizontalDestinations[configScene][threeToFour[i][j]] && opDestinations[configScene][threeToFour[i][j]][fourToThree[threeToFour[i][j]][i]])
                                displayAlgoName[configScene].set(12 + i, false);
                        }
                    }
                }
            }
        }
        //Otherwise, randomize everything
        else {
            for (int i = 0; i < 3; i++) {
                bool noCarrier = true;
                algoName[i].reset();    //Initialize
                for (int j = 0; j < 4; j++) {
                    horizontalDestinations[i][j] = false;   //Initialize
                    if (modeB) {
                        bool disabled = true;           //Initialize
                        forcedCarrier[i][j] = false;    //Initialize
                        if (random::uniform() > .5f) {
                            forcedCarrier[i][j] = true;
                            noCarrier = false;
                            disabled = false;
                        }
                        if (random::uniform() > .5f) {
                            horizontalDestinations[i][j] = true;
                            //Do not set algoName, because the operator is not disabled
                            disabled = false;
                        }
                        for (int k = 0; k < 3; k++) {
                            opDestinations[i][j][k] = false;    //Initialize
                            if (random::uniform() > .5f) {
                                opDestinations[i][j][k] = true;
                                algoName[i].flip(j * 3 + k);
                                disabled = false;
                            }
                        }
                        if (disabled)
                            algoName[configScene].flip(12 + j);
                    }
                    else {
                        forcedCarrier[i][j] = false;   //Disable
                        if (random::uniform() > .5f) {      //If true, operator is a carrier
                            noCarrier = false;
                            for (int k = 0; k < 3; k++)
                                opDestinations[i][j][k] = false;
                        }
                        else {
                            if (random::uniform() > .5) {     //If true, operator is disabled
                                horizontalDestinations[i][j] = true;
                                algoName[i].flip(12 + j);
                                for (int k = 0; k < 3; k++)
                                    opDestinations[i][j][k] = false;
                            }
                            else {
                                for (int k = 0; k < 3; k++) {
                                    opDestinations[i][j][k] = false;    //Initialize
                                    if (random::uniform() > .5f) {
                                        opDestinations[i][j][k] = true;
                                        algoName[i].flip(j * 3 + k);    
                                    }
                                }
                            }
                        }
                    }
                }
                if (noCarrier) {
                    int shortStraw = std::floor(random::uniform() * 4);
                    while (shortStraw == 4)
                        shortStraw = std::floor(random::uniform() * 4);
                    if (modeB) {
                        forcedCarrier[i][shortStraw] = true;
                        algoName[i].set(12 + shortStraw, false);
                    }
                    else {
                        horizontalDestinations[i][shortStraw] = false;
                        algoName[i].set(12 + shortStraw, false);
                        for (int k = 0; k < 3; k++) {
                            opDestinations[i][shortStraw][k] = false;
                            algoName[i].set(shortStraw * 3 + k, false);
                        }
                    }
                }
                displayAlgoName[i] = algoName[i];   // Sync
                if (!modeB) {
                    for (int j = 0; j < 4; j++) {
                        if (horizontalDestinations[i][j]) {
                            for (int k = 0; k < 3; k++) {
                                displayAlgoName[i].set(j * 3 + k, false);     // Set destinations to false in the display
                                // If another operator is modulating this one, enable this operator in the display
                                if (!horizontalDestinations[i][threeToFour[j][k]] && opDestinations[i][threeToFour[j][k]][fourToThree[threeToFour[j][k]][j]])
                                    displayAlgoName[i].set(12 + j, false);
                            }
                        }
                    }
                }
            }
            Module::onRandomize();
        }
        graphDirty = true;
    }

    void process(const ProcessArgs& args) override {
        float in[16] = {0.f};                                   // Operator input channels
        float modOut[4][16] = {{0.f}};                          // Modulator outputs & channels
        float sumOut[16] = {0.f};                               // Sum output channels
        float clickGain[2][4][5][16] = {{{{0.f}}}};             // Click filter gains:  [noRing/ring][op][legal mod][channel], clickGain[x][y][3][z] = sum output
        bool carrier[3][4] = {  {true, true, true, true},       // Per-algorithm operator carriership status
                                {true, true, true, true},       // [scene][op]
                                {true, true, true, true} };
        int sceneOffset[16] = {0};                               //Offset to the base scene
        int channels = 1;                                       // Max channels of operator inputs
        bool processCV = cvDivider.process();

        if (inputs[AUX_INPUT].isConnected()) {
            auxInput.connected = true;
            auxInput.updateVoltage();
        }
        else {
            if (auxInput.connected) {
                auxInput.connected = false;
                auxInput.resetVoltages();
                rescaleVoltages();
                running = true;
            }
        }

        //Determine polyphony count
        for (int i = 0; i < 4; i++) {
            if (channels < inputs[OPERATOR_INPUTS + i].getChannels())
                channels = inputs[OPERATOR_INPUTS + i].getChannels();
        }
        if (channels < inputs[MORPH_INPUT].getChannels())
            channels = inputs[MORPH_INPUT].getChannels();
        if (channels < inputs[AUX_INPUT].getChannels())
            channels = inputs[AUX_INPUT].getChannels();
        auxInput.channels = channels;

        if (processCV) {
            //Reset trigger
            if (resetCVTrigger.process(inputs[RESET_INPUT].getVoltage() + auxInput.voltage[AuxInputModes::RESET][0])) {
                initRun();// must be after sequence reset
                sceneAdvCVTrigger.reset();
            }

            //Run trigger
            if (auxInput.runCVTrigger.process(auxInput.voltage[AuxInputModes::RUN][0])) {
                running ^= true;
                if (running) {
                    if (resetOnRun)
                        initRun();
                }
            }

            //Check to change scene
            //Scene buttons
            for (int i = 0; i < 3; i++) {
                if (sceneButtonTrigger[i].process(params[SCENE_BUTTONS + i].getValue() > 0.f)) {
                    if (configMode) {
                        //If not changing to a new scene
                        if (configScene == i) {
                            //Exit config mode
                            configMode = false;
                        }
                        else {
                            //Switch scene
                            configScene = i;
                        }
                    }
                    else {
                        //If the clicked button does not correspond to the current base scene
                        if (baseScene != i) {
                            //Switch scene
                            // History
                            AlgorithmSceneChangeAction<Algomorph4>* h = new AlgorithmSceneChangeAction<Algomorph4>;
                            h->moduleId = this->id;
                            h->oldScene = baseScene;
                            h->newScene = i;
                            
                            baseScene = i;

                            APP->history->push(h);

                        }
                    }
                    graphDirty = true;
                }
            }
            //Clock input
            if (running && clockIgnoreOnReset == 0l) {
                //Scene advance trigger input
                if (sceneAdvCVTrigger.process(inputs[SCENE_ADV_INPUT].getVoltage())) {
                    //Advance base scene
                    if (!ccwSceneSelection)
                    baseScene = (baseScene + 1) % 3;
                    else
                    baseScene = (baseScene + 2) % 3;
                    graphDirty = true;
                }
                if (auxInput.sceneAdvCVTrigger.process(auxInput.voltage[AuxInputModes::CLOCK][0])) {
                    //Advance base scene
                    if (!ccwSceneSelection)
                    baseScene = (baseScene + 1) % 3;
                    else
                    baseScene = (baseScene + 2) % 3;
                    graphDirty = true;
                }
                if (auxInput.reverseSceneAdvCVTrigger.process(auxInput.voltage[AuxInputModes::REVERSE_CLOCK][0])) {
                    //Advance base scene
                    if (!ccwSceneSelection)
                    baseScene = (baseScene + 2) % 3;
                    else
                    baseScene = (baseScene + 1) % 3;
                    graphDirty = true;
                }
            }
        }

        //Update scene offset
        for (int c = 0; c < channels; c++) {
            float sceneOffsetVoltage = auxInput.voltage[AuxInputModes::SCENE_OFFSET][c];
            if (sceneOffsetVoltage > FIVE_D_THREE)
                sceneOffset[c] = 1;
            else if (sceneOffsetVoltage < -FIVE_D_THREE)
                sceneOffset[c] = 2;
            else
                sceneOffset[c] = 0;
        }

        //  Update morph status
        // Only redraw display if morph on channel 1 has changed
        float morphAttenuversion = clamp(auxInput.voltage[AuxInputModes::MORPH_ATTEN][0] / 5.f + params[AUX_KNOBS + AuxKnobModes::MORPH_ATTEN].getValue(), -1.f, 1.f);
        float newMorph0 =  clamp(inputs[MORPH_INPUT].getVoltage(0) / 5.f, -1.f, 1.f)
                            * morphAttenuversion
                            + params[MORPH_KNOB].getValue()
                            + params[AUX_KNOBS + AuxKnobModes::MORPH].getValue()
                            + params[AUX_KNOBS + AuxKnobModes::DOUBLE_MORPH].getValue()
                            + params[AUX_KNOBS + AuxKnobModes::TRIPLE_MORPH].getValue()
                            + params[AUX_KNOBS + AuxKnobModes::UNI_MORPH].getValue()
                            + fmod(params[AUX_KNOBS + AuxKnobModes::ENDLESS_MORPH].getValue(), 3.f)
                            + clamp(auxInput.voltage[AuxInputModes::MORPH][0] / 5.f, -1.f, 1.f)
                            * morphAttenuversion
                            + clamp(auxInput.voltage[AuxInputModes::DOUBLE_MORPH][0] / FIVE_D_TWO, -2.f, 2.f)
                            * morphAttenuversion
                            + clamp(auxInput.voltage[AuxInputModes::TRIPLE_MORPH][0] / FIVE_D_THREE, -3.f, 3.f)
                            * morphAttenuversion;
        while (newMorph0 > 3.f)
            newMorph0 = -3.f + (newMorph0 - 3.f);
        while (newMorph0 < -3.f)
            newMorph0 = 3.f + (newMorph0 + 3.f);
        newMorph0 = clamp(newMorph0, -3.f, 3.f);
        if (morph[0] != newMorph0) {
            morph[0] = newMorph0;
            graphDirty = true;
        }
        // morph[0] was just done, so start this loop with [1]
        for (int c = 1; c < channels; c++) {
            morph[c] =  clamp(inputs[MORPH_INPUT].getPolyVoltage(c) / 5.f, -1.f, 1.f)
                        * morphAttenuversion
                        + params[MORPH_KNOB].getValue()
                        + params[AUX_KNOBS + AuxKnobModes::MORPH].getValue()
                        + params[AUX_KNOBS + AuxKnobModes::DOUBLE_MORPH].getValue()
                        + params[AUX_KNOBS + AuxKnobModes::TRIPLE_MORPH].getValue()
                        + params[AUX_KNOBS + AuxKnobModes::UNI_MORPH].getValue()
                        + fmod(params[AUX_KNOBS + AuxKnobModes::ENDLESS_MORPH].getValue(), 3.f)
                        + clamp(auxInput.voltage[AuxInputModes::MORPH][c] / 5.f, -1.f, 1.f)
                        * morphAttenuversion
                        + clamp(auxInput.voltage[AuxInputModes::DOUBLE_MORPH][c] / FIVE_D_TWO, -2.f, 2.f)
                        * morphAttenuversion
                        + clamp(auxInput.voltage[AuxInputModes::TRIPLE_MORPH][c] / FIVE_D_THREE, -3.f, 3.f)
                        * morphAttenuversion;
            while (morph[c] > 3.f)
                morph[c] = -3.f + (morph[c] - 3.f);
            while (morph[c] < -3.f)
                morph[c] = 3.f + (morph[c] + 3.f);
        }

        // Update relative morph magnitude and scenes
        float lightRelativeMorphMagnitude[16] = {0.f};
        if (!ringMorph) {
            for (int c = 0; c < channels; c++) {
                relativeMorphMagnitude[c] = lightRelativeMorphMagnitude[c] = morph[c];
                if (morph[c] > 0.f) {
                    if (morph[c] < 1.f) {
                        centerMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                        forwardMorphScene[c] = (baseScene + sceneOffset[c] + 1) % 3;
                        backwardMorphScene[c] = (baseScene + sceneOffset[c] + 2) % 3;
                    }
                    else if (morph[c] == 1.f) {
                        relativeMorphMagnitude[c] = 0.f;
                        lightRelativeMorphMagnitude[c] = 1.f;
                        centerMorphScene[c] = forwardMorphScene[c] = backwardMorphScene[c] = (baseScene + sceneOffset[c] + 1) % 3;
                    }
                    else if (morph[c] < 2.f) {
                        relativeMorphMagnitude[c] -= 1.f;
                        lightRelativeMorphMagnitude[c] = relativeMorphMagnitude[c];
                        centerMorphScene[c] = (baseScene + sceneOffset[c] + 1) % 3;
                        forwardMorphScene[c] = (baseScene + sceneOffset[c] + 2) % 3;
                        backwardMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                    }
                    else if (morph[c] == 2.f) {
                        relativeMorphMagnitude[c] = 0.f;
                        lightRelativeMorphMagnitude[c] = 1.f;
                        centerMorphScene[c] = forwardMorphScene[c] = backwardMorphScene[c] = (baseScene + sceneOffset[c] + 2) % 3;
                    }
                    else if (morph[c] < 3.f) {
                        relativeMorphMagnitude[c] -= 2.f;
                        lightRelativeMorphMagnitude[c] = relativeMorphMagnitude[c];
                        centerMorphScene[c] = (baseScene + sceneOffset[c] + 2) % 3;
                        forwardMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                        backwardMorphScene[c] = (baseScene + sceneOffset[c] + 1) % 3;
                    }
                    else {
                        relativeMorphMagnitude[c] = 0.f;
                        lightRelativeMorphMagnitude[c] = 1.f;
                        centerMorphScene[c] = forwardMorphScene[c] = backwardMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                    }
                }
                else if (morph[c] == 0.f)
                    centerMorphScene[c] = forwardMorphScene[c] = backwardMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                else {
                    relativeMorphMagnitude[c] *= -1.f;
                    lightRelativeMorphMagnitude[c] = relativeMorphMagnitude[c];
                    if (morph[c] > -1.f) {
                        centerMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                        forwardMorphScene[c] = (baseScene + sceneOffset[c] + 2) % 3;
                        backwardMorphScene[c] = (baseScene + sceneOffset[c] + 1) % 3;
                    }
                    else if (morph[c] == -1.f) {
                        relativeMorphMagnitude[c] = 0.f;
                        lightRelativeMorphMagnitude[c] = 1.f;
                        centerMorphScene[c] = forwardMorphScene[c] = backwardMorphScene[c] = (baseScene + sceneOffset[c] + 2) % 3;
                    }
                    else if (morph[c] > -2.f) {
                        relativeMorphMagnitude[c] -= 1.f;
                        lightRelativeMorphMagnitude[c] = relativeMorphMagnitude[c];
                        centerMorphScene[c] = (baseScene + sceneOffset[c] + 2) % 3;
                        forwardMorphScene[c] = (baseScene + sceneOffset[c] + 1) % 3;
                        backwardMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                    }
                    else if (morph[c] == -2.f) {
                        relativeMorphMagnitude[c] = 0.f;
                        lightRelativeMorphMagnitude[c] = 1.f;
                        centerMorphScene[c] = forwardMorphScene[c] = backwardMorphScene[c] = (baseScene + sceneOffset[c] + 1) % 3;
                    }
                    else if (morph[c] < 3.f) {
                        relativeMorphMagnitude[c] -= 2.f;
                        lightRelativeMorphMagnitude[c] = relativeMorphMagnitude[c];
                        centerMorphScene[c] = (baseScene + sceneOffset[c] + 1) % 3;
                        forwardMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                        backwardMorphScene[c] = (baseScene + sceneOffset[c] + 2) % 3;
                    }
                    else {
                        relativeMorphMagnitude[c] = 0.f;
                        lightRelativeMorphMagnitude[c] = 1.f;
                        centerMorphScene[c] = forwardMorphScene[c] = backwardMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                    }
                }
            }
        }
        else {
            for (int c = 0; c < channels; c++) {
                relativeMorphMagnitude[c] = lightRelativeMorphMagnitude[c] = morph[c];
                if (morph[c] > 0.f) {
                    if (morph[c] <= 1.f) {
                        centerMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                        forwardMorphScene[c] = (baseScene + sceneOffset[c] + 1) % 3;
                        backwardMorphScene[c] = (baseScene + sceneOffset[c] + 2) % 3;
                    }
                    else if (morph[c] < 2.f) {
                        relativeMorphMagnitude[c] -= (relativeMorphMagnitude[c] - 1.f) * 2.f;
                        lightRelativeMorphMagnitude[c] = relativeMorphMagnitude[c];
                        centerMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                        forwardMorphScene[c] = (baseScene + sceneOffset[c] + 1) % 3;
                        backwardMorphScene[c] = (baseScene + sceneOffset[c] + 2) % 3;
                    }
                    else if (morph[c] == 2.f) {
                        relativeMorphMagnitude[c] = 0.f;
                        lightRelativeMorphMagnitude[c] = 1.f;
                        centerMorphScene[c] = forwardMorphScene[c] = backwardMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                    }
                    else {
                        relativeMorphMagnitude[c] -= (relativeMorphMagnitude[c] - 1.f) * 2.f;
                        lightRelativeMorphMagnitude[c] = relativeMorphMagnitude[c];
                        centerMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                        forwardMorphScene[c] = (baseScene + sceneOffset[c] + 2) % 3;
                        backwardMorphScene[c] = (baseScene + sceneOffset[c] + 1) % 3;
                    }
                }
                else if (morph[c] == 0.f)
                    centerMorphScene[c] = forwardMorphScene[c] = backwardMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                else {
                    relativeMorphMagnitude[c] *= -1.f;
                    lightRelativeMorphMagnitude[c] = relativeMorphMagnitude[c];
                    if (morph[c] >= -1.f) {
                        centerMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                        forwardMorphScene[c] = (baseScene + sceneOffset[c] + 2) % 3;
                        backwardMorphScene[c] = (baseScene + sceneOffset[c] + 1) % 3;
                    }
                    else if (morph[c] > -2.f) {
                        relativeMorphMagnitude[c] -= (relativeMorphMagnitude[c] - 1.f) * 2.f;
                        lightRelativeMorphMagnitude[c] = relativeMorphMagnitude[c];
                        centerMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                        forwardMorphScene[c] = (baseScene + sceneOffset[c] + 2) % 3;
                        backwardMorphScene[c] = (baseScene + sceneOffset[c] + 1) % 3;
                    }
                    else if (morph[c] == -2.f) {
                        relativeMorphMagnitude[c] = 0.f;
                        centerMorphScene[c] = forwardMorphScene[c] = backwardMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                    }
                    else {
                        relativeMorphMagnitude[c] -= (relativeMorphMagnitude[c] - 1.f) * 2.f;
                        lightRelativeMorphMagnitude[c] = relativeMorphMagnitude[c];
                        centerMorphScene[c] = (baseScene + sceneOffset[c]) % 3;
                        forwardMorphScene[c] = (baseScene + sceneOffset[c] + 1) % 3;
                        backwardMorphScene[c] = (baseScene + sceneOffset[c] + 2) % 3;
                    }
                }
            }
        }

        if (processCV) {
            //Edit button
            if (editTrigger.process(params[EDIT_BUTTON].getValue() > 0.f)) {
                configMode ^= true;
                if (configMode) {
                    blinkStatus = true;
                    blinkTimer = 0.f;
                    if (relativeMorphMagnitude[0] > .5f)
                        configScene = forwardMorphScene[0];
                    else if (relativeMorphMagnitude[0] < -.5f)
                        configScene = backwardMorphScene[0];
                    else
                        configScene = centerMorphScene[0];
                }
                configOp = -1;
            }

            //Check to select/deselect operators
            for (int i = 0; i < 4; i++) {
                if (operatorTrigger[i].process(params[OPERATOR_BUTTONS + i].getValue() > 0.f)) {
                    if (params[OPERATOR_BUTTONS + i].getValue() > 0.f) {
                        if (!configMode) {
                            configMode = true;
                            configOp = i;
                            if (relativeMorphMagnitude[0] > .5f)
                                configScene = forwardMorphScene[0];
                            else if (relativeMorphMagnitude[0] < -.5f)
                                configScene = backwardMorphScene[0];
                            else
                                configScene = centerMorphScene[0];
                            blinkStatus = true;
                            blinkTimer = 0.f;
                        }
                        else if (configOp == i) {  
                            //Deselect operator
                            configOp = -1;
                            configMode = false;
                        }
                        else {
                            configOp = i;
                            blinkStatus = true;
                            blinkTimer = 0.f;
                        }
                        graphDirty = true;
                        break;
                    }
                }
            }

            //Check for config mode destination selection and forced operator designation
            if (configMode) {
                if (configOp > -1) {
                    if (modulatorTrigger[configOp].process(params[MODULATOR_BUTTONS + configOp].getValue() > 0.f)) {  //Op is connected to itself
                        // History
                        AlgorithmHorizontalChangeAction<Algomorph4>* h = new AlgorithmHorizontalChangeAction<Algomorph4>;
                        h->moduleId = this->id;
                        h->scene = configScene;
                        h->op = configOp;

                        horizontalDestinations[configScene][configOp] ^= true;
                        if (!modeB) {
                            algoName[configScene].flip(12 + configOp);
                            displayAlgoName[configScene].set(12 + configOp, algoName[configScene][12 + configOp]);
                            for (int j = 0; j < 4; j++) {
                                if (horizontalDestinations[configScene][j]) {
                                    bool fullDisable = true;
                                    for (int i = 0; i < 3; i++)     // Set all of the disabled operators' destinations fo false
                                        displayAlgoName[configScene].set(j * 3 + i, false);
                                    for (int i = 0; i < 4; i++) {     // Check if any other operators are modulating this operator
                                        if (i != j && !horizontalDestinations[configScene][i] && opDestinations[configScene][i][fourToThree[i][j]])
                                            fullDisable = false;
                                    }
                                    // If anything is modulating the operator, set it enabled in the display. Otherwise, disable it in the display.
                                    if (fullDisable)
                                        displayAlgoName[configScene].set(12 + j, true);
                                    else
                                        displayAlgoName[configScene].set(12 + j, false);
                                }
                                else {
                                    if (j == configOp) {
                                        // Reenable destinations in the display and handle the consequences
                                        for (int i = 0; i < 3; i++) {
                                            if (opDestinations[configScene][configOp][i]) {
                                                displayAlgoName[configScene].set(configOp * 3 + i, true);
                                                // the consequences
                                                if (horizontalDestinations[configScene][threeToFour[configOp][i]])
                                                    displayAlgoName[configScene].set(12 + threeToFour[configOp][i], false);
                                            }
                                        }  
                                    }
                                }
                            }
                        }

                        APP->history->push(h);

                        if (exitConfigOnConnect)
                            configMode = false;
                        
                        graphDirty = true;
                    }
                    else {
                        for (int i = 0; i < 3; i++) {
                            if (modulatorTrigger[threeToFour[configOp][i]].process(params[MODULATOR_BUTTONS + threeToFour[configOp][i]].getValue() > 0.f)) {
                                // History
                                AlgorithmDiagonalChangeAction<Algomorph4>* h = new AlgorithmDiagonalChangeAction<Algomorph4>;
                                h->moduleId = this->id;
                                h->scene = configScene;
                                h->op = configOp;
                                h->mod = i;
                                h->displayAlgoName = displayAlgoName[configScene];

                                opDestinations[configScene][configOp][i] ^= true;
                                algoName[configScene].flip(configOp * 3 + i);
                                if (modeB || !horizontalDestinations[configScene][configOp])
                                    displayAlgoName[configScene].flip(configOp * 3 + i);
                                if (!modeB) {
                                    // If the configOp is not disabled and the mod output in question corresponds to a disabled operator
                                    if (!horizontalDestinations[configScene][configOp] && horizontalDestinations[configScene][threeToFour[configOp][i]]) {
                                        // If a connection has been made, enable that operator visually
                                        if (opDestinations[configScene][configOp][i]) {
                                            displayAlgoName[configScene].set(12 + threeToFour[configOp][i], false);
                                        }
                                        // If a connection has been broken, 
                                        else {
                                            if (horizontalDestinations[configScene][threeToFour[configOp][i]]) {
                                                bool disabled = true;
                                                for (int j = 0; j < 4; j++) {
                                                    if (j != configOp && j != threeToFour[configOp][i] && opDestinations[configScene][j][fourToThree[j][configOp]])
                                                        disabled = false;
                                                }
                                                if (disabled)
                                                    displayAlgoName[configScene].set(12 + threeToFour[configOp][i], true);
                                                else
                                                    displayAlgoName[configScene].set(12 + threeToFour[configOp][i], false);
                                            }
                                        }
                                    }
                                }
                                
                                APP->history->push(h);

                                if (exitConfigOnConnect)
                                    configMode = false;

                                graphDirty = true;
                                break;
                            }
                        }
                    }
                }
                else {
                    for (int i = 0; i < 4; i++) {
                        if (modulatorTrigger[i].process(params[MODULATOR_BUTTONS + i].getValue() > 0.f)) {
                            // History
                            AlgorithmForcedCarrierChangeAction<Algomorph4>* h = new AlgorithmForcedCarrierChangeAction<Algomorph4>;
                            h->moduleId = this->id;
                            h->scene = configScene;
                            h->op = i;

                            forcedCarrier[configScene][i] ^= true;

                            APP->history->push(h);
                            break;
                        }
                    }
                }
            }
            else {
                for (int i = 0; i < 4; i++) {
                    if (modulatorTrigger[i].process(params[MODULATOR_BUTTONS + i].getValue() > 0.f)) {
                        int scene = 1;
                        if (relativeMorphMagnitude[0] > .5f)
                            scene = forwardMorphScene[0];
                        else if (relativeMorphMagnitude[0] < -.5f)
                            scene = backwardMorphScene[0];
                        else
                            scene = centerMorphScene[0];
                        
                        // History
                        AlgorithmForcedCarrierChangeAction<Algomorph4>* h = new AlgorithmForcedCarrierChangeAction<Algomorph4>;
                        h->moduleId = this->id;
                        h->scene = scene;
                        h->op = i;

                        forcedCarrier[scene][i] ^= true;

                        APP->history->push(h);
                        
                        break;
                    }
                }
            }
        }
        
        //Update clickfilter rise/fall times
        if (clickFilterDivider.process()) {
            if (auxInput.mode[AuxInputModes::CLICK_FILTER])
                scaleAuxClickFilterVoltage(channels);
            else {
                for (int c = 0; c < channels; c++) {
                    // Still factor-in scaledAuxVoltage, in case it is remembered. Proper bookkeeping ensures it is set to 1.f in all other cases.
                    float clickFilterResult = clickFilterSlew * scaledAuxVoltage[AuxInputModes::CLICK_FILTER][c] * params[AUX_KNOBS + AuxKnobModes::CLICK_FILTER].getValue();
                    auxInput.wildcardModClickFilter[c].setRiseFall(clickFilterResult, clickFilterResult);
                    auxInput.wildcardSumClickFilter[c].setRiseFall(clickFilterResult, clickFilterResult);
                    for (int i = 0; i < 4; i++) {
                        for (int j = 0; j < 4; j++) {
                            clickFilters[0][i][j][c].setRiseFall(clickFilterResult, clickFilterResult);
                            clickFilters[1][i][j][c].setRiseFall(clickFilterResult, clickFilterResult);
                            auxInput.shadowOpClickFilters[0][i][j][c].setRiseFall(clickFilterResult, clickFilterResult);
                            auxInput.shadowOpClickFilters[1][i][j][c].setRiseFall(clickFilterResult, clickFilterResult);
                        }
                    }
                }
            }
        }

        //Get operator input channel then route to modulation output channel or to sum output channel
        float wildcardMod[16] = {0.f};
        float wildcardSum[16] = {0.f};
        float shadowOp[16] = {0.f};
        float runClickFilterGain;
        if (runSilencer)
            runClickFilterGain = runClickFilter.process(args.sampleTime, running);
        else
            runClickFilterGain = 1.f;
        if (auxInput.mode[AuxInputModes::MOD_ATTEN])
            scaleAuxModAttenVoltage(channels);
        if (auxInput.mode[AuxInputModes::SUM_ATTEN])
            scaleAuxSumAttenVoltage(channels);
        for (int c = 0; c < channels; c++) {
            //Note: clickGain[][][][] and clickFilters[][][][] do not convert index j with threeToFour[][], because j = 3 is used for the sum output
            if (ringMorph) {
                for (int i = 0; i < 4; i++) {
                    if (inputs[OPERATOR_INPUTS + i].isConnected()) {
                        in[c] = inputs[OPERATOR_INPUTS + i].getPolyVoltage(c) * params[AUX_KNOBS + AuxKnobModes::OP_GAIN].getValue();
                        if (!auxInput.mode[AuxInputModes::SHADOW + i]) {
                            //Check current algorithm and morph target
                            if (modeB) {
                                float ringHorizontalConnection = horizontalDestinations[backwardMorphScene[c]][i] * relativeMorphMagnitude[c];
                                clickGain[1][i][3][c] = clickFilterEnabled ? clickFilters[1][i][3][c].process(args.sampleTime, ringHorizontalConnection) : ringHorizontalConnection;
                                float horizontalConnectionA = horizontalDestinations[centerMorphScene[c]][i] * (1.f - relativeMorphMagnitude[c]);
                                float horizontalConnectionB = horizontalDestinations[forwardMorphScene[c]][i] * (relativeMorphMagnitude[c]);
                                float morphedHorizontalConnection = horizontalConnectionA + horizontalConnectionB;
                                clickGain[0][i][3][c] = clickFilterEnabled ? clickFilters[0][i][3][c].process(args.sampleTime, morphedHorizontalConnection) : morphedHorizontalConnection;
                                modOut[i][c] += (in[c] * clickGain[0][i][3][c] - in[c] * clickGain[1][i][3][c]) * scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::MOD_GAIN].getValue() * runClickFilterGain;
                                for (int j = 0; j < 3; j++) {
                                    float ringConnection = opDestinations[backwardMorphScene[c]][i][j] * relativeMorphMagnitude[c];
                                    clickGain[1][i][j][c] = clickFilterEnabled ? clickFilters[1][i][j][c].process(args.sampleTime, ringConnection) : ringConnection; 
                                    float connectionA = opDestinations[centerMorphScene[c]][i][j]   * (1.f - relativeMorphMagnitude[c]);
                                    float connectionB = opDestinations[forwardMorphScene[c]][i][j]  * relativeMorphMagnitude[c];
                                    float morphedConnection = connectionA + connectionB;
                                    clickGain[0][i][j][c] = clickFilterEnabled ? clickFilters[0][i][j][c].process(args.sampleTime, morphedConnection) : morphedConnection;
                                    modOut[threeToFour[i][j]][c] += (in[c] * clickGain[0][i][j][c] - in[c] * clickGain[1][i][j][c]) * scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::MOD_GAIN].getValue() * runClickFilterGain;
                                }
                                carrier[backwardMorphScene[c]][i] = forcedCarrier[backwardMorphScene[c]][i];
                                float ringSumConnection = carrier[backwardMorphScene[c]][i] * relativeMorphMagnitude[c];
                                clickGain[1][i][3][c] = clickFilterEnabled ? clickFilters[1][i][3][c].process(args.sampleTime, ringSumConnection) : ringSumConnection;
                                carrier[centerMorphScene[c]][i] = forcedCarrier[centerMorphScene[c]][i];
                                carrier[forwardMorphScene[c]][i] = forcedCarrier[forwardMorphScene[c]][i];
                                float sumConnection =   carrier[centerMorphScene[c]][i]     * (1.f - relativeMorphMagnitude[c])
                                                    +   carrier[forwardMorphScene[c]][i]    * relativeMorphMagnitude[c];
                                clickGain[0][i][3][c] = clickFilterEnabled ? clickFilters[0][i][3][c].process(args.sampleTime, sumConnection) : sumConnection;
                                sumOut[c] += (in[c] * clickGain[0][i][3][c] - in[c] * clickGain[1][i][3][c]) * scaledAuxVoltage[AuxInputModes::SUM_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::SUM_GAIN].getValue() * runClickFilterGain;
                            }
                            else {
                                for (int j = 0; j < 3; j++) {
                                    float ringConnection = opDestinations[backwardMorphScene[c]][i][j] * relativeMorphMagnitude[c] * !horizontalDestinations[backwardMorphScene[c]][i];
                                    carrier[backwardMorphScene[c]][i] = ringConnection == 0.f && !horizontalDestinations[backwardMorphScene[c]][i] ? carrier[backwardMorphScene[c]][i] : false;
                                    clickGain[1][i][j][c] = clickFilterEnabled ? clickFilters[1][i][j][c].process(args.sampleTime, ringConnection) : ringConnection; 
                                    float connectionA = opDestinations[centerMorphScene[c]][i][j]   * (1.f - relativeMorphMagnitude[c])  * !horizontalDestinations[centerMorphScene[c]][i];
                                    float connectionB = opDestinations[forwardMorphScene[c]][i][j]  * relativeMorphMagnitude[c]          * !horizontalDestinations[forwardMorphScene[c]][i];
                                    float morphedConnection = connectionA + connectionB;
                                    carrier[centerMorphScene[c]][i]  = connectionA > 0.f ? false : carrier[centerMorphScene[c]][i];
                                    carrier[forwardMorphScene[c]][i] = connectionB > 0.f ? false : carrier[forwardMorphScene[c]][i];
                                    clickGain[0][i][j][c] = clickFilterEnabled ? clickFilters[0][i][j][c].process(args.sampleTime, morphedConnection) : morphedConnection;
                                    modOut[threeToFour[i][j]][c] += (in[c] * clickGain[0][i][j][c] - in[c] * clickGain[1][i][j][c]) * scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::MOD_GAIN].getValue() * runClickFilterGain;
                                }
                                carrier[backwardMorphScene[c]][i] |= forcedCarrier[backwardMorphScene[c]][i];
                                float ringSumConnection = carrier[backwardMorphScene[c]][i] * relativeMorphMagnitude[c] * !horizontalDestinations[backwardMorphScene[c]][i];
                                clickGain[1][i][3][c] = clickFilterEnabled ? clickFilters[1][i][3][c].process(args.sampleTime, ringSumConnection) : ringSumConnection;
                                carrier[centerMorphScene[c]][i] |= forcedCarrier[centerMorphScene[c]][i];
                                carrier[forwardMorphScene[c]][i] |= forcedCarrier[forwardMorphScene[c]][i];
                                float sumConnection =   carrier[centerMorphScene[c]][i]     * (1.f - relativeMorphMagnitude[c])  * !horizontalDestinations[centerMorphScene[c]][i]
                                                    +   carrier[forwardMorphScene[c]][i]    * relativeMorphMagnitude[c]          * !horizontalDestinations[forwardMorphScene[c]][i];
                                clickGain[0][i][3][c] = clickFilterEnabled ? clickFilters[0][i][3][c].process(args.sampleTime, sumConnection) : sumConnection;
                                sumOut[c] += (in[c] * clickGain[0][i][3][c] - in[c] * clickGain[1][i][3][c]) * scaledAuxVoltage[AuxInputModes::SUM_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::SUM_GAIN].getValue() * runClickFilterGain;
                            }
                        }
                        else {
                            shadowOp[c] = auxInput.voltage[AuxInputModes::SHADOW + i][c];
                            //Check current algorithm and morph target
                            if (modeB) {
                                float ringHorizontalConnection = horizontalDestinations[backwardMorphScene[c]][i] * relativeMorphMagnitude[c];
                                clickGain[1][i][3][c] = clickFilterEnabled ? clickFilters[1][i][3][c].process(args.sampleTime, ringHorizontalConnection) : ringHorizontalConnection;
                                auxInput.shadowOpClickGains[1][i][3][c] = clickFilterEnabled ? auxInput.shadowOpClickFilters[1][i][3][c].process(args.sampleTime, ringHorizontalConnection) : ringHorizontalConnection;
                                float horizontalConnectionA = horizontalDestinations[centerMorphScene[c]][i] * (1.f - relativeMorphMagnitude[c]);
                                float horizontalConnectionB = horizontalDestinations[forwardMorphScene[c]][i] * (relativeMorphMagnitude[c]);
                                float morphedHorizontalConnection = horizontalConnectionA + horizontalConnectionB;
                                clickGain[0][i][3][c] = clickFilterEnabled ? clickFilters[0][i][3][c].process(args.sampleTime, morphedHorizontalConnection) : morphedHorizontalConnection;
                                auxInput.shadowOpClickGains[0][i][3][c] = clickFilterEnabled ? auxInput.shadowOpClickFilters[0][i][3][c].process(args.sampleTime, morphedHorizontalConnection) : morphedHorizontalConnection;
                                modOut[i][c] += ((in[c] * clickGain[0][i][3][c] - in[c] * clickGain[1][i][3][c]) + (shadowOp[c] * auxInput.shadowOpClickGains[0][i][3][c] - shadowOp[c] * auxInput.shadowOpClickGains[1][i][3][c])) * scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::MOD_GAIN].getValue() * runClickFilterGain;
                                for (int j = 0; j < 3; j++) {
                                    float ringConnection = opDestinations[backwardMorphScene[c]][i][j] * relativeMorphMagnitude[c];
                                    clickGain[1][i][j][c] = clickFilterEnabled ? clickFilters[1][i][j][c].process(args.sampleTime, ringConnection) : ringConnection; 
                                    auxInput.shadowOpClickGains[1][i][j][c] = clickFilterEnabled ? auxInput.shadowOpClickFilters[1][i][j][c].process(args.sampleTime, ringConnection) : ringConnection;
                                    float connectionA = opDestinations[centerMorphScene[c]][i][j]   * (1.f - relativeMorphMagnitude[c]);
                                    float connectionB = opDestinations[forwardMorphScene[c]][i][j]  * relativeMorphMagnitude[c];
                                    float morphedConnection = connectionA + connectionB;
                                    clickGain[0][i][j][c] = clickFilterEnabled ? clickFilters[0][i][j][c].process(args.sampleTime, morphedConnection) : morphedConnection;
                                    auxInput.shadowOpClickGains[0][i][j][c] = clickFilterEnabled ? auxInput.shadowOpClickFilters[0][i][j][c].process(args.sampleTime, morphedConnection) : morphedConnection;
                                    modOut[threeToFour[i][j]][c] += ((in[c] * clickGain[0][i][j][c] - in[c] * clickGain[1][i][j][c]) + (shadowOp[c] * auxInput.shadowOpClickGains[0][i][j][c] - shadowOp[c] * auxInput.shadowOpClickGains[1][i][j][c])) * scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::MOD_GAIN].getValue() * runClickFilterGain;
                                }
                                carrier[backwardMorphScene[c]][i] = forcedCarrier[backwardMorphScene[c]][i];
                                float ringSumConnection = carrier[backwardMorphScene[c]][i] * relativeMorphMagnitude[c];
                                clickGain[1][i][3][c] = clickFilterEnabled ? clickFilters[1][i][3][c].process(args.sampleTime, ringSumConnection) : ringSumConnection;
                                auxInput.shadowOpClickGains[1][i][3][c] = clickFilterEnabled ? auxInput.shadowOpClickFilters[1][i][3][c].process(args.sampleTime, ringSumConnection) : ringSumConnection;
                                carrier[centerMorphScene[c]][i] = forcedCarrier[centerMorphScene[c]][i];
                                carrier[forwardMorphScene[c]][i] = forcedCarrier[forwardMorphScene[c]][i];
                                float sumConnection =   carrier[centerMorphScene[c]][i]     * (1.f - relativeMorphMagnitude[c])
                                                    +   carrier[forwardMorphScene[c]][i]    * relativeMorphMagnitude[c];
                                clickGain[0][i][3][c] = clickFilterEnabled ? clickFilters[0][i][3][c].process(args.sampleTime, sumConnection) : sumConnection;
                                auxInput.shadowOpClickGains[0][i][3][c] = clickFilterEnabled ? auxInput.shadowOpClickFilters[0][i][3][c].process(args.sampleTime, sumConnection) : sumConnection;
                                sumOut[c] += ((in[c] * clickGain[0][i][3][c] - in[c] * clickGain[1][i][3][c]) + (shadowOp[c] * auxInput.shadowOpClickGains[0][i][3][c] - shadowOp[c] * auxInput.shadowOpClickGains[1][i][3][c])) * scaledAuxVoltage[AuxInputModes::SUM_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::SUM_GAIN].getValue() * runClickFilterGain;
                            }
                            else {
                                for (int j = 0; j < 3; j++) {
                                    float ringConnection = opDestinations[backwardMorphScene[c]][i][j] * relativeMorphMagnitude[c] * !horizontalDestinations[backwardMorphScene[c]][i];
                                    clickGain[1][i][j][c] = clickFilterEnabled ? clickFilters[1][i][j][c].process(args.sampleTime, ringConnection) : ringConnection; 
                                    carrier[backwardMorphScene[c]][i] = ringConnection == 0.f && !horizontalDestinations[backwardMorphScene[c]][i] ? carrier[backwardMorphScene[c]][i] : false;
                                    auxInput.shadowOpClickGains[1][i][j][c] = clickFilterEnabled ? auxInput.shadowOpClickFilters[1][i][j][c].process(args.sampleTime, ringConnection) : ringConnection;
                                    float connectionA = opDestinations[centerMorphScene[c]][i][j]   * (1.f - relativeMorphMagnitude[c])  * !horizontalDestinations[centerMorphScene[c]][i];
                                    float connectionB = opDestinations[forwardMorphScene[c]][i][j]  * relativeMorphMagnitude[c]          * !horizontalDestinations[forwardMorphScene[c]][i];
                                    float morphedConnection = connectionA + connectionB;
                                    clickGain[0][i][j][c] = clickFilterEnabled ? clickFilters[0][i][j][c].process(args.sampleTime, morphedConnection) : morphedConnection;
                                    carrier[centerMorphScene[c]][i]  = connectionA > 0.f ? false : carrier[centerMorphScene[c]][i];
                                    carrier[forwardMorphScene[c]][i] = connectionB > 0.f ? false : carrier[forwardMorphScene[c]][i];
                                    auxInput.shadowOpClickGains[0][i][j][c] = clickFilterEnabled ? auxInput.shadowOpClickFilters[0][i][j][c].process(args.sampleTime, morphedConnection) : morphedConnection;
                                    modOut[threeToFour[i][j]][c] += ((in[c] * clickGain[0][i][j][c] - in[c] * clickGain[1][i][j][c]) + (shadowOp[c] * auxInput.shadowOpClickGains[0][i][j][c] - shadowOp[c] * auxInput.shadowOpClickGains[1][i][j][c])) * scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::MOD_GAIN].getValue() * runClickFilterGain;
                                }
                                carrier[backwardMorphScene[c]][i] |= forcedCarrier[backwardMorphScene[c]][i];
                                float ringSumConnection = carrier[backwardMorphScene[c]][i] * relativeMorphMagnitude[c] * !horizontalDestinations[backwardMorphScene[c]][i];
                                clickGain[1][i][3][c] = clickFilterEnabled ? clickFilters[1][i][3][c].process(args.sampleTime, ringSumConnection) : ringSumConnection;
                                auxInput.shadowOpClickGains[1][i][3][c] = clickFilterEnabled ? auxInput.shadowOpClickFilters[1][i][3][c].process(args.sampleTime, ringSumConnection) : ringSumConnection;
                                carrier[centerMorphScene[c]][i] |= forcedCarrier[centerMorphScene[c]][i];
                                carrier[forwardMorphScene[c]][i] |= forcedCarrier[forwardMorphScene[c]][i];
                                float sumConnection =   carrier[centerMorphScene[c]][i]     * (1.f - relativeMorphMagnitude[c])  * !horizontalDestinations[centerMorphScene[c]][i]
                                                    +   carrier[forwardMorphScene[c]][i]    * relativeMorphMagnitude[c]          * !horizontalDestinations[forwardMorphScene[c]][i];
                                clickGain[0][i][3][c] = clickFilterEnabled ? clickFilters[0][i][3][c].process(args.sampleTime, sumConnection) : sumConnection;
                                auxInput.shadowOpClickGains[0][i][3][c] = clickFilterEnabled ? auxInput.shadowOpClickFilters[0][i][3][c].process(args.sampleTime, sumConnection) : sumConnection;
                                sumOut[c] += ((in[c] * clickGain[0][i][3][c] - in[c] * clickGain[1][i][3][c]) + (shadowOp[c] * auxInput.shadowOpClickGains[0][i][3][c] - shadowOp[c] * auxInput.shadowOpClickGains[1][i][3][c])) * scaledAuxVoltage[AuxInputModes::SUM_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::SUM_GAIN].getValue() * runClickFilterGain;
                            }
                        }
                    }
                }
            }
            else {
                for (int i = 0; i < 4; i++) {
                    if (inputs[OPERATOR_INPUTS + i].isConnected()) {
                        in[c] = inputs[OPERATOR_INPUTS + i].getPolyVoltage(c) * params[AUX_KNOBS + AuxKnobModes::OP_GAIN].getValue();
                        if (!auxInput.mode[AuxInputModes::SHADOW + i]) {
                            //Check current algorithm and morph target
                            if (modeB) {
                                float horizontalConnectionA = horizontalDestinations[centerMorphScene[c]][i] * (1.f - relativeMorphMagnitude[c]);
                                float horizontalConnectionB = horizontalDestinations[forwardMorphScene[c]][i] * (relativeMorphMagnitude[c]);
                                float morphedHorizontalConnection = horizontalConnectionA + horizontalConnectionB;
                                clickGain[0][i][3][c] = clickFilterEnabled ? clickFilters[0][i][3][c].process(args.sampleTime, morphedHorizontalConnection) : morphedHorizontalConnection;
                                modOut[i][c] += in[c] * clickGain[0][i][3][c] * scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::MOD_GAIN].getValue() * runClickFilterGain;
                                for (int j = 0; j < 3; j++) {
                                    float connectionA = opDestinations[centerMorphScene[c]][i][j]   * (1.f - relativeMorphMagnitude[c]);
                                    float connectionB = opDestinations[forwardMorphScene[c]][i][j]  * relativeMorphMagnitude[c];
                                    float morphedConnection = connectionA + connectionB;
                                    clickGain[0][i][j][c] = clickFilterEnabled ? clickFilters[0][i][j][c].process(args.sampleTime, morphedConnection) : morphedConnection;
                                    modOut[threeToFour[i][j]][c] += in[c] * clickGain[0][i][j][c]  * scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::MOD_GAIN].getValue() * runClickFilterGain;
                                }
                                carrier[centerMorphScene[c]][i] = forcedCarrier[centerMorphScene[c]][i];
                                carrier[forwardMorphScene[c]][i] = forcedCarrier[forwardMorphScene[c]][i];
                                float sumConnection =   carrier[centerMorphScene[c]][i]     * (1.f - relativeMorphMagnitude[c])
                                                    +   carrier[forwardMorphScene[c]][i]    * relativeMorphMagnitude[c];
                                clickGain[0][i][4][c] = clickFilterEnabled ? clickFilters[0][i][4][c].process(args.sampleTime, sumConnection) : sumConnection;
                                sumOut[c] += in[c] * clickGain[0][i][4][c] * scaledAuxVoltage[AuxInputModes::SUM_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::SUM_GAIN].getValue() * runClickFilterGain;
                            }
                            else {
                                for (int j = 0; j < 3; j++) {
                                    float connectionA = opDestinations[centerMorphScene[c]][i][j]   * (1.f - relativeMorphMagnitude[c])  * !horizontalDestinations[centerMorphScene[c]][i];
                                    float connectionB = opDestinations[forwardMorphScene[c]][i][j]  * relativeMorphMagnitude[c]          * !horizontalDestinations[forwardMorphScene[c]][i];
                                    float morphedConnection = connectionA + connectionB;
                                    clickGain[0][i][j][c] = clickFilterEnabled ? clickFilters[0][i][j][c].process(args.sampleTime, morphedConnection) : morphedConnection;
                                    carrier[centerMorphScene[c]][i]  = connectionA > 0.f ? false : carrier[centerMorphScene[c]][i];
                                    carrier[forwardMorphScene[c]][i] = connectionB > 0.f ? false : carrier[forwardMorphScene[c]][i];
                                    modOut[threeToFour[i][j]][c] += in[c] * clickGain[0][i][j][c]  * scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::MOD_GAIN].getValue() * runClickFilterGain;
                                }
                                carrier[centerMorphScene[c]][i] |= forcedCarrier[centerMorphScene[c]][i];
                                carrier[forwardMorphScene[c]][i] |= forcedCarrier[forwardMorphScene[c]][i];
                                float sumConnection =   carrier[centerMorphScene[c]][i]     * (1.f - relativeMorphMagnitude[c])  * !horizontalDestinations[centerMorphScene[c]][i]
                                                    +   carrier[forwardMorphScene[c]][i]    * relativeMorphMagnitude[c]          * !horizontalDestinations[forwardMorphScene[c]][i];
                                clickGain[0][i][4][c] = clickFilterEnabled ? clickFilters[0][i][4][c].process(args.sampleTime, sumConnection) : sumConnection;
                                sumOut[c] += in[c] * clickGain[0][i][4][c] * scaledAuxVoltage[AuxInputModes::SUM_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::SUM_GAIN].getValue() * runClickFilterGain;
                            }
                        }
                        else {
                            shadowOp[c] = auxInput.voltage[AuxInputModes::SHADOW + i][c];
                            //Check current algorithm and morph target
                            if (modeB) {
                                float horizontalConnectionA = horizontalDestinations[centerMorphScene[c]][i] * (1.f - relativeMorphMagnitude[c]);
                                float horizontalConnectionB = horizontalDestinations[forwardMorphScene[c]][i] * (relativeMorphMagnitude[c]);
                                float morphedHorizontalConnection = horizontalConnectionA + horizontalConnectionB;
                                clickGain[0][i][3][c] = clickFilterEnabled ? clickFilters[0][i][3][c].process(args.sampleTime, morphedHorizontalConnection) : morphedHorizontalConnection;
                                auxInput.shadowOpClickGains[0][i][3][c] = clickFilterEnabled ? auxInput.shadowOpClickFilters[0][i][3][c].process(args.sampleTime, morphedHorizontalConnection) : morphedHorizontalConnection;
                                modOut[i][c] += (in[c] * clickGain[0][i][3][c] + shadowOp[c] * auxInput.shadowOpClickGains[0][i][3][c]) * scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::MOD_GAIN].getValue() * runClickFilterGain;
                                for (int j = 0; j < 3; j++) {
                                    float connectionA = opDestinations[centerMorphScene[c]][i][j]   * (1.f - relativeMorphMagnitude[c]);
                                    float connectionB = opDestinations[forwardMorphScene[c]][i][j]  * relativeMorphMagnitude[c];
                                    float morphedConnection = connectionA + connectionB;
                                    clickGain[0][i][j][c] = clickFilterEnabled ? clickFilters[0][i][j][c].process(args.sampleTime, morphedConnection) : morphedConnection;
                                    auxInput.shadowOpClickGains[0][i][j][c] = clickFilterEnabled ? auxInput.shadowOpClickFilters[0][i][j][c].process(args.sampleTime, morphedConnection) : morphedConnection;
                                    modOut[threeToFour[i][j]][c] += (in[c] * clickGain[0][i][j][c] + shadowOp[c] * auxInput.shadowOpClickGains[0][i][j][c]) * scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::MOD_GAIN].getValue() * runClickFilterGain;
                                }
                                carrier[centerMorphScene[c]][i] = forcedCarrier[centerMorphScene[c]][i];
                                carrier[forwardMorphScene[c]][i] = forcedCarrier[forwardMorphScene[c]][i];
                                float sumConnection =   carrier[centerMorphScene[c]][i]     * (1.f - relativeMorphMagnitude[c])
                                                    +   carrier[forwardMorphScene[c]][i]    * relativeMorphMagnitude[c];
                                clickGain[0][i][4][c] = clickFilterEnabled ? clickFilters[0][i][4][c].process(args.sampleTime, sumConnection) : sumConnection;
                                auxInput.shadowOpClickGains[0][i][4][c] = clickFilterEnabled ? auxInput.shadowOpClickFilters[0][i][4][c].process(args.sampleTime, sumConnection) : sumConnection;
                                sumOut[c] += (in[c] * clickGain[0][i][4][c] + shadowOp[c] * auxInput.shadowOpClickGains[0][i][4][c]) * runClickFilterGain;
                            }
                            else {
                                for (int j = 0; j < 3; j++) {
                                    float connectionA = opDestinations[centerMorphScene[c]][i][j]   * (1.f - relativeMorphMagnitude[c])  * !horizontalDestinations[centerMorphScene[c]][i];
                                    float connectionB = opDestinations[forwardMorphScene[c]][i][j]  * relativeMorphMagnitude[c]          * !horizontalDestinations[forwardMorphScene[c]][i];
                                    float morphedConnection = connectionA + connectionB;
                                    clickGain[0][i][j][c] = clickFilterEnabled ? clickFilters[0][i][j][c].process(args.sampleTime, morphedConnection) : morphedConnection;
                                    carrier[centerMorphScene[c]][i]  = connectionA > 0.f ? false : carrier[centerMorphScene[c]][i];
                                    carrier[forwardMorphScene[c]][i] = connectionB > 0.f ? false : carrier[forwardMorphScene[c]][i];
                                    auxInput.shadowOpClickGains[0][i][j][c] = clickFilterEnabled ? auxInput.shadowOpClickFilters[0][i][j][c].process(args.sampleTime, morphedConnection) : morphedConnection;
                                    modOut[threeToFour[i][j]][c] += (in[c] * clickGain[0][i][j][c] + shadowOp[c] * auxInput.shadowOpClickGains[0][i][j][c]) * scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::MOD_GAIN].getValue() * runClickFilterGain;
                                }
                                carrier[centerMorphScene[c]][i] |= forcedCarrier[centerMorphScene[c]][i];
                                carrier[forwardMorphScene[c]][i] |= forcedCarrier[forwardMorphScene[c]][i];
                                float sumConnection =   carrier[centerMorphScene[c]][i]     * (1.f - relativeMorphMagnitude[c])  * !horizontalDestinations[centerMorphScene[c]][i]
                                                    +   carrier[forwardMorphScene[c]][i]    * relativeMorphMagnitude[c]          * !horizontalDestinations[forwardMorphScene[c]][i];
                                clickGain[0][i][4][c] = clickFilterEnabled ? clickFilters[0][i][4][c].process(args.sampleTime, sumConnection) : sumConnection;
                                auxInput.shadowOpClickGains[0][i][4][c] = clickFilterEnabled ? auxInput.shadowOpClickFilters[0][i][4][c].process(args.sampleTime, sumConnection) : sumConnection;
                                sumOut[c] += (in[c] * clickGain[0][i][4][c] + shadowOp[c] * auxInput.shadowOpClickGains[0][i][4][c]) * scaledAuxVoltage[AuxInputModes::SUM_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::SUM_GAIN].getValue() * scaledAuxVoltage[AuxInputModes::SUM_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::SUM_GAIN].getValue() * runClickFilterGain;
                            }
                        }
                    }
                }
            }
            if (inputs[AUX_INPUT].isConnected()) {
                if (auxInput.mode[AuxInputModes::WILDCARD_MOD]) {
                    wildcardMod[c] = auxInput.voltage[AuxInputModes::WILDCARD_MOD][c];
                    auxInput.wildcardModClickGain = (clickFilterEnabled ? auxInput.wildcardModClickFilter[c].process(args.sampleTime, auxInput.mode[AuxInputModes::WILDCARD_MOD]) : auxInput.mode[AuxInputModes::WILDCARD_MOD]);
                    for (int i = 0; i < 4; i++) {
                        modOut[i][c] += wildcardMod[c] * auxInput.wildcardModClickGain * scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::MOD_GAIN].getValue() * runClickFilterGain;

                    }
                }
                if (auxInput.mode[AuxInputModes::WILDCARD_SUM]) {
                    wildcardSum[c] = auxInput.voltage[AuxInputModes::WILDCARD_SUM][c];
                    auxInput.wildcardSumClickGain = (clickFilterEnabled ? auxInput.wildcardSumClickFilter[c].process(args.sampleTime, auxInput.mode[AuxInputModes::WILDCARD_SUM]) : auxInput.mode[AuxInputModes::WILDCARD_SUM]);
                    sumOut[c] += wildcardSum[c] * auxInput.wildcardSumClickGain * scaledAuxVoltage[AuxInputModes::SUM_ATTEN][c] * params[AUX_KNOBS + AuxKnobModes::SUM_GAIN].getValue() * runClickFilterGain;
                }
            }
        }

        //Set outputs
        for (int i = 0; i < 4; i++) {
            if (outputs[MODULATOR_OUTPUTS + i].isConnected()) {
                outputs[MODULATOR_OUTPUTS + i].setChannels(channels);
                outputs[MODULATOR_OUTPUTS + i].writeVoltages(modOut[i]);
            }
        }
        if (outputs[SUM_OUTPUT].isConnected()) {
            outputs[SUM_OUTPUT].setChannels(channels);
            outputs[SUM_OUTPUT].writeVoltages(sumOut);
        }

        //Set lights
        if (lightDivider.process()) {
            if (configMode) {   //Display state without morph, highlight configScene
                //Set purple component to off
                lights[DISPLAY_BACKLIGHT].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                //Set yellow component
                lights[DISPLAY_BACKLIGHT + 1].setSmoothBrightness(getPortBrightness(outputs[SUM_OUTPUT]) / 2048.f + 0.014325f, args.sampleTime * lightDivider.getDivision());
                //Set edit light
                lights[EDIT_LIGHT].setSmoothBrightness(1.f, args.sampleTime * lightDivider.getDivision());
                //Set scene lights
                for (int i = 0; i < 3; i++) {
                    //Set purple components to off
                    lights[SCENE_LIGHTS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    lights[SCENE_INDICATORS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    //Set yellow components depending on config scene and blink status
                    lights[SCENE_LIGHTS + i * 3 + 1].setSmoothBrightness(configScene == i ? 1.f : 0.f, args.sampleTime * lightDivider.getDivision());
                    lights[SCENE_INDICATORS + i * 3 + 1].setSmoothBrightness(configScene == i ?
                        INDICATOR_BRIGHTNESS
                        : 0.f, args.sampleTime * lightDivider.getDivision());
                }
                for (int i = 0; i < 3; i++) {
                    //Set purple component to off
                    lights[SCENE_LIGHTS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                }
                //Set op/mod lights
                for (int i = 0; i < 4; i++) {
                    if (horizontalDestinations[configScene][i]) {
                        if (modeB) {
                            //Set carrier indicator
                            if (forcedCarrier[configScene][i]) {
                                //Purple light
                                lights[CARRIER_INDICATORS + i * 3].setSmoothBrightness(configOp == i ?
                                    blinkStatus ?
                                        0.f
                                        : INDICATOR_BRIGHTNESS
                                    : INDICATOR_BRIGHTNESS, args.sampleTime * lightDivider.getDivision());
                                //Yellow light
                                lights[CARRIER_INDICATORS + i * 3 + 1].setSmoothBrightness(configOp == i ?
                                    blinkStatus
                                    : 0.f, args.sampleTime * lightDivider.getDivision());
                                //Red light
                                lights[CARRIER_INDICATORS + i * 3 + 2].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                            }
                            else {
                                //Purple light
                                lights[CARRIER_INDICATORS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                                //Yellow light
                                lights[CARRIER_INDICATORS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                                //Red light
                                lights[CARRIER_INDICATORS + i * 3 + 2].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                            }
                            //Set op lights
                            //Purple lights
                            lights[OPERATOR_LIGHTS + i * 3].setSmoothBrightness(configOp == i ?
                                blinkStatus ?
                                    0.f
                                    : DEF_RED_BRIGHTNESS
                                : DEF_RED_BRIGHTNESS, args.sampleTime * lightDivider.getDivision());
                            //Yellow Lights
                            lights[OPERATOR_LIGHTS + i * 3 + 1].setSmoothBrightness(configOp == i ?
                                blinkStatus
                                : 0.f, args.sampleTime * lightDivider.getDivision());
                            //Red lights
                            lights[OPERATOR_LIGHTS + i * 3 + 2].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        }
                        else {
                            //Set carrier indicator
                            if (forcedCarrier[configScene][i]) {
                                //Purple light
                                lights[CARRIER_INDICATORS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                                //Yellow light
                                lights[CARRIER_INDICATORS + i * 3 + 1].setSmoothBrightness(configOp == i ?
                                    blinkStatus
                                    : 0.f, args.sampleTime * lightDivider.getDivision());
                                //Red light
                                lights[CARRIER_INDICATORS + i * 3 + 2].setSmoothBrightness(configOp == i ?
                                    blinkStatus ?
                                        0.f
                                        : DEF_RED_BRIGHTNESS
                                    : DEF_RED_BRIGHTNESS, args.sampleTime * lightDivider.getDivision());
                            }
                            else {
                                //Purple light
                                lights[CARRIER_INDICATORS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                                //Yellow light
                                lights[CARRIER_INDICATORS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                                //Red light
                                lights[CARRIER_INDICATORS + i * 3 + 2].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                            }
                            //Set op lights
                            //Purple lights
                            lights[OPERATOR_LIGHTS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                            //Yellow Lights
                            lights[OPERATOR_LIGHTS + i * 3 + 1].setSmoothBrightness(configOp == i ?
                                blinkStatus
                                : 0.f, args.sampleTime * lightDivider.getDivision());
                            //Red lights
                            lights[OPERATOR_LIGHTS + i * 3 + 2].setSmoothBrightness(configOp == i ?
                                blinkStatus ?
                                    0.f
                                    : DEF_RED_BRIGHTNESS
                                : DEF_RED_BRIGHTNESS, args.sampleTime * lightDivider.getDivision());
                        }
                    }
                    else {
                        //Set carrier indicator
                        if (forcedCarrier[configScene][i]) {
                            //Purple light
                            lights[CARRIER_INDICATORS + i * 3].setSmoothBrightness(configOp == i ?
                                blinkStatus ?
                                    0.f
                                    : INDICATOR_BRIGHTNESS
                                : INDICATOR_BRIGHTNESS, args.sampleTime * lightDivider.getDivision());
                            //Yellow light
                            lights[CARRIER_INDICATORS + i * 3 + 1].setSmoothBrightness(configOp == i ?
                                blinkStatus
                                : 0.f, args.sampleTime * lightDivider.getDivision());
                            //Red light
                            lights[CARRIER_INDICATORS + i * 3 + 2].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        }
                        else {
                            //Purple light
                            lights[CARRIER_INDICATORS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                            //Yellow light
                            lights[CARRIER_INDICATORS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                            //Red light
                            lights[CARRIER_INDICATORS + i * 3 + 2].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        }
                        //Set op lights
                        //Purple lights
                        lights[OPERATOR_LIGHTS + i * 3].setSmoothBrightness(configOp == i ?
                            blinkStatus ?
                                0.f
                                : getPortBrightness(inputs[OPERATOR_INPUTS + i])
                            : getPortBrightness(inputs[OPERATOR_INPUTS + i]), args.sampleTime * lightDivider.getDivision());
                        //Yellow Lights
                        lights[OPERATOR_LIGHTS + i * 3 + 1].setSmoothBrightness(configOp == i ? blinkStatus : 0.f, args.sampleTime * lightDivider.getDivision());
                        //Red lights
                        lights[OPERATOR_LIGHTS + i * 3 + 2].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    }
                    //Set mod lights
                    if (i != configOp) {
                        //Purple lights
                        lights[MODULATOR_LIGHTS + i * 3].setSmoothBrightness(configOp > -1 ?
                            opDestinations[configScene][configOp][i < configOp ? i : i - 1] ?
                                blinkStatus ?
                                    0.f
                                    : getPortBrightness(outputs[MODULATOR_OUTPUTS + i])
                                : getPortBrightness(outputs[MODULATOR_OUTPUTS + i])
                            : getPortBrightness(outputs[MODULATOR_OUTPUTS + i]), args.sampleTime * lightDivider.getDivision());
                        //Yellow lights
                        lights[MODULATOR_LIGHTS + i * 3 + 1].setSmoothBrightness(configOp > -1 ?
                            (opDestinations[configScene][configOp][i < configOp ? i : i - 1] ?
                                blinkStatus
                                : 0.f)
                            : 0.f, args.sampleTime * lightDivider.getDivision());
                    }
                    else {
                        if (modeB) {
                            //Purple lights
                            lights[MODULATOR_LIGHTS + i * 3].setSmoothBrightness(getPortBrightness(outputs[MODULATOR_OUTPUTS + i]), args.sampleTime * lightDivider.getDivision());
                            //Yellow lights
                            lights[MODULATOR_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        }
                        else {
                            //Purple lights
                            lights[MODULATOR_LIGHTS + i * 3].setSmoothBrightness(configOp > -1 ?
                                !horizontalDestinations[configScene][configOp] ?
                                    getPortBrightness(outputs[MODULATOR_OUTPUTS + i])
                                    : blinkStatus ?
                                        0.f
                                        : getPortBrightness(outputs[MODULATOR_OUTPUTS + i])
                                : getPortBrightness(outputs[MODULATOR_OUTPUTS + i]), args.sampleTime * lightDivider.getDivision());
                            //Yellow lights
                            lights[MODULATOR_LIGHTS + i * 3 + 1].setSmoothBrightness(configOp > -1 ?
                                !horizontalDestinations[configScene][configOp] ?
                                    0.f
                                    : blinkStatus
                                : 0.f, args.sampleTime * lightDivider.getDivision());
                        }
                    }
                }
                //Check and update blink timer
                if (blinkTimer > BLINK_INTERVAL / lightDivider.getDivision()) {
                    blinkStatus ^= true;
                    blinkTimer = 0.f;
                }
                else
                    blinkTimer += args.sampleTime;
                //Set connection lights
                if (modeB) {
                    for (int i = 0; i < 12; i++) {
                        //Purple lights
                        lights[CONNECTION_LIGHTS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        //Yellow lights
                        lights[CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(opDestinations[configScene][i / 3][i % 3] ?
                            0.4f
                            : 0.f, args.sampleTime * lightDivider.getDivision());
                        //Set diagonal disable lights
                        lights[D_DISABLE_LIGHTS + i].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    }
                    for (int i = 0; i < 4; i++) {
                        //Purple lights
                        lights[H_CONNECTION_LIGHTS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        //Yellow lights
                        lights[H_CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(horizontalDestinations[configScene][i] ?
                            0.4f
                            : 0.f, args.sampleTime * lightDivider.getDivision());
                        //Red lights
                        lights[H_CONNECTION_LIGHTS + i * 3 + 2].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    }
                }
                else {
                    for (int i = 0; i < 12; i++) {
                        //Purple lights
                        lights[CONNECTION_LIGHTS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        //Yellow lights
                        lights[CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(!horizontalDestinations[configScene][i / 3] ? 
                            opDestinations[configScene][i / 3][i % 3] ?
                                0.4f
                                : 0.f
                            : 0.f, args.sampleTime * lightDivider.getDivision());
                        //Set diagonal disable lights
                        lights[D_DISABLE_LIGHTS + i].setSmoothBrightness(!horizontalDestinations[configScene][i / 3] ? 
                            0.f
                            : opDestinations[configScene][i / 3][i % 3] ?
                                DEF_RED_BRIGHTNESS
                                : 0.f, args.sampleTime * lightDivider.getDivision());
                    }
                    //Set horizontal lights
                    for (int i = 0; i < 4; i++) {
                        //Purple lights
                        lights[H_CONNECTION_LIGHTS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        //Yellow lights
                        lights[H_CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        //Red lights
                        lights[H_CONNECTION_LIGHTS + i * 3 + 2].setSmoothBrightness(!horizontalDestinations[configScene][i] ?
                            0.f
                            : DEF_RED_BRIGHTNESS, args.sampleTime * lightDivider.getDivision());
                    }
                }
            } 
            else {
                //Set backlight
                //Set yellow component to off
                lights[DISPLAY_BACKLIGHT + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                //Set purple component
                lights[DISPLAY_BACKLIGHT].setSmoothBrightness(getPortBrightness(outputs[SUM_OUTPUT]) / 1024.f + 0.014325f, args.sampleTime * lightDivider.getDivision());
                //Set edit light
                lights[EDIT_LIGHT].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                //Display morphed state
                float brightness;
                //Set scene lights
                for (int i = 0; i < 3; i++) {
                    //Set all components to off
                    lights[SCENE_LIGHTS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    lights[SCENE_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    lights[SCENE_INDICATORS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    lights[SCENE_INDICATORS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    //Set base scene indicator purple component
                    lights[SCENE_INDICATORS + i * 3].setSmoothBrightness(i == (baseScene + sceneOffset[0]) % 3 ? 
                        INDICATOR_BRIGHTNESS
                        : 0.f, args.sampleTime * lightDivider.getDivision());
                }
                //Set center morph scene light's purple component depending on morph
                lights[SCENE_LIGHTS + centerMorphScene[0] * 3].setSmoothBrightness(1.f - lightRelativeMorphMagnitude[0], args.sampleTime * lightDivider.getDivision());
                //Set morph target light's purple component depending on morph
                lights[SCENE_LIGHTS + forwardMorphScene[0] * 3].setSmoothBrightness(lightRelativeMorphMagnitude[0], args.sampleTime * lightDivider.getDivision());
                //Set op/mod lights and carrier indicators
                updateSceneBrightnesses();
                for (int i = 0; i < 4; i++) {
                    //Purple, yellow, and red lights
                    for (int j = 0; j < 3; j++) {
                        lights[OPERATOR_LIGHTS + i * 3 + j].setSmoothBrightness(crossfade(sceneBrightnesses[centerMorphScene[0]][i][j], sceneBrightnesses[forwardMorphScene[0]][i][j], relativeMorphMagnitude[0]), args.sampleTime * lightDivider.getDivision()); 
                        lights[MODULATOR_LIGHTS + i * 3 + j].setSmoothBrightness(crossfade(sceneBrightnesses[centerMorphScene[0]][i + 4][j], sceneBrightnesses[forwardMorphScene[0]][i + 4][j], relativeMorphMagnitude[0]), args.sampleTime * lightDivider.getDivision()); 
                        lights[CARRIER_INDICATORS + i * 3 + j].setSmoothBrightness(crossfade(sceneBrightnesses[centerMorphScene[0]][i + 8][j], sceneBrightnesses[forwardMorphScene[0]][i + 8][j], relativeMorphMagnitude[0]), args.sampleTime * lightDivider.getDivision()); 
                    }
                }
                //Set connection lights
                if (modeB) {
                    for (int i = 0; i < 4; i++) {
                        brightness = 0.f;
                        if (horizontalDestinations[centerMorphScene[0]][i])
                            brightness += getPortBrightness(inputs[OPERATOR_INPUTS + i]) * (1.f - relativeMorphMagnitude[0]);
                        if (horizontalDestinations[forwardMorphScene[0]][i])
                            brightness += getPortBrightness(inputs[OPERATOR_INPUTS + i]) * relativeMorphMagnitude[0];
                        //Purple lights
                        lights[H_CONNECTION_LIGHTS + i * 3].setSmoothBrightness(brightness, args.sampleTime * lightDivider.getDivision());
                        //Yellow
                        lights[H_CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        //Red
                        lights[H_CONNECTION_LIGHTS + i * 3 + 2].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    }
                    for (int i = 0; i < 12; i++) {
                        brightness = 0.f;
                        if (opDestinations[centerMorphScene[0]][i / 3][i % 3])
                            brightness += getPortBrightness(inputs[OPERATOR_INPUTS + i / 3]) * (1.f - relativeMorphMagnitude[0]);
                        if (opDestinations[forwardMorphScene[0]][i / 3][i % 3])
                            brightness += getPortBrightness(inputs[OPERATOR_INPUTS + i / 3]) * relativeMorphMagnitude[0];
                        //Purple lights
                        lights[CONNECTION_LIGHTS + i * 3].setSmoothBrightness(brightness, args.sampleTime * lightDivider.getDivision());
                        //Yellow
                        lights[CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        //Set diagonal disable lights
                        lights[D_DISABLE_LIGHTS + i].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    }
                }
                else {
                    for (int i = 0; i < 4; i++) {
                        brightness = getPortBrightness(inputs[OPERATOR_INPUTS + i]);
                        for (int j = 0; j < 3; j++) {
                            float morphBrightness = 0.f;
                            if (opDestinations[centerMorphScene[0]][i][j]) {
                                if (!horizontalDestinations[centerMorphScene[0]][i])
                                    morphBrightness += brightness * (1.f - relativeMorphMagnitude[0]);
                            }
                            if (opDestinations[forwardMorphScene[0]][i][j]) {
                                if (!horizontalDestinations[forwardMorphScene[0]][i])
                                    morphBrightness += brightness * relativeMorphMagnitude[0];
                            }
                            //Purple lights
                            lights[CONNECTION_LIGHTS + i * 9 + j * 3].setSmoothBrightness(morphBrightness, args.sampleTime * lightDivider.getDivision());
                            //Yellow
                            lights[CONNECTION_LIGHTS + i * 9 + j * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                            //Set diagonal disable lights
                            lights[D_DISABLE_LIGHTS + i * 3 + j].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        }
                        //Set horizontal disable lights
                        float horizontalMorphBrightness = 0.f;
                        if (horizontalDestinations[centerMorphScene[0]][i])
                            horizontalMorphBrightness += (1.f - relativeMorphMagnitude[0]);
                        if (horizontalDestinations[forwardMorphScene[0]][i])
                            horizontalMorphBrightness += relativeMorphMagnitude[0];
                        //Purple lights
                        lights[H_CONNECTION_LIGHTS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        //Yellow lights
                        lights[H_CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        //Red lights
                        lights[H_CONNECTION_LIGHTS + i * 3 + 2].setSmoothBrightness(horizontalMorphBrightness, args.sampleTime * lightDivider.getDivision());
                    }
                }
            }
        }

        if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
    }

    inline void scaleAuxSumAttenVoltage(int channels) {
        for (int c = 0; c < channels; c++) {
            scaledAuxVoltage[AuxInputModes::SUM_ATTEN][c] = clamp(auxInput.voltage[AuxInputModes::SUM_ATTEN][c] / 5.f, -1.f, 1.f);
        }    
    }

    inline void scaleAuxModAttenVoltage(int channels) {
        for (int c = 0; c < channels; c++) {
            scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] = clamp(auxInput.voltage[AuxInputModes::MOD_ATTEN][c] / 5.f, -1.f, 1.f);
        }    
    }

    inline void scaleAuxClickFilterVoltage(int channels) {
        for (int c = 0; c < channels; c++) {
            //+/-5V = 0V-2V
            scaledAuxVoltage[AuxInputModes::CLICK_FILTER][c] = (clamp(auxInput.voltage[AuxInputModes::CLICK_FILTER][c] / 5.f, -1.f, 1.f) + 1.001f);
        }    
    }

    void rescaleVoltage(int mode) {
        switch (mode) {
            case AuxInputModes::CLICK_FILTER:
                scaleAuxClickFilterVoltage(16);
                break;
            case AuxInputModes::SUM_ATTEN:
                scaleAuxSumAttenVoltage(16);
                break;
            case AuxInputModes::MOD_ATTEN:
                scaleAuxModAttenVoltage(16);
                break;
            default:
                break;
        }
    }

    void rescaleVoltages() {
        scaleAuxClickFilterVoltage(16);
        scaleAuxSumAttenVoltage(16);
        scaleAuxModAttenVoltage(16);
    }

    void initRun() {
		clockIgnoreOnReset = (long) (CLOCK_IGNORE_DURATION * APP->engine->getSampleRate());
        baseScene = resetScene;
	}

    inline float getPortBrightness(Port port) {
        if (vuLights)
            return std::max(    {   port.plugLights[0].getBrightness(),
                                    port.plugLights[1].getBrightness() * 4,
                                    port.plugLights[2].getBrightness()          }   );
        else
            return 1.f;
    }

    void updateSceneBrightnesses() {
        if (modeB) {
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 4; j++) {
                    //Op lights
                    //Purple lights
                    sceneBrightnesses[i][j][0] = getPortBrightness(inputs[OPERATOR_INPUTS + j]);

                    //Mod lights
                    //Purple lights
                    sceneBrightnesses[i][j + 4][0] = getPortBrightness(outputs[MODULATOR_OUTPUTS + j]);

                    //Op lights
                    //Yellow Lights
                    sceneBrightnesses[i][j][1] = 0.f;
                    //Red lights
                    sceneBrightnesses[i][j][2] = 0.f;

                    //Carrier indicators
                    if (forcedCarrier[i][j]) {
                        //Purple lights
                        sceneBrightnesses[i][j + 8][0] = INDICATOR_BRIGHTNESS;
                    }
                    else {
                        //Purple lights
                        sceneBrightnesses[i][j + 8][0] = 0.f;
                    }
                    //Yellow lights
                    sceneBrightnesses[i][j + 8][1] = 0.f;
                    //Red lights
                    sceneBrightnesses[i][j + 8][2] = 0.f;
                }
            }
        }
        else {
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 4; j++) {
                    if (!horizontalDestinations[i][j]) {
                        //Op lights
                        //Purple lights
                        sceneBrightnesses[i][j][0] = getPortBrightness(inputs[OPERATOR_INPUTS + j]);
                        //Red lights
                        sceneBrightnesses[i][j][2] = 0.f;

                        //Mod lights
                        //Purple lights
                        sceneBrightnesses[i][j + 4][0] = getPortBrightness(outputs[MODULATOR_OUTPUTS + j]);

                        //Carrier indicators
                        if (forcedCarrier[i][j]) {
                            //Purple lights
                            sceneBrightnesses[i][j + 8][0] = INDICATOR_BRIGHTNESS;
                        }
                        else {
                            //Purple lights
                            sceneBrightnesses[i][j + 8][0] = 0.f;
                        }
                        //Red lights
                        sceneBrightnesses[i][j + 8][2] = 0.f;
                    }
                    else {
                        //Op lights
                        //Purple lights
                        sceneBrightnesses[i][j][0] = 0.f;
                        //Red lights
                        sceneBrightnesses[i][j][2] = DEF_RED_BRIGHTNESS;

                        //Mod lights
                        //Purple lights
                        sceneBrightnesses[i][j + 4][0] = getPortBrightness(outputs[MODULATOR_OUTPUTS + j]);

                        //Carrier indicators
                        if (forcedCarrier[i][j]) {
                            //Red lights
                            sceneBrightnesses[i][j + 8][2] = INDICATOR_BRIGHTNESS;
                        }
                        else {
                            //Red lights
                            sceneBrightnesses[i][j + 8][2] = 0.f;
                        }
                        //Purple lights
                        sceneBrightnesses[i][j + 8][0] = 0.f;
                    }

                    //Op lights
                    //Yellow Lights
                    sceneBrightnesses[i][j][1] = 0.f;
                    
                    //Carrier indicators
                    //Yellow lights
                    sceneBrightnesses[i][j + 8][1] = 0.f;
                }
            }
        }
    }

    void swapAlgorithms(int a, int b) {
        bool swap[4][3] = {false};
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 3; j++) {
                swap[i][j] = opDestinations[a][i][j];
                opDestinations[a][i][j] = opDestinations[b][i][j];
                opDestinations[b][i][j] = swap[i][j];
            }
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "Config Enabled", json_boolean(configMode));
        json_object_set_new(rootJ, "Config Mode", json_integer(configOp));
        json_object_set_new(rootJ, "Config Scene", json_integer(configOp));
        json_object_set_new(rootJ, "Current Scene", json_integer(baseScene));
        json_object_set_new(rootJ, "Horizontal Allowed", json_integer(modeB));
        json_object_set_new(rootJ, "Reset Scene", json_integer(resetScene));
        json_object_set_new(rootJ, "Ring Morph", json_boolean(ringMorph));
        json_object_set_new(rootJ, "Randomize Ring Morph", json_boolean(randomRingMorph));
        json_object_set_new(rootJ, "Auto Exit", json_boolean(exitConfigOnConnect));
        json_object_set_new(rootJ, "CCW Scene Selection", json_boolean(ccwSceneSelection));
        json_object_set_new(rootJ, "Reset on Run", json_boolean(resetOnRun));
        json_object_set_new(rootJ, "Click Filter Enabled", json_boolean(clickFilterEnabled));
        json_object_set_new(rootJ, "Glowing Ink", json_boolean(glowingInk));
        json_object_set_new(rootJ, "VU Lights", json_boolean(vuLights));
        
        json_object_set_new(rootJ, "Aux Input Last Set Mode", json_integer(auxInput.lastSetMode));
        json_object_set_new(rootJ, "Aux Input Active Modes", json_integer(auxInput.activeModes));
        json_object_set_new(rootJ, "Allow Multiple Modes", json_boolean(auxInput.allowMultipleModes));
        json_object_set_new(rootJ, "Forget Aux Voltage", json_boolean(auxInput.forgetVoltage));
        json_object_set_new(rootJ, "Aux Input Connected", json_boolean(auxInput.connected));

        json_object_set_new(rootJ, "Aux Knob Mode", json_integer(knobMode));
        
        json_t* opInputModesJ = json_array();
        for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
            json_t* inputModeJ = json_object();
            json_object_set_new(inputModeJ, "Mode", json_boolean(auxInput.mode[i]));
            json_array_append_new(opInputModesJ, inputModeJ);
        }
        json_object_set_new(rootJ, "Aux Input Modes", opInputModesJ);
        
        json_t* opDestinationsJ = json_array();
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                for (int k = 0; k < 3; k++) {
                    json_t* destinationJ = json_object();
                    json_object_set_new(destinationJ, "Destination", json_boolean(opDestinations[i][j][k]));
                    json_array_append_new(opDestinationsJ, destinationJ);
                }
            }
        }
        json_object_set_new(rootJ, "Operator Destinations", opDestinationsJ);

        json_t* algoNamesJ = json_array();
        for (int i = 0; i < 3; i++) {
            json_t* nameJ = json_object();
            json_object_set_new(nameJ, "Name", json_integer(algoName[i].to_ullong()));
            json_array_append_new(algoNamesJ, nameJ);
        }
        json_object_set_new(rootJ, "Algorithm Names", algoNamesJ);

        json_t* displayAlgoNamesJ = json_array();
        for (int i = 0; i < 3; i++) {
            json_t* displayNameJ = json_object();
            json_object_set_new(displayNameJ, "Display Name", json_integer(displayAlgoName[i].to_ullong()));
            json_array_append_new(displayAlgoNamesJ, displayNameJ);
        }
        json_object_set_new(rootJ, "Display Algorithm Names", displayAlgoNamesJ);

        json_t* horizontalConnectionsJ = json_array();
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                json_t* connectionJ = json_object();
                json_object_set_new(connectionJ, "Enabled Op", json_boolean(!horizontalDestinations[i][j]));
                json_array_append_new(horizontalConnectionsJ, connectionJ);
            }
        }
        json_object_set_new(rootJ, "Operators Enabled", horizontalConnectionsJ);

        json_t* forcedCarriersJ = json_array();
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                json_t* forcedCarrierJ = json_object();
                json_object_set_new(forcedCarrierJ, "Forced Carrier", json_boolean(forcedCarrier[i][j]));
                json_array_append_new(forcedCarriersJ, forcedCarrierJ);
            }
        }
        json_object_set_new(rootJ, "Forced Carriers", forcedCarriersJ);

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        configMode = json_integer_value(json_object_get(rootJ, "Config Enabled"));
        configOp = json_integer_value(json_object_get(rootJ, "Config Mode"));
        configScene = json_integer_value(json_object_get(rootJ, "Config Scene"));
        baseScene = json_integer_value(json_object_get(rootJ, "Current Scene"));
        modeB = json_integer_value(json_object_get(rootJ, "Horizontal Allowed"));
        resetScene = json_integer_value(json_object_get(rootJ, "Reset Scene"));
        ringMorph = json_boolean_value(json_object_get(rootJ, "Ring Morph"));
        randomRingMorph = json_boolean_value(json_object_get(rootJ, "Randomize Ring Morph"));
        exitConfigOnConnect = json_boolean_value(json_object_get(rootJ, "Auto Exit"));
        ccwSceneSelection = json_boolean_value(json_object_get(rootJ, "CCW Scene Selection"));
        resetOnRun = json_boolean_value(json_object_get(rootJ, "Reset on Run"));
        clickFilterEnabled = json_boolean_value(json_object_get(rootJ, "Click Filter Enabled"));
        glowingInk = json_boolean_value(json_object_get(rootJ, "Glowing Ink"));
        vuLights = json_boolean_value(json_object_get(rootJ, "VU Lights"));

        //Set allowMultipleModes and forgetVoltage before loading modes
        auxInput.allowMultipleModes = json_boolean_value(json_object_get(rootJ, "Allow Multiple Modes"));
        auxInput.forgetVoltage = json_boolean_value(json_object_get(rootJ, "Forget Aux Voltage"));

        json_t* opInputModesJ = json_object_get(rootJ, "Aux Input Modes");
        json_t* inputModeJ; size_t inputModeIndex;
        auxInput.clearModes();
        json_array_foreach(opInputModesJ, inputModeIndex, inputModeJ) {
            if (json_boolean_value(json_object_get(inputModeJ, "Mode")))
                auxInput.setMode(inputModeIndex);
        }

        auxInput.connected = json_boolean_value(json_object_get(rootJ, "Aux Input Connected"));
        auxInput.lastSetMode = json_integer_value(json_object_get(rootJ, "Aux Input Last Set Mode"));
        auxInput.activeModes = json_integer_value(json_object_get(rootJ, "Aux Input Active Modes"));
        
        knobMode = json_integer_value(json_object_get(rootJ, "Aux Knob Mode"));

        json_t* opDestinationsJ = json_object_get(rootJ, "Operator Destinations");
        json_t* destinationJ; size_t destinationIndex;
        int i = 0, j = 0, k = 0;
        json_array_foreach(opDestinationsJ, destinationIndex, destinationJ) {
            opDestinations[i][j][k] = json_boolean_value(json_object_get(destinationJ, "Destination"));
            k++;
            if (k > 2) {
                k = 0;
                j++;
                if (j > 3) {
                    j = 0;
                    i++;
                }
            }
        }

        json_t* algoNamesJ = json_object_get(rootJ, "Algorithm Names");
        json_t* nameJ; size_t nameIndex;
        json_array_foreach(algoNamesJ, nameIndex, nameJ) {
            algoName[nameIndex] = json_integer_value(json_object_get(nameJ, "Name"));
        }

        json_t* displayAlgoNamesJ = json_object_get(rootJ, "Display Algorithm Names");
        json_t* displayNameJ; size_t displayNameIndex;
        json_array_foreach(displayAlgoNamesJ, displayNameIndex, displayNameJ) {
            displayAlgoName[displayNameIndex] = json_integer_value(json_object_get(displayNameJ, "Display Name"));
        }

        json_t* forcedCarriersJ = json_object_get(rootJ, "Forced Carriers");
        json_t* forcedCarrierJ;
        size_t forcedCarrierIndex;
        i = j = 0;
        json_array_foreach(forcedCarriersJ, forcedCarrierIndex, forcedCarrierJ) {
            forcedCarrier[i][j] = json_boolean_value(json_object_get(forcedCarrierJ, "Forced Carrier"));
            j++;
            if (j > 3) {
                j = 0;
                i++;
            }
        }

        json_t* horizontalConnectionsJ = json_object_get(rootJ, "Operators Enabled");
        json_t* connectionJ;
        size_t horizontalConnectionIndex;
        i = j = 0;
        json_array_foreach(horizontalConnectionsJ, horizontalConnectionIndex, connectionJ) {
            horizontalDestinations[i][j] = !json_boolean_value(json_object_get(connectionJ, "Enabled Op"));
            j++;
            if (j > 3) {
                j = 0;
                i++;
            }
        }

        //Legacy opDisabled
        json_t* opDisabledJ = json_object_get(rootJ, "Operators Disabled");
        json_t* disabledOpJ;
        size_t disabledOpIndex;
        i = j = 0;
        json_array_foreach(opDisabledJ, disabledOpIndex, disabledOpJ) {
            horizontalDestinations[i][j] = json_boolean_value(json_object_get(disabledOpJ, "Disabled Op"));
            j++;
            if (j > 3) {
                j = 0;
                i++;
            }
        }

        graphDirty = true;
    }
};

template < typename MODULE >
struct AlgoScreenWidget : FramebufferWidget {
    struct AlgoDrawWidget : LightWidget {
        MODULE* module;
        alGraph graphs[3];
        bool firstRun = true;
        std::shared_ptr<Font> font;
        float textBounds[4];

        float xOrigin = box.size.x / 2.f;
        float yOrigin = box.size.y / 2.f;
   
        const NVGcolor NODE_FILL_COLOR = nvgRGBA(0x40, 0x36, 0x4a, 0xff);
        const NVGcolor NODE_STROKE_COLOR = nvgRGB(26, 26, 26);
        const NVGcolor TEXT_COLOR = nvgRGBA(0xb2, 0xa9, 0xb9, 0xff);
        const NVGcolor EDGE_COLOR = nvgRGBA(0x9a,0x9a,0x6f,0xff);

        NVGcolor nodeFillColor = NODE_FILL_COLOR;
        NVGcolor nodeStrokeColor = NODE_STROKE_COLOR;
        NVGcolor textColor = TEXT_COLOR;
        NVGcolor edgeColor = EDGE_COLOR;
        float borderStroke = 0.45f;
        float labelStroke = 0.5f;
        float nodeStroke = 0.75f;
        float radius = 8.35425f;
        float edgeStroke = 0.925f;
        static constexpr float arrowStroke1 = (2.65f/4.f) + (1.f/3.f);
        static constexpr float arrowStroke2 = (7.65f/4.f) + (1.f/3.f);

        AlgoDrawWidget(MODULE* module) {
            this->module = module;
            font = APP->window->loadFont(asset::plugin(pluginInstance, "res/MiriamLibre-Regular.ttf"));
        }

        void drawNodes(NVGcontext* ctx, alGraph source, alGraph destination, float morph) {
            if (source.numNodes >= destination.numNodes)
                renderNodes(ctx, source, destination, morph, false);
            else
                renderNodes(ctx, destination, source, morph, true);
        }

        void renderNodes(NVGcontext* ctx, alGraph mostNodes, alGraph leastNodes, float morph, bool flipped) {
            for (int i = 0; i< 4; i++) {
                Node nodes[2];
                float nodeX[2] = {0.f};
                float nodeY[2] = {0.f};
                float nodeAlpha[2] = {0.f};
                float textAlpha[2] = {0.f};
                nodeFillColor = NODE_FILL_COLOR;
                nodeStrokeColor = NODE_STROKE_COLOR;
                textColor = TEXT_COLOR;
                bool draw = true;

                nvgBeginPath(ctx);
                if (mostNodes.nodes[i].id == 404) {
                    if (leastNodes.nodes[i].id == 404)
                        draw = false;
                    else {
                        nodes[0] = leastNodes.nodes[i];
                        nodes[1] = leastNodes.nodes[i];
                        if (!flipped) {
                            nodeAlpha[0] = 0.f;
                            textAlpha[0] = 0.f;
                            nodeAlpha[1] = NODE_FILL_COLOR.a;
                            textAlpha[1] = TEXT_COLOR.a;
                            nodeX[0] = xOrigin;
                            nodeY[0] = yOrigin;
                            nodeX[1] = nodes[1].coords.x;
                            nodeY[1] = nodes[1].coords.y;
                        }
                        else {
                            nodeAlpha[0] = NODE_FILL_COLOR.a;
                            textAlpha[0] = TEXT_COLOR.a;
                            nodeAlpha[1] = 0.f;
                            textAlpha[1] = 0.f;
                            nodeX[0] = nodes[0].coords.x;
                            nodeY[0] = nodes[0].coords.y;
                            nodeX[1] = xOrigin;
                            nodeY[1] = yOrigin;
                        }
                    }
                }
                else if (leastNodes.nodes[i].id == 404) {
                    nodes[0] = mostNodes.nodes[i];
                    nodes[1] = mostNodes.nodes[i];
                    if (!flipped) {
                        nodeAlpha[0] = NODE_FILL_COLOR.a;
                        textAlpha[0] = TEXT_COLOR.a;
                        nodeAlpha[1] = 0.f;
                        textAlpha[1] = 0.f;
                        nodeX[0] = nodes[0].coords.x;
                        nodeY[0] = nodes[0].coords.y;
                        nodeX[1] = xOrigin;
                        nodeY[1] = yOrigin;
                    }
                    else {
                        nodeAlpha[0] = 0.f;
                        textAlpha[0] = 0.f;
                        nodeAlpha[1] = NODE_FILL_COLOR.a;
                        textAlpha[1] = TEXT_COLOR.a;
                        nodeX[0] = xOrigin;
                        nodeY[0] = yOrigin;
                        nodeX[1] = nodes[0].coords.x;
                        nodeY[1] = nodes[0].coords.y;
                    }
                }
                else {
                    if (!flipped) {
                        nodes[0] = mostNodes.nodes[i];
                        nodes[1] = leastNodes.nodes[i];
                    }
                    else {
                        nodes[0] = leastNodes.nodes[i];
                        nodes[1] = mostNodes.nodes[i];
                    }
                    nodeAlpha[0] = NODE_FILL_COLOR.a;
                    textAlpha[0] = TEXT_COLOR.a;
                    nodeAlpha[1] = NODE_FILL_COLOR.a;
                    textAlpha[1] = TEXT_COLOR.a;
                    nodeX[0] = nodes[0].coords.x;
                    nodeX[1] = nodes[1].coords.x;
                    nodeY[0] = nodes[0].coords.y;
                    nodeY[1] = nodes[1].coords.y;
                }
                if (draw) {
                    nodeFillColor.a = crossfade(nodeAlpha[0], nodeAlpha[1], morph);
                    nodeStrokeColor.a = crossfade(nodeAlpha[0], nodeAlpha[1], morph);
                    textColor.a = crossfade(textAlpha[0], textAlpha[1], morph);
                    
                    nvgCircle(ctx,  crossfade(nodeX[0], nodeX[1], morph),
                                    crossfade(nodeY[0], nodeY[1], morph),
                                    radius);
                    nvgFillColor(ctx, nodeFillColor);
                    nvgFill(ctx);
                    nvgStrokeColor(ctx, nodeStrokeColor);
                    nvgStrokeWidth(ctx, nodeStroke);
                    nvgStroke(ctx);

                    nvgBeginPath(ctx);
                    nvgFontSize(ctx, 11.f);
                    nvgFontFaceId(ctx, font->handle);
                    nvgFillColor(ctx, textColor);
                    std::string s = std::to_string(i + 1);
                    char const *id = s.c_str();
                    nvgTextBounds(ctx, nodes[0].coords.x, nodes[0].coords.y, id, id + 1, textBounds);
                    float xOffset = (textBounds[2] - textBounds[0]) / 2.f;
                    float yOffset = (textBounds[3] - textBounds[1]) / 3.25f;
                    nvgText(ctx,    crossfade(nodeX[0], nodeX[1], morph) - xOffset,
                                    crossfade(nodeY[0], nodeY[1], morph) + yOffset,
                                    id, id + 1);
                }
            }
        }

        void drawEdges(NVGcontext* ctx, alGraph source, alGraph destination, float morph) {
            if (source >= destination)
                renderEdges(ctx, source, destination, morph, false);
            else 
                renderEdges(ctx, destination, source, morph, true);
        }

        void renderEdges(NVGcontext* ctx, alGraph mostEdges, alGraph leastEdges, float morph, bool flipped) {
            for (int i = 0; i < mostEdges.numEdges; i++) {
                Edge edge[2];
                Arrow arrow[2];
                nvgBeginPath(ctx);
                if (leastEdges.numEdges == 0) {
                    if (!flipped) {
                        edge[0] = mostEdges.edges[i];
                        edge[1] = leastEdges.edges[i];
                        nvgMoveTo(ctx, crossfade(edge[0].moveCoords.x, xOrigin, morph), crossfade(edge[0].moveCoords.y, yOrigin, morph));
                        edgeColor.a = crossfade(EDGE_COLOR.a, 0x00, morph);
                    }
                    else {
                        edge[0] = leastEdges.edges[i];
                        edge[1] = mostEdges.edges[i];
                        nvgMoveTo(ctx, crossfade(xOrigin, edge[1].moveCoords.x, morph), crossfade(yOrigin, edge[1].moveCoords.y, morph));
                        edgeColor.a = crossfade(0x00, EDGE_COLOR.a, morph);
                    }
                    arrow[0] = mostEdges.arrows[i];
                    arrow[1] = leastEdges.arrows[i];
                }
                else if (i < leastEdges.numEdges) {
                    if (!flipped) {
                        edge[0] = mostEdges.edges[i];
                        edge[1] = leastEdges.edges[i];
                    }
                    else {
                        edge[0] = leastEdges.edges[i];
                        edge[1] = mostEdges.edges[i];
                    }
                    nvgMoveTo(ctx, crossfade(edge[0].moveCoords.x, edge[1].moveCoords.x, morph), crossfade(edge[0].moveCoords.y, edge[1].moveCoords.y, morph));
                    edgeColor = EDGE_COLOR;
                    arrow[0] = mostEdges.arrows[i];
                    arrow[1] = leastEdges.arrows[i];
                }
                else {
                    if (!flipped) {
                        edge[0] = mostEdges.edges[i];
                        edge[1] = leastEdges.edges[std::max(0, leastEdges.numEdges - 1)];
                    }
                    else {
                        edge[0] = leastEdges.edges[std::max(0, leastEdges.numEdges - 1)];
                        edge[1] = mostEdges.edges[i];
                    }
                    nvgMoveTo(ctx, crossfade(edge[0].moveCoords.x, edge[1].moveCoords.x, morph), crossfade(edge[0].moveCoords.y, edge[1].moveCoords.y, morph));
                    edgeColor = EDGE_COLOR;
                    arrow[0] = mostEdges.arrows[i];
                    arrow[1] = leastEdges.arrows[std::max(0, leastEdges.numEdges - 1)];
                }
                if (edge[0] >= edge[1]) {
                    reticulateEdge(ctx, edge[0], edge[1], morph, false);
                }
                else {
                    reticulateEdge(ctx, edge[1], edge[0], morph, true);
                }

                nvgStrokeColor(ctx, edgeColor);
                nvgStrokeWidth(ctx, edgeStroke);
                nvgStroke(ctx);

                nvgBeginPath(ctx);
                reticulateArrow(ctx, arrow[0], arrow[1], morph, flipped);
                nvgFillColor(ctx, edgeColor);
                nvgFill(ctx);
                nvgStrokeColor(ctx, edgeColor);
                nvgStrokeWidth(ctx, arrowStroke1);
                nvgStroke(ctx);
            }
        }

        void reticulateEdge(NVGcontext* ctx, Edge mostCurved, Edge leastCurved, float morph, bool flipped) {
            for (int j = 0; j < mostCurved.curveLength; j++) {
                if (leastCurved.curveLength == 0) {
                    if (!flipped)
                        nvgBezierTo(ctx, crossfade(mostCurved.curve[j][0].x, xOrigin, morph), crossfade(mostCurved.curve[j][0].y, yOrigin, morph), crossfade(mostCurved.curve[j][1].x, xOrigin, morph), crossfade(mostCurved.curve[j][1].y, yOrigin, morph), crossfade(mostCurved.curve[j][2].x, xOrigin, morph), crossfade(mostCurved.curve[j][2].y, yOrigin, morph));
                    else
                        nvgBezierTo(ctx, crossfade(xOrigin, mostCurved.curve[j][0].x, morph), crossfade(yOrigin, mostCurved.curve[j][0].y, morph), crossfade(xOrigin, mostCurved.curve[j][1].x, morph), crossfade(yOrigin, mostCurved.curve[j][1].y, morph), crossfade(xOrigin, mostCurved.curve[j][2].x, morph), crossfade(yOrigin, mostCurved.curve[j][2].y, morph));
                }
                else if (j < leastCurved.curveLength) {
                    if (!flipped)
                        nvgBezierTo(ctx, crossfade(mostCurved.curve[j][0].x, leastCurved.curve[j][0].x, morph), crossfade(mostCurved.curve[j][0].y, leastCurved.curve[j][0].y, morph), crossfade(mostCurved.curve[j][1].x, leastCurved.curve[j][1].x, morph), crossfade(mostCurved.curve[j][1].y, leastCurved.curve[j][1].y, morph), crossfade(mostCurved.curve[j][2].x, leastCurved.curve[j][2].x, morph), crossfade(mostCurved.curve[j][2].y, leastCurved.curve[j][2].y, morph));
                    else
                        nvgBezierTo(ctx, crossfade(leastCurved.curve[j][0].x, mostCurved.curve[j][0].x, morph), crossfade(leastCurved.curve[j][0].y, mostCurved.curve[j][0].y, morph), crossfade(leastCurved.curve[j][1].x, mostCurved.curve[j][1].x, morph), crossfade(leastCurved.curve[j][1].y, mostCurved.curve[j][1].y, morph), crossfade(leastCurved.curve[j][2].x, mostCurved.curve[j][2].x, morph), crossfade(leastCurved.curve[j][2].y, mostCurved.curve[j][2].y, morph));
                }
                else {
                    if (!flipped)
                        nvgBezierTo(ctx, crossfade(mostCurved.curve[j][0].x, leastCurved.curve[leastCurved.curveLength - 1][0].x, morph), crossfade(mostCurved.curve[j][0].y, leastCurved.curve[leastCurved.curveLength - 1][0].y, morph), crossfade(mostCurved.curve[j][1].x, leastCurved.curve[leastCurved.curveLength - 1][1].x, morph), crossfade(mostCurved.curve[j][1].y, leastCurved.curve[leastCurved.curveLength - 1][1].y, morph), crossfade(mostCurved.curve[j][2].x, leastCurved.curve[leastCurved.curveLength - 1][2].x, morph), crossfade(mostCurved.curve[j][2].y, leastCurved.curve[leastCurved.curveLength - 1][2].y, morph));
                    else
                        nvgBezierTo(ctx, crossfade(leastCurved.curve[leastCurved.curveLength - 1][0].x, mostCurved.curve[j][0].x, morph), crossfade(leastCurved.curve[leastCurved.curveLength - 1][0].y, mostCurved.curve[j][0].y, morph), crossfade(leastCurved.curve[leastCurved.curveLength - 1][1].x, mostCurved.curve[j][1].x, morph), crossfade(leastCurved.curve[leastCurved.curveLength - 1][1].y, mostCurved.curve[j][1].y, morph), crossfade(leastCurved.curve[leastCurved.curveLength - 1][2].x, mostCurved.curve[j][2].x, morph), crossfade(leastCurved.curve[leastCurved.curveLength - 1][2].y, mostCurved.curve[j][2].y, morph));
                }
            }
        }

        void reticulateArrow(NVGcontext* ctx, Arrow mostGregarious, Arrow leastGregarious, float morph, bool flipped) {
            if (leastGregarious.moveCoords.x == 0) {
                if (!flipped)
                    nvgMoveTo(ctx, crossfade(mostGregarious.moveCoords.x, xOrigin, morph), crossfade(mostGregarious.moveCoords.y, yOrigin, morph));
                else
                    nvgMoveTo(ctx, crossfade(xOrigin, mostGregarious.moveCoords.x, morph), crossfade(yOrigin, mostGregarious.moveCoords.y, morph));
                for (int j = 0; j < 9; j++) {
                    if (!flipped)
                        nvgLineTo(ctx, crossfade(mostGregarious.lines[j].x, xOrigin, morph), crossfade(mostGregarious.lines[j].y, yOrigin, morph));
                    else
                        nvgLineTo(ctx, crossfade(xOrigin, mostGregarious.lines[j].x, morph), crossfade(yOrigin, mostGregarious.lines[j].y, morph));
                }
            }
            else {
                if (!flipped)
                    nvgMoveTo(ctx, crossfade(mostGregarious.moveCoords.x, leastGregarious.moveCoords.x, morph), crossfade(mostGregarious.moveCoords.y, leastGregarious.moveCoords.y, morph));
                else
                    nvgMoveTo(ctx, crossfade(leastGregarious.moveCoords.x, mostGregarious.moveCoords.x, morph), crossfade(leastGregarious.moveCoords.y, mostGregarious.moveCoords.y, morph));
                for (int j = 0; j < 9; j++) {
                    if (!flipped)
                        nvgLineTo(ctx, crossfade(mostGregarious.lines[j].x, leastGregarious.lines[j].x, morph), crossfade(mostGregarious.lines[j].y, leastGregarious.lines[j].y, morph));
                    else
                        nvgLineTo(ctx, crossfade(leastGregarious.lines[j].x, mostGregarious.lines[j].x, morph), crossfade(leastGregarious.lines[j].y, mostGregarious.lines[j].y, morph));
                }
            }
        }

        void draw(const Widget::DrawArgs& args) override {
            if (!module) return;

            //Origin must be updated
            xOrigin = box.size.x / 2.f;
            yOrigin = box.size.y / 2.f;

            for (int i = 0; i < 3; i++) {
                int name = module->graphAddressTranslation[module->displayAlgoName[i].to_ullong()];
                if (name != -1)
                    graphs[i] = alGraph(module->graphAddressTranslation[(int)module->displayAlgoName[i].to_ullong()]);
                else {
                    graphs[i] = alGraph(1979);
                    graphs[i].mystery = true;
                }
            }
              
            int scene = module->configMode ? module->configScene : module->centerMorphScene[0];
            int morphScene = module->forwardMorphScene[0];
            float morph = module->relativeMorphMagnitude[0];

            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, box.getTopLeft().x, box.getTopLeft().y, box.size.x, box.size.y, 3.675f);
            nvgStrokeWidth(args.vg, borderStroke);
            nvgStroke(args.vg);

            if (module->configMode) {   //Display state without morph
                if (graphs[scene].numNodes > 0) {
                    // Draw nodes
                    nvgBeginPath(args.vg);
                    for (int i = 0; i < 4; i++) {
                        if (graphs[scene].nodes[i].id != 404)
                            nvgCircle(args.vg, graphs[scene].nodes[i].coords.x, graphs[scene].nodes[i].coords.y, radius);
                    }
                    nvgFillColor(args.vg, nodeFillColor);
                    nvgFill(args.vg);
                    nvgStrokeColor(args.vg, nodeStrokeColor);
                    nvgStrokeWidth(args.vg, nodeStroke);
                    nvgStroke(args.vg);

                    // Draw numbers
                    nvgBeginPath(args.vg);
                    nvgFontSize(args.vg, 11.f);
                    nvgFontFaceId(args.vg, font->handle);
                    nvgFillColor(args.vg, textColor);
                    for (int i = 0; i < 4; i++) {
                        if (graphs[scene].nodes[i].id != 404) {
                            std::string s = std::to_string(i + 1);
                            char const *id = s.c_str();
                            nvgTextBounds(args.vg, graphs[scene].nodes[i].coords.x, graphs[scene].nodes[i].coords.y, id, id + 1, textBounds);
                            float xOffset = (textBounds[2] - textBounds[0]) / 2.f;
                            float yOffset = (textBounds[3] - textBounds[1]) / 3.25f;
                            nvgText(args.vg, graphs[scene].nodes[i].coords.x - xOffset, graphs[scene].nodes[i].coords.y + yOffset, id, id + 1);
                        }
                    }
                }
            }
            else {
                // Draw nodes and numbers
                nvgBeginPath(args.vg);
                drawNodes(args.vg, graphs[scene], graphs[morphScene], morph);
            }

            // Draw question mark
            if (module->configMode) {
                if (graphs[scene].mystery) {
                    nvgBeginPath(args.vg);
                    nvgFontSize(args.vg, 92.f);
                    nvgFontFaceId(args.vg, font->handle);
                    textColor = TEXT_COLOR;
                    nvgFillColor(args.vg, textColor);
                    std::string s = "?";
                    char const *id = s.c_str();
                    nvgTextBounds(args.vg, xOrigin, yOrigin, id, id + 1, textBounds);
                    float xOffset = (textBounds[2] - textBounds[0]) / 2.f;
                    float yOffset = (textBounds[3] - textBounds[1]) / 3.925f;
                    nvgText(args.vg, xOrigin - xOffset, yOrigin + yOffset, id, id + 1);
                }
            }
            else {
                if (graphs[scene].mystery || graphs[morphScene].mystery) {
                    nvgBeginPath(args.vg);
                    nvgFontSize(args.vg, 92.f);
                    nvgFontFaceId(args.vg, font->handle);
                    if (graphs[scene].mystery && graphs[morphScene].mystery)
                        textColor = TEXT_COLOR;
                    else if (graphs[scene].mystery)
                        textColor.a = crossfade(TEXT_COLOR.a, 0x00, morph);
                    else
                        textColor.a = crossfade(0x00, TEXT_COLOR.a, morph);
                    nvgFillColor(args.vg, textColor);
                    std::string s = "?";
                    char const *id = s.c_str();
                    nvgTextBounds(args.vg, xOrigin, yOrigin, id, id + 1, textBounds);
                    float xOffset = (textBounds[2] - textBounds[0]) / 2.f;
                    float yOffset = (textBounds[3] - textBounds[1]) / 3.925f;
                    nvgText(args.vg, xOrigin - xOffset, yOrigin + yOffset, id, id + 1);
                }
            }

            // Draw edges +/ arrows
            if (module->configMode) {
                // Draw edges
                nvgBeginPath(args.vg);
                for (int i = 0; i < graphs[scene].numEdges; i++) {
                    Edge edge = graphs[scene].edges[i];
                    nvgMoveTo(args.vg, edge.moveCoords.x, edge.moveCoords.y);
                    for (int j = 0; j < edge.curveLength; j++) {
                        nvgBezierTo(args.vg, edge.curve[j][0].x, edge.curve[j][0].y, edge.curve[j][1].x, edge.curve[j][1].y, edge.curve[j][2].x, edge.curve[j][2].y);
                    }
                }
                edgeColor = EDGE_COLOR;
                nvgStrokeColor(args.vg, edgeColor);
                nvgStrokeWidth(args.vg, edgeStroke);
                nvgStroke(args.vg);
                // Draw arrows
                for (int i = 0; i < graphs[scene].numEdges; i++) {
                    nvgBeginPath(args.vg);
                    nvgMoveTo(args.vg, graphs[scene].arrows[i].moveCoords.x, graphs[scene].arrows[i].moveCoords.y);
                    for (int j = 0; j < 9; j++)
                        nvgLineTo(args.vg, graphs[scene].arrows[i].lines[j].x, graphs[scene].arrows[i].lines[j].y);
                    edgeColor = EDGE_COLOR;
                    nvgFillColor(args.vg, edgeColor);
                    nvgFill(args.vg);
                    nvgStrokeColor(args.vg, edgeColor);
                    nvgStrokeWidth(args.vg, arrowStroke1);
                    nvgStroke(args.vg);
                }
            }
            else {
                // Draw edges AND arrows
                drawEdges(args.vg, graphs[scene], graphs[morphScene], morph);
            }                
        }
    };

    MODULE* module;
    AlgoDrawWidget* w;

    AlgoScreenWidget(MODULE* module) {
        this->module = module;
        w = new AlgoDrawWidget(module);
        addChild(w);
    }

    void step() override {
        if (module && module->graphDirty) {
            FramebufferWidget::dirty = true;
            w->box.size = box.size;
            module->graphDirty = false;
        }
        FramebufferWidget::step();
    }
};


template < typename MODULE >
struct ForgetAuxVoltageAction : history::ModuleAction {
    int channels;
	bool multipleActive = false, remember[AuxInputModes::NUM_MODES] = {false};
	float voltage[AuxInputModes::NUM_MODES][16], scaledVoltage[AuxInputModes::NUM_MODES][16];

	ForgetAuxVoltageAction() {
		name = "Delexander Algomorph forget option voltage";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		
		m->auxInput.forgetVoltage = false;
		if (multipleActive) {
			for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
				if (remember[i]) {
					for (int c = 0; c < channels; c++) {
						m->auxInput.voltage[i][c] = voltage[i][c];
						m->scaledAuxVoltage[i][c] = scaledVoltage[i][c];
					}
				}
			}
		}
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		
		m->auxInput.forgetVoltage = true;
		if (multipleActive) {
			for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
				if (remember[i]) {
					for (int c = 0; c < channels; c++)
						m->auxInput.voltage[i][c] = m->auxInput.defVoltage[i];
					m->rescaleVoltage(i);
				}
			}
		}
	}
};

template < typename MODULE >
struct DisallowMultipleModesAction : history::ModuleAction {
    int channels;
	bool multipleActive = false, enabled[AuxInputModes::NUM_MODES] = {false}, remember[AuxInputModes::NUM_MODES] = {false};
	float voltage[AuxInputModes::NUM_MODES][16], scaledVoltage[AuxInputModes::NUM_MODES][16];

	DisallowMultipleModesAction() {
		name = "Delexander Algomorph disallow multiple modes";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		
		m->auxInput.allowMultipleModes = true;
		if (multipleActive) {
			for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
				if (enabled[i])
					m->auxInput.setMode(i);
			}
		}
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		
		if (multipleActive) {
			for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
				if (enabled[i]) {
					m->auxInput.unsetMode(i);
					if (remember[i]) {
						for (int c = 0; c < channels; c++) {
							m->auxInput.voltage[i][c] = voltage[i][c];
							m->scaledAuxVoltage[i][c] = scaledVoltage[i][c];
						}
					}
					else {
						for (int c = 0; c < channels; c++)
							m->auxInput.voltage[i][c] = m->auxInput.defVoltage[i];
						m->rescaleVoltage(i);
					}
				}
			}
		}
		m->auxInput.allowMultipleModes = false;
	}
};

struct AuxModeItem : MenuItem {
    Algomorph4 *module;
    int mode;

    void onAction(const event::Action &e) override {
        if (module->auxInput.mode[mode]) {
            if (module->auxInput.forgetVoltage || module->auxInput.mustForget[mode]) {
                // History
                AuxInputUnsetAndForgetAction<Algomorph4>* h = new AuxInputUnsetAndForgetAction<Algomorph4>;
                h->moduleId = module->id;
                h->mode = mode;
                
                module->auxInput.unsetMode(mode);
                for (int c = 0; c < module->auxInput.channels; c++)
                    module->auxInput.voltage[mode][c] = module->auxInput.defVoltage[mode];
                module->rescaleVoltage(mode);

                APP->history->push(h);
            }
            else {
                // History
                AuxInputUnsetAndRememberAction<Algomorph4>* h = new AuxInputUnsetAndRememberAction<Algomorph4>;
                h->moduleId = module->id;
                h->mode = mode;
                h->channels = module->auxInput.channels;

                module->auxInput.unsetMode(mode);
                for (int c = 0; c < h->channels; c++) {
                    h->voltage[c] = module->auxInput.voltage[mode][c];
                    h->scaledVoltage[c] = module->scaledAuxVoltage[mode][c];
                }

                APP->history->push(h);
            }
        }
        else {
            if (module->auxInput.allowMultipleModes) {
                if (module->auxInput.forgetVoltage || module->auxInput.mustForget[mode]) {
                    // History
                    AuxInputSetAndForgetAction<Algomorph4>* h = new AuxInputSetAndForgetAction<Algomorph4>;
                    h->moduleId = module->id;
                    h->mode = mode;
                    h->channels = module->auxInput.channels;

                    module->auxInput.setMode(mode);

                    APP->history->push(h);
                }
                else {
                    // History
                    AuxInputSetAndRememberAction<Algomorph4>* h = new AuxInputSetAndRememberAction<Algomorph4>;
                    h->moduleId = module->id;
                    h->mode = mode;
                    h->channels = module->auxInput.channels;

                    for (int c = 0; c < h->channels; c++) {
                        h->voltage[c] = module->auxInput.voltage[mode][c];
                        h->scaledVoltage[c] = module->scaledAuxVoltage[mode][c];
                    }
                    module->auxInput.setMode(mode);

                    APP->history->push(h);
                }
            }
            else {
                bool forgetOldVoltage = false, forgetNewVoltage = false;

                if (module->auxInput.forgetVoltage || module->auxInput.mustForget[module->auxInput.lastSetMode])
                    forgetNewVoltage = true;
                if (module->auxInput.forgetVoltage || module->auxInput.mustForget[mode])
                    forgetOldVoltage = true;

                if (forgetNewVoltage || forgetOldVoltage) {
                    // History
                    AuxInputSwitchAndForgetAction<Algomorph4>* h = new AuxInputSwitchAndForgetAction<Algomorph4>;
                    h->moduleId = module->id;
                    h->oldMode = module->auxInput.lastSetMode;
                    h->newMode = mode;
                    h->channels = module->auxInput.channels;
                    h->forgetOldVoltage = forgetOldVoltage;
                    h->forgetNewVoltage = forgetNewVoltage;

                    module->auxInput.unsetMode(h->oldMode);
                    if (forgetOldVoltage) {
                        for (int c = 0; c < h->channels; c++)
                            module->auxInput.voltage[h->oldMode][c] = module->auxInput.defVoltage[h->oldMode];
                        module->rescaleVoltage(h->oldMode);
                    }
                    else {
                        for (int c = 0; c < h->channels; c++) {
                            h->oldVoltage[c] = module->auxInput.voltage[h->oldMode][c];
                            h->oldScaledVoltage[c] = module->scaledAuxVoltage[h->oldMode][c];
                        }
                    }
                    if (!forgetNewVoltage) {
                        for (int c = 0; c < h->channels; c++) {
                            h->newVoltage[c] = module->auxInput.voltage[h->newMode][c];
                            h->newScaledVoltage[c] = module->scaledAuxVoltage[h->newMode][c];
                        }
                    }
                    module->auxInput.setMode(mode);

                    APP->history->push(h);
                }
                else {
                    // History
                    AuxInputSwitchAndRememberAction<Algomorph4>* h = new AuxInputSwitchAndRememberAction<Algomorph4>;
                    h->moduleId = module->id;
                    h->oldMode = module->auxInput.lastSetMode;
                    h->newMode = mode;
                    h->channels = module->auxInput.channels;

                    module->auxInput.unsetMode(module->auxInput.lastSetMode);
                    for (int c = 0; c < h->channels; c++) {
                        h->oldVoltage[c] = module->auxInput.voltage[h->oldMode][c];
                        h->oldScaledVoltage[c] = module->scaledAuxVoltage[h->oldMode][c];
                        h->newVoltage[c] = module->auxInput.voltage[h->newMode][c];
                        h->newScaledVoltage[c] = module->scaledAuxVoltage[h->newMode][c];
                    }
                    module->auxInput.setMode(mode);

                    APP->history->push(h);
                }
            }
        }
    }
};

template < typename MODULE >
void createWildcardInputMenu(MODULE* module, ui::Menu* menu) {
    for (int i = AuxInputModes::WILDCARD_MOD; i <= AuxInputModes::WILDCARD_SUM; i++)
        menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[i], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[i]), &AuxModeItem::mode, i));
}

template < typename MODULE >
struct WildcardInputMenuItem : MenuItem {
	MODULE* module;
	
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		createWildcardInputMenu(module, menu);
		return menu;
	}
};

template < typename MODULE >
void createShadowInputMenu(MODULE* module, ui::Menu* menu) {
    for (int i = AuxInputModes::SHADOW; i <= AuxInputModes::SHADOW + 3; i++)
        menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[i], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[i]), &AuxModeItem::mode, i));
}

template < typename MODULE >
struct ShadowInputMenuItem : MenuItem {
	MODULE* module;
	
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		createShadowInputMenu(module, menu);
		return menu;
	}
};

struct ResetOnRunItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        // History
        ToggleResetOnRunAction<Algomorph4>* h = new ToggleResetOnRunAction<Algomorph4>;
        h->moduleId = module->id;

        module->resetOnRun ^= true;

        APP->history->push(h);
    }
};

struct RunSilencerItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        // History
        ToggleRunSilencerAction<Algomorph4>* h = new ToggleRunSilencerAction<Algomorph4>;
        h->moduleId = module->id;

        module->runSilencer ^= true;

        APP->history->push(h);
    }
};

template < typename MODULE >
void createRunAuxMenu(MODULE* module, ui::Menu* menu) {
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::RUN], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[AuxInputModes::RUN]), &AuxModeItem::mode, AuxInputModes::RUN));
    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Run Auxs"));

    ResetOnRunItem *resetOnRunItem = createMenuItem<ResetOnRunItem>("Reset on run", CHECKMARK(module->resetOnRun));
    resetOnRunItem->module = module;
    menu->addChild(resetOnRunItem);
    
    RunSilencerItem *runSilencerItem = createMenuItem<RunSilencerItem>("Silence when not running", CHECKMARK(module->runSilencer));
    runSilencerItem->module = module;
    menu->addChild(runSilencerItem);
}

template < typename MODULE >
struct RunAuxMenuItem : MenuItem {
	MODULE* module;
	
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		createRunAuxMenu(module, menu);
		return menu;
	}
};

template < typename MODULE >
void createAuxInputMenu(MODULE* module, ui::Menu* menu) {
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "5th Operator"));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::WILDCARD_MOD], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[AuxInputModes::WILDCARD_MOD]), &AuxModeItem::mode, AuxInputModes::WILDCARD_MOD));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::WILDCARD_SUM], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[AuxInputModes::WILDCARD_SUM]), &AuxModeItem::mode, AuxInputModes::WILDCARD_SUM));
    menu->addChild(construct<ShadowInputMenuItem<Algomorph4>>(&MenuItem::text, "Shadow operator...", &MenuItem::rightText, RIGHT_ARROW, &ShadowInputMenuItem<Algomorph4>::module, module));

    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Trigger"));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::CLOCK], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[AuxInputModes::CLOCK]), &AuxModeItem::mode, AuxInputModes::CLOCK));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::REVERSE_CLOCK], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[AuxInputModes::REVERSE_CLOCK]), &AuxModeItem::mode, AuxInputModes::REVERSE_CLOCK));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::RESET], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[AuxInputModes::RESET]), &AuxModeItem::mode, AuxInputModes::RESET));
    menu->addChild(construct<RunAuxMenuItem<Algomorph4>>(&MenuItem::text, "Run...", &MenuItem::rightText, RIGHT_ARROW, &RunAuxMenuItem<Algomorph4>::module, module));
   
    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "CV"));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::MORPH], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[AuxInputModes::MORPH]), &AuxModeItem::mode, AuxInputModes::MORPH));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::SUM_ATTEN], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[AuxInputModes::SUM_ATTEN]), &AuxModeItem::mode, AuxInputModes::SUM_ATTEN));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::MOD_ATTEN], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[AuxInputModes::MOD_ATTEN]), &AuxModeItem::mode, AuxInputModes::MOD_ATTEN));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::MORPH_ATTEN], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[AuxInputModes::MORPH_ATTEN]), &AuxModeItem::mode, AuxInputModes::MORPH_ATTEN));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::SCENE_OFFSET], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[AuxInputModes::SCENE_OFFSET]), &AuxModeItem::mode, AuxInputModes::SCENE_OFFSET));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::CLICK_FILTER], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[AuxInputModes::CLICK_FILTER]), &AuxModeItem::mode, AuxInputModes::CLICK_FILTER));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::DOUBLE_MORPH], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[AuxInputModes::DOUBLE_MORPH]), &AuxModeItem::mode, AuxInputModes::DOUBLE_MORPH));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::TRIPLE_MORPH], &AuxModeItem::module, module, &AuxModeItem::rightText, CHECKMARK(module->auxInput.mode[AuxInputModes::TRIPLE_MORPH]), &AuxModeItem::mode, AuxInputModes::TRIPLE_MORPH));
}

template < typename MODULE >
struct AuxInputMenuItem : MenuItem {
	MODULE* module;
	
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		createAuxInputMenu(module, menu);
		return menu;
	}
};

struct AllowMultipleModesItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        if (module->auxInput.allowMultipleModes) {
            // History
            DisallowMultipleModesAction<Algomorph4>* h = new DisallowMultipleModesAction<Algomorph4>;
            h->moduleId = module->id;
            h->channels = module->auxInput.channels;

            if (module->auxInput.activeModes > 1) {
                h->multipleActive = true;
                if (module->auxInput.forgetVoltage) {
                    for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
                        if (module->auxInput.mode[i] && i != module->auxInput.lastSetMode) {
                            h->enabled[i] = true;
                            module->auxInput.unsetMode(i);
                            for (int c = 0; c < h->channels; c++)
                                module->auxInput.voltage[i][c] = module->auxInput.defVoltage[i];
                            module->rescaleVoltage(i);
                        }
                    }
                }
                else {
                    for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
                        if (module->auxInput.mode[i] && i != module->auxInput.lastSetMode) {
                            h->enabled[i] = true;
                            if (module->auxInput.mustForget[i]) {
                                module->auxInput.unsetMode(i);
                                for (int c = 0; c < h->channels; c++)
                                    module->auxInput.voltage[i][c] = module->auxInput.defVoltage[i];
                                module->rescaleVoltage(i);
                            }
                            else {
                                h->remember[i] = true;
                                module->auxInput.unsetMode(i);
                                for (int c = 0; c < h->channels; c++) {
                                    h->voltage[i][c] = module->auxInput.voltage[i][c];
                                    h->scaledVoltage[i][c] = module->scaledAuxVoltage[i][c];
                                }
                            }
                        }
                    }
                }
            }
            module->auxInput.allowMultipleModes = false;

            APP->history->push(h);
        }
        else {
            // History
            AllowMultipleModesAction<Algomorph4>* h = new AllowMultipleModesAction<Algomorph4>;
            h->moduleId = module->id;

            module->auxInput.allowMultipleModes = true;

            APP->history->push(h);
        }

    }
};

struct ForgetAuxVoltageItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        if (module->auxInput.forgetVoltage) {
            // History
            RememberAuxVoltageAction<Algomorph4>* h = new RememberAuxVoltageAction<Algomorph4>;
            h->moduleId = module->id;

            module->auxInput.forgetVoltage = false;

            APP->history->push(h);
        }
        else {
            // History
            ForgetAuxVoltageAction<Algomorph4>* h = new ForgetAuxVoltageAction<Algomorph4>;
            h->moduleId = module->id;
            h->channels = module->auxInput.channels;

            if (module->auxInput.activeModes > 1) {
                h->multipleActive = true;
                for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
                    for (int c = 0; c < h->channels; c++) {
                        if (!module->auxInput.mode[i] && module->auxInput.voltage[i][c] != module->auxInput.defVoltage[i]) {
                            h->remember[i] = true;
                            break;
                        }
                    }
                    if (h->remember[i]) {
                        for (int c = 0; c < h->channels; c++) {
                            h->voltage[i][c] = module->auxInput.voltage[i][c];
                            h->scaledVoltage[i][c] = module->scaledAuxVoltage[i][c];
                            module->auxInput.voltage[i][c] = module->auxInput.defVoltage[i];
                        }
                        module->rescaleVoltage(i);
                    }
                }
            }
            module->auxInput.forgetVoltage = true;

            APP->history->push(h);
        }
    }
};

struct ResetSceneItem : MenuItem {
    Algomorph4 *module;
    int scene;

    void onAction(const event::Action &e) override {
        // History
        ResetSceneAction<Algomorph4>* h = new ResetSceneAction<Algomorph4>;
        h->moduleId = module->id;
        h->oldResetScene = module->resetScene;
        h->newResetScene = scene;

        module->resetScene = scene;

        APP->history->push(h);
    }
};

template < typename MODULE >
void createResetSceneMenu(MODULE* module, ui::Menu* menu) {
    menu->addChild(construct<ResetSceneItem>(&MenuItem::text, "1", &ResetSceneItem::module, module, &ResetSceneItem::rightText, CHECKMARK(module->resetScene == 0), &ResetSceneItem::scene, 0));
    menu->addChild(construct<ResetSceneItem>(&MenuItem::text, "2", &ResetSceneItem::module, module, &ResetSceneItem::rightText, CHECKMARK(module->resetScene == 1), &ResetSceneItem::scene, 1));
    menu->addChild(construct<ResetSceneItem>(&MenuItem::text, "3", &ResetSceneItem::module, module, &ResetSceneItem::rightText, CHECKMARK(module->resetScene == 2), &ResetSceneItem::scene, 2));
}

template < typename MODULE >
struct ResetSceneMenuItem : MenuItem {
	MODULE* module;
	
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		createResetSceneMenu(module, menu);
		return menu;
	}
};

struct ToggleModeBItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        // History
        ToggleModeBAction<Algomorph4>* h = new ToggleModeBAction<Algomorph4>;
        h->moduleId = module->id;

        module->modeB ^= true;
        if (module->modeB) {
            for (int i = 0; i < 3; i++) {
                h->algoName[i] = module->algoName[i];
                h->displayAlgoName[i] = module->displayAlgoName[i];
                for (int j = 0; j < 4; j++)
                    module->algoName[i].set(12 + j, false);
                module->displayAlgoName[i] = module->algoName[i];
            }
        }
        else {
            for (int i = 0; i < 3; i++) {
                h->algoName[i] = module->algoName[i];
                h->displayAlgoName[i] = module->displayAlgoName[i];
                for (int j = 0; j < 4; j++) {
                    if (module->horizontalDestinations[i][j]) {
                        module->algoName[i].set(12 + j, true);
                        module->displayAlgoName[i] = module->algoName[i];
                        for (int k = 0; k < 3; k++)
                            module->displayAlgoName[i].set(j * 3 + k, false);
                    }
                }
            }
        }
        module->graphDirty = true;

        APP->history->push(h);
    }
};

struct RingMorphItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        // History
        ToggleRingMorphAction<Algomorph4>* h = new ToggleRingMorphAction<Algomorph4>;
        h->moduleId = module->id;

        module->ringMorph ^= true;

        APP->history->push(h);
    }
};

struct RandomizeRingMorphItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        // History
        ToggleRandomizeRingMorphAction<Algomorph4>* h = new ToggleRandomizeRingMorphAction<Algomorph4>;
        h->moduleId = module->id;

        module->randomRingMorph ^= true;

        APP->history->push(h);
    }
};

template < typename MODULE >
void createRandomizationMenu(MODULE* module, ui::Menu* menu) {    
    RandomizeRingMorphItem *ramdomizeRingMorphItem = createMenuItem<RandomizeRingMorphItem>("Enabling Ring Morph", CHECKMARK(module->randomRingMorph));
    ramdomizeRingMorphItem->module = module;
    menu->addChild(ramdomizeRingMorphItem);
}

template < typename MODULE >
struct RandomizationMenuItem : MenuItem {
	MODULE* module;
	
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		createRandomizationMenu(module, menu);
		return menu;
	}
};

struct ExitConfigItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        // History
        ToggleExitConfigOnConnectAction<Algomorph4>* h = new ToggleExitConfigOnConnectAction<Algomorph4>;
        h->moduleId = module->id;

        module->exitConfigOnConnect ^= true;

        APP->history->push(h);
    }
};

struct CCWScenesItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        // History
        ToggleCCWSceneSelectionAction<Algomorph4>* h = new ToggleCCWSceneSelectionAction<Algomorph4>;
        h->moduleId = module->id;

        module->ccwSceneSelection ^= true;

        APP->history->push(h);
    }
};

struct ClickFilterEnabledItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        // History
        ToggleClickFilterAction<Algomorph4>* h = new ToggleClickFilterAction<Algomorph4>;
        h->moduleId = module->id;

        module->clickFilterEnabled ^= true;

        APP->history->push(h);
    }
};

struct VULightsItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        // History
        ToggleVULightsAction<Algomorph4>* h = new ToggleVULightsAction<Algomorph4>;
        h->moduleId = module->id;

        module->vuLights ^= true;

        APP->history->push(h);
    }
};

// struct DebugItem : MenuItem {
//     Algomorph4 *module;
//     void onAction(const event::Action &e) override {
//         module->debug ^= true;
//     }
// };
template < typename MODULE >
struct ClickFilterSlider : ui::Slider {
    float oldValue = DEF_CLICK_FILTER_SLEW;
    MODULE* module;

	struct ClickFilterQuantity : Quantity {
        MODULE* module;
		float v = -1.f;

        ClickFilterQuantity(MODULE* m) {
            module = m;
        }
		void setValue(float value) override {
			v = clamp(value, 16.f, 7500.f);
            module->clickFilterSlew = v;
		}
		float getValue() override {
			v = module->clickFilterSlew;
			return v;
		}
		float getDefaultValue() override {
			return DEF_CLICK_FILTER_SLEW;
		}
		float getMinValue() override {
			return 16.f;
		}
		float getMaxValue() override {
			return 7500.f;
		}
		float getDisplayValue() override {
			return getValue();
		}
		std::string getDisplayValueString() override {
			int i = int(getValue());
			return string::f("%i", i);
		}
		void setDisplayValue(float displayValue) override {
			setValue(displayValue);
		}
		std::string getLabel() override {
			return "Click Filter Slew";
		}
		std::string getUnit() override {
			return "Hz";
		}
	};

	ClickFilterSlider(MODULE* m) {
		quantity = new ClickFilterQuantity(m);
        module = m;
	}
	~ClickFilterSlider() {
		delete quantity;
	}
    void onDragStart(const event::DragStart& e) override {
        if (quantity)
            oldValue = quantity->getValue();
    }
	void onDragMove(const event::DragMove& e) override {
		if (quantity)
			quantity->moveScaledValue(0.002f * e.mouseDelta.x);
	}
    void onDragEnd(const event::DragEnd& e) override {
        if (quantity) {
            // History
            SetClickFilterAction<Algomorph4>* h = new SetClickFilterAction<Algomorph4>;
            h->moduleId = module->id;
            h->oldSlew = oldValue;
            h->newSlew = quantity->getValue();

            APP->history->push(h);
        }    
    }
};

template < typename MODULE >
void createClickFilterMenu(MODULE* module, ui::Menu* menu) {
    ClickFilterEnabledItem *clickFilterEnabledItem = createMenuItem<ClickFilterEnabledItem>("Disable Click Filter", CHECKMARK(!module->clickFilterEnabled));
    clickFilterEnabledItem->module = module;
    menu->addChild(clickFilterEnabledItem);
    ClickFilterSlider<MODULE>* clickFilterSlider = new ClickFilterSlider<MODULE>(module);
    clickFilterSlider->box.size.x = 200.0;
    menu->addChild(clickFilterSlider);
}

template < typename MODULE >
struct ClickFilterMenuItem : MenuItem {
	MODULE* module;
	
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		createClickFilterMenu(module, menu);
		return menu;
	}
};

struct Algomorph4Widget : ModuleWidget {
    std::vector<Vec> SceneButtonCenters = {  {mm2px(53.831), mm2px(46.862)},
                                            {mm2px(53.831), mm2px(55.262)},
                                            {mm2px(53.831), mm2px(63.662)} };
    std::vector<Vec> OpButtonCenters = { {mm2px(22.416), mm2px(49.614)},
                                        {mm2px(22.416), mm2px(60.634)},
                                        {mm2px(22.416), mm2px(71.655)},
                                        {mm2px(22.416), mm2px(82.677)} };
    std::vector<Vec> ModButtonCenters = {{mm2px(38.732), mm2px(49.614)},
                                        {mm2px(38.732), mm2px(60.634)},
                                        {mm2px(38.732), mm2px(71.655)},
                                        {mm2px(38.732), mm2px(82.677)} };
    DLXGlowingInk* ink;
    
    DLXRingIndicator* createRingIndicator(Vec pos, float r, engine::Module* module, int firstLightId, float s = 0) {
        DLXRingIndicator* o = new DLXRingIndicator(r, s);
        o->box.pos = pos;
        o->module = module;
        o->firstLightId = firstLightId;
        return o;
    }

    DLXRingIndicator* createRingIndicatorCentered(Vec pos, float r, engine::Module* module, int firstLightId, float s = 0) {
        DLXRingIndicator* o = createRingIndicator(pos, r, module, firstLightId, s);
        o->box.pos.x -= r;
        o->box.pos.y -= r;
        return o;
    }

    Algomorph4Widget(Algomorph4* module) {
        setModule(module);
        
        if (module) {
            setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Algomorph.svg")));

            addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
            addChild(createWidget<ScrewBlack>(Vec(box.size.x - RACK_GRID_WIDTH * 2, 0)));
            addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 365)));
            addChild(createWidget<ScrewBlack>(Vec(box.size.x - RACK_GRID_WIDTH * 2, 365)));

            ink = createSvgLight<DLXGlowingInk>(Vec(0,0), module, Algomorph4::GLOWING_INK);
            if (!module->glowingInk)
                ink->hide();
            addChildBottom(ink);

            AlgoScreenWidget<Algomorph4>* screenWidget = new AlgoScreenWidget<Algomorph4>(module);
            screenWidget->box.pos = mm2px(Vec(11.333, 7.195));
            screenWidget->box.size = mm2px(Vec(38.295, 31.590));
            addChild(screenWidget);
            //Place backlight _above_ in scene in order for brightness to affect screenWidget
            addChild(createBacklight<DLXScreenMultiLight>(mm2px(Vec(11.333, 7.195)), mm2px(Vec(38.295, 31.590)), module, Algomorph4::DISPLAY_BACKLIGHT));

            addChild(createRingLightCentered<DLXMultiLight>(mm2px(Vec(30.760, 45.048)), 8.862, module, Algomorph4::SCREEN_BUTTON_RING_LIGHT, .4));
            addParam(createParamCentered<DLXPurpleButton>(mm2px(Vec(30.760, 45.048)), module, Algomorph4::SCREEN_BUTTON));
            addChild(createSvgSwitchLightCentered<DLXScreenButtonLight>(mm2px(Vec(30.760, 45.048)), module, Algomorph4::SCREEN_BUTTON_LIGHT, Algomorph4::SCREEN_BUTTON));

            addChild(createRingLightCentered<DLXMultiLight>(SceneButtonCenters[0], 8.862, module, Algomorph4::SCENE_LIGHTS + 0, .75));
            addChild(createRingIndicatorCentered(SceneButtonCenters[0], 8.862, module, Algomorph4::SCENE_INDICATORS + 0, .75));
            addParam(createParamCentered<DLXTL1105B>(SceneButtonCenters[0], module, Algomorph4::SCENE_BUTTONS + 0));
            addChild(createSvgSwitchLightCentered<DLX1Light>(SceneButtonCenters[0], module, Algomorph4::ONE_LIGHT, Algomorph4::SCENE_BUTTONS + 0));

            addChild(createRingLightCentered<DLXMultiLight>(SceneButtonCenters[1], 8.862, module, Algomorph4::SCENE_LIGHTS + 3, .75));
            addChild(createRingIndicatorCentered(SceneButtonCenters[1], 8.862, module, Algomorph4::SCENE_INDICATORS + 3, .75));
            addParam(createParamCentered<DLXTL1105B>(SceneButtonCenters[1], module, Algomorph4::SCENE_BUTTONS + 1));
            addChild(createSvgSwitchLightCentered<DLX2Light>(SceneButtonCenters[1], module, Algomorph4::TWO_LIGHT, Algomorph4::SCENE_BUTTONS + 1));

            addChild(createRingLightCentered<DLXMultiLight>(SceneButtonCenters[2], 8.862, module, Algomorph4::SCENE_LIGHTS + 6, .75));
            addChild(createRingIndicatorCentered(SceneButtonCenters[2], 8.862, module, Algomorph4::SCENE_INDICATORS + 6, .75));
            addParam(createParamCentered<DLXTL1105B>(SceneButtonCenters[2], module, Algomorph4::SCENE_BUTTONS + 2));
            addChild(createSvgSwitchLightCentered<DLX3Light>(SceneButtonCenters[2], module, Algomorph4::THREE_LIGHT, Algomorph4::SCENE_BUTTONS + 2));

            addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 42.771)), module, Algomorph4::AUX_INPUT));
            addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 52.792)), module, Algomorph4::MORPH_INPUT));
            addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 62.812)), module, Algomorph4::SCENE_ADV_INPUT));
            addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 72.834)), module, Algomorph4::RESET_INPUT));

            DLXKnobLight* kl = createLight<DLXKnobLight>(mm2px(Vec(20.305, 96.721)), module, Algomorph4::KNOB_LIGHT);
            addParam(createLightKnob(mm2px(Vec(20.305, 96.721)), module, Algomorph4::MORPH_KNOB, kl));
            addChild(kl);

            for (int i = 0; i < AuxKnobModes::NUM_MODES; i++) {
                DLXSmallKnobLight* auxKl = createLight<DLXSmallKnobLight>(mm2px(Vec(26.632, 103.047)), module, Algomorph4::AUX_KNOB_LIGHTS + i);
                DLXSmallLightKnob* auxKlParam = createLightKnob<DLXSmallKnobLight, DLXSmallLightKnob>(mm2px(Vec(26.632, 103.047)), module, Algomorph4::AUX_KNOBS + i, auxKl);
                if (i != module->knobMode) {
                    auxKlParam->hide();
                    auxKlParam->sibling->hide();
                }
                addChild(auxKl);
                addParam(auxKlParam);
            }

            addOutput(createOutput<DLXPortPolyOut>(mm2px(Vec(50.182, 73.040)), module, Algomorph4::SUM_OUTPUT));

            addChild(createRingLightCentered<DLXYellowLight>(mm2px(Vec(30.545, 87.440)), 8.862, module, Algomorph4::EDIT_LIGHT, .4));
            addChild(createParamCentered<DLXPurpleButton>(mm2px(Vec(30.545, 87.440)), module, Algomorph4::EDIT_BUTTON));
            addChild(createSvgSwitchLightCentered<DLXPencilLight>(mm2px(Vec(30.545 + 0.07, 87.440 - 0.297)), module, Algomorph4::EDIT_LIGHT, Algomorph4::EDIT_BUTTON));

            addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 85.526)), module, Algomorph4::OPERATOR_INPUTS + 0));
            addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 95.546)), module, Algomorph4::OPERATOR_INPUTS + 1));
            addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 105.567)), module, Algomorph4::OPERATOR_INPUTS + 2));
            addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 115.589)), module, Algomorph4::OPERATOR_INPUTS + 3));

            addOutput(createOutput<DLXPortPolyOut>(mm2px(Vec(50.184, 85.526)), module, Algomorph4::MODULATOR_OUTPUTS + 0));
            addOutput(createOutput<DLXPortPolyOut>(mm2px(Vec(50.184, 95.546)), module, Algomorph4::MODULATOR_OUTPUTS + 1));
            addOutput(createOutput<DLXPortPolyOut>(mm2px(Vec(50.184, 105.567)), module, Algomorph4::MODULATOR_OUTPUTS + 2));
            addOutput(createOutput<DLXPortPolyOut>(mm2px(Vec(50.184, 115.589)), module, Algomorph4::MODULATOR_OUTPUTS + 3));

            ConnectionBgWidget* connectionBgWidget = new ConnectionBgWidget(OpButtonCenters, ModButtonCenters);
            connectionBgWidget->box.pos = this->box.pos;
            connectionBgWidget->box.size = this->box.size;
            addChild(connectionBgWidget);

            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[0], ModButtonCenters[0], module, Algomorph4::H_CONNECTION_LIGHTS + 0));
            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[1], ModButtonCenters[1], module, Algomorph4::H_CONNECTION_LIGHTS + 3));
            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[2], ModButtonCenters[2], module, Algomorph4::H_CONNECTION_LIGHTS + 6));
            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[3], ModButtonCenters[3], module, Algomorph4::H_CONNECTION_LIGHTS + 9));

            addChild(createLineLight<DLXRedLight>(OpButtonCenters[0], ModButtonCenters[1], module, Algomorph4::D_DISABLE_LIGHTS + 0));
            addChild(createLineLight<DLXRedLight>(OpButtonCenters[0], ModButtonCenters[2], module, Algomorph4::D_DISABLE_LIGHTS + 1));
            addChild(createLineLight<DLXRedLight>(OpButtonCenters[0], ModButtonCenters[3], module, Algomorph4::D_DISABLE_LIGHTS + 2));

            addChild(createLineLight<DLXRedLight>(OpButtonCenters[1], ModButtonCenters[0], module, Algomorph4::D_DISABLE_LIGHTS + 3));
            addChild(createLineLight<DLXRedLight>(OpButtonCenters[1], ModButtonCenters[2], module, Algomorph4::D_DISABLE_LIGHTS + 4));
            addChild(createLineLight<DLXRedLight>(OpButtonCenters[1], ModButtonCenters[3], module, Algomorph4::D_DISABLE_LIGHTS + 5));
        
            addChild(createLineLight<DLXRedLight>(OpButtonCenters[2], ModButtonCenters[0], module, Algomorph4::D_DISABLE_LIGHTS + 6));
            addChild(createLineLight<DLXRedLight>(OpButtonCenters[2], ModButtonCenters[1], module, Algomorph4::D_DISABLE_LIGHTS + 7));
            addChild(createLineLight<DLXRedLight>(OpButtonCenters[2], ModButtonCenters[3], module, Algomorph4::D_DISABLE_LIGHTS + 8));

            addChild(createLineLight<DLXRedLight>(OpButtonCenters[3], ModButtonCenters[0], module, Algomorph4::D_DISABLE_LIGHTS + 9));
            addChild(createLineLight<DLXRedLight>(OpButtonCenters[3], ModButtonCenters[1], module, Algomorph4::D_DISABLE_LIGHTS + 10));
            addChild(createLineLight<DLXRedLight>(OpButtonCenters[3], ModButtonCenters[2], module, Algomorph4::D_DISABLE_LIGHTS + 11));

            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[0], ModButtonCenters[1], module, Algomorph4::CONNECTION_LIGHTS + 0));
            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[0], ModButtonCenters[2], module, Algomorph4::CONNECTION_LIGHTS + 3));
            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[0], ModButtonCenters[3], module, Algomorph4::CONNECTION_LIGHTS + 6));

            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[1], ModButtonCenters[0], module, Algomorph4::CONNECTION_LIGHTS + 9));
            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[1], ModButtonCenters[2], module, Algomorph4::CONNECTION_LIGHTS + 12));
            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[1], ModButtonCenters[3], module, Algomorph4::CONNECTION_LIGHTS + 15));

            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[2], ModButtonCenters[0], module, Algomorph4::CONNECTION_LIGHTS + 18));
            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[2], ModButtonCenters[1], module, Algomorph4::CONNECTION_LIGHTS + 21));
            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[2], ModButtonCenters[3], module, Algomorph4::CONNECTION_LIGHTS + 24));

            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[3], ModButtonCenters[0], module, Algomorph4::CONNECTION_LIGHTS + 27));
            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[3], ModButtonCenters[1], module, Algomorph4::CONNECTION_LIGHTS + 30));
            addChild(createLineLight<DLXMultiLight>(OpButtonCenters[3], ModButtonCenters[2], module, Algomorph4::CONNECTION_LIGHTS + 33));
            
            addChild(createRingLightCentered<DLXMultiLight>(OpButtonCenters[0], 8.862, module, Algomorph4::OPERATOR_LIGHTS + 0));
            addChild(createRingLightCentered<DLXMultiLight>(OpButtonCenters[1], 8.862, module, Algomorph4::OPERATOR_LIGHTS + 3));
            addChild(createRingLightCentered<DLXMultiLight>(OpButtonCenters[2], 8.862, module, Algomorph4::OPERATOR_LIGHTS + 6));
            addChild(createRingLightCentered<DLXMultiLight>(OpButtonCenters[3], 8.862, module, Algomorph4::OPERATOR_LIGHTS + 9));
        
            addChild(createRingIndicatorCentered(OpButtonCenters[0], 8.862, module, Algomorph4::CARRIER_INDICATORS + 0, 0.8f));
            addChild(createRingIndicatorCentered(OpButtonCenters[1], 8.862, module, Algomorph4::CARRIER_INDICATORS + 3, 0.8f));
            addChild(createRingIndicatorCentered(OpButtonCenters[2], 8.862, module, Algomorph4::CARRIER_INDICATORS + 6, 0.8f));
            addChild(createRingIndicatorCentered(OpButtonCenters[3], 8.862, module, Algomorph4::CARRIER_INDICATORS + 9, 0.8f));

            addChild(createRingLightCentered<DLXMultiLight>(ModButtonCenters[0], 8.862, module, Algomorph4::MODULATOR_LIGHTS + 0));
            addChild(createRingLightCentered<DLXMultiLight>(ModButtonCenters[1], 8.862, module, Algomorph4::MODULATOR_LIGHTS + 3));
            addChild(createRingLightCentered<DLXMultiLight>(ModButtonCenters[2], 8.862, module, Algomorph4::MODULATOR_LIGHTS + 6));
            addChild(createRingLightCentered<DLXMultiLight>(ModButtonCenters[3], 8.862, module, Algomorph4::MODULATOR_LIGHTS + 9));

            addParam(createParamCentered<DLXPurpleButton>(OpButtonCenters[0], module, Algomorph4::OPERATOR_BUTTONS + 0));
            addParam(createParamCentered<DLXPurpleButton>(OpButtonCenters[1], module, Algomorph4::OPERATOR_BUTTONS + 1));
            addParam(createParamCentered<DLXPurpleButton>(OpButtonCenters[2], module, Algomorph4::OPERATOR_BUTTONS + 2));
            addParam(createParamCentered<DLXPurpleButton>(OpButtonCenters[3], module, Algomorph4::OPERATOR_BUTTONS + 3));

            addParam(createParamCentered<DLXPurpleButton>(ModButtonCenters[0], module, Algomorph4::MODULATOR_BUTTONS + 0));
            addParam(createParamCentered<DLXPurpleButton>(ModButtonCenters[1], module, Algomorph4::MODULATOR_BUTTONS + 1));
            addParam(createParamCentered<DLXPurpleButton>(ModButtonCenters[2], module, Algomorph4::MODULATOR_BUTTONS + 2));
            addParam(createParamCentered<DLXPurpleButton>(ModButtonCenters[3], module, Algomorph4::MODULATOR_BUTTONS + 3));
        }
        else
            setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Algomorph Components.svg")));
    }

    void appendContextMenu(Menu* menu) override {
        Algomorph4* module = dynamic_cast<Algomorph4*>(this->module);


        struct GlowingInkItem : MenuItem {
            Algomorph4 *module;
            Algomorph4Widget* moduleWidget;
            void onAction(const event::Action &e) override {
                // History
                ToggleGlowingInkAction<Algomorph4>* h = new ToggleGlowingInkAction<Algomorph4>;
                h->moduleId = module->id;

                moduleWidget->toggleInkVisibility();

                APP->history->push(h);
            }
        };
        
        struct KnobModeItem : MenuItem {
            Algomorph4* module;
            Algomorph4Widget* moduleWidget;
            int mode;

            void onAction(const event::Action &e) override {
                // History
                KnobModeAction<Algomorph4, Algomorph4Widget>* h = new KnobModeAction<Algomorph4, Algomorph4Widget>;
                h->moduleId = module->id;
                h->oldKnobMode = module->knobMode;
                h->newKnobMode = mode;

                moduleWidget->setKnobMode(mode);

                APP->history->push(h);
            }
        };

        struct KnobModeMenuItem : MenuItem {
            Algomorph4* module;
            Algomorph4Widget* moduleWidget;
            
            Menu* createChildMenu() override {
                Menu* menu = new Menu;
                menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[1], &KnobModeItem::module, module, &KnobModeItem::moduleWidget, moduleWidget, &KnobModeItem::rightText, CHECKMARK(module->knobMode == 1), &KnobModeItem::mode, 1));
                menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[0], &KnobModeItem::module, module, &KnobModeItem::moduleWidget, moduleWidget, &KnobModeItem::rightText, CHECKMARK(module->knobMode == 0), &KnobModeItem::mode, 0));
                for (int i = 2; i < AuxKnobModes::NUM_MODES; i++)
                    menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[i], &KnobModeItem::module, module, &KnobModeItem::moduleWidget, moduleWidget, &KnobModeItem::rightText, CHECKMARK(module->knobMode == i), &KnobModeItem::mode, i));
                return menu;
            }
        };

        menu->addChild(new MenuSeparator());
        ToggleModeBItem *toggleModeBItem = createMenuItem<ToggleModeBItem>("Modus Operandi", module->modeB ? "B" : "A");
        toggleModeBItem->module = module;
        menu->addChild(toggleModeBItem);


        menu->addChild(new MenuSeparator());
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "AUX Knob"));

		menu->addChild(construct<KnobModeMenuItem>(&MenuItem::text, "Functions...", &MenuItem::rightText, RIGHT_ARROW, &KnobModeMenuItem::module, module, &KnobModeMenuItem::moduleWidget, this));

        menu->addChild(new MenuSeparator());
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "AUX Input"));

		menu->addChild(construct<AuxInputMenuItem<Algomorph4>>(&MenuItem::text, "Functions...", &MenuItem::rightText, RIGHT_ARROW, &AuxInputMenuItem<Algomorph4>::module, module));
        
        AllowMultipleModesItem *allowMultipleModesItem = createMenuItem<AllowMultipleModesItem>("Allow multiple functions", CHECKMARK(module->auxInput.allowMultipleModes));
        allowMultipleModesItem->module = module;
        menu->addChild(allowMultipleModesItem);
        
        ForgetAuxVoltageItem *forgetAuxVoltageItem = createMenuItem<ForgetAuxVoltageItem>("Remember voltage", CHECKMARK(!module->auxInput.forgetVoltage));
        forgetAuxVoltageItem->module = module;
        menu->addChild(forgetAuxVoltageItem);


        menu->addChild(new MenuSeparator());
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Audio"));
        
		menu->addChild(construct<ClickFilterMenuItem<Algomorph4>>(&MenuItem::text, "Click Filter...", &MenuItem::rightText, RIGHT_ARROW, &ClickFilterMenuItem<Algomorph4>::module, module));

        RingMorphItem *ringMorphItem = createMenuItem<RingMorphItem>("Enable Ring Morph", CHECKMARK(module->ringMorph));
        ringMorphItem->module = module;
        menu->addChild(ringMorphItem);


        menu->addChild(new MenuSeparator());
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Interaction"));
        
		menu->addChild(construct<ResetSceneMenuItem<Algomorph4>>(&MenuItem::text, "Reset Scene...", &MenuItem::rightText, RIGHT_ARROW, &ResetSceneMenuItem<Algomorph4>::module, module));
        
        CCWScenesItem *ccwScenesItem = createMenuItem<CCWScenesItem>("Trigger input - reverse sequence", CHECKMARK(!module->ccwSceneSelection));
        ccwScenesItem->module = module;
        menu->addChild(ccwScenesItem);

        ExitConfigItem *exitConfigItem = createMenuItem<ExitConfigItem>("Exit Edit Mode after connection", CHECKMARK(module->exitConfigOnConnect));
        exitConfigItem->module = module;
        menu->addChild(exitConfigItem);


        menu->addChild(new MenuSeparator());
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Visual"));
        
        VULightsItem *vuLightsItem = createMenuItem<VULightsItem>("Disable VU lighting", CHECKMARK(!module->vuLights));
        vuLightsItem->module = module;
        menu->addChild(vuLightsItem);
        
        GlowingInkItem *glowingInkItem = createMenuItem<GlowingInkItem>("Enable glowing panel ink", CHECKMARK(module->glowingInk));
        glowingInkItem->module = module;
        glowingInkItem->moduleWidget = this;
        menu->addChild(glowingInkItem);

        // DebugItem *debugItem = createMenuItem<DebugItem>("The system is down", CHECKMARK(module->debug));
        // debugItem->module = module;
        // menu->addChild(debugItem);
    }

    void setKnobMode(int knobMode) {
		if (!module)
			return;

        Algomorph4* m = dynamic_cast<Algomorph4*>(module);

        DLXSmallLightKnob* oldKnob = dynamic_cast<DLXSmallLightKnob*>(getParam(Algomorph4::AUX_KNOBS + m->knobMode));
        oldKnob->hide();
        oldKnob->sibling->hide();
        DLXSmallLightKnob* newKnob = dynamic_cast<DLXSmallLightKnob*>(getParam(Algomorph4::AUX_KNOBS + knobMode));
        newKnob->show();
        newKnob->sibling->show();
		m->knobMode = knobMode;
	}

    void toggleInkVisibility() {
        if (!module)
            return;

        Algomorph4* m = dynamic_cast<Algomorph4*>(module);

        if (ink->visible)
            ink->hide();
        else
            ink->show();
        m->glowingInk ^= true;
    }
};


Model* modelAlgomorph4 = createModel<Algomorph4, Algomorph4Widget>("Algomorph4");
