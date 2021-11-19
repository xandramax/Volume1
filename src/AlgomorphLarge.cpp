#include "AlgomorphLarge.hpp"
#include "AlgomorphDisplayWidget.hpp"
#include "AlgomorphAuxInputPanelWidget.hpp"
#include "ConnectionBgWidget.hpp"
#include "Components.hpp"
#include "plugin.hpp" // For constants
#include <rack.hpp>
using rack::math::crossfade;
using rack::event::Action;
using rack::ModuleWidget;
using rack::ui::Menu;
using rack::ui::MenuLabel;
using rack::ui::MenuSeparator;
using rack::construct;
using rack::RACK_GRID_WIDTH;


AlgomorphLarge::AlgomorphLarge() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    configParam(MORPH_KNOB, -1.f, 1.f, 0.f, "Morph", " millimorphs", 0, 1000);
    configParam(AUX_KNOBS + AuxKnobModes::MORPH_ATTEN, -1.f, 1.f, 0.f, AuxKnobModeLabels[AuxKnobModes::MORPH_ATTEN], "%", 0, 100);
    configParam(AUX_KNOBS + AuxKnobModes::DOUBLE_MORPH_ATTEN, -2.f, 2.f, 1.f, AuxKnobModeLabels[AuxKnobModes::MORPH_ATTEN], "%", 0, 100);
    configParam(AUX_KNOBS + AuxKnobModes::TRIPLE_MORPH_ATTEN, -3.f, 3.f, 1.f, AuxKnobModeLabels[AuxKnobModes::MORPH_ATTEN], "%", 0, 100);
    configParam(AUX_KNOBS + AuxKnobModes::MORPH, -1.f, 1.f, 0.f, "AUX " + AuxKnobModeLabels[AuxKnobModes::MORPH], " millimorphs", 0, 1000);
    configParam(AUX_KNOBS + AuxKnobModes::DOUBLE_MORPH, -2.f, 2.f, 0.f, AuxKnobModeLabels[AuxKnobModes::DOUBLE_MORPH], " millimorphs", 0, 1000);
    configParam(AUX_KNOBS + AuxKnobModes::TRIPLE_MORPH, -3.f, 3.f, 0.f, AuxKnobModeLabels[AuxKnobModes::TRIPLE_MORPH], " millimorphs", 0, 1000);
    configParam(AUX_KNOBS + AuxKnobModes::SUM_GAIN, 0.f, 2.f, 1.f, AuxKnobModeLabels[AuxKnobModes::SUM_GAIN], "%", 0, 100);
    configParam(AUX_KNOBS + AuxKnobModes::MOD_GAIN, 0.f, 2.f, 1.f, AuxKnobModeLabels[AuxKnobModes::MOD_GAIN], "%", 0, 100);
    configParam(AUX_KNOBS + AuxKnobModes::OP_GAIN, 0.f, 2.f, 1.f, AuxKnobModeLabels[AuxKnobModes::OP_GAIN], "%", 0, 100);
    configParam(AUX_KNOBS + AuxKnobModes::WILDCARD_MOD_GAIN, 0.f, 2.f, 1.f, AuxKnobModeLabels[AuxKnobModes::WILDCARD_MOD_GAIN], "%", 0, 100);
    configParam(AUX_KNOBS + AuxKnobModes::UNI_MORPH, 0.f, 3.f, 0.f, AuxKnobModeLabels[AuxKnobModes::UNI_MORPH], " millimorphs", 0, 1000);
    configParam(AUX_KNOBS + AuxKnobModes::ENDLESS_MORPH, -INFINITY, INFINITY, 0.f, AuxKnobModeLabels[AuxKnobModes::ENDLESS_MORPH], " limits", 0, 0);
    configParam(AUX_KNOBS + AuxKnobModes::CLICK_FILTER, 0.004, 2.004f, 1.f, AuxKnobModeLabels[AuxKnobModes::CLICK_FILTER], "x", 0, 1);

    getParamQuantity(AUX_KNOBS + AuxKnobModes::MORPH)->randomizeEnabled = false;
    getParamQuantity(AUX_KNOBS + AuxKnobModes::DOUBLE_MORPH)->randomizeEnabled = false;
    getParamQuantity(AUX_KNOBS + AuxKnobModes::TRIPLE_MORPH)->randomizeEnabled = false;
    getParamQuantity(AUX_KNOBS + AuxKnobModes::UNI_MORPH)->randomizeEnabled = false;
    getParamQuantity(AUX_KNOBS + AuxKnobModes::ENDLESS_MORPH)->randomizeEnabled = false;
    
    for (int i = 0; i < 4; i++) {
        configButton(OPERATOR_BUTTONS + i, "Operator " + std::to_string(i + 1) + " button");
        configButton(MODULATOR_BUTTONS + i, "Modulator " + std::to_string(i + 1) + " button");
    }
    for (int i = 0; i < 3; i++) {
        configButton(SCENE_BUTTONS + i, "Algorithm " + std::to_string(i + 1) + " button");
    }
    configButton(EDIT_BUTTON, "Algorithm Edit button");
    configButton(SCREEN_BUTTON, "Screen button (out of service)");

    for (int i = 0; i < 4; i++)
        configInput(OPERATOR_INPUTS + i, "Operator " + std::to_string(i + 1));

    for (int i = 0; i < 4; i++)
        configOutput(MODULATOR_OUTPUTS + i, "Modulator " + std::to_string(i + 1));

    configOutput(CARRIER_SUM_OUTPUT, "Carrier Sum");
    configOutput(MODULATOR_SUM_OUTPUT, "Modulator Sum");
    configOutput(PHASE_OUTPUT, "Phase");

    for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++)
        auxInput[auxIndex] = new AuxInput(auxIndex, this);
    
    for (int i = 0; i < NUM_AUX_INPUTS; i++)
        configAuxInput(AUX_INPUTS + i, auxInput[i], this);

    runClickFilter.setRiseFall(400.f, 400.f);

    for (int i = 0; i < 4; i++) {
        auxInput[i]->shadowClickFilter[i].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
        for (int c = 0; c < 16; c++) {
            auxInput[i]->wildcardModClickFilter[c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
            auxInput[i]->wildcardSumClickFilter[c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
        }
    }

    AlgomorphLarge::onReset();
}

void AlgomorphLarge::onReset() {
    Algomorph::onReset();

    for  (int mode = 0; mode < AuxInputModes::NUM_MODES; mode++)
        auxModeFlags[mode] = false;

    for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++) {
        auxInput[auxIndex]->clearAuxModes();
        auxInput[auxIndex]->resetVoltages();
        auxInput[auxIndex]->allowMultipleModes = pluginSettings.allowMultipleModes[auxIndex];
    }

    for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++) {
        for (int mode = 0; mode < AuxInputModes::NUM_MODES; mode++) {
            if (pluginSettings.auxInputDefaults[auxIndex][mode])
                auxInput[auxIndex]->setMode(mode);
        }
    }
    rescaleVoltages(16);

    running = true;
    runSilencer = true;
    resetOnRun = false;
    resetScene = 1;
    ccwSceneSelection = true;
    wildModIsSummed =  false;
}

void AlgomorphLarge::unsetAuxMode(int auxIndex, int mode) {
    auxInput[auxIndex]->unsetAuxMode(mode);

    auxModeFlags[mode] = false;
    for (int i = 0; i < NUM_AUX_INPUTS; i++) {
        if (i != auxIndex)
            if (auxInput[i]->modeIsActive[mode]) {
                auxModeFlags[mode] = true;
                break;
            }
    }

    auxPanelDirty = true;
}

