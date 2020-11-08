#include "Algomorph.hpp"
#include "plugin.hpp"
#include "GraphStructure.hpp"
#include <bitset>

Algomorph4::Algomorph4() {
    glowingInk = pluginSettings.glowingInkDefault;
    vuLights = pluginSettings.vuLightsDefault;
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
    configParam(AUX_KNOBS + AuxKnobModes::ENDLESS_MORPH, -INFINITY, INFINITY, 0.f, AuxKnobModeLabels[AuxKnobModes::ENDLESS_MORPH], " limits", 0, 0);
    configParam(AUX_KNOBS + AuxKnobModes::CLICK_FILTER, 0.004, 2.004f, 1.f, AuxKnobModeLabels[AuxKnobModes::CLICK_FILTER], "x", 0, 1);
    for (int i = 0; i < 4; i++) {
        configParam(OPERATOR_BUTTONS + i, 0.f, 1.f, 0.f);
        configParam(MODULATOR_BUTTONS + i, 0.f, 1.f, 0.f);
    }
    for (int i = 0; i < 3; i++) {
        configParam(SCENE_BUTTONS + i, 0.f, 1.f, 0.f);
    }

    for (int i = 0; i < 4; i++) {
        for (int c = 0; c < 16; c++) {
            sumClickFilters[i][c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
            sumRingClickFilters[i][c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
            for (int j = 0; j < 4; j++) {
                modClickFilters[i][j][c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
                modRingClickFilters[i][j][c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
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

    for (int i = 0; i < 4; i++) {
        auxInput[i] = new AuxInput<Algomorph4>(i, this);
        auxInput[i]->clearModes();
    }

    onReset();
}

void Algomorph4::onReset() {
    for (int i = 0; i < 3; i++) {
        algoName[i].reset();
        displayAlgoName[i].reset();
        for (int j = 0; j < 4; j++) {
            horizontalDestinations[i][j] = false;
            forcedCarrier[i][j] = false;
            carrier[i][j] = true;
            for (int k = 0; k < 3; k++) {
                opDestinations[i][j][k] = false;
            }
        }
    }

    for  (int mode = 0; mode < AuxInputModes::NUM_MODES; mode++)
        auxModeFlags[mode] = false;

    for (int i = 0; i < 4; i++) {
        auxInput[i]->clearModes();
        auxInput[i]->resetVoltages();
        auxInput[i]->allowMultipleModes = false;
    }
    auxInput[2]->setMode(pluginSettings.auxInputDefaults[2]);
    auxInput[1]->setMode(pluginSettings.auxInputDefaults[1]);
    auxInput[3]->setMode(pluginSettings.auxInputDefaults[3]);
    auxInput[0]->setMode(pluginSettings.auxInputDefaults[0]);
    rescaleVoltages(16);

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

void Algomorph4::onRandomize() {
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
    }
    graphDirty = true;
}

void Algomorph4::process(const ProcessArgs& args) {
    float in[16] = {0.f};                                   // Operator input channels
    float modOut[4][16] = {{0.f}};                          // Modulator outputs & channels
    float sumOut[16] = {0.f};                               // Sum output channels
    int sceneOffset[16] = {0};                               //Offset to the base scene
    int channels = 1;                                       // Max channels of operator inputs
    bool processCV = cvDivider.process();

    for (int i = 0; i < 4; i++) {
        if (inputs[AUX_INPUTS + i].isConnected()) {
            auxInput[i]->connected = true;
            auxInput[i]->updateVoltage();
        }
        else {
            if (auxInput[i]->connected) {
                auxInput[i]->connected = false;
                auxInput[i]->resetVoltages();
                running = true;
                rescaleVoltages(16);
            }
        }
    }

    //Determine polyphony count
    for (int i = 0; i < 4; i++) {
        if (channels < inputs[OPERATOR_INPUTS + i].getChannels())
            channels = inputs[OPERATOR_INPUTS + i].getChannels();
        if (channels < inputs[AUX_INPUTS + i].getChannels())
            channels = inputs[AUX_INPUTS + i].getChannels();
    }
    for (int i = 0; i < 4; i++)
        auxInput[i]->channels = channels;

    if (processCV) {
        for (int i = 0; i < 4; i++) {
            //Reset trigger
            if (resetCVTrigger.process(auxInput[i]->voltage[AuxInputModes::RESET][0])) {
                initRun();// must be after sequence reset
                sceneAdvCVTrigger.reset();
            }

            //Run trigger
            if (auxInput[i]->runCVTrigger.process(auxInput[i]->voltage[AuxInputModes::RUN][0])) {
                running ^= true;
                if (running) {
                    if (resetOnRun)
                        initRun();
                }
            }

            //Clock input
            if (running && clockIgnoreOnReset == 0l) {
                //Scene advance trigger input
                if (auxInput[i]->sceneAdvCVTrigger.process(auxInput[i]->voltage[AuxInputModes::CLOCK][0])) {
                    //Advance base scene
                    if (!ccwSceneSelection)
                    baseScene = (baseScene + 1) % 3;
                    else
                    baseScene = (baseScene + 2) % 3;
                    graphDirty = true;
                }
                if (auxInput[i]->reverseSceneAdvCVTrigger.process(auxInput[i]->voltage[AuxInputModes::REVERSE_CLOCK][0])) {
                    //Advance base scene
                    if (!ccwSceneSelection)
                    baseScene = (baseScene + 2) % 3;
                    else
                    baseScene = (baseScene + 1) % 3;
                    graphDirty = true;
                }
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
    }

    //Update scene offset
    if (auxModeFlags[AuxInputModes::SCENE_OFFSET])
        rescaleVoltage(AuxInputModes::SCENE_OFFSET, channels);
    for (int c = 0; c < channels; c++) {
        float sceneOffsetVoltage = scaledAuxVoltage[AuxInputModes::SCENE_OFFSET][c];
        if (sceneOffsetVoltage > FIVE_D_THREE)
            sceneOffset[c] += 1;
        else if (sceneOffsetVoltage < -FIVE_D_THREE)
            sceneOffset[c] += 2;
    }

    //  Update morph status
    if (auxModeFlags[AuxInputModes::MORPH_ATTEN])
        rescaleVoltage(AuxInputModes::MORPH_ATTEN, channels);
    float morphAttenuversion[16] = {0.f};
    for (int c = 0; c < channels; c++) {
        morphAttenuversion[c] = scaledAuxVoltage[AuxInputModes::MORPH_ATTEN][c]
                                * params[AUX_KNOBS + AuxKnobModes::MORPH_ATTEN].getValue();
    }
    if (auxModeFlags[AuxInputModes::MORPH])
        rescaleVoltage(AuxInputModes::MORPH, channels);
    if (auxModeFlags[AuxInputModes::DOUBLE_MORPH])
        rescaleVoltage(AuxInputModes::DOUBLE_MORPH, channels);
    if (auxModeFlags[AuxInputModes::TRIPLE_MORPH])
        rescaleVoltage(AuxInputModes::TRIPLE_MORPH, channels);
    // Only redraw display if morph on channel 1 has changed
    float newMorph0 =   + params[MORPH_KNOB].getValue()
                        + params[AUX_KNOBS + AuxKnobModes::MORPH].getValue()
                        + params[AUX_KNOBS + AuxKnobModes::DOUBLE_MORPH].getValue()
                        + params[AUX_KNOBS + AuxKnobModes::TRIPLE_MORPH].getValue()
                        + params[AUX_KNOBS + AuxKnobModes::UNI_MORPH].getValue()
                        + params[AUX_KNOBS + AuxKnobModes::ENDLESS_MORPH].getValue()
                        + (scaledAuxVoltage[AuxInputModes::MORPH][0]
                        + scaledAuxVoltage[AuxInputModes::DOUBLE_MORPH][0]
                        + scaledAuxVoltage[AuxInputModes::TRIPLE_MORPH][0])
                        * morphAttenuversion[0];
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
        morph[c] =  + params[MORPH_KNOB].getValue()
                    + params[AUX_KNOBS + AuxKnobModes::MORPH].getValue()
                    + params[AUX_KNOBS + AuxKnobModes::DOUBLE_MORPH].getValue()
                    + params[AUX_KNOBS + AuxKnobModes::TRIPLE_MORPH].getValue()
                    + params[AUX_KNOBS + AuxKnobModes::UNI_MORPH].getValue()
                    + params[AUX_KNOBS + AuxKnobModes::ENDLESS_MORPH].getValue()
                    + (scaledAuxVoltage[AuxInputModes::MORPH][c]
                    + scaledAuxVoltage[AuxInputModes::DOUBLE_MORPH][c]
                    + scaledAuxVoltage[AuxInputModes::TRIPLE_MORPH][c])
                    * morphAttenuversion[c];
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

                    toggleHorizontalDestination(configScene, configOp);

                    APP->history->push(h);

                    if (exitConfigOnConnect)
                        configMode = false;
                    
                    graphDirty = true;
                }
                else {
                    for (int mod = 0; mod < 3; mod++) {
                        if (modulatorTrigger[threeToFour[configOp][mod]].process(params[MODULATOR_BUTTONS + threeToFour[configOp][mod]].getValue() > 0.f)) {
                            // History
                            AlgorithmDiagonalChangeAction<Algomorph4>* h = new AlgorithmDiagonalChangeAction<Algomorph4>;
                            h->moduleId = this->id;
                            h->scene = configScene;
                            h->op = configOp;
                            h->mod = mod;

                            toggleDiagonalDestination(configScene, configOp, mod);
                            
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

                        toggleForcedCarrier(configScene, i);

                        APP->history->push(h);
                        
                        graphDirty = true;
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
                    configScene = scene;
                    configMode = true;
                    
                    // History
                    AlgorithmForcedCarrierChangeAction<Algomorph4>* h = new AlgorithmForcedCarrierChangeAction<Algomorph4>;
                    h->moduleId = this->id;
                    h->scene = configScene;
                    h->op = i;

                    toggleForcedCarrier(configScene, i);

                    APP->history->push(h);
                    
                    graphDirty = true;
                    break;
                }
            }
        }
    }
    
    //Update clickfilter rise/fall times
    if (clickFilterDivider.process()) {
        if (auxModeFlags[AuxInputModes::CLICK_FILTER]) {
            for (int i = 0; i < 4; i++) {
                if (auxInput[i]->mode[AuxInputModes::CLICK_FILTER]) {
                    rescaleVoltage(AuxInputModes::CLICK_FILTER, channels);
                    break;
                }
            }
        }


        for (int c = 0; c < channels; c++) {
            float clickFilterResult = clickFilterSlew * params[AUX_KNOBS + AuxKnobModes::CLICK_FILTER].getValue();
            
            clickFilterResult *= scaledAuxVoltage[AuxInputModes::CLICK_FILTER][c];

            for (int auxIndex = 0; auxIndex < 4; auxIndex++) {
                auxInput[auxIndex]->wildcardModClickFilter[c].setRiseFall(clickFilterResult, clickFilterResult);
                auxInput[auxIndex]->wildcardSumClickFilter[c].setRiseFall(clickFilterResult, clickFilterResult);                    
            }

            for (int op = 0; op < 4; op++) {
                sumClickFilters[op][c].setRiseFall(clickFilterResult, clickFilterResult);
                sumRingClickFilters[op][c].setRiseFall(clickFilterResult, clickFilterResult);
                for (int mod = 0; mod < 4; mod++) {
                    modClickFilters[op][mod][c].setRiseFall(clickFilterResult, clickFilterResult);
                    modRingClickFilters[op][mod][c].setRiseFall(clickFilterResult, clickFilterResult);
                }
            }
        }
    }

    //Get operator input channel then route to modulation output channel or to sum output channel
    float wildcardMod[16] = {0.f};
    float wildcardSum[16] = {0.f};
    float runClickFilterGain;
    if (runSilencer)
        runClickFilterGain = runClickFilter.process(args.sampleTime, running);
    else
        runClickFilterGain = 1.f;
    if (auxModeFlags[AuxInputModes::MOD_ATTEN])
        rescaleVoltage(AuxInputModes::MOD_ATTEN, channels);
    if (auxModeFlags[AuxInputModes::SUM_ATTEN])
        rescaleVoltage(AuxInputModes::SUM_ATTEN, channels);
    float modGain = params[AUX_KNOBS + AuxKnobModes::MOD_GAIN].getValue();
    float sumGain = params[AUX_KNOBS + AuxKnobModes::SUM_GAIN].getValue();
    float modAttenuversion[16] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
    if (auxModeFlags[AuxInputModes::MOD_ATTEN]) {
        for (int c = 0; c < channels; c++)
            modAttenuversion[c] *= scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c];
    }
    float sumAttenuversion[16] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
    if (auxModeFlags[AuxInputModes::SUM_ATTEN]) {
        for (int c = 0; c < channels; c++)
            sumAttenuversion[c] *= scaledAuxVoltage[AuxInputModes::SUM_ATTEN][c];
    }
    for (int i = 0; i < 4; i++) {
        if (auxModeFlags[AuxInputModes::SHADOW + i])
            scaleAuxShadow(args.sampleTime, i, channels);
    }
    for (int c = 0; c < channels; c++) {
        if (ringMorph) {
            for (int i = 0; i < 4; i++) {
                if (inputs[OPERATOR_INPUTS + i].isConnected()) {
                    in[c] = inputs[OPERATOR_INPUTS + i].getPolyVoltage(c) * params[AUX_KNOBS + AuxKnobModes::OP_GAIN].getValue();
                    if (!auxModeFlags[AuxInputModes::SHADOW + i]) {
                        //Check current algorithm and morph target
                        if (modeB) {
                            modOut[i][c] += routeHorizontal(args.sampleTime, in[c], i, c);
                            modOut[i][c] += routeHorizontalRing(args.sampleTime, in[c], i, c);
                            for (int j = 0; j < 3; j++) {
                                modOut[i][c] += routeDiagonalB(args.sampleTime, in[c], i, j, c);
                                modOut[i][c] += routeDiagonalRingB(args.sampleTime, in[c], i, j, c);
                            }
                            sumOut[c] += routeSumRingB(args.sampleTime, in[c], i, c);
                            sumOut[c] += routeSumB(args.sampleTime, in[c], i, c);
                        }
                        else {
                            for (int j = 0; j < 3; j++) {
                                modOut[threeToFour[i][j]][c] += routeDiagonal(args.sampleTime, in[c], i, j, c);
                                modOut[threeToFour[i][j]][c] += routeDiagonalRing(args.sampleTime, in[c], i, j, c);
                            }
                            sumOut[c] += routeSumRing(args.sampleTime, in[c], i, c);
                            sumOut[c] += routeSum(args.sampleTime, in[c], i, c);
                        }
                    }
                    else {
                        //Check current algorithm and morph target
                        if (modeB) {
                            modOut[i][c] += routeHorizontalRing(args.sampleTime, in[c], i, c);
                            modOut[i][c] += routeHorizontal(args.sampleTime, in[c], i, c);
                            for (int j = 0; j < 3; j++) {
                                modOut[threeToFour[i][j]][c] += routeDiagonalRingB(args.sampleTime, in[c], i, j, c);
                                modOut[threeToFour[i][j]][c] += routeDiagonalB(args.sampleTime, in[c], i, j, c);
                            }
                            sumOut[c] += routeSumRingB(args.sampleTime, in[c], i, c);
                            sumOut[c] += routeSumB(args.sampleTime, in[c], i, c);
                        }
                        else {
                            for (int j = 0; j < 3; j++) {
                                modOut[threeToFour[i][j]][c] += routeDiagonalRing(args.sampleTime, in[c], i, j, c);
                                modOut[threeToFour[i][j]][c] += routeDiagonal(args.sampleTime, in[c], i, j, c);
                            }
                            sumOut[c] += routeSumRing(args.sampleTime, in[c], i, c);
                            sumOut[c] += routeSum(args.sampleTime, in[c], i, c);
                        }
                    }
                }
                modOut[i][c] *=  modAttenuversion[c] * modGain * runClickFilterGain;
                sumOut[c] *= sumAttenuversion[c] * sumGain * runClickFilterGain;
            }
        }
        else {
            for (int i = 0; i < 4; i++) {
                if (inputs[OPERATOR_INPUTS + i].isConnected()) {
                    in[c] = inputs[OPERATOR_INPUTS + i].getPolyVoltage(c) * params[AUX_KNOBS + AuxKnobModes::OP_GAIN].getValue();
                    if (!auxModeFlags[AuxInputModes::SHADOW + i]) {
                        //Check current algorithm and morph target
                        if (modeB) {
                            modOut[i][c] += routeHorizontal(args.sampleTime, in[c], i, c);
                            for (int j = 0; j < 3; j++) {
                                modOut[threeToFour[i][j]][c] += routeDiagonalB(args.sampleTime, in[c], i, j, c);
                            }
                            sumOut[c] += routeSumB(args.sampleTime, in[c], i, c);
                        }
                        else {
                            for (int j = 0; j < 3; j++) {
                                modOut[threeToFour[i][j]][c] += routeDiagonal(args.sampleTime, in[c], i, j, c);
                            }
                            sumOut[c] += routeSum(args.sampleTime, in[c], i, c);
                        }
                    }
                    else {
                        //Check current algorithm and morph target
                        if (modeB) {
                            modOut[i][c] += routeHorizontal(args.sampleTime, in[c], i, c);
                            for (int j = 0; j < 3; j++) {
                                modOut[threeToFour[i][j]][c] += routeDiagonalB(args.sampleTime, in[c], i, j, c);
                            }
                            sumOut[c] += routeSumB(args.sampleTime, in[c], i, c);
                        }
                        else {
                            for (int j = 0; j < 3; j++) {
                                modOut[threeToFour[i][j]][c] += routeDiagonal(args.sampleTime, in[c], i, j, c);
                            }
                            sumOut[c] += routeSum(args.sampleTime, in[c], i, c);
                        }
                    }
                }
            }
        }
        if (auxModeFlags[AuxInputModes::WILDCARD_MOD]) {
            for (int auxIndex = 0; auxIndex < 4; auxIndex++) {
                auxInput[auxIndex]->wildcardModClickGain = (clickFilterEnabled ? auxInput[auxIndex]->wildcardModClickFilter[c].process(args.sampleTime, auxInput[auxIndex]->mode[AuxInputModes::WILDCARD_MOD]) : auxInput[auxIndex]->mode[AuxInputModes::WILDCARD_MOD]);
                wildcardMod[c] += auxInput[auxIndex]->voltage[AuxInputModes::WILDCARD_MOD][c] * auxInput[auxIndex]->wildcardModClickGain;
            }
            for (int mod = 0; mod < 4; mod++) {
                modOut[mod][c] += wildcardMod[c];
            }
        }
        if (auxModeFlags[AuxInputModes::WILDCARD_SUM]) {
            for (int auxIndex = 0; auxIndex < 4; auxIndex++) {
                auxInput[auxIndex]->wildcardSumClickGain = (clickFilterEnabled ? auxInput[auxIndex]->wildcardSumClickFilter[c].process(args.sampleTime, auxInput[auxIndex]->mode[AuxInputModes::WILDCARD_SUM]) : auxInput[auxIndex]->mode[AuxInputModes::WILDCARD_SUM]);
                wildcardSum[c] += auxInput[auxIndex]->voltage[AuxInputModes::WILDCARD_SUM][c] * auxInput[auxIndex]->wildcardSumClickGain;
            }
            sumOut[c] += wildcardSum[c];
        }
        for (int mod = 0; mod < 4; mod++)
            modOut[mod][c] *= modAttenuversion[c] * modGain * runClickFilterGain;
        sumOut[c] *= sumAttenuversion[c] * sumGain * runClickFilterGain;
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

inline float Algomorph4::routeHorizontal(float sampleTime, float inputVoltage, int op, int c) {
    float connectionA = horizontalDestinations[centerMorphScene[c]][op] * (1.f - relativeMorphMagnitude[c]);
    float connectionB = horizontalDestinations[forwardMorphScene[c]][op] * (relativeMorphMagnitude[c]);
    float morphedConnection = connectionA + connectionB;
    modClickGain[op][op][c] = clickFilterEnabled ? modClickFilters[op][op][c].process(sampleTime, morphedConnection) : morphedConnection;
    return (inputVoltage + scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * modClickGain[op][op][c];
}

inline float Algomorph4::routeHorizontalRing(float sampleTime, float inputVoltage, int op, int c) {
    float connection = horizontalDestinations[backwardMorphScene[c]][op] * relativeMorphMagnitude[c];
    modRingClickGain[op][op][c] = clickFilterEnabled ? modRingClickFilters[op][op][c].process(sampleTime, connection) : connection;
    return (-inputVoltage - scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * modRingClickGain[op][op][c];
}

inline float Algomorph4::routeDiagonal(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connectionA = opDestinations[centerMorphScene[c]][op][mod]   * (1.f - relativeMorphMagnitude[c])  * !horizontalDestinations[centerMorphScene[c]][op];
    float connectionB = opDestinations[forwardMorphScene[c]][op][mod]  * relativeMorphMagnitude[c]          * !horizontalDestinations[forwardMorphScene[c]][op];
    float morphedConnection = connectionA + connectionB;
    modClickGain[op][threeToFour[op][mod]][c] = clickFilterEnabled ? modClickFilters[op][threeToFour[op][mod]][c].process(sampleTime, morphedConnection) : morphedConnection;
    return (inputVoltage + scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * modClickGain[op][threeToFour[op][mod]][c];
}

inline float Algomorph4::routeDiagonalB(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connectionA = opDestinations[centerMorphScene[c]][op][mod]   * (1.f - relativeMorphMagnitude[c]);
    float connectionB = opDestinations[forwardMorphScene[c]][op][mod]  * relativeMorphMagnitude[c];
    float morphedConnection = connectionA + connectionB;
    modClickGain[op][threeToFour[op][mod]][c] = clickFilterEnabled ? modClickFilters[op][threeToFour[op][mod]][c].process(sampleTime, morphedConnection) : morphedConnection;
    return (inputVoltage + scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * modClickGain[op][threeToFour[op][mod]][c];
}

inline float Algomorph4::routeDiagonalRing(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connection = opDestinations[backwardMorphScene[c]][op][mod] * relativeMorphMagnitude[c] * !horizontalDestinations[backwardMorphScene[c]][op];
    modRingClickGain[op][threeToFour[op][mod]][c] = clickFilterEnabled ? modRingClickFilters[op][threeToFour[op][mod]][c].process(sampleTime, connection) : connection; 
    return (-inputVoltage + scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * modRingClickGain[op][threeToFour[op][mod]][c];
}

inline float Algomorph4::routeDiagonalRingB(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connection = opDestinations[backwardMorphScene[c]][op][mod] * relativeMorphMagnitude[c];
    modRingClickGain[op][threeToFour[op][mod]][c] = clickFilterEnabled ? modRingClickFilters[op][threeToFour[op][mod]][c].process(sampleTime, connection) : connection; 
    return (-inputVoltage + scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * modRingClickGain[op][threeToFour[op][mod]][c];
}

inline float Algomorph4::routeSum(float sampleTime, float inputVoltage, int op, int c) {
    float connection    =   carrier[centerMorphScene[c]][op]     * (1.f - relativeMorphMagnitude[c])  * !horizontalDestinations[centerMorphScene[c]][op]
                        +   carrier[forwardMorphScene[c]][op]    * relativeMorphMagnitude[c]          * !horizontalDestinations[forwardMorphScene[c]][op];
    sumClickGain[op][c] = clickFilterEnabled ? sumClickFilters[op][c].process(sampleTime, connection) : connection;
    return (inputVoltage + scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * sumClickGain[op][c];
}

inline float Algomorph4::routeSumB(float sampleTime, float inputVoltage, int op, int c) {
    float connection    =   carrier[centerMorphScene[c]][op]     * (1.f - relativeMorphMagnitude[c])
                        +   carrier[forwardMorphScene[c]][op]    * relativeMorphMagnitude[c];
    sumClickGain[op][c] = clickFilterEnabled ? sumClickFilters[op][c].process(sampleTime, connection) : connection;
    return (inputVoltage + scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * sumClickGain[op][c];
}

inline float Algomorph4::routeSumRing(float sampleTime, float inputVoltage, int op, int c) {
    float connection = carrier[backwardMorphScene[c]][op] * relativeMorphMagnitude[c] * !horizontalDestinations[backwardMorphScene[c]][op];
    sumRingClickGain[op][c] = clickFilterEnabled ? sumRingClickFilters[op][c].process(sampleTime, connection) : connection;
    return (-inputVoltage - scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * sumRingClickGain[op][c];
}

inline float Algomorph4::routeSumRingB(float sampleTime, float inputVoltage, int op, int c) {
    float connection = carrier[backwardMorphScene[c]][op] * relativeMorphMagnitude[c];
    sumRingClickGain[op][c] = clickFilterEnabled ? sumRingClickFilters[op][c].process(sampleTime, connection) : connection;
    return (-inputVoltage - scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * sumRingClickGain[op][c];
}

inline void Algomorph4::scaleAuxSumAttenCV(int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::SUM_ATTEN][c] = 1.f;
        for (int auxIndex = 0; auxIndex < 4; auxIndex++)
            scaledAuxVoltage[AuxInputModes::SUM_ATTEN][c] *= clamp(auxInput[auxIndex]->voltage[AuxInputModes::SUM_ATTEN][c] / 5.f, -1.f, 1.f);
    }
}

inline void Algomorph4::scaleAuxModAttenCV(int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] = 1.f;
        for (int auxIndex = 0; auxIndex < 4; auxIndex++)
            scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] *= clamp(auxInput[auxIndex]->voltage[AuxInputModes::MOD_ATTEN][c] / 5.f, -1.f, 1.f);
    }    
}

inline void Algomorph4::scaleAuxClickFilterCV(int channels) {
    for (int c = 0; c < channels; c++) {
        //+/-5V = 0V-2V
        scaledAuxVoltage[AuxInputModes::CLICK_FILTER][c] = 1.f;
        for (int auxIndex = 0; auxIndex < 4; auxIndex++)
            scaledAuxVoltage[AuxInputModes::CLICK_FILTER][c] *= (clamp(auxInput[auxIndex]->voltage[AuxInputModes::CLICK_FILTER][c] / 5.f, -1.f, 1.f) + 1.001f);
    }    
}

inline void Algomorph4::scaleAuxMorphCV(int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::MORPH][c] = 0.f;
        for (int auxIndex = 0; auxIndex < 4; auxIndex++)
            scaledAuxVoltage[AuxInputModes::MORPH][c] += auxInput[auxIndex]->voltage[AuxInputModes::MORPH][c] / 5.f;
    }
}

inline void Algomorph4::scaleAuxDoubleMorphCV(int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::DOUBLE_MORPH][c] = 0.f;
        for (int auxIndex = 0; auxIndex < 4; auxIndex++)
            scaledAuxVoltage[AuxInputModes::DOUBLE_MORPH][c] += auxInput[auxIndex]->voltage[AuxInputModes::DOUBLE_MORPH][c] / FIVE_D_TWO;
    }
}

inline void Algomorph4::scaleAuxTripleMorphCV(int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::TRIPLE_MORPH][c] = 0.f;
        for (int auxIndex = 0; auxIndex < 4; auxIndex++)
            scaledAuxVoltage[AuxInputModes::TRIPLE_MORPH][c] += auxInput[auxIndex]->voltage[AuxInputModes::TRIPLE_MORPH][c] / FIVE_D_THREE;
    }
}

inline void Algomorph4::scaleAuxMorphAttenCV(int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::MORPH_ATTEN][c] = 1.f;
        for (int auxIndex = 0; auxIndex < 4; auxIndex++)
            scaledAuxVoltage[AuxInputModes::MORPH_ATTEN][c] *= auxInput[auxIndex]->voltage[AuxInputModes::MORPH_ATTEN][c] / 5.f;
    }
}

inline void Algomorph4::scaleAuxShadow(float sampleTime, int op, int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::SHADOW + op][c] = 0.f;
        for (int auxIndex = 0; auxIndex < 4; auxIndex++) {
            float gain = clickFilterEnabled ? auxInput[auxIndex]->shadowClickFilter[op].process(sampleTime, auxInput[auxIndex]->mode[AuxInputModes::SHADOW + op] * auxInput[auxIndex]->connected) : auxInput[auxIndex]->mode[AuxInputModes::SHADOW + op] * auxInput[auxIndex]->connected;
            scaledAuxVoltage[AuxInputModes::SHADOW + op][c] += gain * auxInput[auxIndex]->voltage[AuxInputModes::SHADOW + op][c];
        }
    }
}

void Algomorph4::rescaleVoltage(int mode, int channels) {
    switch (mode) {
        case AuxInputModes::CLICK_FILTER:
            scaleAuxClickFilterCV(channels);
            break;
        case AuxInputModes::SUM_ATTEN:
            scaleAuxSumAttenCV(channels);
            break;
        case AuxInputModes::MOD_ATTEN:
            scaleAuxModAttenCV(channels);
            break;
        case AuxInputModes::MORPH_ATTEN:
            scaleAuxMorphAttenCV(channels);
            break;
        case AuxInputModes::MORPH:
            scaleAuxMorphCV(channels);
            break;
        case AuxInputModes::DOUBLE_MORPH:
            scaleAuxDoubleMorphCV(channels);
            break;
        case AuxInputModes::TRIPLE_MORPH:
            scaleAuxTripleMorphCV(channels);
            break;
        default:
            for (int c = 0; c < channels; c++) {
                scaledAuxVoltage[mode][c] = 0.f;
                for (int auxIndex = 0; auxIndex < 4; auxIndex++)
                    scaledAuxVoltage[mode][c] += auxInput[auxIndex]->voltage[mode][c];
            }
            break;
    }
}

void Algomorph4::rescaleVoltages(int channels) {
    for (int i = 0; i < AuxInputModes::NUM_MODES; i++)
        rescaleVoltage(i, channels);
}

void Algomorph4::initRun() {
    clockIgnoreOnReset = (long) (CLOCK_IGNORE_DURATION * APP->engine->getSampleRate());
    baseScene = resetScene;
}

void Algomorph4::toggleHorizontalDestination(int scene, int op) {
    horizontalDestinations[scene][op] ^= true;
    if (!modeB) {
        toggleDisabled(scene, op);
        if (horizontalDestinations[scene][op])
            carrier[scene][op] = false;
        else
            carrier[scene][op] = isCarrier(scene, op);
    }
    else {
        bool mismatch = opDisabled[scene][op] ^ isDisabled(scene, op);
        if (mismatch)
            toggleDisabled(scene, op);
    }
}

void Algomorph4::toggleDiagonalDestination(int scene, int op, int mod) {
    opDestinations[scene][op][mod] ^= true;
    algoName[scene].flip(op * 3 + mod);
    if (!modeB) {
        if (opDestinations[scene][op][mod])
            carrier[scene][op] = false;
        else
            carrier[scene][op] = isCarrier(scene, op);
    }
    else {   
        bool mismatch = opDisabled[scene][op] ^ isDisabled(scene, op);
        if (mismatch)
            toggleDisabled(scene, op);
    }
    if (!opDisabled[scene][op]) {
        displayAlgoName[scene].set(op * 3 + mod, algoName[scene][op * 3 + mod]);
        // If the mod output in question corresponds to a disabled operator
        if (opDisabled[scene][threeToFour[op][mod]]) {
            // If a connection has been made, enable that operator visually
            if (opDestinations[scene][op][mod]) {
                displayAlgoName[scene].set(12 + threeToFour[op][mod], false);
            }
            // If a connection has been broken, 
            else {
                bool disabled = true;
                for (int j = 0; j < 4; j++) {
                    if (j != op && j != threeToFour[op][mod] && opDestinations[scene][j][fourToThree[j][op]])
                        disabled = false;
                }
                if (disabled)
                    displayAlgoName[scene].set(12 + threeToFour[op][mod], true);
                else
                    displayAlgoName[scene].set(12 + threeToFour[op][mod], false);
            }
        }
    }
}

bool Algomorph4::isCarrier(int scene, int op) {
    bool isCarrier = true;
    if (modeB)
        isCarrier = false;
    else {
        for (int mod = 0; mod < 3; mod++) {
            if (opDestinations[scene][op][mod])
                isCarrier = false;
        }
        if (horizontalDestinations[scene][op])
            isCarrier = false;
    }
    isCarrier |= forcedCarrier[scene][op];
    return isCarrier;
}

bool Algomorph4::isDisabled(int scene, int op) {
    if (!modeB) {
        if (horizontalDestinations[scene][op])
            return true;
        else
            return false;
    }
    else {
        if (forcedCarrier[scene][op])
            return false;
        else {
            if (horizontalDestinations[scene][op])
                return false;
            else {
                for (int mod = 0; mod < 3; mod++) {
                    if (opDestinations[scene][op][mod]) {
                        return false;
                    }
                }
                return true;
            }
        }
    }
}

void Algomorph4::toggleDisabled(int scene, int op) {
    algoName[scene].flip(12 + op);
    displayAlgoName[scene].set(12 + op, algoName[scene][12 + op]);
    opDisabled[scene][op] ^= true;
    updateDisplayAlgo(scene);
}

void Algomorph4::updateDisplayAlgo(int scene) {
    // Set display algorithm
    for (int op = 0; op < 4; op++) {
        if (opDisabled[scene][op]) {
            // Set all destinations to false
            for (int mod = 0; mod < 3; mod++)
                displayAlgoName[scene].set(op * 3 + mod, false);
            // Check if any operators are modulating this operator
            bool fullDisable = true;
            for (int i = 0; i < 4; i++) {     
                if (!opDisabled[scene][i] && opDestinations[scene][i][fourToThree[i][op]])
                    fullDisable = false;
            }
            if (fullDisable) {
                displayAlgoName[scene].set(12 + op, true);
            }
            else
                displayAlgoName[scene].set(12 + op, false);
        }
        else {
            // Enable destinations in the display and handle the consequences
            for (int i = 0; i < 3; i++) {
                if (opDestinations[scene][op][i]) {
                    displayAlgoName[scene].set(op * 3 + i, true);
                    // the consequences
                    if (opDisabled[scene][threeToFour[op][i]])
                        displayAlgoName[scene].set(12 + threeToFour[op][i], false);
                }
            }  
        }
    }
}

void Algomorph4::toggleModeB() {
    modeB ^= true;
    if (modeB) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                carrier[i][j] = forcedCarrier[i][j];
                bool mismatch = opDisabled[i][j] ^ isDisabled(i, j);
                if (mismatch)
                    toggleDisabled(i, j);
            }
        }
    }
    else {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                bool mismatch = opDisabled[i][j] ^ isDisabled(i, j);
                if (mismatch)
                    toggleDisabled(i, j);
            }
            // Check carrier status after above loop is complete
            for (int op = 0; op < 4; op++)
                carrier[i][op] = isCarrier(i, op);
        }
    }
}

