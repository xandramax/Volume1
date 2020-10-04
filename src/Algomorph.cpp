#include "plugin.hpp"
#include "GraphData.hpp"
#include <bitset>

constexpr float BLINK_INTERVAL = 0.42857142857f;

struct Algomorph4 : Module {
    enum ParamIds {
        ENUMS(OPERATOR_BUTTONS, 4),
        ENUMS(MODULATOR_BUTTONS, 4),
        ENUMS(SCENE_BUTTONS, 3),
        MORPH_KNOB,
        EDIT_BUTTON,
        NUM_PARAMS
    };
    enum InputIds {
        ENUMS(OPERATOR_INPUTS, 4),
        MORPH_INPUT,
        SCENE_ADV_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        ENUMS(MODULATOR_OUTPUTS, 4),
        SUM_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(DISPLAY_BACKLIGHT, 3),    // 3 colors
        ENUMS(SCENE_LIGHTS, 9),         // 3 colors per light
        ENUMS(H_DISABLE_LIGHTS, 4),
        ENUMS(D_DISABLE_LIGHTS, 12),
        ENUMS(CONNECTION_LIGHTS, 36),   // 3 colors per light
        ENUMS(OPERATOR_LIGHTS, 12),     // 3 colors per light
        ENUMS(MODULATOR_LIGHTS, 12),    // 3 colors per light
        EDIT_LIGHT,
        KNOB_LIGHT,
        GLOWING_INK,
        NUM_LIGHTS
    };
    float morph[16] = {0.f};        // Range -1.f -> 1.f
    bool opEnabled[3][4];          // [scene][op]
    bool opDestinations[3][4][3];   // [scene][op][legal mod]
    std::bitset<12> algoName[3];    // 12-bit IDs of the three stored algorithms
    int sixteenToTwelve[4089];      // Graph ID conversion
                                    // The algorithm graph data are stored with IDs in 12-bit space:
                                    //       000 000 000 000 -> 111 111 111 000
                                    // Each set of 3 bits corresponds to an operator.
                                    // Each bit represents an oscillator's "legal" mod destinations.
                                    // At least one operator is a carrier (no mod destinations, all bits zero).
                                    // However, the algorithms are accessed via 16-bit IDs:
                                    //       0000 0000 0000 0000 -> 1110 1101 1011 0000
                                    // In 16-bit space, the the feedback destinations are included but never equal 1.  
                                    // sixteenToTwelve is indexed by 16-bit ID and returns equivalent 12-bit ID.
    int threeToFour[4][3];          // Modulator ID conversion ([op][x] = y, where x is 0..2 and y is 0..3)
    bool configMode = true;
    int configOp = -1;      // Set to 0-3 when configuring mod destinations for operators 1-4
    int configScene = 1;
    int baseScene = 1;      // Center the Morph knob on saved algorithm 0, 1, or 2

    bool graphDirty = true;
    // bool debug = false;

    //User settings
    bool clickFilterEnabled = true;
    bool ringMorph = false;
    bool randomRingMorph = false;
    bool exitConfigOnConnect = false;
    bool ccwSceneSelection = true;      // Default true to interface with rising ramp LFO at Morph CV input
    bool glowingInk = false;

    dsp::BooleanTrigger sceneButtonTrigger[3];
    dsp::BooleanTrigger sceneAdvButtonTrigger;
    dsp::SchmittTrigger sceneAdvCVTrigger;
    dsp::BooleanTrigger editTrigger;
    dsp::BooleanTrigger operatorTrigger[4];
    dsp::BooleanTrigger modulatorTrigger[4];

    dsp::SlewLimiter clickFilters[2][4][4][16];    // [noRing/ring][op][legal mod][channel], [op][3] = Sum output

    dsp::ClockDivider lightDivider;
    float sceneBrightnesses[3][8][3] = {{{}}};
    float blinkTimer = BLINK_INTERVAL;
    bool blinkStatus = true;
    const float DEF_RED_BRIGHTNESS = 0.4975f;