void AlgomorphLarge::process(const ProcessArgs& args) {
    float in[16] = {0.f};                                   // Operator input channels
    float modOut[4][16] = {{0.f}};                          // Modulator outputs & channels
    float carSumOut[16] = {0.f};                            // Carrier sum output channels
    float modSumOut[16] = {0.f};                            // Modulator sum output channels
    float phaseOut[16] = {0.f};                             // Phase output channels
    int sceneOffset[16] = {0};                              // Offset to the base scene
    int channels = 1;                                       // Max channels of operator inputs
    bool processCV = cvDivider.process();

    for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++) {
        if (inputs[AUX_INPUTS + auxIndex].isConnected()) {
            auxInput[auxIndex]->connected = true;
            auxInput[auxIndex]->updateVoltage();
        }
        else {
            if (auxInput[auxIndex]->connected) {
                auxInput[auxIndex]->connected = false;
                auxInput[auxIndex]->resetVoltages();
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
    for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++)
        auxInput[auxIndex]->channels = channels;

    if (processCV) {
        for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++) {
            //Reset trigger
            if (resetCVTrigger.process(auxInput[auxIndex]->voltage[AuxInputModes::RESET][0])) {
                initRun();// must be after sequence reset
                sceneAdvCVTrigger.reset();
            }

            //Run trigger
            if (auxInput[auxIndex]->runCVTrigger.process(auxInput[auxIndex]->voltage[AuxInputModes::RUN][0])) {
                running ^= true;
                if (running) {
                    if (resetOnRun)
                        initRun();
                }
            }

            //Clock input
            if (running && clockIgnoreOnReset == 0l) {
                //Scene advance trigger input
                if (auxInput[auxIndex]->sceneAdvCVTrigger.process(auxInput[auxIndex]->voltage[AuxInputModes::CLOCK][0])) {
                    //Advance base scene
                    if (!ccwSceneSelection)
                        baseScene = (baseScene + 1) % 3;
                    else
                        baseScene = (baseScene + 2) % 3;
                    graphDirty = true;
                }
                if (auxInput[auxIndex]->reverseSceneAdvCVTrigger.process(auxInput[auxIndex]->voltage[AuxInputModes::REVERSE_CLOCK][0])) {
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
                        AlgorithmSceneChangeAction<>* h = new AlgorithmSceneChangeAction<>();
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
    if (auxModeFlags[AuxInputModes::DOUBLE_MORPH_ATTEN])
        rescaleVoltage(AuxInputModes::DOUBLE_MORPH_ATTEN, channels);
    if (auxModeFlags[AuxInputModes::TRIPLE_MORPH_ATTEN])
        rescaleVoltage(AuxInputModes::TRIPLE_MORPH_ATTEN, channels);
    float morphAttenuversion[16] = {0.f};
    for (int c = 0; c < channels; c++) {
        morphAttenuversion[c] = scaledAuxVoltage[AuxInputModes::MORPH_ATTEN][c]
                                * scaledAuxVoltage[AuxInputModes::DOUBLE_MORPH_ATTEN][c]
                                * scaledAuxVoltage[AuxInputModes::TRIPLE_MORPH_ATTEN][c]
                                * params[AUX_KNOBS + AuxKnobModes::MORPH_ATTEN].getValue()
                                * params[AUX_KNOBS + AuxKnobModes::DOUBLE_MORPH_ATTEN].getValue()
                                * params[AUX_KNOBS + AuxKnobModes::TRIPLE_MORPH_ATTEN].getValue();
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
    phaseOut[0] = newMorph0;
    while (phaseOut[0] > 1.f)
        phaseOut[0] = -1 + (phaseOut[0] - 1.f);
    while (phaseOut[0] < -1.f)
        phaseOut[0] = 1.f + (phaseOut[0] + 1.f);
    phaseOut[0] = rack::math::rescale(phaseOut[0], -1.f, 1.f, phaseMin, phaseMax);
    if (morph[0] != newMorph0) {
        morph[0] = newMorph0;
        graphDirty = true;
    }
    // morph[0] was just processed, so start this loop with [1]
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
        phaseOut[c] = morph[c];
        while (phaseOut[c] > 1.f)
            phaseOut[c] = -1 + (phaseOut[c] - 1.f);
        while (phaseOut[c] < -1.f)
            phaseOut[c] = 1.f + (phaseOut[c] + 1.f);
        phaseOut[c] = rack::math::rescale(phaseOut[c], -1.f, 1.f, phaseMin, phaseMax);
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

        //Check for config mode destination selection and forced carrier designation
        if (configMode) {
            if (configOp > -1) {
                if (modulatorTrigger[configOp].process(params[MODULATOR_BUTTONS + configOp].getValue() > 0.f)) {  //Op is connected to itself
                    // History
                    AlgorithmHorizontalChangeAction<>* h = new AlgorithmHorizontalChangeAction<>();
                    h->moduleId = this->id;
                    h->scene = configScene;
                    h->op = configOp;

                    toggleHorizontalDestination(configScene, configOp);

                    APP->history->push(h);

                    if (exitConfigOnConnect) {
                        configMode = false;
                        configOp = -1;
                    }
                    
                    graphDirty = true;
                }
                else {
                    for (int mod = 0; mod < 3; mod++) {
                        if (modulatorTrigger[relToAbs[configOp][mod]].process(params[MODULATOR_BUTTONS + relToAbs[configOp][mod]].getValue() > 0.f)) {
                            // History
                            AlgorithmDiagonalChangeAction<>* h = new AlgorithmDiagonalChangeAction<>();
                            h->moduleId = this->id;
                            h->scene = configScene;
                            h->op = configOp;
                            h->mod = mod;

                            toggleDiagonalDestination(configScene, configOp, mod);
                            
                            APP->history->push(h);

                            if (exitConfigOnConnect) {
                                configMode = false;
                                configOp = -1;
                            }

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
                        AlgorithmForcedCarrierChangeAction<>* h = new AlgorithmForcedCarrierChangeAction<>();
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
                    AlgorithmForcedCarrierChangeAction<>* h = new AlgorithmForcedCarrierChangeAction<>();
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

    // Update display
    displayMorph.push(relativeMorphMagnitude[0]);
    if (configMode) {
        displayScene.push(configScene);
    }
    else {
        displayScene.push(centerMorphScene[0]);
        displayMorphScene.push(forwardMorphScene[0]);
    }
    
    //Update clickfilter rise/fall times
    if (clickFilterDivider.process()) {
        if (auxModeFlags[AuxInputModes::CLICK_FILTER]) {
            for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++) {
                if (auxInput[auxIndex]->modeIsActive[AuxInputModes::CLICK_FILTER]) {
                    rescaleVoltage(AuxInputModes::CLICK_FILTER, channels);
                    break;
                }
            }
        }


        for (int c = 0; c < channels; c++) {
            float clickFilterResult = clickFilterSlew * params[AUX_KNOBS + AuxKnobModes::CLICK_FILTER].getValue();
            
            clickFilterResult *= scaledAuxVoltage[AuxInputModes::CLICK_FILTER][c];

            for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++) {
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
    float wildcardModGain = params[AUX_KNOBS + AuxKnobModes::WILDCARD_MOD_GAIN].getValue();
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
                            carSumOut[c] += routeSumRingB(args.sampleTime, in[c], i, c);
                            carSumOut[c] += routeSumB(args.sampleTime, in[c], i, c);
                        }
                        else {
                            for (int j = 0; j < 3; j++) {
                                modOut[relToAbs[i][j]][c] += routeDiagonal(args.sampleTime, in[c], i, j, c);
                                modOut[relToAbs[i][j]][c] += routeDiagonalRing(args.sampleTime, in[c], i, j, c);
                            }
                            carSumOut[c] += routeSumRing(args.sampleTime, in[c], i, c);
                            carSumOut[c] += routeSum(args.sampleTime, in[c], i, c);
                        }
                    }
                    else {
                        //Check current algorithm and morph target
                        if (modeB) {
                            modOut[i][c] += routeHorizontalRing(args.sampleTime, in[c], i, c);
                            modOut[i][c] += routeHorizontal(args.sampleTime, in[c], i, c);
                            for (int j = 0; j < 3; j++) {
                                modOut[relToAbs[i][j]][c] += routeDiagonalRingB(args.sampleTime, in[c], i, j, c);
                                modOut[relToAbs[i][j]][c] += routeDiagonalB(args.sampleTime, in[c], i, j, c);
                            }
                            carSumOut[c] += routeSumRingB(args.sampleTime, in[c], i, c);
                            carSumOut[c] += routeSumB(args.sampleTime, in[c], i, c);
                        }
                        else {
                            for (int j = 0; j < 3; j++) {
                                modOut[relToAbs[i][j]][c] += routeDiagonalRing(args.sampleTime, in[c], i, j, c);
                                modOut[relToAbs[i][j]][c] += routeDiagonal(args.sampleTime, in[c], i, j, c);
                            }
                            carSumOut[c] += routeSumRing(args.sampleTime, in[c], i, c);
                            carSumOut[c] += routeSum(args.sampleTime, in[c], i, c);
                        }
                    }
                }
                modOut[i][c] *=  modAttenuversion[c] * modGain * runClickFilterGain;
                carSumOut[c] *= sumAttenuversion[c] * sumGain * runClickFilterGain;
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
                                modOut[relToAbs[i][j]][c] += routeDiagonalB(args.sampleTime, in[c], i, j, c);
                            }
                            carSumOut[c] += routeSumB(args.sampleTime, in[c], i, c);
                        }
                        else {
                            for (int j = 0; j < 3; j++) {
                                modOut[relToAbs[i][j]][c] += routeDiagonal(args.sampleTime, in[c], i, j, c);
                            }
                            carSumOut[c] += routeSum(args.sampleTime, in[c], i, c);
                        }
                    }
                    else {
                        //Check current algorithm and morph target
                        if (modeB) {
                            modOut[i][c] += routeHorizontal(args.sampleTime, in[c], i, c);
                            for (int j = 0; j < 3; j++) {
                                modOut[relToAbs[i][j]][c] += routeDiagonalB(args.sampleTime, in[c], i, j, c);
                            }
                            carSumOut[c] += routeSumB(args.sampleTime, in[c], i, c);
                        }
                        else {
                            for (int j = 0; j < 3; j++) {
                                modOut[relToAbs[i][j]][c] += routeDiagonal(args.sampleTime, in[c], i, j, c);
                            }
                            carSumOut[c] += routeSum(args.sampleTime, in[c], i, c);
                        }
                    }
                }
            }
        }
        for (int mod = 0; mod < 4; mod++)
            modSumOut[c] += modOut[mod][c] * sumAttenuversion[c] * sumGain * runClickFilterGain;
        if (auxModeFlags[AuxInputModes::WILDCARD_MOD]) {
            for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++) {
                auxInput[auxIndex]->wildcardModClickGain = (clickFilterEnabled ? auxInput[auxIndex]->wildcardModClickFilter[c].process(args.sampleTime, auxInput[auxIndex]->modeIsActive[AuxInputModes::WILDCARD_MOD]) : auxInput[auxIndex]->modeIsActive[AuxInputModes::WILDCARD_MOD]);
                wildcardMod[c] += auxInput[auxIndex]->voltage[AuxInputModes::WILDCARD_MOD][c] * auxInput[auxIndex]->wildcardModClickGain;
            }
            for (int mod = 0; mod < 4; mod++) {
                modOut[mod][c] += wildcardMod[c] * wildcardModGain;
            }
            if (wildModIsSummed)
                modSumOut[c] += wildcardMod[c] * wildcardModGain * sumAttenuversion[c] * sumGain * runClickFilterGain;
        }
        for (int mod = 0; mod < 4; mod++)
            modOut[mod][c] *= runClickFilterGain * modAttenuversion[c] * modGain;
        if (auxModeFlags[AuxInputModes::WILDCARD_SUM]) {
            for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++) {
                auxInput[auxIndex]->wildcardSumClickGain = (clickFilterEnabled ? auxInput[auxIndex]->wildcardSumClickFilter[c].process(args.sampleTime, auxInput[auxIndex]->modeIsActive[AuxInputModes::WILDCARD_SUM]) : auxInput[auxIndex]->modeIsActive[AuxInputModes::WILDCARD_SUM]);
                wildcardSum[c] += auxInput[auxIndex]->voltage[AuxInputModes::WILDCARD_SUM][c] * auxInput[auxIndex]->wildcardSumClickGain;
            }
            carSumOut[c] += wildcardSum[c];
        }
        carSumOut[c] *= sumAttenuversion[c] * sumGain * runClickFilterGain;
    }

    //Set outputs
    for (int i = 0; i < 4; i++) {
        if (outputs[MODULATOR_OUTPUTS + i].isConnected()) {
            outputs[MODULATOR_OUTPUTS + i].setChannels(channels);
            outputs[MODULATOR_OUTPUTS + i].writeVoltages(modOut[i]);
        }
    }
    if (outputs[CARRIER_SUM_OUTPUT].isConnected()) {
        outputs[CARRIER_SUM_OUTPUT].setChannels(channels);
        outputs[CARRIER_SUM_OUTPUT].writeVoltages(carSumOut);
    }
    if (outputs[MODULATOR_SUM_OUTPUT].isConnected()) {
        outputs[MODULATOR_SUM_OUTPUT].setChannels(channels);
        outputs[MODULATOR_SUM_OUTPUT].writeVoltages(modSumOut);
    }
    if (outputs[PHASE_OUTPUT].isConnected()) {
        outputs[PHASE_OUTPUT].setChannels(channels);
        outputs[PHASE_OUTPUT].writeVoltages(phaseOut);
    }

    //Set lights
    if (lightDivider.process()) {
        rotor.step(args.sampleTime * lightDivider.getDivision());
        if (configMode) {   //Display state without morph, highlight configScene
            //Set purple component to off
            lights[DISPLAY_BACKLIGHT].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
            //Set yellow component
            lights[DISPLAY_BACKLIGHT + 1].setSmoothBrightness(getOutputBrightness(CARRIER_SUM_OUTPUT) / 2048.f + .005f / 3.f, args.sampleTime * lightDivider.getDivision());
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
            //Set op/mod lights
            for (int i = 0; i < 4; i++) {
                if (horizontalMarks[configScene].test(i)) {
                    if (modeB) {
                        //Set carrier indicator
                        // In (config + alter ego) mode, when a selected operator is a forced-carrier,
                        // the purple indicator should blink on/off whenever the yellow selection indicator blinks off/on,
                        // regardless of whether it is horizontally marked.
                        if (forcedCarriers[configScene].test(i)) {
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
                        // In config mode, when a selected operator is both a forced-carrier and disabled,
                        // the red indicator should blink on/off whenever the yellow selection indicator blinks off/on.
                        if (forcedCarriers[configScene].test(i)) {
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
                    // In config mode, if a non-horizontal-marked operator is a forced-carrier,
                    // the purple indicator should blink on/off whenever the yellow selection indicator blinks off/on.
                    if (forcedCarriers[configScene].test(i)) {
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
                            : getInputBrightness(OPERATOR_INPUTS + i)
                        : getInputBrightness(OPERATOR_INPUTS + i), args.sampleTime * lightDivider.getDivision());
                    //Yellow Lights
                    lights[OPERATOR_LIGHTS + i * 3 + 1].setSmoothBrightness(configOp == i ? blinkStatus : 0.f, args.sampleTime * lightDivider.getDivision());
                    //Red lights
                    lights[OPERATOR_LIGHTS + i * 3 + 2].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                }
                //Set mod lights
                if (i != configOp) {
                    //Purple lights
                    lights[MODULATOR_LIGHTS + i * 3].setSmoothBrightness(configOp > -1 ?
                        algoName[configScene].test(configOp * 3 + absToRel[configOp][i]) ?
                            blinkStatus ?
                                0.f
                                : getOutputBrightness(MODULATOR_OUTPUTS + i)
                            : getOutputBrightness(MODULATOR_OUTPUTS + i)
                        : getOutputBrightness(MODULATOR_OUTPUTS + i), args.sampleTime * lightDivider.getDivision());
                    //Yellow lights
                    lights[MODULATOR_LIGHTS + i * 3 + 1].setSmoothBrightness(configOp > -1 ?
                        (algoName[configScene].test(configOp * 3 + absToRel[configOp][i]) ?
                            blinkStatus
                            : 0.f)
                        : 0.f, args.sampleTime * lightDivider.getDivision());
                }
                else {
                    if (modeB) {
                        //Purple lights
                        lights[MODULATOR_LIGHTS + i * 3].setSmoothBrightness(getOutputBrightness(MODULATOR_OUTPUTS + i), args.sampleTime * lightDivider.getDivision());
                        //Yellow lights
                        lights[MODULATOR_LIGHTS + i * 3 + 1].setSmoothBrightness(horizontalMarks[configScene].test(configOp) ?
                            blinkStatus
                            : 0.f, args.sampleTime * lightDivider.getDivision());
                    }
                    else {
                        //Purple lights
                        lights[MODULATOR_LIGHTS + i * 3].setSmoothBrightness(horizontalMarks[configScene].test(configOp) ?
                            blinkStatus ?
                                0.f
                                : getOutputBrightness(MODULATOR_OUTPUTS + i)
                            : getOutputBrightness(MODULATOR_OUTPUTS + i), args.sampleTime * lightDivider.getDivision());
                        //Yellow lights
                        lights[MODULATOR_LIGHTS + i * 3 + 1].setSmoothBrightness(horizontalMarks[configScene].test(configOp) ?
                            blinkStatus
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
                    lights[CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(algoName[configScene].test((i / 3) * 3 + (i % 3)) ?
                        0.4f
                        : 0.f, args.sampleTime * lightDivider.getDivision());
                    //Set diagonal disable lights
                    lights[D_DISABLE_LIGHTS + i].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                }
                for (int i = 0; i < 4; i++) {
                    //Purple lights
                    lights[H_CONNECTION_LIGHTS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    //Yellow lights
                    lights[H_CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(horizontalMarks[configScene].test(i) ?
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
                    lights[CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(!horizontalMarks[configScene].test(i / 3) ? 
                        algoName[configScene].test((i / 3) * 3 + (i % 3)) ?
                            0.4f
                            : 0.f
                        : 0.f, args.sampleTime * lightDivider.getDivision());
                    //Set diagonal disable lights
                    lights[D_DISABLE_LIGHTS + i].setSmoothBrightness(!horizontalMarks[configScene].test(i / 3) ? 
                        0.f
                        : algoName[configScene].test((i / 3) * 3 + (i % 3)) ?
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
                    lights[H_CONNECTION_LIGHTS + i * 3 + 2].setSmoothBrightness(!horizontalMarks[configScene].test(i) ?
                        0.f
                        : DEF_RED_BRIGHTNESS, args.sampleTime * lightDivider.getDivision());
                }
            }
            //Set screen button ring light if pressed
            //Purple lights
            lights[SCREEN_BUTTON_RING_LIGHT].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
            //Yellow lights
            lights[SCREEN_BUTTON_RING_LIGHT + 1].setSmoothBrightness(params[SCREEN_BUTTON].getValue(), args.sampleTime * lightDivider.getDivision());
        } 
        else {
            //Set backlight
            //Set yellow component to off
            lights[DISPLAY_BACKLIGHT + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
            //Set purple component
            lights[DISPLAY_BACKLIGHT].setSmoothBrightness(getOutputBrightness(CARRIER_SUM_OUTPUT) / 1024.f + 0.0975f, args.sampleTime * lightDivider.getDivision());
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
                    if (horizontalMarks[centerMorphScene[0]].test(i))
                        brightness += getOutputBrightness(MODULATOR_OUTPUTS + i) * (1.f - relativeMorphMagnitude[0]);
                    if (horizontalMarks[forwardMorphScene[0]].test(i))
                        brightness += getOutputBrightness(MODULATOR_OUTPUTS + i) * relativeMorphMagnitude[0];
                    //Purple lights
                    lights[H_CONNECTION_LIGHTS + i * 3].setSmoothBrightness(brightness, args.sampleTime * lightDivider.getDivision());
                    //Yellow
                    lights[H_CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    //Red
                    lights[H_CONNECTION_LIGHTS + i * 3 + 2].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                }
                for (int i = 0; i < 12; i++) {
                    brightness = 0.f;
                    if (algoName[centerMorphScene[0]].test((i / 3) * 3 + (i % 3)))
                        brightness += getOutputBrightness(MODULATOR_OUTPUTS + i / 3) * (1.f - relativeMorphMagnitude[0]);
                    if (algoName[forwardMorphScene[0]].test((i / 3) * 3 + (i % 3)))
                        brightness += getOutputBrightness(MODULATOR_OUTPUTS + i / 3) * relativeMorphMagnitude[0];
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
                    brightness = getInputBrightness(OPERATOR_INPUTS + i);
                    for (int j = 0; j < 3; j++) {
                        float morphBrightness = 0.f;
                        if (algoName[centerMorphScene[0]].test(i * 3 + j)) {
                            if (!horizontalMarks[centerMorphScene[0]].test(i))
                                morphBrightness += brightness * (1.f - relativeMorphMagnitude[0]);
                        }
                        if (algoName[forwardMorphScene[0]].test(i * 3 + j)) {
                            if (!horizontalMarks[forwardMorphScene[0]].test(i))
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
                    if (horizontalMarks[centerMorphScene[0]].test(i))
                        horizontalMorphBrightness += (1.f - relativeMorphMagnitude[0]);
                    if (horizontalMarks[forwardMorphScene[0]].test(i))
                        horizontalMorphBrightness += relativeMorphMagnitude[0];
                    //Purple lights
                    lights[H_CONNECTION_LIGHTS + i * 3].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    //Yellow lights
                    lights[H_CONNECTION_LIGHTS + i * 3 + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
                    //Red lights
                    lights[H_CONNECTION_LIGHTS + i * 3 + 2].setSmoothBrightness(horizontalMorphBrightness * DEF_RED_BRIGHTNESS, args.sampleTime * lightDivider.getDivision());
                }
            }
            //Set screen button ring light if pressed
            //Purple lights
            lights[SCREEN_BUTTON_RING_LIGHT].setSmoothBrightness(params[SCREEN_BUTTON].getValue(), args.sampleTime * lightDivider.getDivision());
            //Yellow lights
            lights[SCREEN_BUTTON_RING_LIGHT + 1].setSmoothBrightness(0.f, args.sampleTime * lightDivider.getDivision());
        }
    }

    if (clockIgnoreOnReset > 0l)
        clockIgnoreOnReset--;
}

float AlgomorphLarge::routeHorizontal(float sampleTime, float inputVoltage, int op, int c) {
    float connectionA = horizontalMarks[centerMorphScene[c]].test(op) * (1.f - relativeMorphMagnitude[c]);
    float connectionB = horizontalMarks[forwardMorphScene[c]].test(op) * (relativeMorphMagnitude[c]);
    float morphedConnection = connectionA + connectionB;
    modClickGain[op][op][c] = clickFilterEnabled ? modClickFilters[op][op][c].process(sampleTime, morphedConnection) : morphedConnection;
    return (inputVoltage + scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * modClickGain[op][op][c];
}

float AlgomorphLarge::routeHorizontalRing(float sampleTime, float inputVoltage, int op, int c) {
    float connection = horizontalMarks[backwardMorphScene[c]].test(op) * relativeMorphMagnitude[c];
    modRingClickGain[op][op][c] = clickFilterEnabled ? modRingClickFilters[op][op][c].process(sampleTime, connection) : connection;
    return (-inputVoltage - scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * modRingClickGain[op][op][c];
}

float AlgomorphLarge::routeDiagonal(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connectionA = algoName[centerMorphScene[c]].test(op * 3 + mod)   * (1.f - relativeMorphMagnitude[c])  * !horizontalMarks[centerMorphScene[c]].test(op);
    float connectionB = algoName[forwardMorphScene[c]].test(op * 3 + mod)  * relativeMorphMagnitude[c]          * !horizontalMarks[forwardMorphScene[c]].test(op);
    float morphedConnection = connectionA + connectionB;
    modClickGain[op][relToAbs[op][mod]][c] = clickFilterEnabled ? modClickFilters[op][relToAbs[op][mod]][c].process(sampleTime, morphedConnection) : morphedConnection;
    return (inputVoltage + scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * modClickGain[op][relToAbs[op][mod]][c];
}

float AlgomorphLarge::routeDiagonalB(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connectionA = algoName[centerMorphScene[c]].test(op * 3 + mod)   * (1.f - relativeMorphMagnitude[c]);
    float connectionB = algoName[forwardMorphScene[c]].test(op * 3 + mod)  * relativeMorphMagnitude[c];
    float morphedConnection = connectionA + connectionB;
    modClickGain[op][relToAbs[op][mod]][c] = clickFilterEnabled ? modClickFilters[op][relToAbs[op][mod]][c].process(sampleTime, morphedConnection) : morphedConnection;
    return (inputVoltage + scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * modClickGain[op][relToAbs[op][mod]][c];
}

float AlgomorphLarge::routeDiagonalRing(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connection = algoName[backwardMorphScene[c]].test(op * 3 + mod) * relativeMorphMagnitude[c] * !horizontalMarks[backwardMorphScene[c]].test(op);
    modRingClickGain[op][relToAbs[op][mod]][c] = clickFilterEnabled ? modRingClickFilters[op][relToAbs[op][mod]][c].process(sampleTime, connection) : connection; 
    return (-inputVoltage + scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * modRingClickGain[op][relToAbs[op][mod]][c];
}

float AlgomorphLarge::routeDiagonalRingB(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connection = algoName[backwardMorphScene[c]].test(op * 3 + mod) * relativeMorphMagnitude[c];
    modRingClickGain[op][relToAbs[op][mod]][c] = clickFilterEnabled ? modRingClickFilters[op][relToAbs[op][mod]][c].process(sampleTime, connection) : connection; 
    return (-inputVoltage + scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * modRingClickGain[op][relToAbs[op][mod]][c];
}

float AlgomorphLarge::routeSum(float sampleTime, float inputVoltage, int op, int c) {
    float connection    =   carriers[centerMorphScene[c]].test(op)     * (1.f - relativeMorphMagnitude[c])  * !horizontalMarks[centerMorphScene[c]].test(op)
                        +   carriers[forwardMorphScene[c]].test(op)    * relativeMorphMagnitude[c]          * !horizontalMarks[forwardMorphScene[c]].test(op);
    sumClickGain[op][c] = clickFilterEnabled ? sumClickFilters[op][c].process(sampleTime, connection) : connection;
    return (inputVoltage + scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * sumClickGain[op][c];
}

float AlgomorphLarge::routeSumB(float sampleTime, float inputVoltage, int op, int c) {
    float connection    =   carriers[centerMorphScene[c]].test(op)     * (1.f - relativeMorphMagnitude[c])
                        +   carriers[forwardMorphScene[c]].test(op)    * relativeMorphMagnitude[c];
    sumClickGain[op][c] = clickFilterEnabled ? sumClickFilters[op][c].process(sampleTime, connection) : connection;
    return (inputVoltage + scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * sumClickGain[op][c];
}

float AlgomorphLarge::routeSumRing(float sampleTime, float inputVoltage, int op, int c) {
    float connection = carriers[backwardMorphScene[c]].test(op) * relativeMorphMagnitude[c] * !horizontalMarks[backwardMorphScene[c]].test(op);
    sumRingClickGain[op][c] = clickFilterEnabled ? sumRingClickFilters[op][c].process(sampleTime, connection) : connection;
    return (-inputVoltage - scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * sumRingClickGain[op][c];
}

float AlgomorphLarge::routeSumRingB(float sampleTime, float inputVoltage, int op, int c) {
    float connection = carriers[backwardMorphScene[c]].test(op) * relativeMorphMagnitude[c];
    sumRingClickGain[op][c] = clickFilterEnabled ? sumRingClickFilters[op][c].process(sampleTime, connection) : connection;
    return (-inputVoltage - scaledAuxVoltage[AuxInputModes::SHADOW + op][c]) * sumRingClickGain[op][c];
}

void AlgomorphLarge::scaleAuxSumAttenCV(int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::SUM_ATTEN][c] = 1.f;
        for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++)
            scaledAuxVoltage[AuxInputModes::SUM_ATTEN][c] *= rack::math::clamp(auxInput[auxIndex]->voltage[AuxInputModes::SUM_ATTEN][c] / 5.f, -1.f, 1.f);
    }
}

void AlgomorphLarge::scaleAuxModAttenCV(int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] = 1.f;
        for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++)
            scaledAuxVoltage[AuxInputModes::MOD_ATTEN][c] *= rack::math::clamp(auxInput[auxIndex]->voltage[AuxInputModes::MOD_ATTEN][c] / 5.f, -1.f, 1.f);
    }    
}

void AlgomorphLarge::scaleAuxClickFilterCV(int channels) {
    for (int c = 0; c < channels; c++) {
        //+/-5V = 0V-2V
        scaledAuxVoltage[AuxInputModes::CLICK_FILTER][c] = 1.f;
        for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++)
            scaledAuxVoltage[AuxInputModes::CLICK_FILTER][c] *= (rack::math::clamp(auxInput[auxIndex]->voltage[AuxInputModes::CLICK_FILTER][c] / 5.f, -1.f, 1.f) + 1.001f);
    }    
}

void AlgomorphLarge::scaleAuxMorphCV(int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::MORPH][c] = 0.f;
        for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++)
            scaledAuxVoltage[AuxInputModes::MORPH][c] += auxInput[auxIndex]->voltage[AuxInputModes::MORPH][c] / 5.f;
    }
}

void AlgomorphLarge::scaleAuxDoubleMorphCV(int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::DOUBLE_MORPH][c] = 0.f;
        for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++)
            scaledAuxVoltage[AuxInputModes::DOUBLE_MORPH][c] += auxInput[auxIndex]->voltage[AuxInputModes::DOUBLE_MORPH][c] / FIVE_D_TWO;
    }
}

void AlgomorphLarge::scaleAuxTripleMorphCV(int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::TRIPLE_MORPH][c] = 0.f;
        for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++)
            scaledAuxVoltage[AuxInputModes::TRIPLE_MORPH][c] += auxInput[auxIndex]->voltage[AuxInputModes::TRIPLE_MORPH][c] / FIVE_D_THREE;
    }
}

void AlgomorphLarge::scaleAuxMorphAttenCV(int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::MORPH_ATTEN][c] = 1.f;
        for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++)
            scaledAuxVoltage[AuxInputModes::MORPH_ATTEN][c] *= auxInput[auxIndex]->voltage[AuxInputModes::MORPH_ATTEN][c] / 5.f;
    }
}

void AlgomorphLarge::scaleAuxMorphDoubleAttenCV(int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::DOUBLE_MORPH_ATTEN][c] = 1.f;
        for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++)
            scaledAuxVoltage[AuxInputModes::DOUBLE_MORPH_ATTEN][c] *= auxInput[auxIndex]->voltage[AuxInputModes::DOUBLE_MORPH_ATTEN][c] / FIVE_D_TWO;
    }
}