void Algomorph4::toggleForcedCarrier(int scene, int op) {
    forcedCarrier[scene][op] ^= true;
    if (forcedCarrier[scene][op])
        carrier[scene][op] = true;
    else
        carrier[scene][op] = isCarrier(scene, op);
    if (modeB) {
        bool mismatch = opDisabled[scene][op] ^ isDisabled(scene, op);
        if (mismatch)
            toggleDisabled(scene, op);
    }
}

inline float Algomorph4::getPortBrightness(Port port) {
    if (vuLights)
        return std::max(    {   port.plugLights[0].getBrightness(),
                                port.plugLights[1].getBrightness() * 4,
                                port.plugLights[2].getBrightness()          }   );
    else
        return 1.f;
}

void Algomorph4::updateSceneBrightnesses() {
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

void Algomorph4::swapAlgorithms(int a, int b) {
    bool swap[4][3] = {false};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            swap[i][j] = opDestinations[a][i][j];
            opDestinations[a][i][j] = opDestinations[b][i][j];
            opDestinations[b][i][j] = swap[i][j];
        }
    }
}

json_t* Algomorph4::dataToJson() {
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
    

    json_t* lastSetModesJ = json_array();
    for (int i = 0; i < 4; i++) {
        json_t* lastModeJ = json_object();
        json_object_set_new(lastModeJ, "Last Set Mode", json_integer(auxInput[i]->lastSetMode));
        json_array_append_new(lastSetModesJ, lastModeJ);
    }
    json_object_set_new(rootJ, "Last Set Aux Input Modes", lastSetModesJ);

    json_t* allowMultipleModesJ = json_array();
    for (int i = 0; i < 4; i++) {
        json_t* allowanceJ = json_object();
        json_object_set_new(allowanceJ, "Allowance", json_boolean(auxInput[i]->allowMultipleModes));
        json_array_append_new(allowMultipleModesJ, allowanceJ);
    }
    json_object_set_new(rootJ, "Allow Multiple Modes", allowMultipleModesJ);

    json_t* auxConnectionsJ = json_array();
    for (int i = 0; i < 4; i++) {
        json_t* auxConnectedJ = json_object();
        json_object_set_new(auxConnectedJ, "Aux Connection", json_boolean(auxInput[i]->connected));
        json_array_append_new(auxConnectionsJ, auxConnectedJ);
    }
    json_object_set_new(rootJ, "Algorithm Names", auxConnectionsJ);

    json_object_set_new(rootJ, "Aux Knob Mode", json_integer(knobMode));
    
    json_t* opInputModesJ = json_array();
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < AuxInputModes::NUM_MODES; j++) {
            json_t* inputModeJ = json_object();
            json_object_set_new(inputModeJ, "Mode", json_boolean(auxInput[i]->mode[j]));
            json_array_append_new(opInputModesJ, inputModeJ);
        }
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

    json_t* horizontalConnectionsJ = json_array();
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            json_t* connectionJ = json_object();
            json_object_set_new(connectionJ, "Horizontal Connection", json_boolean(!horizontalDestinations[i][j]));
            json_array_append_new(horizontalConnectionsJ, connectionJ);
        }
    }
    json_object_set_new(rootJ, "Horizontal Connections", horizontalConnectionsJ);

    json_t* opEnabledJ = json_array();
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            json_t* opJ = json_object();
            json_object_set_new(opJ, "Enabled Op", json_boolean(!opDisabled[i][j]));
            json_array_append_new(opEnabledJ, opJ);
        }
    }
    json_object_set_new(rootJ, "Operators Enabled", opEnabledJ);

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