    Algomorph4() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(MORPH_KNOB, -1.f, 1.f, 0.f, "Algorithm Morph", "", 0, 100);
        for (int i = 0; i < 4; i++) {
            configParam(OPERATOR_BUTTONS + i, 0.f, 1.f, 0.f);
            configParam(MODULATOR_BUTTONS + i, 0.f, 1.f, 0.f);
        }
        for (int i = 0; i < 3; i++) {
            configParam(SCENE_BUTTONS + i, 0.f, 1.f, 0.f);
        }
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                for (int c = 0; c < 16; c++) {
                    clickFilters[0][i][j][c].setRiseFall(3750.f, 3750.f);
                    clickFilters[1][i][j][c].setRiseFall(3750.f, 3750.f);
                }
            }
        }
        lightDivider.setDivision(16);

        //  Initialize opEnabled[] to true and opDestinations[] to false;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                opEnabled[i][j] = true;
                for (int k = 0; k < 3; k++) {
                    opDestinations[i][j][k] = false;
                }
            }
        }

        // Map 3-bit operator-relative mod output indices to 4-bit generalized equivalents
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (i != j) {
                    threeToFour[i][i > j ? j : j - 1] = j;
                }
            }
        }

        // Initialize sixteenToTwelve[] to -1, then index 9-bit IDs by the 12-bit equivalents
        for (int i = 0; i < 4089; i++) {
            sixteenToTwelve[i] = -1;
        }
        for (int i = 0; i < 1695; i++) {
            sixteenToTwelve[(int)xNodeData[i][0]] = i;
        }

        onReset();
    }

    void onReset() override {
        for (int i = 0; i < 3; i++) {
            algoName[i].reset();
            for (int j = 0; j < 4; j++) {
                opEnabled[i][j] = true;
                for (int k = 0; k < 3; k++) {
                    opDestinations[i][j][k] = false;
                }
            }
        }
        configMode = false;
        configOp = -1;
        configScene = 1;
        baseScene = 1;
        clickFilterEnabled = true;
        ringMorph = false;
        randomRingMorph = false;
        exitConfigOnConnect = false;
        ccwSceneSelection = true;
        blinkStatus = true;
        blinkTimer = BLINK_INTERVAL;
        graphDirty = true;
    }

    void onRandomize() override {
        bool carrier[3][4];
        //If in config mode, only randomize the current algorithm,
        //do not change the base scene and do not enable/disable ring morph
        if (configMode) {
            bool noCarrier = true;
            algoName[configScene].reset();    //Initialize
            for (int j = 0; j < 4; j++) {
                opEnabled[configScene][j] = true;   //Initialize
                if (random::uniform() > .5f) {
                    carrier[configScene][j] = true;
                    noCarrier = false;
                }
                if (!carrier[configScene][j])
                    opEnabled[configScene][j] = random::uniform() > .5f;
                for (int k = 0; k < 3; k++) {
                    opDestinations[configScene][j][k] = false;    //Initialize
                    if (!carrier[configScene][j] && opEnabled[configScene][j]) {
                        if (random::uniform() > .5f) {
                            opDestinations[configScene][j][k] = true;
                            algoName[configScene].flip(j * 3 + k);    
                        }
                    }
                }
            }
            if (noCarrier) {
                int shortStraw = std::floor(random::uniform() * 4);
                while (shortStraw == 4)
                    shortStraw = std::floor(random::uniform() * 4);
                carrier[configScene][shortStraw] = true;
                opEnabled[configScene][shortStraw] = true;
                for (int k = 0; k < 3; k++) {
                    opDestinations[configScene][shortStraw][k] = false;
                    algoName[configScene].set(shortStraw * 3 + k, false);
                }
            }
        }
        //Otherwise, randomize everything
        else {
            for (int i = 0; i < 3; i++) {
                bool noCarrier = true;
                algoName[i].reset();    //Initialize
                for (int j = 0; j < 4; j++) {
                    opEnabled[i][j] = true;   //Initialize
                    if (random::uniform() > .5f) {
                        carrier[i][j] = true;
                        noCarrier = false;
                    }
                    if (!carrier[i][j])
                        opEnabled[i][j] = random::uniform() > .5f;
                    for (int k = 0; k < 3; k++) {
                        opDestinations[i][j][k] = false;    //Initialize
                        if (!carrier[i][j] && opEnabled[i][j]) {
                            if (random::uniform() > .5f) {
                                opDestinations[i][j][k] = true;
                                algoName[i].flip(j * 3 + k);    
                            }
                        }
                    }
                }
                if (noCarrier) {
                    int shortStraw = std::floor(random::uniform() * 4);
                    while (shortStraw == 4)
                        shortStraw = std::floor(random::uniform() * 4);
                    carrier[i][shortStraw] = true;
                    opEnabled[i][shortStraw] = true;
                    for (int k = 0; k < 3; k++) {
                        opDestinations[i][shortStraw][k] = false;
                        algoName[i].set(shortStraw * 3 + k, false);
                    }
                }
            }
            baseScene = 1;
            if (randomRingMorph)
                ringMorph = random::uniform() > .5f;
            Module::onRandomize();
        }
        graphDirty = true;
    }

    void process(const ProcessArgs& args) override {
        float in[16] = {0.f};                                   // Operator input channels
        float modOut[4][16] = {{0.f}};                          // Modulator outputs & channels
        float sumOut[16] = {0.f};                               // Sum output channels
        float gain[2][4][4][16] = {{{{0.f}}}};                  // Click filter gains:  [noRing/ring][op][legal mod][channel], gain[x][y][3][z] = sum output
        bool carrier[3][4] = {  {true, true, true, true},       // Per-algorithm operator carriership status
                                {true, true, true, true},       // [scene][op]
                                {true, true, true, true} };
        int channels = 1;                                       // Max channels of operator inputs

        // Only redraw display if morph on channel 1 has changed
        float newMorph0 = clamp(inputs[MORPH_INPUT].getVoltage(0) / 5.f + params[MORPH_KNOB].getValue(), -1.f, 1.f);
        if (morph[0] != newMorph0) {
            morph[0] = newMorph0;
            graphDirty = true;
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
                    //If not changing to a new scene
                    if (baseScene == i) {
                        //Reset morph knob
                        params[MORPH_KNOB].setValue(0.f);
                    }
                    else {
                        //Switch scene
                        baseScene = i;
                    }
                }
                graphDirty = true;
            }
        }
        //Scene advance trigger input
        if (sceneAdvCVTrigger.process(inputs[SCENE_ADV_INPUT].getVoltage())) {
            //Advance base scene
            if (!ccwSceneSelection)
               baseScene = (baseScene + 1) % 3;
            else
               baseScene = (baseScene + 2) % 3;
            graphDirty = true;
        }

        //Edit button
        if (editTrigger.process(params[EDIT_BUTTON].getValue() > 0.f)) {
            configMode ^= true;
            if (configMode) {
                blinkStatus = true;
                blinkTimer = 0.f;
                if (morph[0] > .5f)
                    configScene = (baseScene + 1) % 3;
                else if (morph[0] < -.5f)
                    configScene = (baseScene + 2) % 3;
                else
                    configScene = baseScene;
            }
            configOp = -1;
        }
        //Check to select/deselect operators
        for (int i = 0; i < 4; i++) {
            if (operatorTrigger[i].process(params[OPERATOR_BUTTONS + i].getValue() > 0.f)) {
                if (!configMode) {
                    configMode = true;
                    configOp = i;
                    if (morph[0] > .5f)
                        configScene = (baseScene + 1) % 3;
                    else if (morph[0] < -.5f)
                        configScene = (baseScene + 2) % 3;
                    else
                        configScene = baseScene;
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

        //Check for config mode destination selection
        if (configMode && configOp > -1) {
            if (modulatorTrigger[configOp].process(params[MODULATOR_BUTTONS + configOp].getValue() > 0.f)) {  //Op is connected to itself
                opEnabled[configScene][configOp] ^= true;

                if (exitConfigOnConnect)
                    configMode = false;
                
                graphDirty = true;
            }
            else {
                for (int i = 0; i < 3; i++) {
                    if (modulatorTrigger[threeToFour[configOp][i]].process(params[MODULATOR_BUTTONS + threeToFour[configOp][i]].getValue() > 0.f)) {

                        opDestinations[configScene][configOp][i] ^= true;
                        algoName[configScene].flip(configOp * 3 + i);

                        if (exitConfigOnConnect)
                            configMode = false;

                        graphDirty = true;
                        break;
                    }
                }
            }
        }

        //Determine polyphony count
        for (int i = 0; i < 4; i++) {
            if (channels < inputs[OPERATOR_INPUTS + i].getChannels())
                channels = inputs[OPERATOR_INPUTS + i].getChannels();
        }

        //Get operator input channel then route to modulation output channel or to sum output channel
        for (int c = 0; c < channels; c++) {
            if (c > 0) {   //morph[0] is calculated earlier
                morph[c] = inputs[MORPH_INPUT].getPolyVoltage(c) / 5.f + params[MORPH_KNOB].getValue();
                morph[c] = clamp(morph[c], -1.f, 1.f);
            }

            for (int i = 0; i < 4; i++) {
                if (inputs[OPERATOR_INPUTS + i].isConnected()) {
                    in[c] = inputs[OPERATOR_INPUTS + i].getPolyVoltage(c);
                    //Simple case, do not check adjacent algorithms
                    if (morph[c] == 0.f) {
                        for (int j = 0; j < 3; j++) {
                            bool connected = opDestinations[baseScene][i][j] && opEnabled[baseScene][i];
                            carrier[baseScene][i] = connected ? false : carrier[baseScene][i];
                            gain[0][i][j][c] = clickFilterEnabled ? clickFilters[0][i][j][c].process(args.sampleTime, connected) : connected;
                            modOut[threeToFour[i][j]][c] += in[c] * gain[0][i][j][c];
                        }
                        bool sumConnected = carrier[baseScene][i] && opEnabled[baseScene][i];
                        gain[0][i][3][c] = clickFilterEnabled ? clickFilters[0][i][3][c].process(args.sampleTime, sumConnected) : sumConnected;
                        sumOut[c] += in[c] * gain[0][i][3][c];
                    }
                    //Check current algorithm and morph target
                    else {
                        int forwardScene, backwardScene;
                        float absMorph = morph[c];  //Absolute value of morph[c]
                        if (morph[c] > 0.f) {
                            forwardScene = (baseScene + 1) % 3;
                            backwardScene = (baseScene + 2) % 3;
                        }
                        else {
                            absMorph *= -1.f;
                            forwardScene = (baseScene + 2) % 3;
                            backwardScene = (baseScene + 1) % 3;
                        }
                        for (int j = 0; j < 3; j++) {
                            if (ringMorph) {
                                float ringConnection = opDestinations[backwardScene][i][j] * absMorph * opEnabled[backwardScene][i];
                                carrier[backwardScene][i] = ringConnection == 0.f && opEnabled[backwardScene][i] ? carrier[backwardScene][i] : false;
                                gain[1][i][j][c] = clickFilterEnabled ? clickFilters[1][i][j][c].process(args.sampleTime, ringConnection) : ringConnection; 
                            }
                            float connectionA = opDestinations[baseScene][i][j]     * (1.f - absMorph)  * opEnabled[baseScene][i];
                            float connectionB = opDestinations[forwardScene][i][j]  * absMorph          * opEnabled[forwardScene][i];
                            carrier[baseScene][i]  = connectionA > 0.f ? false : carrier[baseScene][i];
                            carrier[forwardScene][i] = connectionB > 0.f ? false : carrier[forwardScene][i];
                            gain[0][i][j][c] = clickFilterEnabled ? clickFilters[0][i][j][c].process(args.sampleTime, connectionA + connectionB) : connectionA + connectionB;
                            modOut[threeToFour[i][j]][c] += in[c] * gain[0][i][j][c] - in[c] * gain[1][i][j][c];
                        }
                        if (ringMorph) {
                            float ringSumConnection = carrier[backwardScene][i] * absMorph * opEnabled[backwardScene][i];
                            gain[1][i][3][c] = clickFilterEnabled ? clickFilters[1][i][3][c].process(args.sampleTime, ringSumConnection) : ringSumConnection;
                        }
                        float sumConnection =     carrier[baseScene][i]     * (1.f - absMorph)  * opEnabled[baseScene][i]
                                            +   carrier[forwardScene][i]    * absMorph          * opEnabled[forwardScene][i];
                        gain[0][i][3][c] = clickFilterEnabled ? clickFilters[0][i][3][c].process(args.sampleTime, sumConnection) : sumConnection;
                        sumOut[c] += in[c] * gain[0][i][3][c] - in[c] * gain[1][i][3][c];
                    }
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
                //Set backlight
                //Set purple component to off
                lights[DISPLAY_BACKLIGHT].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                //Set yellow component
                lights[DISPLAY_BACKLIGHT + 1].setSmoothBrightness(getPortBrightness(outputs[SUM_OUTPUT]) / 1024.f + 0.01f, args.sampleTime * lightDivider.getDivision());
                //Set edit light
                lights[EDIT_LIGHT].setSmoothBrightness(1.f, args.sampleTime * lightDivider.getDivision());
                //Set scene lights
                for (int i = 0; i < 3; i++) {
                    //Set purple component to off
                    lights[SCENE_LIGHTS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    //Set yellow component depending on config scene and blink status
                    lights[SCENE_LIGHTS + i * 3 + 1].setSmoothBrightness(configScene == i ? blinkStatus : 0.f, args.sampleTime * lightDivider.getDivision());
                }
                //Set op/mod lights
                for (int i = 0; i < 4; i++) {
                    if (opEnabled[configScene][i]) {
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
                    else {
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
                        //Purple lights
                        lights[MODULATOR_LIGHTS + i * 3].setSmoothBrightness(configOp > -1 ?
                            opEnabled[configScene][configOp] ?
                                getPortBrightness(outputs[MODULATOR_OUTPUTS + i])
                                : blinkStatus ?
                                    0.f
                                    : getPortBrightness(outputs[MODULATOR_OUTPUTS + i])
                            : getPortBrightness(outputs[MODULATOR_OUTPUTS + i]), args.sampleTime * lightDivider.getDivision());
                        //Yellow lights
                        lights[MODULATOR_LIGHTS + i * 3 + 1].setSmoothBrightness(configOp > -1 ?
                            opEnabled[configScene][configOp] ?
                                0.f
                                : blinkStatus
                            : 0.f, args.sampleTime * lightDivider.getDivision());

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
                for (int i = 0; i < 12; i++) {
                    //Purple lights
                    lights[CONNECTION_LIGHTS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    //Yellow lights
                    lights[CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(opEnabled[configScene][i / 3] ? 
                        opDestinations[configScene][i / 3][i % 3] ?
                            0.4f
                            : 0.f
                        : 0.f, args.sampleTime * lightDivider.getDivision());
                }
                //Set horizontal disable lights
                for (int i = 0; i < 4; i++)
                    lights[H_DISABLE_LIGHTS + i].setSmoothBrightness(opEnabled[configScene][i] ?
                        0.f
                        : DEF_RED_BRIGHTNESS, args.sampleTime * lightDivider.getDivision());
                //Set diagonal disable lights
                for (int i = 0; i < 12; i++)
                    lights[D_DISABLE_LIGHTS + i].setSmoothBrightness(opEnabled[configScene][i / 3] ? 
                        0.f
                        : opDestinations[configScene][i / 3][i % 3] ?
                            DEF_RED_BRIGHTNESS
                            : 0.f, args.sampleTime * lightDivider.getDivision());
            } 
            else {
                //Set backlight
                //Set yellow component to off
                lights[DISPLAY_BACKLIGHT + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                //Set purple component
                lights[DISPLAY_BACKLIGHT].setSmoothBrightness(getPortBrightness(outputs[SUM_OUTPUT]) / 1024.f + 0.014325f, args.sampleTime * lightDivider.getDivision());
                //Set edit light
                lights[EDIT_LIGHT].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                if (morph[0] == 0.f) {  //Display state without morph
                    //Set base scene light
                    for (int i = 0; i < 3; i++) {
                        //Set yellow component to off
                        lights[SCENE_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        //Set purple component depending on base scene
                        lights[SCENE_LIGHTS + i * 3].setSmoothBrightness(i == baseScene ? 1.f : 0.f, args.sampleTime * lightDivider.getDivision());
                    }
                    //Set op/mod lights
                    for (int i = 0; i < 4; i++) {
                        if (opEnabled[baseScene][i]) {
                            //Set op lights
                            //Purple lights
                            lights[OPERATOR_LIGHTS + i * 3].setSmoothBrightness(getPortBrightness(inputs[OPERATOR_INPUTS + i]), args.sampleTime * lightDivider.getDivision());
                            //Red lights
                            lights[OPERATOR_LIGHTS + i * 3 + 2].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());

                        }
                        else {
                            //Set op lights
                            //Purple lights
                            lights[OPERATOR_LIGHTS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                            //Red lights
                            lights[OPERATOR_LIGHTS + i * 3 + 2].setSmoothBrightness(DEF_RED_BRIGHTNESS, args.sampleTime * lightDivider.getDivision());
                        }
                        //Set op lights
                        //Yellow Lights
                        lights[OPERATOR_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        //Set mod lights
                        //Purple lights
                        lights[MODULATOR_LIGHTS + i * 3].setSmoothBrightness(getPortBrightness(outputs[MODULATOR_OUTPUTS + i]), args.sampleTime * lightDivider.getDivision());
                        //Yellow lights
                        lights[MODULATOR_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    }
                    //Set connection lights
                    for (int i = 0; i < 12; i++) {
                        //Purple lights
                        lights[CONNECTION_LIGHTS + i * 3].setSmoothBrightness(opEnabled[baseScene][i / 3] ? 
                            opDestinations[baseScene][i / 3][i % 3] ?
                                getPortBrightness(inputs[OPERATOR_INPUTS + i / 3])
                                : 0.f
                            : 0.f, args.sampleTime * lightDivider.getDivision());
                        //Yellow lights
                        lights[CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    }
                    //Set horizontal disable lights
                    for (int i = 0; i < 4; i++)
                        lights[H_DISABLE_LIGHTS + i].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    //Set diagonal disable lights
                    for (int i = 0; i < 12; i++)
                        lights[D_DISABLE_LIGHTS + i].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                }
                else {  //Display morphed state
                    float brightness;
                    updateSceneBrightnesses();
                    if (morph[0] > 0.f) {
                        int targetScene = (baseScene + 1) % 3;
                        //Set scene lights
                        //Set base scene light
                        for (int i = 0; i < 3; i++) {
                            //Set yellow component to off
                            lights[SCENE_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                            //Set purple component depending on inverse morph
                            lights[SCENE_LIGHTS + i * 3].setSmoothBrightness(i == baseScene ? 1.f - rescale(morph[0], 0.f, 1.f, 0.f, .635f) : 0.f, args.sampleTime * lightDivider.getDivision());
                        }
                        //Set morph target light's purple component depending on morph
                        lights[SCENE_LIGHTS + ((baseScene + 1) % 3) * 3].setSmoothBrightness(morph[0], args.sampleTime * lightDivider.getDivision());
                        //Set op/mod lights
                        for (int i = 0; i < 4; i++) {
                            //Purple, yellow, and red lights
                            for (int j = 0; j < 3; j++) {
                                lights[OPERATOR_LIGHTS + i * 3 + j].setSmoothBrightness(crossfade(sceneBrightnesses[baseScene][i][j], sceneBrightnesses[targetScene][i][j], morph[0]), args.sampleTime * lightDivider.getDivision()); 
                                lights[MODULATOR_LIGHTS + i * 3 + j].setSmoothBrightness(crossfade(sceneBrightnesses[baseScene][i + 4][j], sceneBrightnesses[targetScene][i + 4][j], morph[0]), args.sampleTime * lightDivider.getDivision()); 
                            }
                        }
                        //Set connection lights
                        for (int i = 0; i < 12; i++) {
                            brightness = 0.f;
                            if (opDestinations[baseScene][i / 3][i % 3]) {
                                if (opEnabled[baseScene][i / 3])
                                    brightness += getPortBrightness(inputs[OPERATOR_INPUTS + i / 3]) * (1.f - morph[0]);
                            }
                            if (opDestinations[(baseScene + 1) % 3][i / 3][i % 3]) {
                                if (opEnabled[(baseScene + 1) % 3][i / 3])
                                    brightness += getPortBrightness(inputs[OPERATOR_INPUTS + i / 3]) * morph[0];
                            }
                            //Purple lights
                            lights[CONNECTION_LIGHTS + i * 3].setSmoothBrightness(brightness, args.sampleTime * lightDivider.getDivision());
                            //Yellow
                            lights[CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                            //Set diagonal disable lights
                            lights[D_DISABLE_LIGHTS + i].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        }
                        //Set horizontal disable lights
                        for (int i = 0; i < 4; i++)
                            lights[H_DISABLE_LIGHTS + i].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    }
                    else {
                        int targetScene = (baseScene + 2) % 3;
                        //Set scene lights
                        //Set base scene light
                        for (int i = 0; i < 3; i++) {
                            //Set yellow component to off
                            lights[SCENE_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                            //Set purple component depending on inverse morph
                            lights[SCENE_LIGHTS + i * 3].setSmoothBrightness(i == baseScene ? 1.f - rescale(morph[0] * -1, 0.f, 1.f, 0.f, .635f) : 0.f, args.sampleTime * lightDivider.getDivision());
                        }
                        //Set morph target light's purple component depending on morph
                        lights[SCENE_LIGHTS + targetScene * 3].setSmoothBrightness(morph[0] * -1, args.sampleTime * lightDivider.getDivision());
                        //Set op/mod lights
                        for (int i = 0; i < 4; i++) {
                            //Purple, yellow, and red lights
                            for (int j = 0; j < 3; j++) {
                                lights[OPERATOR_LIGHTS + i * 3 + j].setSmoothBrightness(crossfade(sceneBrightnesses[baseScene][i][j], sceneBrightnesses[targetScene][i][j], morph[0] * -1), args.sampleTime * lightDivider.getDivision()); 
                                lights[MODULATOR_LIGHTS + i * 3 + j].setSmoothBrightness(crossfade(sceneBrightnesses[baseScene][i + 4][j], sceneBrightnesses[targetScene][i + 4][j], morph[0] * -1), args.sampleTime * lightDivider.getDivision()); 
                            }
                        }
                        //Set connection lights
                        for (int i = 0; i < 12; i++) {
                            brightness = 0.f;
                            if (opDestinations[baseScene][i / 3][i % 3]) {
                                if (opEnabled[baseScene][i / 3])
                                    brightness += getPortBrightness(inputs[OPERATOR_INPUTS + i / 3]) * (1.f - (morph[0] * -1.f));
                            }
                            if (opDestinations[targetScene][i / 3][i % 3]) {
                                if (opEnabled[targetScene][i / 3])
                                    brightness += getPortBrightness(inputs[OPERATOR_INPUTS + i / 3]) * (morph[0] * -1.f);
                            }
                            //Purple lights
                            lights[CONNECTION_LIGHTS + i * 3].setSmoothBrightness(brightness, args.sampleTime * lightDivider.getDivision());
                            //Yellow lights
                            lights[CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                            //Set diagonal disable lights
                            lights[D_DISABLE_LIGHTS + i].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                        }
                        //Set horizontal disable lights
                        for (int i = 0; i < 4; i++)
                            lights[H_DISABLE_LIGHTS + i].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    }
                }
            }
        }
    }

    inline float getPortBrightness(Port port) {
        return std::max(    {   port.plugLights[0].getBrightness(),
                                port.plugLights[1].getBrightness() * 4,
                                port.plugLights[2].getBrightness()          }   );
    }

    void updateSceneBrightnesses() {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                if (opEnabled[i][j]) {
                    //Op lights
                    //Purple lights
                    sceneBrightnesses[i][j][0] = getPortBrightness(inputs[OPERATOR_INPUTS + j]);
                    //Mod lights
                    //Purple lights
                    sceneBrightnesses[i][j + 4][0] = getPortBrightness(outputs[MODULATOR_OUTPUTS + j]);
                }
                else {
                    //Op lights
                    //Purple lights
                    sceneBrightnesses[i][j][0] = 0.f;
                }
                //Op lights
                //Yellow Lights
                sceneBrightnesses[i][j][1] = 0.f;
                //Red lights
                sceneBrightnesses[i][j][2] = !opEnabled[i][j];
            }
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "Config Enabled", json_boolean(configMode));
        json_object_set_new(rootJ, "Config Mode", json_integer(configOp));
        json_object_set_new(rootJ, "Config Scene", json_integer(configOp));
        json_object_set_new(rootJ, "Current Scene", json_integer(baseScene));
        json_object_set_new(rootJ, "Ring Morph", json_boolean(ringMorph));
        json_object_set_new(rootJ, "Randomize Ring Morph", json_boolean(randomRingMorph));
        json_object_set_new(rootJ, "Auto Exit", json_boolean(exitConfigOnConnect));
        json_object_set_new(rootJ, "CCW Scene Selection", json_boolean(ccwSceneSelection));
        json_object_set_new(rootJ, "Click Filter Enabled", json_boolean(clickFilterEnabled));
        json_object_set_new(rootJ, "Glowing Ink", json_boolean(glowingInk));
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
        json_t* opEnabledJ = json_array();
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                json_t* enabledJ = json_object();
                json_object_set_new(enabledJ, "Enabled Op", json_boolean(opEnabled[i][j]));
                json_array_append_new(opEnabledJ, enabledJ);
            }
        }
        json_object_set_new(rootJ, "Operators Enabled", opEnabledJ);
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        configMode = json_integer_value(json_object_get(rootJ, "Config Enabled"));
        configOp = json_integer_value(json_object_get(rootJ, "Config Mode"));
        configScene = json_integer_value(json_object_get(rootJ, "Config Scene"));
        baseScene = json_integer_value(json_object_get(rootJ, "Current Scene"));
        ringMorph = json_boolean_value(json_object_get(rootJ, "Ring Morph"));
        randomRingMorph = json_boolean_value(json_object_get(rootJ, "Randomize Ring Morph"));
        exitConfigOnConnect = json_boolean_value(json_object_get(rootJ, "Auto Exit"));
        ccwSceneSelection = json_boolean_value(json_object_get(rootJ, "CCW Scene Selection"));
        clickFilterEnabled = json_boolean_value(json_object_get(rootJ, "Click Filter Enabled"));
        glowingInk = json_boolean_value(json_object_get(rootJ, "Glowing Ink"));
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
        json_t* nameJ; size_t sixteenToTwelve;
        json_array_foreach(algoNamesJ, sixteenToTwelve, nameJ) {
            algoName[sixteenToTwelve] = json_integer_value(json_object_get(nameJ, "Name"));
        }
        json_t* opEnabledJ = json_object_get(rootJ, "Operators Enabled");
        json_t* enabledOpJ;
        size_t enabledOpIndex;
        i = j = 0;
        json_array_foreach(opEnabledJ, enabledOpIndex, enabledOpJ) {
            opEnabled[i][j] = json_boolean_value(json_object_get(enabledOpJ, "Enabled Op"));
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
            opEnabled[i][j] = !json_boolean_value(json_object_get(disabledOpJ, "Disabled Op"));
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
        float edgeStroke = 0.925f;
        static constexpr float arrowStroke1 = (2.65f/4.f) + (1.f/3.f);
        static constexpr float arrowStroke2 = (7.65f/4.f) + (1.f/3.f);

        AlgoDrawWidget(MODULE* module) {
            this->module = module;
            font = APP->window->loadFont(asset::plugin(pluginInstance, "res/terminal-grotesque.ttf"));
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
                    edgeColor = nvgRGBA(0x9a, 0x9a, 0x6f, 0xff);
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

            xOrigin = box.size.x / 2.f;
            yOrigin = box.size.y / 2.f;

            for (int i = 0; i < 3; i++) {
                int name = module->sixteenToTwelve[module->algoName[i].to_ullong()];
                if (name != -1)
                    graphs[i] = alGraph(module->sixteenToTwelve[(int)module->algoName[i].to_ullong()]);
                else
                    graphs[i] = alGraph(0);
            }

            bool noMorph = false;                
            int scene = module->configMode ? module->configScene : module->baseScene;

            if (module->morph[0] == 0.f || module->configMode)
                noMorph = true;

            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, box.getTopLeft().x, box.getTopLeft().y, box.size.x, box.size.y, 3.675f);
            nvgStrokeWidth(args.vg, borderStroke);
            nvgStroke(args.vg);

            // Draw nodes
            float radius = 8.35425f;
            nvgBeginPath(args.vg);
            for (int i = 0; i < 4; i++) {
                if (noMorph)    //Display state without morph
                    nvgCircle(args.vg, graphs[scene].nodes[i].coords.x, graphs[scene].nodes[i].coords.y, radius);
                else {  //Display moprhed state
                    if (module->morph[0] > 0.f) {
                        nvgCircle(args.vg,  crossfade(graphs[scene].nodes[i].coords.x, graphs[(scene + 1) % 3].nodes[i].coords.x, module->morph[0]),
                                            crossfade(graphs[scene].nodes[i].coords.y, graphs[(scene + 1) % 3].nodes[i].coords.y, module->morph[0]),
                                            radius);
                    }
                    else {
                        nvgCircle(args.vg,  crossfade(graphs[scene].nodes[i].coords.x, graphs[(scene + 2) % 3].nodes[i].coords.x, -module->morph[0]),
                                            crossfade(graphs[scene].nodes[i].coords.y, graphs[(scene + 2) % 3].nodes[i].coords.y, -module->morph[0]),
                                            radius);
                    }
                }
            }
            nvgFillColor(args.vg, nodeFillColor);
            nvgFill(args.vg);
            nvgStrokeColor(args.vg, nodeStrokeColor);
            nvgStrokeWidth(args.vg, nodeStroke);
            nvgStroke(args.vg);

            // Draw node numbers
            nvgBeginPath(args.vg);
            nvgFontSize(args.vg, 10.f);
            nvgFontFaceId(args.vg, font->handle);
            nvgFillColor(args.vg, textColor);
            for (int i = 0; i < 4; i++) {
                std::string s = std::to_string(i + 1);
                char const *id = s.c_str();
                nvgTextBounds(args.vg, graphs[scene].nodes[i].coords.x, graphs[scene].nodes[i].coords.y, id, id + 1, textBounds);
                float xOffset = (textBounds[2] - textBounds[0]) / 2.f;
                float yOffset = (textBounds[3] - textBounds[1]) / 3.25f;
                if (module->morph[0] == 0.f || module->configMode)    //Display state without morph
                    nvgText(args.vg, graphs[scene].nodes[i].coords.x - xOffset, graphs[scene].nodes[i].coords.y + yOffset, id, id + 1);
                else {  //Display moprhed state
                    if (module->morph[0] > 0.f) {
                        nvgText(args.vg,  crossfade(graphs[scene].nodes[i].coords.x, graphs[(scene + 1) % 3].nodes[i].coords.x, module->morph[0]) - xOffset,
                                            crossfade(graphs[scene].nodes[i].coords.y, graphs[(scene + 1) % 3].nodes[i].coords.y, module->morph[0]) + yOffset,
                                            id, id + 1);
                    }
                    else {
                        nvgText(args.vg,  crossfade(graphs[scene].nodes[i].coords.x, graphs[(scene + 2) % 3].nodes[i].coords.x, -module->morph[0]) - xOffset,
                                            crossfade(graphs[scene].nodes[i].coords.y, graphs[(scene + 2) % 3].nodes[i].coords.y, -module->morph[0]) + yOffset,
                                            id, id + 1);
                    }
                }
            }

            // Draw edges +/ arrows
            if (noMorph) {
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
                if (module->morph[0] > 0.f)
                    drawEdges(args.vg, graphs[scene], graphs[(scene + 1) % 3], module->morph[0]);
                else
                    drawEdges(args.vg, graphs[scene], graphs[(scene + 2) % 3], -module->morph[0]);
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

struct RingMorphItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        module->ringMorph ^= true;
    }
};

struct RandomizeRingMorphItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        module->randomRingMorph ^= true;
    }
};

struct ExitConfigItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        module->exitConfigOnConnect ^= true;
    }
};

struct CCWScenesItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        module->ccwSceneSelection ^= true;
    }
};

struct ClickFilterEnabledItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        module->clickFilterEnabled ^= true;
    }
};

struct GlowingInkItem : MenuItem {
    Algomorph4 *module;
    void onAction(const event::Action &e) override {
        module->glowingInk ^= true;
    }
};

// struct DebugItem : MenuItem {
//     Algomorph4 *module;
//     void onAction(const event::Action &e) override {
//         module->debug ^= true;
//     }
// };

struct Algomorph4Widget : ModuleWidget {
    std::vector<Vec> SceneButtonCenters = {  {mm2px(10.169), mm2px(53.224)},
                                            {mm2px(18.751), mm2px(53.224)},
                                            {mm2px(27.334), mm2px(53.224)} };
    std::vector<Vec> OpButtonCenters = { {mm2px(16.592), mm2px(84.771)},
                                        {mm2px(16.592), mm2px(95.175)},
                                        {mm2px(16.592), mm2px(105.579)},
                                        {mm2px(16.592), mm2px(115.984)} };
    std::vector<Vec> ModButtonCenters = {{mm2px(33.942), mm2px(84.771)},
                                        {mm2px(33.942), mm2px(95.175)},
                                        {mm2px(33.942), mm2px(105.579)},
                                        {mm2px(33.942), mm2px(115.984)} };
    DLXGlowingInk* ink;
    

    Algomorph4Widget(Algomorph4* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Algomorph.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - RACK_GRID_WIDTH * 2, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 365)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - RACK_GRID_WIDTH * 2, 365)));

        ink = createSvgLight<DLXGlowingInk>(Vec(0,0), module, Algomorph4::GLOWING_INK);
        ink->visible = false;
        addChild(ink);

        AlgoScreenWidget<Algomorph4>* screenWidget = new AlgoScreenWidget<Algomorph4>(module);
        screenWidget->box.pos = mm2px(Vec(6.253, 9.954));
        screenWidget->box.size = mm2px(Vec(38.295, 31.590));
        addChild(screenWidget);
        //Place backlight _above_ in scene in order for brightness to affect screenWidget
        addChild(createBacklight(mm2px(Vec(6.253, 9.954)), mm2px(Vec(38.295, 31.590)), module, Algomorph4::DISPLAY_BACKLIGHT));

        addChild(createRingLightCentered<DLXMultiLight>(SceneButtonCenters[0], 8.862, module, Algomorph4::SCENE_LIGHTS + 0, .75));
        addChild(createRingLightCentered<DLXMultiLight>(SceneButtonCenters[1], 8.862, module, Algomorph4::SCENE_LIGHTS + 3, .75));
        addChild(createRingLightCentered<DLXMultiLight>(SceneButtonCenters[2], 8.862, module, Algomorph4::SCENE_LIGHTS + 6, .75));

        addChild(createParamCentered<DLXTL1105B>(SceneButtonCenters[0], module, Algomorph4::SCENE_BUTTONS + 0));
        addChild(createParamCentered<DLXTL1105B>(SceneButtonCenters[1], module, Algomorph4::SCENE_BUTTONS + 1));
        addChild(createParamCentered<DLXTL1105B>(SceneButtonCenters[2], module, Algomorph4::SCENE_BUTTONS + 2));

        addInput(createInputCentered<DLXPortPoly>(mm2px(Vec(39.950, 53.224)), module, Algomorph4::SCENE_ADV_INPUT));

        addInput(createInput<DLXPortPoly>(mm2px(Vec(7.732, 64.118)), module, Algomorph4::MORPH_INPUT));

        DLXKnobLight* kl = createLight<DLXKnobLight>(mm2px(Vec(19.708, 62.457)), module, Algomorph4::KNOB_LIGHT);
        addChild(createLightKnob(mm2px(Vec(19.708, 62.457)), module, Algomorph4::MORPH_KNOB, kl));
        addChild(kl);

        addOutput(createOutput<DLXPortPolyOut>(mm2px(Vec(36.582, 64.118)), module, Algomorph4::SUM_OUTPUT));

        addChild(createRingLightCentered<DLXYellowLight>(mm2px(Vec(25.268, 79.625)), 8.862, module, Algomorph4::EDIT_LIGHT, .4));
        addChild(createParamCentered<DLXPurpleButton>(mm2px(Vec(25.268, 79.625)), module, Algomorph4::EDIT_BUTTON));
        addChild(createSvgSwitchLightCentered<DLXPencilLight>(mm2px(Vec(25.258, 79.483)), module, Algomorph4::EDIT_LIGHT, Algomorph4::EDIT_BUTTON));

        addInput(createInput<DLXPortPoly>(mm2px(Vec(3.780, 81.439)), module, Algomorph4::OPERATOR_INPUTS + 0));
        addInput(createInput<DLXPortPoly>(mm2px(Vec(3.780, 91.844)), module, Algomorph4::OPERATOR_INPUTS + 1));
        addInput(createInput<DLXPortPoly>(mm2px(Vec(3.780, 102.248)), module, Algomorph4::OPERATOR_INPUTS + 2));
        addInput(createInput<DLXPortPoly>(mm2px(Vec(3.780, 112.653)), module, Algomorph4::OPERATOR_INPUTS + 3));

        addOutput(createOutput<DLXPortPolyOut>(mm2px(Vec(40.021, 81.439)), module, Algomorph4::MODULATOR_OUTPUTS + 0));
        addOutput(createOutput<DLXPortPolyOut>(mm2px(Vec(40.021, 91.844)), module, Algomorph4::MODULATOR_OUTPUTS + 1));
        addOutput(createOutput<DLXPortPolyOut>(mm2px(Vec(40.021, 102.248)), module, Algomorph4::MODULATOR_OUTPUTS + 2));
        addOutput(createOutput<DLXPortPolyOut>(mm2px(Vec(40.021, 112.653)), module, Algomorph4::MODULATOR_OUTPUTS + 3));

        ConnectionBgWidget* connectionBgWidget = new ConnectionBgWidget(OpButtonCenters, ModButtonCenters);
        connectionBgWidget->box.pos = this->box.pos;
        connectionBgWidget->box.size = this->box.size;
        addChild(connectionBgWidget);

        addChild(createLineLight<DLXRedLight>(OpButtonCenters[0], ModButtonCenters[0], module, Algomorph4::H_DISABLE_LIGHTS + 0));
        addChild(createLineLight<DLXRedLight>(OpButtonCenters[1], ModButtonCenters[1], module, Algomorph4::H_DISABLE_LIGHTS + 1));
        addChild(createLineLight<DLXRedLight>(OpButtonCenters[2], ModButtonCenters[2], module, Algomorph4::H_DISABLE_LIGHTS + 2));
        addChild(createLineLight<DLXRedLight>(OpButtonCenters[3], ModButtonCenters[3], module, Algomorph4::H_DISABLE_LIGHTS + 3));

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

    void appendContextMenu(Menu* menu) override {
        Algomorph4* module = dynamic_cast<Algomorph4*>(this->module);

        menu->addChild(new MenuSeparator());

        RingMorphItem *ringMorphItem = createMenuItem<RingMorphItem>("Enable Ring Morph", CHECKMARK(module->ringMorph));
        ringMorphItem->module = module;
        menu->addChild(ringMorphItem);

        RandomizeRingMorphItem *ramdomizeRingMorphItem = createMenuItem<RandomizeRingMorphItem>("Randomization includes Ring Morph", CHECKMARK(module->randomRingMorph));
        ramdomizeRingMorphItem->module = module;
        menu->addChild(ramdomizeRingMorphItem);
        
        ExitConfigItem *exitConfigItem = createMenuItem<ExitConfigItem>("Exit Config Mode after Connection", CHECKMARK(module->exitConfigOnConnect));
        exitConfigItem->module = module;
        menu->addChild(exitConfigItem);
        
        CCWScenesItem *ccwScenesItem = createMenuItem<CCWScenesItem>("Trigger input advances counter-clockwise", CHECKMARK(module->ccwSceneSelection));
        ccwScenesItem->module = module;
        menu->addChild(ccwScenesItem);
        
        ClickFilterEnabledItem *clickFilterEnabledItem = createMenuItem<ClickFilterEnabledItem>("Enable click filter", CHECKMARK(module->clickFilterEnabled));
        clickFilterEnabledItem->module = module;
        menu->addChild(clickFilterEnabledItem);
        
        GlowingInkItem *glowingInkItem = createMenuItem<GlowingInkItem>("Enable glowing panel ink", CHECKMARK(module->glowingInk));
        glowingInkItem->module = module;
        menu->addChild(glowingInkItem);

        // DebugItem *debugItem = createMenuItem<DebugItem>("The system is down", CHECKMARK(module->debug));
        // debugItem->module = module;
        // menu->addChild(debugItem);
    }
};


Model* modelAlgomorph4 = createModel<Algomorph4, Algomorph4Widget>("Algomorph4");