void AlgomorphLarge::scaleAuxMorphTripleAttenCV(int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::TRIPLE_MORPH_ATTEN][c] = 1.f;
        for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++)
            scaledAuxVoltage[AuxInputModes::TRIPLE_MORPH_ATTEN][c] *= auxInput[auxIndex]->voltage[AuxInputModes::TRIPLE_MORPH_ATTEN][c] / FIVE_D_THREE;
    }
}

void AlgomorphLarge::scaleAuxShadow(float sampleTime, int op, int channels) {
    for (int c = 0; c < channels; c++) {
        scaledAuxVoltage[AuxInputModes::SHADOW + op][c] = 0.f;
        for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++) {
            float gain = clickFilterEnabled ? auxInput[auxIndex]->shadowClickFilter[op].process(sampleTime, auxInput[auxIndex]->modeIsActive[AuxInputModes::SHADOW + op] * auxInput[auxIndex]->connected) : auxInput[auxIndex]->modeIsActive[AuxInputModes::SHADOW + op] * auxInput[auxIndex]->connected;
            scaledAuxVoltage[AuxInputModes::SHADOW + op][c] += gain * auxInput[auxIndex]->voltage[AuxInputModes::SHADOW + op][c];
        }
    }
}

void AlgomorphLarge::rescaleVoltage(int mode, int channels) {
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
        case AuxInputModes::DOUBLE_MORPH_ATTEN:
            scaleAuxMorphDoubleAttenCV(channels);
            break;
        case AuxInputModes::TRIPLE_MORPH_ATTEN:
            scaleAuxMorphTripleAttenCV(channels);
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
                for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++)
                    scaledAuxVoltage[mode][c] += auxInput[auxIndex]->voltage[mode][c];
            }
            break;
    }
}