void Algomorph4::dataFromJson(json_t* rootJ) {
    // Algomorph4Widget* mw = dynamic_cast<Algomorph4Widget*>(APP->scene->rack->getModule(id));
    // if (json_boolean_value(json_object_get(rootJ, "Glowing Ink")))
    //     mw->toggleInkVisibility();
    // knobMode = json_integer_value(json_object_get(rootJ, "Aux Knob Mode"));
    // mw->setKnobMode(knobMode);

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

    //Set allowMultipleModes before loading modes
    json_t* allowMultipleModesJ = json_object_get(rootJ, "Allow Multiple Modes");
    json_t* allowanceJ; size_t allowIndex;
    json_array_foreach(allowMultipleModesJ, allowIndex, allowanceJ) {
        auxInput[allowIndex]->allowMultipleModes = json_boolean_value(json_object_get(allowanceJ, "Allowance"));
    }

    json_t* opInputModesJ = json_object_get(rootJ, "Aux Input Modes");
    json_t* inputModeJ; size_t inputModeIndex;
    int i = 0; int j = 0;
    json_array_foreach(opInputModesJ, inputModeIndex, inputModeJ) {
        if (json_boolean_value(json_object_get(inputModeJ, "Mode")))
            auxInput[i]->setMode(j);
        j++;
        if (j >= AuxInputModes::NUM_MODES) {
            j = 0;
            i++;
        } 
    }

    json_t* auxConnectionsJ = json_object_get(rootJ, "Aux Input Connections");
    json_t* auxConnectedJ; size_t auxConnectionIndexJ;
    json_array_foreach(auxConnectionsJ, auxConnectionIndexJ, auxConnectedJ) {
        auxInput[auxConnectionIndexJ]->connected = json_boolean_value(json_object_get(auxConnectedJ, "Aux Connection"));
    }
    json_t* lastSetModesJ = json_object_get(rootJ, "Last Set Aux Input Modes");
    json_t* lastModeJ; size_t lastModeIndex;
    json_array_foreach(lastSetModesJ, lastModeIndex, lastModeJ) {
        auxInput[lastModeIndex]->lastSetMode = json_integer_value(json_object_get(lastModeJ, "Last Set Mode"));
    }

    json_t* opDestinationsJ = json_object_get(rootJ, "Operator Destinations");
    json_t* destinationJ; size_t destinationIndex;
    i = 0, j = 0;
    int k = 0;
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

    for (int scene = 0; scene < 3; scene++)
        updateDisplayAlgo(scene);

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

    json_t* horizontalConnectionsJ = json_object_get(rootJ, "Horizontal Connections");
    json_t* connectionJ;
    size_t horizontalConnectionIndex;
    i = j = 0;
    json_array_foreach(horizontalConnectionsJ, horizontalConnectionIndex, connectionJ) {
        horizontalDestinations[i][j] = !json_boolean_value(json_object_get(connectionJ, "Horizontal Connection"));
        j++;
        if (j > 3) {
            j = 0;
            i++;
        }
    }

    json_t* opEnabledJ = json_object_get(rootJ, "Operators Enabled");
    json_t* enabledJ;
    size_t opEnabledIndex;
    i = j = 0;
    json_array_foreach(opEnabledJ, opEnabledIndex, enabledJ) {
        opDisabled[i][j] = !json_boolean_value(json_object_get(enabledJ, "Enabled Op"));
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

    //Legacy inputs
    bool fix = false;
    for (int auxIndex = 0; auxIndex < 4; auxIndex++) {
        if (auxInput[auxIndex]->activeModes == -1) {
            fix = true;
            auxInput[auxIndex]->activeModes = 1;
        }
    }
    if (fix) {
        knobMode = 1;
        params[AUX_KNOBS + AuxKnobModes::MORPH_ATTEN].setValue(1.f);

        auxInput[2]->setMode(AuxInputModes::RESET);
        auxInput[1]->setMode(AuxInputModes::CLOCK);
        auxInput[3]->setMode(AuxInputModes::MORPH);
        auxInput[0]->setMode(AuxInputModes::MORPH);
        rescaleVoltages(16);
    }

    // Update carriers
    for (int scene = 0; scene < 3; scene++) {
        for (int op = 0; op < 4; op++) {
            carrier[scene][op] = isCarrier(scene, op);
        }
    }

    graphDirty = true;
}

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
struct DisallowMultipleModesAction : history::ModuleAction {
    int auxIndex, channels;
	bool multipleActive = false, enabled[AuxInputModes::NUM_MODES] = {false};

	DisallowMultipleModesAction() {
		name = "Delexander Algomorph disallow multiple modes";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		
		m->auxInput[auxIndex]->allowMultipleModes = true;
		if (multipleActive) {
			for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
				if (enabled[i])
					m->auxInput[auxIndex]->setMode(i);
                    m->rescaleVoltage(i, channels);
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
					m->auxInput[auxIndex]->unsetMode(i);
                    for (int c = 0; c < channels; c++)
                        m->auxInput[auxIndex]->voltage[i][c] = m->auxInput[auxIndex]->defVoltage[i];
                    m->rescaleVoltage(i, channels);
				}
			}
		}
		m->auxInput[auxIndex]->allowMultipleModes = false;
	}
};