void AlgomorphLarge::rescaleVoltages(int channels) {
    for (int mode = 0; mode < AuxInputModes::NUM_MODES; mode++)
        rescaleVoltage(mode, channels);
}

void AlgomorphLarge::initRun() {
    clockIgnoreOnReset = (long) (CLOCK_IGNORE_DURATION * APP->engine->getSampleRate());
    baseScene = resetScene;
}

void AlgomorphLarge::updateSceneBrightnesses() {
    if (modeB) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                //Op lights
                //Purple lights
                sceneBrightnesses[i][j][0] = getInputBrightness(OPERATOR_INPUTS + j);

                //Mod lights
                //Purple lights
                sceneBrightnesses[i][j + 4][0] = getOutputBrightness(MODULATOR_OUTPUTS + j);

                //Op lights
                //Yellow Lights
                sceneBrightnesses[i][j][1] = 0.f;
                //Red lights
                sceneBrightnesses[i][j][2] = 0.f;

                //Carrier indicators
                if (forcedCarriers[i].test(j)) {
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
                if (!horizontalMarks[i].test(j)) {
                    //Op lights
                    //Purple lights
                    sceneBrightnesses[i][j][0] = getInputBrightness(OPERATOR_INPUTS + j);
                    //Red lights
                    sceneBrightnesses[i][j][2] = 0.f;

                    //Mod lights
                    //Purple lights
                    sceneBrightnesses[i][j + 4][0] = getOutputBrightness(MODULATOR_OUTPUTS + j);

                    //Carrier indicators
                    if (forcedCarriers[i].test(j)) {
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
                    sceneBrightnesses[i][j + 4][0] = getOutputBrightness(MODULATOR_OUTPUTS + j);

                    //Carrier indicators
                    if (forcedCarriers[i].test(j)) {
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

bool AlgomorphLarge::auxInputsAreDefault() {
    for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++) {
        if (auxInput[auxIndex]->allowMultipleModes != pluginSettings.allowMultipleModes[auxIndex])
            return false;
        for (int mode = 0; mode < AuxInputModes::NUM_MODES; mode++) {
            if (auxInput[auxIndex]->modeIsActive[mode] != pluginSettings.auxInputDefaults[auxIndex][mode])
                return false;
        }
    }
    return true;
}

json_t* AlgomorphLarge::dataToJson() {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "Config Enabled", json_boolean(configMode));
    json_object_set_new(rootJ, "Config Mode", json_integer(configOp));
    json_object_set_new(rootJ, "Config Scene", json_integer(configScene));
    json_object_set_new(rootJ, "Current Scene", json_integer(baseScene));
    json_object_set_new(rootJ, "Horizontal Allowed", json_boolean(modeB));
    json_object_set_new(rootJ, "Reset Scene", json_integer(resetScene));
    json_object_set_new(rootJ, "Ring Morph", json_boolean(ringMorph));
    json_object_set_new(rootJ, "Randomize Ring Morph", json_boolean(randomRingMorph));
    json_object_set_new(rootJ, "Auto Exit", json_boolean(exitConfigOnConnect));
    json_object_set_new(rootJ, "CCW Scene Selection", json_boolean(ccwSceneSelection));
    json_object_set_new(rootJ, "Wildcard Modulator Summing Enabled", json_boolean(wildModIsSummed));
    json_object_set_new(rootJ, "Reset on Run", json_boolean(resetOnRun));
    json_object_set_new(rootJ, "Click Filter Enabled", json_boolean(clickFilterEnabled));
    // json_object_set_new(rootJ, "Glowing Ink", json_boolean(glowingInk));
    json_object_set_new(rootJ, "VU Lights", json_boolean(vuLights));
    
    json_t* lastSetModesJ = json_array();
    for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++) {
        json_t* lastModeJ = json_object();
        json_object_set_new(lastModeJ, (std::string("Aux Input ") + std::to_string(auxIndex)).c_str(), json_integer(auxInput[auxIndex]->lastSetMode));
        json_array_append_new(lastSetModesJ, lastModeJ);
    }
    json_object_set_new(rootJ, "Aux Inputs: Last Set Mode", lastSetModesJ);

    json_t* allowMultipleModesJ = json_array();
    for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++) {
        json_t* allowanceJ = json_object();
        json_object_set_new(allowanceJ, (std::string("Aux Input ") + std::to_string(auxIndex)).c_str(), json_boolean(auxInput[auxIndex]->allowMultipleModes));
        json_array_append_new(allowMultipleModesJ, allowanceJ);
    }
    json_object_set_new(rootJ, "Aux Inputs: Allow Multiple Modes", allowMultipleModesJ);

    json_t* auxConnectionsJ = json_array();
    for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++) {
        json_t* auxConnectedJ = json_object();
        json_object_set_new(auxConnectedJ, (std::string("Aux Input ") + std::to_string(auxIndex)).c_str(), json_boolean(auxInput[auxIndex]->connected));
        json_array_append_new(auxConnectionsJ, auxConnectedJ);
    }
    json_object_set_new(rootJ, "Aux Inputs: Connection Status", auxConnectionsJ);
    
    for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++) {
        json_t* auxInputModesJ = json_array();
        for (int mode = 0; mode < AuxInputModes::NUM_MODES; mode++) {
            json_t* inputModeJ = json_object();
            json_object_set_new(inputModeJ, AuxInputModeLabels[mode].c_str(), json_boolean(auxInput[auxIndex]->modeIsActive[mode]));
            json_array_append_new(auxInputModesJ, inputModeJ);
        }
        json_object_set_new(rootJ, (std::string("Aux Input ") + std::to_string(auxIndex) + " Modes").c_str(), auxInputModesJ);
    }

    json_object_set_new(rootJ, "Aux Knob Mode", json_integer(knobMode));

    json_t* algoNamesJ = json_array();
    for (int scene = 0; scene < 3; scene++) {
        json_t* nameJ = json_object();
        json_object_set_new(nameJ, (std::string("Algorithm ") + std::to_string(scene)).c_str(), json_integer(algoName[scene].to_ullong()));
        json_array_append_new(algoNamesJ, nameJ);
    }
    json_object_set_new(rootJ, "Algorithms: Algorithm IDs", algoNamesJ);
    
    json_t* horizontalMarksJ = json_array();
    for (int scene = 0; scene < 3; scene++) {
        json_t* sceneMarksJ = json_object();
        json_object_set_new(sceneMarksJ, (std::string("Algorithm ") + std::to_string(scene)).c_str(), json_integer(horizontalMarks[scene].to_ullong()));
        json_array_append_new(horizontalMarksJ, sceneMarksJ);
    }
    json_object_set_new(rootJ, "Algorithms: Horizontal Marks", horizontalMarksJ);

    json_t* forcedCarriersJ = json_array();
    for (int scene = 0; scene < 3; scene++) {
        json_t* sceneForcedCarriers = json_object();
        json_object_set_new(sceneForcedCarriers, (std::string("Algorithm ") + std::to_string(scene)).c_str(), json_integer(forcedCarriers[scene].to_ullong()));
        json_array_append_new(forcedCarriersJ, sceneForcedCarriers);
    }
    json_object_set_new(rootJ, "Algorithms: Forced Carriers", forcedCarriersJ);

    return rootJ;
}

void AlgomorphLarge::dataFromJson(json_t* rootJ) {
    auto configMode = json_object_get(rootJ, "Config Enabled");
    if (configMode)
        this->configMode = json_boolean_value(configMode);
    
    auto configOp = json_object_get(rootJ, "Config Mode");
    if (configOp)
        this->configOp = json_integer_value(configOp);
    
    auto configScene = json_object_get(rootJ, "Config Scene");
    if (configScene)
        this->configScene = json_integer_value(configScene);
    
    auto baseScene = json_object_get(rootJ, "Current Scene");
    if (baseScene)
        this->baseScene = json_integer_value(baseScene);
    
    auto modeB = json_object_get(rootJ, "Horizontal Allowed");
    if (modeB)
        this->modeB = json_boolean_value(modeB);
    
    auto resetScene = json_object_get(rootJ, "Reset Scene");
    if (resetScene)
        this->resetScene = json_integer_value(resetScene);
    
    auto knobMode = json_object_get(rootJ, "Aux Knob Mode");
    if (knobMode)
        this->knobMode = json_integer_value(knobMode);
    
    auto ringMorph = json_object_get(rootJ, "Ring Morph");
    if (ringMorph)
        this->ringMorph = json_boolean_value(ringMorph);

    auto randomRingMorph = json_object_get(rootJ, "Randomize Ring Morph");
    if (randomRingMorph)
        this->randomRingMorph = json_boolean_value(randomRingMorph);

    auto exitConfigOnConnect = json_object_get(rootJ, "Auto Exit");
    if (exitConfigOnConnect)
        this->exitConfigOnConnect = json_boolean_value(exitConfigOnConnect);

    auto ccwSceneSelection = json_object_get(rootJ, "CCW Scene Selection");
    if (ccwSceneSelection)
        this->ccwSceneSelection = json_boolean_value(ccwSceneSelection);

    auto wildModIsSummed = json_object_get(rootJ, "Wildcard Modulator Summing Enabled");
    if (wildModIsSummed)
        this->wildModIsSummed = json_boolean_value(wildModIsSummed);

    auto resetOnRun = json_object_get(rootJ, "Reset on Run");
    if (resetOnRun)
        this->resetOnRun = json_boolean_value(resetOnRun);

    auto clickFilterEnabled = json_object_get(rootJ, "Click Filter Enabled");
    if (clickFilterEnabled)
        this->clickFilterEnabled = json_boolean_value(clickFilterEnabled);

    // auto glowingInk = json_object_get(rootJ, "Glowing Ink");
    // if (glowingInk)
    //     this->glowingInk = json_boolean_value(glowingInk);

    auto vuLights = json_object_get(rootJ, "VU Lights");
    if (vuLights)
        this->vuLights = json_boolean_value(vuLights);

    bool reset = true;

    //Set allowMultipleModes before loading modes
    json_t* allowMultipleModesJ = json_object_get(rootJ, "Aux Inputs: Allow Multiple Modes");
    if (allowMultipleModesJ) {
        if (reset) {
            reset = false;
            for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++)
                auxInput[auxIndex]->clearAuxModes();
        }
        json_t* allowanceJ; size_t allowIndex;
        json_array_foreach(allowMultipleModesJ, allowIndex, allowanceJ) {
            auxInput[allowIndex]->allowMultipleModes = json_boolean_value(json_object_get(allowanceJ, (std::string("Aux Input ") + std::to_string(allowIndex)).c_str()));
        }
    }

    for (int auxIndex = 0; auxIndex < 5; auxIndex++) {
        json_t* auxInputModesJ = json_object_get(rootJ, (std::string("Aux Input ") + std::to_string(auxIndex) + " Modes").c_str());
        if (auxInputModesJ) {
            if (reset) {
                reset = false;
                for (int auxIndex = 0; auxIndex < NUM_AUX_INPUTS; auxIndex++)
                    auxInput[auxIndex]->clearAuxModes();
            }
            json_t* inputModeJ; size_t inputModeIndex;
            json_array_foreach(auxInputModesJ, inputModeIndex, inputModeJ) {
                if (json_boolean_value(json_object_get(inputModeJ, AuxInputModeLabels[inputModeIndex].c_str())))
                    auxInput[auxIndex]->setMode(inputModeIndex);
                else
                    unsetAuxMode(auxIndex, inputModeIndex);
            }
        }
    }

    json_t* auxConnectionsJ = json_object_get(rootJ, "Aux Inputs: Connection Status");
    if (auxConnectionsJ) {
        json_t* auxConnectedJ; size_t auxConnectionIndexJ;
        json_array_foreach(auxConnectionsJ, auxConnectionIndexJ, auxConnectedJ) {
            auxInput[auxConnectionIndexJ]->connected = json_boolean_value(json_object_get(auxConnectedJ, (std::string("Aux Input ") + std::to_string(auxConnectionIndexJ)).c_str()));
        }
    }

    json_t* lastSetModesJ = json_object_get(rootJ, "Aux Inputs: Last Set Mode");
    if (lastSetModesJ) {
        json_t* lastModeJ; size_t lastModeIndex;
        json_array_foreach(lastSetModesJ, lastModeIndex, lastModeJ) {
            auxInput[lastModeIndex]->lastSetMode = json_integer_value(json_object_get(lastModeJ, (std::string("Aux Input ") + std::to_string(lastModeIndex)).c_str()));
        }
    }

    json_t* algoNamesJ = json_object_get(rootJ, "Algorithms: Algorithm IDs");
    if (algoNamesJ) {
        json_t* nameJ; size_t nameIndex;
        json_array_foreach(algoNamesJ, nameIndex, nameJ) {
            algoName[nameIndex] = json_integer_value(json_object_get(nameJ, (std::string("Algorithm ") + std::to_string(nameIndex)).c_str()));
        }
    }
    
    json_t* horizontalMarksJ = json_object_get(rootJ, "Algorithms: Horizontal Marks");
    if (horizontalMarksJ) {
        json_t* sceneMarksJ; size_t sceneIndex;
        json_array_foreach(horizontalMarksJ, sceneIndex, sceneMarksJ) {
            horizontalMarks[sceneIndex] = json_integer_value(json_object_get(sceneMarksJ, (std::string("Algorithm ") + std::to_string(sceneIndex)).c_str()));
        }
    }
    
    json_t* forcedCarriersJ = json_object_get(rootJ, "Algorithms: Forced Carriers");
    if (forcedCarriersJ) {
        json_t* sceneForcedCarriersJ; size_t sceneIndex;
        json_array_foreach(forcedCarriersJ, sceneIndex, sceneForcedCarriersJ) {
            forcedCarriers[sceneIndex] = json_integer_value(json_object_get(sceneForcedCarriersJ, (std::string("Algorithm ") + std::to_string(sceneIndex)).c_str()));
        }
    }

    // Update carriers, disabled status, and display algorithm
    for (int scene = 0; scene < 3; scene++) {
        updateCarriers(scene);
        updateOpsDisabled(scene);
        updateDisplayAlgo(scene);
    }

    graphDirty = true;
}