struct AuxModeItem : MenuItem {
    Algomorph4 *module;
    int auxIndex, mode;

    void onAction(const event::Action &e) override {
        if (module->auxInput[auxIndex]->mode[mode]) {
            // History
            AuxInputUnsetAction<Algomorph4>* h = new AuxInputUnsetAction<Algomorph4>;
            h->moduleId = module->id;
            h->auxIndex = auxIndex;
            h->mode = mode;
            h->channels = module->auxInput[auxIndex]->channels;
            
            module->auxInput[auxIndex]->unsetMode(mode);
            for (int c = 0; c < h->channels; c++)
                module->auxInput[auxIndex]->voltage[mode][c] = module->auxInput[auxIndex]->defVoltage[mode];
            module->rescaleVoltage(mode, h->channels);

            APP->history->push(h);
        }
        else {
            if (module->auxInput[auxIndex]->allowMultipleModes) {
                // History
                AuxInputSetAction<Algomorph4>* h = new AuxInputSetAction<Algomorph4>;
                h->moduleId = module->id;
                h->auxIndex = auxIndex;
                h->mode = mode;
                h->channels = module->auxInput[auxIndex]->channels;

                module->auxInput[auxIndex]->setMode(mode);

                APP->history->push(h);
            }
            else {
                // History
                AuxInputSwitchAction<Algomorph4>* h = new AuxInputSwitchAction<Algomorph4>;
                h->moduleId = module->id;
                h->auxIndex = auxIndex;
                h->oldMode = module->auxInput[auxIndex]->lastSetMode;
                h->newMode = mode;
                h->channels = module->auxInput[auxIndex]->channels;

                module->auxInput[auxIndex]->unsetMode(h->oldMode);
                for (int c = 0; c < h->channels; c++)
                    module->auxInput[auxIndex]->voltage[h->oldMode][c] = module->auxInput[auxIndex]->defVoltage[h->oldMode];
                module->rescaleVoltage(h->oldMode, h->channels);
                module->auxInput[auxIndex]->setMode(mode);

                APP->history->push(h);
            }
        }
    }
};