float AlgomorphLarge::getInputBrightness(int portID) {
    if (vuLights)
        return std::max(    {   inputs[portID].plugLights[0].getBrightness(),
                                inputs[portID].plugLights[1].getBrightness() * 4,
                                inputs[portID].plugLights[2].getBrightness()          }   );
    else
        return 1.f;
}

float AlgomorphLarge::getOutputBrightness(int portID) {
    if (vuLights)
        return std::max(    {   outputs[portID].plugLights[0].getBrightness(),
                                outputs[portID].plugLights[1].getBrightness() * 4,
                                outputs[portID].plugLights[2].getBrightness()          }   );
    else
        return 1.f;
}


///// Panel Widget

/// Disallow Multiple Modes Menu Item

AlgomorphLargeWidget::DisallowMultipleModesAction::DisallowMultipleModesAction() {
    name = "Delexander Algomorph disallow multiple modes";
}

void AlgomorphLargeWidget::DisallowMultipleModesAction::undo() {
    ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
    assert(mw);
    AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
    assert(m);
    
    m->auxInput[auxIndex]->allowMultipleModes = true;
    if (multipleActive) {
        for (int mode = 0; mode < AuxInputModes::NUM_MODES; mode++) {
            if (enabled[mode]) {
                m->auxInput[auxIndex]->setMode(mode);
                m->rescaleVoltage(mode, channels);
            }
        }
    }
}

void AlgomorphLargeWidget::DisallowMultipleModesAction::redo() {
    ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
    assert(mw);
    AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
    assert(m);
    
    if (multipleActive) {
        for (int mode = 0; mode < AuxInputModes::NUM_MODES; mode++) {
            if (enabled[mode]) {
                m->unsetAuxMode(auxIndex, mode);
                for (int c = 0; c < channels; c++)
                    m->auxInput[auxIndex]->voltage[mode][c] = m->auxInput[auxIndex]->defVoltage[mode];
                m->rescaleVoltage(mode, channels);
            }
        }
    }
    m->auxInput[auxIndex]->allowMultipleModes = false;
}

void AlgomorphLargeWidget::AllowMultipleModesItem::onAction(const Action &e) {
    if (module->auxInput[auxIndex]->allowMultipleModes) {
        // History
        DisallowMultipleModesAction* h = new DisallowMultipleModesAction;
        h->moduleId = module->id;
        h->auxIndex = auxIndex;
        h->channels = module->auxInput[auxIndex]->channels;

        if (module->auxInput[auxIndex]->activeModes > 1) {
            h->multipleActive = true;
            for (int mode = 0; mode < AuxInputModes::NUM_MODES; mode++) {
                if (module->auxInput[auxIndex]->modeIsActive[mode] && mode != module->auxInput[auxIndex]->lastSetMode) {
                    h->enabled[mode] = true;
                    module->unsetAuxMode(auxIndex, mode);
                    for (int c = 0; c < h->channels; c++)
                        module->auxInput[auxIndex]->voltage[mode][c] = module->auxInput[auxIndex]->defVoltage[mode];
                    module->rescaleVoltage(mode, h->channels);
                }
            }
        }
        module->auxInput[auxIndex]->allowMultipleModes = false;

        APP->history->push(h);
    }
    else {
        // History
        AllowMultipleModesAction<>* h = new AllowMultipleModesAction<>();
        h->moduleId = module->id;
        h->auxIndex = auxIndex;

        module->auxInput[auxIndex]->allowMultipleModes = true;

        APP->history->push(h);
    }
}

Menu* AlgomorphLargeWidget::AllowMultipleModesMenuItem::createChildMenu() {
    Menu* menu = new Menu;
    createAllowMultipleModesMenu(module, menu);
    return menu;
}

void AlgomorphLargeWidget::AllowMultipleModesMenuItem::createAllowMultipleModesMenu(AlgomorphLarge* module, Menu* menu) {
    menu->addChild(construct<AllowMultipleModesItem>(&MenuItem::text, "AUX 1", &AllowMultipleModesItem::module, module, &AllowMultipleModesItem::auxIndex, 0, &AllowMultipleModesItem::rightText, CHECKMARK(module->auxInput[0]->allowMultipleModes)));
    menu->addChild(construct<AllowMultipleModesItem>(&MenuItem::text, "AUX 2", &AllowMultipleModesItem::module, module, &AllowMultipleModesItem::auxIndex, 1, &AllowMultipleModesItem::rightText, CHECKMARK(module->auxInput[1]->allowMultipleModes)));
    menu->addChild(construct<AllowMultipleModesItem>(&MenuItem::text, "AUX 3", &AllowMultipleModesItem::module, module, &AllowMultipleModesItem::auxIndex, 2, &AllowMultipleModesItem::rightText, CHECKMARK(module->auxInput[2]->allowMultipleModes)));
    menu->addChild(construct<AllowMultipleModesItem>(&MenuItem::text, "AUX 4", &AllowMultipleModesItem::module, module, &AllowMultipleModesItem::auxIndex, 3, &AllowMultipleModesItem::rightText, CHECKMARK(module->auxInput[3]->allowMultipleModes)));
    menu->addChild(construct<AllowMultipleModesItem>(&MenuItem::text, "AUX 5", &AllowMultipleModesItem::module, module, &AllowMultipleModesItem::auxIndex, 4, &AllowMultipleModesItem::rightText, CHECKMARK(module->auxInput[4]->allowMultipleModes)));
}

/// Wildcard Input Menu

Menu* AlgomorphLargeWidget::WildcardInputMenuItem::createChildMenu() {
    Menu* menu = new Menu;
    createWildcardInputMenu(module, menu, auxIndex);
    return menu;
}

void AlgomorphLargeWidget::WildcardInputMenuItem::createWildcardInputMenu(AlgomorphLarge* module, Menu* menu, int auxIndex) {
    for (int i = AuxInputModes::WILDCARD_MOD; i <= AuxInputModes::WILDCARD_SUM; i++)
        menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[i], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[i]), &AuxModeItem::mode, i));
}

/// Shadow Input Menu

Menu* AlgomorphLargeWidget::ShadowInputMenuItem::createChildMenu() {
    Menu* menu = new Menu;
    createShadowInputMenu(module, menu, auxIndex);
    return menu;
}

void AlgomorphLargeWidget::ShadowInputMenuItem::createShadowInputMenu(AlgomorphLarge* module, Menu* menu, int auxIndex) {
    for (int i = AuxInputModes::SHADOW; i <= AuxInputModes::SHADOW + 3; i++)
        menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[i], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[i]), &AuxModeItem::mode, i));
}

void AlgomorphLargeWidget::ResetOnRunItem::onAction(const Action &e) {
    // History
    ToggleResetOnRunAction<>* h = new ToggleResetOnRunAction<>();
    h->moduleId = module->id;

    module->resetOnRun ^= true;

    APP->history->push(h);
}

void AlgomorphLargeWidget::RunSilencerItem::onAction(const Action &e) {
    // History
    ToggleRunSilencerAction<>* h = new ToggleRunSilencerAction<>();
    h->moduleId = module->id;

    module->runSilencer ^= true;

    APP->history->push(h);
}

void AlgomorphLargeWidget::AuxModeItem::onAction(const Action &e) {
    if (module->auxInput[auxIndex]->modeIsActive[mode]) {
        // History
        AuxInputUnsetAction<>* h = new AuxInputUnsetAction<>();
        h->moduleId = module->id;
        h->auxIndex = auxIndex;
        h->mode = mode;
        h->channels = module->auxInput[auxIndex]->channels;
        
        module->unsetAuxMode(auxIndex, mode);
        for (int c = 0; c < h->channels; c++)
            module->auxInput[auxIndex]->voltage[mode][c] = module->auxInput[auxIndex]->defVoltage[mode];
        module->rescaleVoltage(mode, h->channels);

        APP->history->push(h);
    }
    else {
        if (module->auxInput[auxIndex]->allowMultipleModes) {
            // History
            AuxInputSetAction<>* h = new AuxInputSetAction<>();
            h->moduleId = module->id;
            h->auxIndex = auxIndex;
            h->mode = mode;
            h->channels = module->auxInput[auxIndex]->channels;

            module->auxInput[auxIndex]->setMode(mode);

            APP->history->push(h);
        }
        else {
            // History
            AuxInputSwitchAction<>* h = new AuxInputSwitchAction<>();
            h->moduleId = module->id;
            h->auxIndex = auxIndex;
            h->oldMode = module->auxInput[auxIndex]->lastSetMode;
            h->newMode = mode;
            h->channels = module->auxInput[auxIndex]->channels;

            module->unsetAuxMode(auxIndex, h->oldMode);
            for (int c = 0; c < h->channels; c++)
                module->auxInput[auxIndex]->voltage[h->oldMode][c] = module->auxInput[auxIndex]->defVoltage[h->oldMode];
            module->rescaleVoltage(h->oldMode, h->channels);
            module->auxInput[auxIndex]->setMode(mode);

            APP->history->push(h);
        }
    }
}

Menu* AlgomorphLargeWidget::AuxInputModeMenuItem::createChildMenu() {
    Menu* menu = new Menu;
    createAuxInputModeMenu(module, menu, auxIndex);
    return menu;
}

void AlgomorphLargeWidget::AuxInputModeMenuItem::createAuxInputModeMenu(AlgomorphLarge* module, Menu* menu, int auxIndex) {
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Audio Input"));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::WILDCARD_MOD], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::WILDCARD_MOD]), &AuxModeItem::mode, AuxInputModes::WILDCARD_MOD));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::WILDCARD_SUM], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::WILDCARD_SUM]), &AuxModeItem::mode, AuxInputModes::WILDCARD_SUM));
    for (int i = AuxInputModes::SHADOW; i <= AuxInputModes::SHADOW + 3; i++)
        menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[i], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[i]), &AuxModeItem::mode, i));

    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Trigger Input"));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::CLOCK], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::CLOCK]), &AuxModeItem::mode, AuxInputModes::CLOCK));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::REVERSE_CLOCK], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::REVERSE_CLOCK]), &AuxModeItem::mode, AuxInputModes::REVERSE_CLOCK));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::RESET], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::RESET]), &AuxModeItem::mode, AuxInputModes::RESET));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::RUN], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::RUN]), &AuxModeItem::mode, AuxInputModes::RUN));

    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "CV Input"));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::MORPH], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::MORPH]), &AuxModeItem::mode, AuxInputModes::MORPH));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::DOUBLE_MORPH], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::DOUBLE_MORPH]), &AuxModeItem::mode, AuxInputModes::DOUBLE_MORPH));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::TRIPLE_MORPH], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::TRIPLE_MORPH]), &AuxModeItem::mode, AuxInputModes::TRIPLE_MORPH));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::MOD_ATTEN], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::MOD_ATTEN]), &AuxModeItem::mode, AuxInputModes::MOD_ATTEN));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::SUM_ATTEN], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::SUM_ATTEN]), &AuxModeItem::mode, AuxInputModes::SUM_ATTEN));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::MORPH_ATTEN], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::MORPH_ATTEN]), &AuxModeItem::mode, AuxInputModes::MORPH_ATTEN));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::DOUBLE_MORPH_ATTEN], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::DOUBLE_MORPH_ATTEN]), &AuxModeItem::mode, AuxInputModes::DOUBLE_MORPH_ATTEN));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::TRIPLE_MORPH_ATTEN], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::TRIPLE_MORPH_ATTEN]), &AuxModeItem::mode, AuxInputModes::TRIPLE_MORPH_ATTEN));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::SCENE_OFFSET], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::SCENE_OFFSET]), &AuxModeItem::mode, AuxInputModes::SCENE_OFFSET));
    menu->addChild(construct<AuxModeItem>(&MenuItem::text, AuxInputModeLabels[AuxInputModes::CLICK_FILTER], &AuxModeItem::module, module, &AuxModeItem::auxIndex, auxIndex, &AuxModeItem::rightText, CHECKMARK(module->auxInput[auxIndex]->modeIsActive[AuxInputModes::CLICK_FILTER]), &AuxModeItem::mode, AuxInputModes::CLICK_FILTER));
}

void AlgomorphLargeWidget::SaveAuxInputSettingsItem::onAction(const Action& e) {
    for (int auxIndex = 0; auxIndex < module->NUM_AUX_INPUTS; auxIndex++){
        pluginSettings.allowMultipleModes[auxIndex] = module->auxInput[auxIndex]->allowMultipleModes;
        for (int mode = 0; mode < AuxInputModes::NUM_MODES; mode++)
            pluginSettings.auxInputDefaults[auxIndex][mode] = module->auxInput[auxIndex]->modeIsActive[mode];
    }
    pluginSettings.saveToJson();
}

void AlgomorphLargeWidget::ResetSceneItem::onAction(const Action &e) {
    // History
    ResetSceneAction<>* h = new ResetSceneAction<>();
    h->moduleId = module->id;
    h->oldResetScene = module->resetScene;
    h->newResetScene = scene;

    module->resetScene = scene;

    APP->history->push(h);
}

Menu* AlgomorphLargeWidget::ResetSceneMenuItem::createChildMenu() {
    Menu* menu = new Menu;
    createResetSceneMenu(module, menu);
    return menu;
}

void AlgomorphLargeWidget::ResetSceneMenuItem::createResetSceneMenu(AlgomorphLarge* module, Menu* menu) {
    menu->addChild(construct<ResetSceneItem>(&MenuItem::text, "3", &ResetSceneItem::module, module, &ResetSceneItem::rightText, CHECKMARK(module->resetScene == 2), &ResetSceneItem::scene, 2));
    menu->addChild(construct<ResetSceneItem>(&MenuItem::text, "2", &ResetSceneItem::module, module, &ResetSceneItem::rightText, CHECKMARK(module->resetScene == 1), &ResetSceneItem::scene, 1));
    menu->addChild(construct<ResetSceneItem>(&MenuItem::text, "1", &ResetSceneItem::module, module, &ResetSceneItem::rightText, CHECKMARK(module->resetScene == 0), &ResetSceneItem::scene, 0));
}

Menu* AlgomorphLargeWidget::AudioSettingsMenuItem::createChildMenu() {
    Menu* menu = new Menu;
    createAudioSettingsMenu(module, menu);
    return menu;
}

void AlgomorphLargeWidget::WildModSumItem::onAction(const Action &e) {
    // History
    ToggleWildModSumAction<>* h = new ToggleWildModSumAction<>();
    h->moduleId = module->id;

    module->wildModIsSummed ^= true;

    APP->history->push(h);
}

void AlgomorphLargeWidget::AudioSettingsMenuItem::createAudioSettingsMenu(AlgomorphLarge* module, Menu* menu) {
    menu->addChild(construct<ClickFilterMenuItem>(&MenuItem::text, "Click Filter…", &MenuItem::rightText, (module->clickFilterEnabled ? "Enabled ▸" : "Disabled ▸"), &ClickFilterMenuItem::module, module));

    RingMorphItem *ringMorphItem = rack::createMenuItem<RingMorphItem>("Enable Ring Morph", CHECKMARK(module->ringMorph));
    ringMorphItem->module = module;
    menu->addChild(ringMorphItem);

    RunSilencerItem *runSilencerItem = rack::createMenuItem<RunSilencerItem>("Route audio when not running", CHECKMARK(!module->runSilencer));
    runSilencerItem->module = module;
    menu->addChild(runSilencerItem);

    WildModSumItem *wildModSumItem = rack::createMenuItem<WildModSumItem>("Add Wildcard Mod to Mod Sum", CHECKMARK(module->wildModIsSummed));
    wildModSumItem->module = module;
    menu->addChild(wildModSumItem);
}

void AlgomorphLargeWidget::CCWScenesItem::onAction(const Action &e) {
    // History
    ToggleCCWSceneSelectionAction<>* h = new ToggleCCWSceneSelectionAction<>();
    h->moduleId = module->id;

    module->ccwSceneSelection ^= true;

    APP->history->push(h);
}

Menu* AlgomorphLargeWidget::InteractionSettingsMenuItem::createChildMenu() {
    Menu* menu = new Menu;
    createInteractionSettingsMenu(module, menu);
    return menu;
}

void AlgomorphLargeWidget::InteractionSettingsMenuItem::createInteractionSettingsMenu(AlgomorphLarge* module, Menu* menu) {
    menu->addChild(construct<AllowMultipleModesMenuItem>(&MenuItem::text, "Multi-function inputs…", &MenuItem::rightText, std::string(module->auxInput[0]->allowMultipleModes ? "AUX 1" : "") + std::string(module->auxInput[1]->allowMultipleModes ? " AUX 2" : "") + std::string(module->auxInput[2]->allowMultipleModes ? " AUX 3" : "") + std::string(module->auxInput[3]->allowMultipleModes ? " AUX 4" : "") + std::string(module->auxInput[4]->allowMultipleModes ? " AUX 5" : "") + ((!module->auxInput[0]->allowMultipleModes && !module->auxInput[1]->allowMultipleModes && !module->auxInput[2]->allowMultipleModes && !module->auxInput[3]->allowMultipleModes && !module->auxInput[4]->allowMultipleModes) ? "None" : "") + " " + RIGHT_ARROW, &AllowMultipleModesMenuItem::module, module));

    menu->addChild(construct<ResetSceneMenuItem>(&MenuItem::text, "Destination on reset…", &MenuItem::rightText, std::to_string(module->resetScene + 1) + " " + RIGHT_ARROW, &ResetSceneMenuItem::module, module));
    
    CCWScenesItem *ccwScenesItem = rack::createMenuItem<CCWScenesItem>("Reverse clock sequence", CHECKMARK(!module->ccwSceneSelection));
    ccwScenesItem->module = module;
    menu->addChild(ccwScenesItem);
            
    ResetOnRunItem *resetOnRunItem = rack::createMenuItem<ResetOnRunItem>("Reset on run", CHECKMARK(module->resetOnRun));
    resetOnRunItem->module = module;
    menu->addChild(resetOnRunItem);

    ExitConfigItem *exitConfigItem = rack::createMenuItem<ExitConfigItem>("Exit Edit Mode after connection", CHECKMARK(module->exitConfigOnConnect));
    exitConfigItem->module = module;
    menu->addChild(exitConfigItem);
}

void AlgomorphLargeWidget::KnobModeItem::onAction(const Action &e) {
    // History
    KnobModeAction<>* h = new KnobModeAction<>();
    h->moduleId = module->id;
    h->oldKnobMode = module->knobMode;
    h->newKnobMode = mode;

    module->knobMode = mode;

    APP->history->push(h);
}

/// Reset Knobs Menu Item

AlgomorphLargeWidget::ResetKnobsAction::ResetKnobsAction() {
    name = "Delexander Algomorph reset knobs";
}

void AlgomorphLargeWidget::ResetKnobsAction::undo() {
    ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
    assert(mw);
    AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
    assert(m);
    for (int i = 0; i < AuxKnobModes::NUM_MODES; i++)
        m->params[AlgomorphLarge::AUX_KNOBS + i].setValue(knobValue[i]);
}

void AlgomorphLargeWidget::ResetKnobsAction::redo() {
    ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
    assert(mw);
    AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
    assert(m);
    for (int i = 0; i < AuxKnobModes::NUM_MODES; i++)
        m->params[AlgomorphLarge::AUX_KNOBS + i].setValue(DEF_KNOB_VALUES[i]);
}

void AlgomorphLargeWidget::ResetKnobsItem::onAction(const Action &e) {
    // History
    ResetKnobsAction* h = new ResetKnobsAction;
    h->moduleId = module->id;

    for (int i = 0; i < AuxKnobModes::NUM_MODES; i++) {
        h->knobValue[i] = module->params[AlgomorphLarge::AUX_KNOBS + i].getValue();
        module->params[AlgomorphLarge::AUX_KNOBS + i].setValue(DEF_KNOB_VALUES[i]);
    }

    APP->history->push(h);
}

AlgomorphLargeWidget::TogglePhaseOutRangeAction::TogglePhaseOutRangeAction() {
    name = "Delexander Algomorph toggle phase output range";
}

void AlgomorphLargeWidget::TogglePhaseOutRangeAction::undo() {
    ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
    assert(mw);
    AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
    assert(m);
    if (m->phaseMin == 0.f) {
        m->phaseMin = -5.f;
        m->phaseMax = 5.f;
    }
    else {
        m->phaseMin = 0.f;
        m->phaseMax = 10.f;
    }
}

void AlgomorphLargeWidget::TogglePhaseOutRangeAction::redo() {
    ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
    assert(mw);
    AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
    assert(m);
    if (m->phaseMin == 0.f) {
        m->phaseMin = -5.f;
        m->phaseMax = 5.f;
    }
    else {
        m->phaseMin = 0.f;
        m->phaseMax = 10.f;
    }
}

void AlgomorphLargeWidget::PhaseOutRangeItem::onAction(const Action &e) {
    // History
    TogglePhaseOutRangeAction* h = new TogglePhaseOutRangeAction;
    h->moduleId = module->id;

    if (module->phaseMin == 0.f) {
        module->phaseMin = -5.f;
        module->phaseMax = 5.f;
    }
    else {
        module->phaseMin = 0.f;
        module->phaseMax = 10.f;
    }

    APP->history->push(h);
}