template < typename MODULE >
void createWildcardInputMenu(MODULE* module, ui::Menu* menu, int auxIndex) {
    for (int i = AuxInputModes::WILDCARD_MOD; i <= AuxInputModes::WILDCARD_SUM; i++)
        menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[i], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[i]), &AuxModeItem::mode, i));
}

template < typename MODULE >
struct WildcardInputMenuItem : MenuItem {
	MODULE* module;
    int auxIndex;
	
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		createWildcardInputMenu(module, menu, auxIndex);
		return menu;
	}
};

template < typename MODULE >
void createShadowInputMenu(MODULE* module, ui::Menu* menu, int auxIndex) {
    for (int i = AuxInputModes::SHADOW; i <= AuxInputModes::SHADOW + 3; i++)
        menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[i], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[i]), &AuxModeItem::mode, i));
}

template < typename MODULE >
struct ShadowInputMenuItem : MenuItem {
	MODULE* module;
    int auxIndex;
	
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		createShadowInputMenu(module, menu, auxIndex);
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

struct AllowMultipleModesItem : MenuItem {
    Algomorph4 *module;
    int auxIndex;
    void onAction(const event::Action &e) override {
        if (module->auxInput[auxIndex]->allowMultipleModes) {
            // History
            DisallowMultipleModesAction<Algomorph4>* h = new DisallowMultipleModesAction<Algomorph4>;
            h->moduleId = module->id;
            h->auxIndex = auxIndex;
            h->channels = module->auxInput[auxIndex]->channels;

            if (module->auxInput[auxIndex]->activeModes > 1) {
                h->multipleActive = true;
                for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
                    if (module->auxInput[auxIndex]->mode[i] && i != module->auxInput[auxIndex]->lastSetMode) {
                        h->enabled[i] = true;
                        module->auxInput[auxIndex]->unsetMode(i);
                        for (int c = 0; c < h->channels; c++)
                            module->auxInput[auxIndex]->voltage[i][c] = module->auxInput[auxIndex]->defVoltage[i];
                        module->rescaleVoltage(i, h->channels);
                    }
                }
            }
            module->auxInput[auxIndex]->allowMultipleModes = false;

            APP->history->push(h);
        }
        else {
            // History
            AllowMultipleModesAction<Algomorph4>* h = new AllowMultipleModesAction<Algomorph4>;
            h->moduleId = module->id;
            h->auxIndex = auxIndex;

            module->auxInput[auxIndex]->allowMultipleModes = true;

            APP->history->push(h);
        }

    }
};