Menu* AlgomorphLargeWidget::KnobModeMenuItem::createChildMenu() {
    Menu* menu = new Menu;
    menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[AuxKnobModes::MORPH_ATTEN], &KnobModeItem::module, module, &KnobModeItem::rightText, std::string(CHECKMARK(module->knobMode == AuxKnobModes::MORPH_ATTEN)) + " " + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::MORPH_ATTEN]->getDisplayValueString() + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::MORPH_ATTEN]->getUnit(), &KnobModeItem::mode, AuxKnobModes::MORPH_ATTEN));
    menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[AuxKnobModes::DOUBLE_MORPH_ATTEN], &KnobModeItem::module, module, &KnobModeItem::rightText, std::string(CHECKMARK(module->knobMode == AuxKnobModes::DOUBLE_MORPH_ATTEN)) + " " + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::DOUBLE_MORPH_ATTEN]->getDisplayValueString() + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::DOUBLE_MORPH_ATTEN]->getUnit(), &KnobModeItem::mode, AuxKnobModes::DOUBLE_MORPH_ATTEN));
    menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[AuxKnobModes::TRIPLE_MORPH_ATTEN], &KnobModeItem::module, module, &KnobModeItem::rightText, std::string(CHECKMARK(module->knobMode == AuxKnobModes::TRIPLE_MORPH_ATTEN)) + " " + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::TRIPLE_MORPH_ATTEN]->getDisplayValueString() + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::TRIPLE_MORPH_ATTEN]->getUnit(), &KnobModeItem::mode, AuxKnobModes::TRIPLE_MORPH_ATTEN));
    menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[AuxKnobModes::MOD_GAIN], &KnobModeItem::module, module, &KnobModeItem::rightText, std::string(CHECKMARK(module->knobMode == AuxKnobModes::MOD_GAIN)) + " " + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::MOD_GAIN]->getDisplayValueString() + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::MOD_GAIN]->getUnit(), &KnobModeItem::mode, AuxKnobModes::MOD_GAIN));
    menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[AuxKnobModes::SUM_GAIN], &KnobModeItem::module, module, &KnobModeItem::rightText, std::string(CHECKMARK(module->knobMode == AuxKnobModes::SUM_GAIN)) + " " + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::SUM_GAIN]->getDisplayValueString() + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::SUM_GAIN]->getUnit(), &KnobModeItem::mode, AuxKnobModes::SUM_GAIN));
    menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[AuxKnobModes::OP_GAIN], &KnobModeItem::module, module, &KnobModeItem::rightText, std::string(CHECKMARK(module->knobMode == AuxKnobModes::OP_GAIN)) + " " + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::OP_GAIN]->getDisplayValueString() + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::OP_GAIN]->getUnit(), &KnobModeItem::mode, AuxKnobModes::OP_GAIN));
    menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[AuxKnobModes::WILDCARD_MOD_GAIN], &KnobModeItem::module, module, &KnobModeItem::rightText, std::string(CHECKMARK(module->knobMode == AuxKnobModes::WILDCARD_MOD_GAIN)) + " " + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::WILDCARD_MOD_GAIN]->getDisplayValueString() + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::WILDCARD_MOD_GAIN]->getUnit(), &KnobModeItem::mode, AuxKnobModes::WILDCARD_MOD_GAIN));
    menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[AuxKnobModes::CLICK_FILTER], &KnobModeItem::module, module, &KnobModeItem::rightText, std::string(CHECKMARK(module->knobMode == AuxKnobModes::CLICK_FILTER)) + " " + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::CLICK_FILTER]->getDisplayValueString() + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::CLICK_FILTER]->getUnit(), &KnobModeItem::mode, AuxKnobModes::CLICK_FILTER));
    menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[AuxKnobModes::MORPH], &KnobModeItem::module, module, &KnobModeItem::rightText, std::string(CHECKMARK(module->knobMode == AuxKnobModes::MORPH)) + " " + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::MORPH]->getDisplayValueString() + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::MORPH]->getUnit(), &KnobModeItem::mode, AuxKnobModes::MORPH));
    menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[AuxKnobModes::DOUBLE_MORPH], &KnobModeItem::module, module, &KnobModeItem::rightText, std::string(CHECKMARK(module->knobMode == AuxKnobModes::DOUBLE_MORPH)) + " " + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::DOUBLE_MORPH]->getDisplayValueString() + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::DOUBLE_MORPH]->getUnit(), &KnobModeItem::mode, AuxKnobModes::DOUBLE_MORPH));
    menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[AuxKnobModes::TRIPLE_MORPH], &KnobModeItem::module, module, &KnobModeItem::rightText, std::string(CHECKMARK(module->knobMode == AuxKnobModes::TRIPLE_MORPH)) + " " + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::TRIPLE_MORPH]->getDisplayValueString() + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::TRIPLE_MORPH]->getUnit(), &KnobModeItem::mode, AuxKnobModes::TRIPLE_MORPH));
    menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[AuxKnobModes::UNI_MORPH], &KnobModeItem::module, module, &KnobModeItem::rightText, std::string(CHECKMARK(module->knobMode == AuxKnobModes::UNI_MORPH)) + " " + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::UNI_MORPH]->getDisplayValueString() + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::UNI_MORPH]->getUnit(), &KnobModeItem::mode, AuxKnobModes::UNI_MORPH));
    menu->addChild(construct<KnobModeItem>(&MenuItem::text, AuxKnobModeLabels[AuxKnobModes::ENDLESS_MORPH], &KnobModeItem::module, module, &KnobModeItem::rightText, std::string(CHECKMARK(module->knobMode == AuxKnobModes::ENDLESS_MORPH)) + " " + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::ENDLESS_MORPH]->getDisplayValueString() + module->paramQuantities[AlgomorphLarge::AUX_KNOBS + AuxKnobModes::ENDLESS_MORPH]->getUnit(), &KnobModeItem::mode, AuxKnobModes::ENDLESS_MORPH));
    menu->addChild(new MenuSeparator());
    menu->addChild(construct<ResetKnobsItem>(&MenuItem::text, "Reset all knobs", &ResetKnobsItem::module, module));
    return menu;
}

Menu* AlgomorphLargeWidget::VisualSettingsMenuItem::createChildMenu() {
    Menu* menu = new Menu;
    createVisualSettingsMenu(module, menu);
    return menu;
}

void AlgomorphLargeWidget::VisualSettingsMenuItem::createVisualSettingsMenu(AlgomorphLarge* module, Menu* menu) {  
    VULightsItem *vuLightsItem = rack::createMenuItem<VULightsItem>("Disable VU lighting", CHECKMARK(!module->vuLights));
    vuLightsItem->module = module;
    menu->addChild(vuLightsItem);
    
    // GlowingInkItem *glowingInkItem = rack::createMenuItem<GlowingInkItem>("Enable glowing panel ink", CHECKMARK(module->glowingInk));
    // glowingInkItem->module = module;
    // menu->addChild(glowingInkItem);
}

AlgomorphLargeWidget::AlgomorphLargeWidget(AlgomorphLarge* module) {
    setModule(module);
    
    if (module)
        setPanel(APP->window->loadSvg(rack::asset::plugin(pluginInstance, "res/AlgomorphLarge.svg")));
    else
        setPanel(APP->window->loadSvg(rack::asset::plugin(pluginInstance, "res/AlgomorphLarge_ModuleBrowser.svg")));

    addChild(rack::createWidget<DLXGameBitBlack>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(rack::createWidget<DLXGameBitBlack>(Vec(box.size.x - RACK_GRID_WIDTH * 2, 0)));
    addChild(rack::createWidget<DLXGameBitBlack>(Vec(RACK_GRID_WIDTH, 365)));
    addChild(rack::createWidget<DLXGameBitBlack>(Vec(box.size.x - RACK_GRID_WIDTH * 2, 365)));

    // ink = rack::createWidget<AlgomorphLargeGlowingInk>(Vec(0,0));
    // if (!module->glowingInk)
    //     ink->hide();
    // addChild(ink);

    AlgomorphDisplayWidget<>* screenWidget = new AlgomorphDisplayWidget<>(module);
    screenWidget->box.pos = mm2px(Vec(16.411, 11.631));
    screenWidget->box.size = mm2px(Vec(38.295, 31.590));
    addChild(screenWidget);
    //Place backlight _above_ in scene in order for brightness to affect screenWidget
    addChild(createBacklight<DLXScreenMultiLight>(mm2px(Vec(16.411, 11.631)), mm2px(Vec(38.295, 31.590)), module, AlgomorphLarge::DISPLAY_BACKLIGHT));

    AlgomorphAuxInputPanelWidget* auxPanelWidget = new AlgomorphAuxInputPanelWidget(module);
    auxPanelWidget->box.pos = mm2px(Vec(2.750, 13.570));
    auxPanelWidget->box.size = mm2px(Vec(9.057, 61.191));
    addChild(auxPanelWidget);

    addChild(createRingLightCentered<DLXMultiLight>(mm2px(Vec(35.428, 49.297)), module, AlgomorphLarge::SCREEN_BUTTON_RING_LIGHT));
    addParam(rack::createParamCentered<DLXPurpleButton>(mm2px(Vec(35.428, 49.297)), module, AlgomorphLarge::SCREEN_BUTTON));
    addChild(rack::createParamCentered<DLXScreenButtonLight>(mm2px(Vec(35.428, 49.297)), module, AlgomorphLarge::SCREEN_BUTTON));

    addChild(createRingLightCentered<DLXMultiLight>(SceneButtonCenters[0], module, AlgomorphLarge::SCENE_LIGHTS + 0));
    addChild(createRingIndicatorCentered<Algomorph<>>(SceneButtonCenters[0], module, AlgomorphLarge::SCENE_INDICATORS + 0));
    addParam(rack::createParamCentered<rack::componentlibrary::TL1105>(SceneButtonCenters[0], module, AlgomorphLarge::SCENE_BUTTONS + 0));
    addChild(rack::createParamCentered<DLX1ButtonLight>(SceneButtonCenters[0], module, AlgomorphLarge::SCENE_BUTTONS + 0));

    addChild(createRingLightCentered<DLXMultiLight>(SceneButtonCenters[1], module, AlgomorphLarge::SCENE_LIGHTS + 3));
    addChild(createRingIndicatorCentered<Algomorph<>>(SceneButtonCenters[1], module, AlgomorphLarge::SCENE_INDICATORS + 3));
    addParam(rack::createParamCentered<rack::componentlibrary::TL1105>(SceneButtonCenters[1], module, AlgomorphLarge::SCENE_BUTTONS + 1));
    addChild(rack::createParamCentered<DLX2ButtonLight>(SceneButtonCenters[1], module, AlgomorphLarge::SCENE_BUTTONS + 1));

    addChild(createRingLightCentered<DLXMultiLight>(SceneButtonCenters[2], module, AlgomorphLarge::SCENE_LIGHTS + 6));
    addChild(createRingIndicatorCentered<Algomorph<>>(SceneButtonCenters[2], module, AlgomorphLarge::SCENE_INDICATORS + 6));
    addParam(rack::createParamCentered<rack::componentlibrary::TL1105>(SceneButtonCenters[2], module, AlgomorphLarge::SCENE_BUTTONS + 2));
    addChild(rack::createParamCentered<DLX3ButtonLight>(SceneButtonCenters[2], module, AlgomorphLarge::SCENE_BUTTONS + 2));

    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(7.285, 19.768)), module, AlgomorphLarge::AUX_INPUTS + 0));
    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(7.285, 32.531)), module, AlgomorphLarge::AUX_INPUTS + 1));
    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(7.285, 45.295)), module, AlgomorphLarge::AUX_INPUTS + 2));
    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(7.285, 58.058)), module, AlgomorphLarge::AUX_INPUTS + 3));
    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(7.285, 70.822)), module, AlgomorphLarge::AUX_INPUTS + 4));

    addParam(rack::createParamCentered<DLXLargeLightKnob<DLXDonutLargeKnobLight<DLXSmallLightKnob>>>(mm2px(Vec(35.559, 107.553)), module, AlgomorphLarge::MORPH_KNOB));

    if (module) {
        for (int i = 0; i < AuxKnobModes::NUM_MODES; i++) {
            DLXSmallLightKnob* auxKlParam = rack::createParamCentered<DLXSmallLightKnob>(mm2px(Vec(35.559, 107.553)), module, AlgomorphLarge::AUX_KNOBS + i);
            if (i != module->knobMode) {
                auxKlParam->hide();
            }
            addParam(auxKlParam);
        }
    }
    else {
            DLXSmallLightKnob* auxKlParam = rack::createParamCentered<DLXSmallLightKnob>(mm2px(Vec(35.559, 107.553)), module, AlgomorphLarge::AUX_KNOBS + 0);
            addParam(auxKlParam);
    }

    addOutput(rack::createOutputCentered<DLXPJ301MPort>(mm2px(Vec(63.842, 49.560)), module, AlgomorphLarge::PHASE_OUTPUT));
    addOutput(rack::createOutputCentered<DLXPJ301MPort>(mm2px(Vec(63.842, 60.141)), module, AlgomorphLarge::MODULATOR_SUM_OUTPUT));
    addOutput(rack::createOutputCentered<DLXPJ301MPort>(mm2px(Vec(63.842, 70.723)), module, AlgomorphLarge::CARRIER_SUM_OUTPUT));

    addChild(createRingLightCentered<DLXYellowLight>(mm2px(Vec(35.428, 91.561)), module, AlgomorphLarge::EDIT_LIGHT));
    addChild(rack::createParamCentered<DLXPurpleButton>(mm2px(Vec(35.428, 91.561)), module, AlgomorphLarge::EDIT_BUTTON));
    addChild(rack::createParamCentered<DLXPencilButtonLight>(mm2px(Vec(35.428, 91.561)), module, AlgomorphLarge::EDIT_BUTTON));

    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(7.285, 81.785)), module, AlgomorphLarge::OPERATOR_INPUTS + 3));
    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(7.285, 92.106)), module, AlgomorphLarge::OPERATOR_INPUTS + 2));
    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(7.285, 102.428)), module, AlgomorphLarge::OPERATOR_INPUTS + 1));
    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(7.285, 112.749)), module, AlgomorphLarge::OPERATOR_INPUTS + 0));

    addOutput(rack::createOutputCentered<DLXPJ301MPort>(mm2px(Vec(63.842, 81.785)), module, AlgomorphLarge::MODULATOR_OUTPUTS + 3));
    addOutput(rack::createOutputCentered<DLXPJ301MPort>(mm2px(Vec(63.842, 92.106)), module, AlgomorphLarge::MODULATOR_OUTPUTS + 2));
    addOutput(rack::createOutputCentered<DLXPJ301MPort>(mm2px(Vec(63.842, 102.428)), module, AlgomorphLarge::MODULATOR_OUTPUTS + 1));
    addOutput(rack::createOutputCentered<DLXPJ301MPort>(mm2px(Vec(63.842, 112.749)), module, AlgomorphLarge::MODULATOR_OUTPUTS + 0));

    ConnectionBgWidget<>* connectionBgWidget = new ConnectionBgWidget<>(OpButtonCenters, ModButtonCenters, module);
    connectionBgWidget->box.pos = OpButtonCenters[3];
    connectionBgWidget->box.size = ModButtonCenters[0].minus(OpButtonCenters[3]);
    addChild(connectionBgWidget);

    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[3], ModButtonCenters[3], module, AlgomorphLarge::H_CONNECTION_LIGHTS + 9));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[2], ModButtonCenters[2], module, AlgomorphLarge::H_CONNECTION_LIGHTS + 6));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[1], ModButtonCenters[1], module, AlgomorphLarge::H_CONNECTION_LIGHTS + 3));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[0], ModButtonCenters[0], module, AlgomorphLarge::H_CONNECTION_LIGHTS + 0));

    addChild(createLineLight<DLXRedLight>(OpButtonCenters[0], ModButtonCenters[3], module, AlgomorphLarge::D_DISABLE_LIGHTS + 2));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[0], ModButtonCenters[2], module, AlgomorphLarge::D_DISABLE_LIGHTS + 1));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[0], ModButtonCenters[1], module, AlgomorphLarge::D_DISABLE_LIGHTS + 0));

    addChild(createLineLight<DLXRedLight>(OpButtonCenters[1], ModButtonCenters[3], module, AlgomorphLarge::D_DISABLE_LIGHTS + 5));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[1], ModButtonCenters[2], module, AlgomorphLarge::D_DISABLE_LIGHTS + 4));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[1], ModButtonCenters[0], module, AlgomorphLarge::D_DISABLE_LIGHTS + 3));

    addChild(createLineLight<DLXRedLight>(OpButtonCenters[2], ModButtonCenters[3], module, AlgomorphLarge::D_DISABLE_LIGHTS + 8));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[2], ModButtonCenters[1], module, AlgomorphLarge::D_DISABLE_LIGHTS + 7));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[2], ModButtonCenters[0], module, AlgomorphLarge::D_DISABLE_LIGHTS + 6));

    addChild(createLineLight<DLXRedLight>(OpButtonCenters[3], ModButtonCenters[2], module, AlgomorphLarge::D_DISABLE_LIGHTS + 11));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[3], ModButtonCenters[1], module, AlgomorphLarge::D_DISABLE_LIGHTS + 10));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[3], ModButtonCenters[0], module, AlgomorphLarge::D_DISABLE_LIGHTS + 9));

    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[0], ModButtonCenters[3], module, AlgomorphLarge::CONNECTION_LIGHTS + 6));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[0], ModButtonCenters[2], module, AlgomorphLarge::CONNECTION_LIGHTS + 3));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[0], ModButtonCenters[1], module, AlgomorphLarge::CONNECTION_LIGHTS + 0));

    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[1], ModButtonCenters[3], module, AlgomorphLarge::CONNECTION_LIGHTS + 15));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[1], ModButtonCenters[2], module, AlgomorphLarge::CONNECTION_LIGHTS + 12));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[1], ModButtonCenters[0], module, AlgomorphLarge::CONNECTION_LIGHTS + 9));

    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[2], ModButtonCenters[3], module, AlgomorphLarge::CONNECTION_LIGHTS + 24));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[2], ModButtonCenters[1], module, AlgomorphLarge::CONNECTION_LIGHTS + 21));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[2], ModButtonCenters[0], module, AlgomorphLarge::CONNECTION_LIGHTS + 18));

    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[3], ModButtonCenters[2], module, AlgomorphLarge::CONNECTION_LIGHTS + 33));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[3], ModButtonCenters[1], module, AlgomorphLarge::CONNECTION_LIGHTS + 30));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[3], ModButtonCenters[0], module, AlgomorphLarge::CONNECTION_LIGHTS + 27));
    
    addChild(createRingLightCentered<DLXMultiLight>(OpButtonCenters[3], module, AlgomorphLarge::OPERATOR_LIGHTS + 9));
    addChild(createRingLightCentered<DLXMultiLight>(OpButtonCenters[2], module, AlgomorphLarge::OPERATOR_LIGHTS + 6));
    addChild(createRingLightCentered<DLXMultiLight>(OpButtonCenters[1], module, AlgomorphLarge::OPERATOR_LIGHTS + 3));
    addChild(createRingLightCentered<DLXMultiLight>(OpButtonCenters[0], module, AlgomorphLarge::OPERATOR_LIGHTS + 0));

    addChild(createRingIndicatorCentered<Algomorph<>>(OpButtonCenters[3], module, AlgomorphLarge::CARRIER_INDICATORS + 9));
    addChild(createRingIndicatorCentered<Algomorph<>>(OpButtonCenters[2], module, AlgomorphLarge::CARRIER_INDICATORS + 6));
    addChild(createRingIndicatorCentered<Algomorph<>>(OpButtonCenters[1], module, AlgomorphLarge::CARRIER_INDICATORS + 3));
    addChild(createRingIndicatorCentered<Algomorph<>>(OpButtonCenters[0], module, AlgomorphLarge::CARRIER_INDICATORS + 0));

    addChild(createRingLightCentered<DLXMultiLight>(ModButtonCenters[3], module, AlgomorphLarge::MODULATOR_LIGHTS + 9));
    addChild(createRingLightCentered<DLXMultiLight>(ModButtonCenters[2], module, AlgomorphLarge::MODULATOR_LIGHTS + 6));
    addChild(createRingLightCentered<DLXMultiLight>(ModButtonCenters[1], module, AlgomorphLarge::MODULATOR_LIGHTS + 3));
    addChild(createRingLightCentered<DLXMultiLight>(ModButtonCenters[0], module, AlgomorphLarge::MODULATOR_LIGHTS + 0));

    addParam(rack::createParamCentered<DLXPurpleButton>(OpButtonCenters[3], module, AlgomorphLarge::OPERATOR_BUTTONS + 3));
    addParam(rack::createParamCentered<DLXPurpleButton>(OpButtonCenters[2], module, AlgomorphLarge::OPERATOR_BUTTONS + 2));
    addParam(rack::createParamCentered<DLXPurpleButton>(OpButtonCenters[1], module, AlgomorphLarge::OPERATOR_BUTTONS + 1));
    addParam(rack::createParamCentered<DLXPurpleButton>(OpButtonCenters[0], module, AlgomorphLarge::OPERATOR_BUTTONS + 0));

    addParam(rack::createParamCentered<DLXPurpleButton>(ModButtonCenters[3], module, AlgomorphLarge::MODULATOR_BUTTONS + 3));
    addParam(rack::createParamCentered<DLXPurpleButton>(ModButtonCenters[2], module, AlgomorphLarge::MODULATOR_BUTTONS + 2));
    addParam(rack::createParamCentered<DLXPurpleButton>(ModButtonCenters[1], module, AlgomorphLarge::MODULATOR_BUTTONS + 1));
    addParam(rack::createParamCentered<DLXPurpleButton>(ModButtonCenters[0], module, AlgomorphLarge::MODULATOR_BUTTONS + 0));        
}