template < typename MODULE >
void createAuxInputModeMenu(MODULE* module, ui::Menu* menu, int auxIndex) {
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Audio Input"));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::WILDCARD_MOD], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[AuxInputModes::WILDCARD_MOD]), &AuxModeItem::mode, AuxInputModes::WILDCARD_MOD));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::WILDCARD_SUM], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[AuxInputModes::WILDCARD_SUM]), &AuxModeItem::mode, AuxInputModes::WILDCARD_SUM));
    for (int i = AuxInputModes::SHADOW; i <= AuxInputModes::SHADOW + 3; i++)
        menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[i], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[i]), &AuxModeItem::mode, i));

    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Trigger Input"));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::CLOCK], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[AuxInputModes::CLOCK]), &AuxModeItem::mode, AuxInputModes::CLOCK));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::REVERSE_CLOCK], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[AuxInputModes::REVERSE_CLOCK]), &AuxModeItem::mode, AuxInputModes::REVERSE_CLOCK));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::RESET], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[AuxInputModes::RESET]), &AuxModeItem::mode, AuxInputModes::RESET));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::RUN], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[AuxInputModes::RUN]), &AuxModeItem::mode, AuxInputModes::RUN));
   
    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "CV Input"));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::MORPH], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[AuxInputModes::MORPH]), &AuxModeItem::mode, AuxInputModes::MORPH));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::DOUBLE_MORPH], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[AuxInputModes::DOUBLE_MORPH]), &AuxModeItem::mode, AuxInputModes::DOUBLE_MORPH));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::TRIPLE_MORPH], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[AuxInputModes::TRIPLE_MORPH]), &AuxModeItem::mode, AuxInputModes::TRIPLE_MORPH));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::SUM_ATTEN], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[AuxInputModes::SUM_ATTEN]), &AuxModeItem::mode, AuxInputModes::SUM_ATTEN));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::MOD_ATTEN], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[AuxInputModes::MOD_ATTEN]), &AuxModeItem::mode, AuxInputModes::MOD_ATTEN));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::MORPH_ATTEN], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[AuxInputModes::MORPH_ATTEN]), &AuxModeItem::mode, AuxInputModes::MORPH_ATTEN));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::SCENE_OFFSET], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[AuxInputModes::SCENE_OFFSET]), &AuxModeItem::mode, AuxInputModes::SCENE_OFFSET));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::CLICK_FILTER], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->mode[AuxInputModes::CLICK_FILTER]), &AuxModeItem::mode, AuxInputModes::CLICK_FILTER));
}