void AlgomorphLargeWidget::appendContextMenu(Menu* menu) {
    AlgomorphLarge* module = dynamic_cast<AlgomorphLarge*>(this->module);


    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "AUX Knob"));
    menu->addChild(construct<KnobModeMenuItem>(&MenuItem::text, "Function…", &MenuItem::rightText, AuxKnobModeLabels[module->knobMode] + " " + RIGHT_ARROW, &KnobModeMenuItem::module, module));

    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "AUX Inputs"));

    menu->addChild(construct<AuxInputModeMenuItem>(&MenuItem::text, "AUX 1…", &MenuItem::rightText, (module->auxInput[0]->activeModes > 1 ? "Multiple" : AuxInputModeLabels[module->auxInput[0]->lastSetMode]) + " " + RIGHT_ARROW, &AuxInputModeMenuItem::module, module, &AuxInputModeMenuItem::auxIndex, 0));
    menu->addChild(construct<AuxInputModeMenuItem>(&MenuItem::text, "AUX 2…", &MenuItem::rightText, (module->auxInput[1]->activeModes > 1 ? "Multiple" : AuxInputModeLabels[module->auxInput[1]->lastSetMode]) + " " + RIGHT_ARROW, &AuxInputModeMenuItem::module, module, &AuxInputModeMenuItem::auxIndex, 1));
    menu->addChild(construct<AuxInputModeMenuItem>(&MenuItem::text, "AUX 3…",  &MenuItem::rightText, (module->auxInput[2]->activeModes > 1 ? "Multiple" : AuxInputModeLabels[module->auxInput[2]->lastSetMode]) + " " + RIGHT_ARROW, &AuxInputModeMenuItem::module, module, &AuxInputModeMenuItem::auxIndex, 2));
    menu->addChild(construct<AuxInputModeMenuItem>(&MenuItem::text, "AUX 4…", &MenuItem::rightText, (module->auxInput[3]->activeModes > 1 ? "Multiple" : AuxInputModeLabels[module->auxInput[3]->lastSetMode]) + " " + RIGHT_ARROW, &AuxInputModeMenuItem::module, module, &AuxInputModeMenuItem::auxIndex, 3));
    menu->addChild(construct<AuxInputModeMenuItem>(&MenuItem::text, "AUX 5…", &MenuItem::rightText, (module->auxInput[4]->activeModes > 1 ? "Multiple" : AuxInputModeLabels[module->auxInput[4]->lastSetMode]) + " " + RIGHT_ARROW, &AuxInputModeMenuItem::module, module, &AuxInputModeMenuItem::auxIndex, 4));

    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Settings"));
    menu->addChild(construct<PhaseOutRangeItem>(&MenuItem::text, "Phase output range", &MenuItem::rightText, module->phaseMin == 0.f ? "0-10V" : "+/-5V", &PhaseOutRangeItem::module, module));
    menu->addChild(construct<AudioSettingsMenuItem>(&MenuItem::text, "Audio…", &MenuItem::rightText, RIGHT_ARROW, &AudioSettingsMenuItem::module, module));
    menu->addChild(construct<InteractionSettingsMenuItem>(&MenuItem::text, "Interaction…", &MenuItem::rightText, RIGHT_ARROW, &InteractionSettingsMenuItem::module, module));
    menu->addChild(construct<VisualSettingsMenuItem>(&MenuItem::text, "Visual…", &MenuItem::rightText, RIGHT_ARROW, &VisualSettingsMenuItem::module, module));

    menu->addChild(new MenuSeparator());
    
    ToggleModeBItem *toggleModeBItem = rack::createMenuItem<ToggleModeBItem>("Alter Ego", CHECKMARK(module->modeB));
    toggleModeBItem->module = module;
    menu->addChild(toggleModeBItem);
    
    menu->addChild(new MenuSeparator());
    
    SaveAuxInputSettingsItem *saveAuxInputSettingsItem = rack::createMenuItem<SaveAuxInputSettingsItem>("Save AUX input modes as default", CHECKMARK(module->auxInputsAreDefault()));
    saveAuxInputSettingsItem->module = module;
    menu->addChild(saveAuxInputSettingsItem);
    
    // SaveVisualSettingsItem *saveVisualSettingsItem = rack::createMenuItem<SaveVisualSettingsItem>("Save visual settings as default", CHECKMARK(module->glowingInk == pluginSettings.glowingInkDefault && module->vuLights == pluginSettings.vuLightsDefault));
    SaveVisualSettingsItem *saveVisualSettingsItem = rack::createMenuItem<SaveVisualSettingsItem>("Save visual settings as default", CHECKMARK(module->vuLights == pluginSettings.vuLightsDefault));
    saveVisualSettingsItem->module = module;
    menu->addChild(saveVisualSettingsItem);

    // menu->addChild(new MenuSeparator());

    // DebugItem *debugItem = rack::createMenuItem<DebugItem>("The system is down", CHECKMARK(module->debug));
    // debugItem->module = module;
    // menu->addChild(debugItem);
}

void AlgomorphLargeWidget::setKnobMode(int mode) {
    if (!module)
        return;

    DLXSmallLightKnob* oldKnob = dynamic_cast<DLXSmallLightKnob*>(getParam(AlgomorphLarge::AUX_KNOBS + activeKnob));
    oldKnob->hide();
    // oldKnob->sibling->hide();
    DLXSmallLightKnob* newKnob = dynamic_cast<DLXSmallLightKnob*>(getParam(AlgomorphLarge::AUX_KNOBS + mode));
    newKnob->show();
    // newKnob->sibling->show();
    activeKnob = mode;
}

void AlgomorphLargeWidget::step() {
    if (module) {
        AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(module);
        // ink->visible = m->glowingInk == 1;
        if (activeKnob != m->knobMode)
            setKnobMode(m->knobMode);
    }
    ModuleWidget::step();
}

Model* modelAlgomorphLarge = createModel<AlgomorphLarge, AlgomorphLargeWidget>("Algomorph");