template < typename MODULE >
struct AuxInputModeMenuItem : MenuItem {
	MODULE* module;
    int auxIndex;
	
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		createAuxInputModeMenu(module, menu, auxIndex);
		return menu;
	}
};

template < typename MODULE >
void createAllowMultipleModesMenu(MODULE* module, ui::Menu* menu) {
    menu->addChild(construct<AllowMultipleModesItem>(&MenuItem::text, "δ", &AllowMultipleModesItem::module, module, &AllowMultipleModesItem::auxIndex, 2, &AllowMultipleModesItem::rightText, CHECKMARK(module->auxInput[2]->allowMultipleModes)));
    menu->addChild(construct<AllowMultipleModesItem>(&MenuItem::text, "ζ", &AllowMultipleModesItem::module, module, &AllowMultipleModesItem::auxIndex, 1, &AllowMultipleModesItem::rightText, CHECKMARK(module->auxInput[1]->allowMultipleModes)));
    menu->addChild(construct<AllowMultipleModesItem>(&MenuItem::text, "λ", &AllowMultipleModesItem::module, module, &AllowMultipleModesItem::auxIndex, 3, &AllowMultipleModesItem::rightText, CHECKMARK(module->auxInput[3]->allowMultipleModes)));
    menu->addChild(construct<AllowMultipleModesItem>(&MenuItem::text, "φ", &AllowMultipleModesItem::module, module, &AllowMultipleModesItem::auxIndex, 0, &AllowMultipleModesItem::rightText, CHECKMARK(module->auxInput[0]->allowMultipleModes)));
}

template < typename MODULE >
struct AllowMultipleModesMenuItem : MenuItem {
    MODULE* module;
    
    Menu* createChildMenu() override {
        Menu* menu = new Menu;
        createAllowMultipleModesMenu(module, menu);
        return menu;
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

        module->toggleModeB();

        APP->history->push(h);

        module->graphDirty = true;
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

Algomorph4Widget::Algomorph4Widget(Algomorph4* module) {
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
        screenWidget->box.pos = mm2px(Vec(11.333, 8.518));
        screenWidget->box.size = mm2px(Vec(38.295, 31.590));
        addChild(screenWidget);
        //Place backlight _above_ in scene in order for brightness to affect screenWidget
        addChild(createBacklight<DLXScreenMultiLight>(mm2px(Vec(11.333, 8.518)), mm2px(Vec(38.295, 31.590)), module, Algomorph4::DISPLAY_BACKLIGHT));

        addChild(createRingLightCentered<DLXMultiLight>(mm2px(Vec(30.760, 46.408)), 8.862, module, Algomorph4::SCREEN_BUTTON_RING_LIGHT, .4));
        addParam(createParamCentered<DLXPurpleButton>(mm2px(Vec(30.760, 46.408)), module, Algomorph4::SCREEN_BUTTON));
        addChild(createSvgSwitchLightCentered<DLXScreenButtonLight>(mm2px(Vec(30.760, 46.408)), module, Algomorph4::SCREEN_BUTTON_LIGHT, Algomorph4::SCREEN_BUTTON));

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

        addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 44.094)), module, Algomorph4::AUX_INPUTS + 2));
        addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 54.115)), module, Algomorph4::AUX_INPUTS + 1));
        addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 64.136)), module, Algomorph4::AUX_INPUTS + 3));
        addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 74.157)), module, Algomorph4::AUX_INPUTS + 0));

        DLXKnobLight* kl = createLight<DLXKnobLight>(mm2px(Vec(20.305, 98.044)), module, Algomorph4::KNOB_LIGHT);
        addParam(createLightKnob(mm2px(Vec(20.305, 98.044)), module, Algomorph4::MORPH_KNOB, kl));
        addChild(kl);

        for (int i = 0; i < AuxKnobModes::NUM_MODES; i++) {
            DLXSmallKnobLight* auxKl = createLight<DLXSmallKnobLight>(mm2px(Vec(26.632, 104.370)), module, Algomorph4::AUX_KNOB_LIGHTS + i);
            DLXSmallLightKnob* auxKlParam = createLightKnob<DLXSmallKnobLight, DLXSmallLightKnob>(mm2px(Vec(26.632, 104.370)), module, Algomorph4::AUX_KNOBS + i, auxKl);
            if (i != module->knobMode) {
                auxKlParam->hide();
                auxKlParam->sibling->hide();
            }
            addChild(auxKl);
            addParam(auxKlParam);
        }

        addOutput(createOutput<DLXPortPolyOut>(mm2px(Vec(50.184, 74.269)), module, Algomorph4::SUM_OUTPUT));

        addChild(createRingLightCentered<DLXYellowLight>(mm2px(Vec(30.760, 88.672)), 8.862, module, Algomorph4::EDIT_LIGHT, .4));
        addChild(createParamCentered<DLXPurpleButton>(mm2px(Vec(30.760, 88.672)), module, Algomorph4::EDIT_BUTTON));
        addChild(createSvgSwitchLightCentered<DLXPencilLight>(mm2px(Vec(30.760 + 0.07, 88.672 - 0.297)), module, Algomorph4::EDIT_LIGHT, Algomorph4::EDIT_BUTTON));

        addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 84.203)), module, Algomorph4::OPERATOR_INPUTS + 0));
        addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 94.224)), module, Algomorph4::OPERATOR_INPUTS + 1));
        addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 104.245)), module, Algomorph4::OPERATOR_INPUTS + 2));
        addInput(createInput<DLXPortPoly>(mm2px(Vec(3.778, 114.266)), module, Algomorph4::OPERATOR_INPUTS + 3));

        addOutput(createOutput<DLXPortPolyOut>(mm2px(Vec(50.184, 84.203)), module, Algomorph4::MODULATOR_OUTPUTS + 0));
        addOutput(createOutput<DLXPortPolyOut>(mm2px(Vec(50.184, 94.224)), module, Algomorph4::MODULATOR_OUTPUTS + 1));
        addOutput(createOutput<DLXPortPolyOut>(mm2px(Vec(50.184, 104.245)), module, Algomorph4::MODULATOR_OUTPUTS + 2));
        addOutput(createOutput<DLXPortPolyOut>(mm2px(Vec(50.184, 114.266)), module, Algomorph4::MODULATOR_OUTPUTS + 3));

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

void Algomorph4Widget::appendContextMenu(Menu* menu) {
    Algomorph4* module = dynamic_cast<Algomorph4*>(this->module);

    struct GlowingInkItem : MenuItem {
        Algomorph4 *module;
        void onAction(const event::Action &e) override {
            // History
            ToggleGlowingInkAction<Algomorph4>* h = new ToggleGlowingInkAction<Algomorph4>;
            h->moduleId = module->id;

            module->glowingInk ^= true;

            APP->history->push(h);
        }
    };

    struct SaveVisualSettingsItem : MenuItem {
        Algomorph4 *module;
        void onAction(const event::Action& e) override {
            pluginSettings.glowingInkDefault = module->glowingInk;
            pluginSettings.vuLightsDefault = module->vuLights;
        }
    };
    
    struct KnobModeItem : MenuItem {
        Algomorph4* module;
        int mode;

        void onAction(const event::Action &e) override {
            // History
            KnobModeAction<Algomorph4>* h = new KnobModeAction<Algomorph4>;
            h->moduleId = module->id;
            h->oldKnobMode = module->knobMode;
            h->newKnobMode = mode;

            module->knobMode = mode;

            APP->history->push(h);
        }
    };

    struct KnobModeMenuItem : MenuItem {
        Algomorph4* module;
        Algomorph4Widget* moduleWidget;
        
        Menu* createChildMenu() override {
            Menu* menu = new Menu;
            menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[1], &KnobModeItem::module, module, &KnobModeItem::rightText, CHECKMARK(module->knobMode == 1), &KnobModeItem::mode, 1));
            menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[0], &KnobModeItem::module, module, &KnobModeItem::rightText, CHECKMARK(module->knobMode == 0), &KnobModeItem::mode, 0));
            for (int i = 2; i < AuxKnobModes::NUM_MODES; i++)
                menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[i], &KnobModeItem::module, module, &KnobModeItem::rightText, CHECKMARK(module->knobMode == i), &KnobModeItem::mode, i));
            return menu;
        }
    };


    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "AUX Knob"));

    menu->addChild(construct<KnobModeMenuItem>(&MenuItem::text, "Function…", &MenuItem::rightText, AuxKnobModeLabels[module->knobMode] + " " + RIGHT_ARROW, &KnobModeMenuItem::module, module, &KnobModeMenuItem::moduleWidget, this));

    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "AUX Inputs"));

    menu->addChild(construct<AuxInputModeMenuItem<Algomorph4>>(&MenuItem::text, "δ…", &MenuItem::rightText, (module->auxInput[2]->activeModes > 1 ? "Multiple" : AuxInputModeLabels[module->auxInput[2]->lastSetMode]) + " " + RIGHT_ARROW, &AuxInputModeMenuItem<Algomorph4>::module, module, &AuxInputModeMenuItem<Algomorph4>::auxIndex, 2));
    menu->addChild(construct<AuxInputModeMenuItem<Algomorph4>>(&MenuItem::text, "ζ…", &MenuItem::rightText, (module->auxInput[1]->activeModes > 1 ? "Multiple" : AuxInputModeLabels[module->auxInput[1]->lastSetMode]) + " " + RIGHT_ARROW, &AuxInputModeMenuItem<Algomorph4>::module, module, &AuxInputModeMenuItem<Algomorph4>::auxIndex, 1));
    menu->addChild(construct<AuxInputModeMenuItem<Algomorph4>>(&MenuItem::text, "λ…", &MenuItem::rightText, (module->auxInput[3]->activeModes > 1 ? "Multiple" : AuxInputModeLabels[module->auxInput[3]->lastSetMode]) + " " + RIGHT_ARROW, &AuxInputModeMenuItem<Algomorph4>::module, module, &AuxInputModeMenuItem<Algomorph4>::auxIndex, 3));
    menu->addChild(construct<AuxInputModeMenuItem<Algomorph4>>(&MenuItem::text, "φ…", &MenuItem::rightText, (module->auxInput[0]->activeModes > 1 ? "Multiple" : AuxInputModeLabels[module->auxInput[0]->lastSetMode]) + " " + RIGHT_ARROW, &AuxInputModeMenuItem<Algomorph4>::module, module, &AuxInputModeMenuItem<Algomorph4>::auxIndex, 0));

    menu->addChild(construct<AllowMultipleModesMenuItem<Algomorph4>>(&MenuItem::text, "Multi-function inputs…", &MenuItem::rightText, std::string(module->auxInput[2]->allowMultipleModes ? "δ" : "") + std::string(module->auxInput[1]->allowMultipleModes ? "ζ" : "") + std::string(module->auxInput[3]->allowMultipleModes ? "λ" : "") + std::string(module->auxInput[0]->allowMultipleModes ? "φ" : "") + " " + RIGHT_ARROW, &AllowMultipleModesMenuItem<Algomorph4>::module, module));

    // DebugItem *debugItem = createMenuItem<DebugItem>("The system is down", CHECKMARK(module->debug));
    // debugItem->module = module;
    // menu->addChild(debugItem);
    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Audio"));
    
    menu->addChild(construct<ClickFilterMenuItem<Algomorph4>>(&MenuItem::text, "Click Filter…", &MenuItem::rightText, (module->clickFilterEnabled ? "Enabled ▸" : "Disabled ▸"), &ClickFilterMenuItem<Algomorph4>::module, module));

    RingMorphItem *ringMorphItem = createMenuItem<RingMorphItem>("Enable Ring Morph", CHECKMARK(module->ringMorph));
    ringMorphItem->module = module;
    menu->addChild(ringMorphItem);

    RunSilencerItem *runSilencerItem = createMenuItem<RunSilencerItem>("Route audio when not running", CHECKMARK(!module->runSilencer));
    runSilencerItem->module = module;
    menu->addChild(runSilencerItem);

    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Interaction"));
    
    menu->addChild(construct<ResetSceneMenuItem<Algomorph4>>(&MenuItem::text, "Destination on reset…", &MenuItem::rightText, std::to_string(module->resetScene + 1) + " " + RIGHT_ARROW, &ResetSceneMenuItem<Algomorph4>::module, module));
    
    CCWScenesItem *ccwScenesItem = createMenuItem<CCWScenesItem>("Reverse clock sequence", CHECKMARK(!module->ccwSceneSelection));
    ccwScenesItem->module = module;
    menu->addChild(ccwScenesItem);
            
    ResetOnRunItem *resetOnRunItem = createMenuItem<ResetOnRunItem>("Reset on run", CHECKMARK(module->resetOnRun));
    resetOnRunItem->module = module;
    menu->addChild(resetOnRunItem);

    ExitConfigItem *exitConfigItem = createMenuItem<ExitConfigItem>("Exit Edit Mode after connection", CHECKMARK(module->exitConfigOnConnect));
    exitConfigItem->module = module;
    menu->addChild(exitConfigItem);

    ToggleModeBItem *toggleModeBItem = createMenuItem<ToggleModeBItem>("Alter Ego", CHECKMARK(module->modeB));
    toggleModeBItem->module = module;
    menu->addChild(toggleModeBItem);


    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Visual"));
    
    VULightsItem *vuLightsItem = createMenuItem<VULightsItem>("Disable VU lighting", CHECKMARK(!module->vuLights));
    vuLightsItem->module = module;
    menu->addChild(vuLightsItem);
    
    GlowingInkItem *glowingInkItem = createMenuItem<GlowingInkItem>("Enable glowing panel ink", CHECKMARK(module->glowingInk));
    glowingInkItem->module = module;
    menu->addChild(glowingInkItem);

    menu->addChild(new MenuSeparator());
    
    SaveVisualSettingsItem *saveVisualSettingsItem = createMenuItem<SaveVisualSettingsItem>("Save visual settings as default", CHECKMARK(module->glowingInk == pluginSettings.glowingInkDefault && module->vuLights == pluginSettings.vuLightsDefault));
    saveVisualSettingsItem->module = module;
    menu->addChild(saveVisualSettingsItem);
}

void Algomorph4Widget::setKnobMode(int knobMode) {
    if (!module)
        return;

    Algomorph4* m = dynamic_cast<Algomorph4*>(module);

    DLXSmallLightKnob* oldKnob = dynamic_cast<DLXSmallLightKnob*>(getParam(Algomorph4::AUX_KNOBS + m->knobMode));
    oldKnob->hide();
    oldKnob->sibling->hide();
    DLXSmallLightKnob* newKnob = dynamic_cast<DLXSmallLightKnob*>(getParam(Algomorph4::AUX_KNOBS + knobMode));
    newKnob->show();
    newKnob->sibling->show();
    this->knobMode = knobMode;
}

void Algomorph4Widget::step() {
    if (module) {
        Algomorph4* m = dynamic_cast<Algomorph4*>(module);
        ink->visible = m->glowingInk == 1;
        if (m->knobMode != this->knobMode)
            setKnobMode(m->knobMode);
    }
    ModuleWidget::step();
}

Model* modelAlgomorph4 = createModel<Algomorph4, Algomorph4Widget>("Algomorph4");
