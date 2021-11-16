#include "AlgomorphSmall.hpp"
#include "AlgomorphHistory.hpp"
#include "Components.hpp"
#include "AlgomorphDisplayWidget.hpp"
#include "ConnectionBgWidget.hpp"
#include "plugin.hpp" // For constants
#include <rack.hpp>
using rack::math::crossfade;
using rack::construct;
using rack::app::RACK_GRID_WIDTH;
using rack::ui::MenuSeparator;
using rack::ui::MenuLabel;


AlgomorphSmall::AlgomorphSmall() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configParam(MORPH_KNOB, -1.f, 1.f, 0.f, "Morph", " millimorphs", 0, 1000);
    configParam(MORPH_ATTEN_KNOB, -3.f, 3.f, 0.f, "Morph CV Triple Ampliverter", "%", 0, 100);
    for (int i = 0; i < 4; i++) {
        configButton(OPERATOR_BUTTONS + i, "Operator " + std::to_string(i + 1) + " button");
        configButton(MODULATOR_BUTTONS + i, "Modulator " + std::to_string(i + 1) + " button");
    }
    for (int i = 0; i < 3; i++) {
        configButton(SCENE_BUTTONS + i, "Algorithm " + std::to_string(i + 1) + " button");
    }
    configButton(EDIT_BUTTON, "Edit button");
    configButton(SCREEN_BUTTON, "Screen button (out of service)");

    configInput(WILDCARD_INPUT, "Wildcard Modulation");
    configInput(OPERATOR_INPUTS + 0, "Operator 1");
    configInput(OPERATOR_INPUTS + 1, "Operator 2");
    configInput(OPERATOR_INPUTS + 2, "Operator 3");
    configInput(OPERATOR_INPUTS + 3, "Operator 4");
    configInput(MORPH_INPUTS + 0, "Morph CV A");
    configInput(MORPH_INPUTS + 1, "Morph CV B");

    configOutput(MODULATOR_OUTPUTS + 0, "Modulator 1");
    configOutput(MODULATOR_OUTPUTS + 1, "Modulator 2");
    configOutput(MODULATOR_OUTPUTS + 2, "Modulator 3");
    configOutput(MODULATOR_OUTPUTS + 3, "Modulator 4");
    configOutput(CARRIER_SUM_OUTPUT, "Carrier Sum");
    
    onReset();
}

void AlgomorphSmall::onReset() {
    morphMult[0] = 1.f;
    morphMult[1] = 2.f;
    gain = 1.f;
    Algomorph::onReset();
}

void AlgomorphSmall::process(const ProcessArgs& args) {
    float in[16] = {0.f};                                   // Operator input channels
    float modOut[4][16] = {{0.f}};                          // Modulator outputs & channels
    float sumOut[16] = {0.f};                               // Sum output channels
    int sceneOffset[16] = {0};                               //Offset to the base scene
    int channels = 1;                                       // Max channels of operator inputs
    bool processCV = cvDivider.process();

    //Determine polyphony count
    for (int i = 0; i < 4; i++) {
        if (channels < inputs[OPERATOR_INPUTS + i].getChannels())
            channels = inputs[OPERATOR_INPUTS + i].getChannels();
    }

    if (processCV) {
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
                        AlgorithmSceneChangeAction* h = new AlgorithmSceneChangeAction;
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

    //  Update morph status
    float morphFromKnob = params[MORPH_KNOB].getValue();
    float morphAttenuversion = params[MORPH_ATTEN_KNOB].getValue();
    // Only redraw display if morph on channel 1 has changed
    float newMorph0 =   + morphFromKnob
                        + ((inputs[MORPH_INPUTS + 0].getVoltage() * morphMult[0]
                        + inputs[MORPH_INPUTS + 1].getVoltage() * morphMult[1])
                        / 5.f)
                        * morphAttenuversion;
    while (newMorph0 > 3.f)
        newMorph0 = -3.f + (newMorph0 - 3.f);
    while (newMorph0 < -3.f)
        newMorph0 = 3.f + (newMorph0 + 3.f);
    if (morph[0] != newMorph0) {
        morph[0] = newMorph0;
        graphDirty = true;
    }
    // morph[0] was just processed, so start this loop with [1]
    for (int c = 1; c < channels; c++) {
        morph[c] =  + morphFromKnob
                    + (inputs[MORPH_INPUTS + 0].getPolyVoltage(c) * morphMult[0]
                    + inputs[MORPH_INPUTS + 1].getPolyVoltage(c) * morphMult[1])
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
                    AlgorithmHorizontalChangeAction* h = new AlgorithmHorizontalChangeAction;
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
                        if (modulatorTrigger[threeToFour[configOp][mod]].process(params[MODULATOR_BUTTONS + threeToFour[configOp][mod]].getValue() > 0.f)) {
                            // History
                            AlgorithmDiagonalChangeAction* h = new AlgorithmDiagonalChangeAction;
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
                        AlgorithmForcedCarrierChangeAction* h = new AlgorithmForcedCarrierChangeAction;
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
                    AlgorithmForcedCarrierChangeAction* h = new AlgorithmForcedCarrierChangeAction;
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
    
    //Get operator input channel then route to modulation output channel or to sum output channel
    float wildcardMod[16] = {0.f};
    for (int c = 0; c < channels; c++) {
        if (ringMorph) {
            for (int i = 0; i < 4; i++) {
                if (inputs[OPERATOR_INPUTS + i].isConnected()) {
                    in[c] = inputs[OPERATOR_INPUTS + i].getPolyVoltage(c);
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
            }
        }
        else {
            for (int i = 0; i < 4; i++) {
                if (inputs[OPERATOR_INPUTS + i].isConnected()) {
                    in[c] = inputs[OPERATOR_INPUTS + i].getPolyVoltage(c);
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
        wildcardMod[c] += inputs[WILDCARD_INPUT].getPolyVoltage(c);
        for (int mod = 0; mod < 4; mod++) {
            modOut[mod][c] += wildcardMod[c];
            modOut[mod][c] *= gain;
        }
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
        outputs[CARRIER_SUM_OUTPUT].writeVoltages(sumOut);
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
                        algoName[configScene].test(configOp * 3 + fourToThree[configOp][i]) ?
                            blinkStatus ?
                                0.f
                                : getOutputBrightness(MODULATOR_OUTPUTS + i)
                            : getOutputBrightness(MODULATOR_OUTPUTS + i)
                        : getOutputBrightness(MODULATOR_OUTPUTS + i), args.sampleTime * lightDivider.getDivision());
                    //Yellow lights
                    lights[MODULATOR_LIGHTS + i * 3 + 1].setSmoothBrightness(configOp > -1 ?
                        (algoName[configScene].test(configOp * 3 + fourToThree[configOp][i]) ?
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
}

float AlgomorphSmall::routeHorizontal(float sampleTime, float inputVoltage, int op, int c) {
    float connectionA = horizontalMarks[centerMorphScene[c]].test(op) * (1.f - relativeMorphMagnitude[c]);
    float connectionB = horizontalMarks[forwardMorphScene[c]].test(op) * (relativeMorphMagnitude[c]);
    float morphedConnection = connectionA + connectionB;
    modClickGain[op][op][c] = clickFilterEnabled ? modClickFilters[op][op][c].process(sampleTime, morphedConnection) : morphedConnection;
    return inputVoltage  * modClickGain[op][op][c];
}

float AlgomorphSmall::routeHorizontalRing(float sampleTime, float inputVoltage, int op, int c) {
    float connection = horizontalMarks[backwardMorphScene[c]].test(op) * relativeMorphMagnitude[c];
    modRingClickGain[op][op][c] = clickFilterEnabled ? modRingClickFilters[op][op][c].process(sampleTime, connection) : connection;
    return -inputVoltage * modRingClickGain[op][op][c];
}

float AlgomorphSmall::routeDiagonal(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connectionA = algoName[centerMorphScene[c]].test(op * 3 + mod)   * (1.f - relativeMorphMagnitude[c])  * !horizontalMarks[centerMorphScene[c]].test(op);
    float connectionB = algoName[forwardMorphScene[c]].test(op * 3 + mod)  * relativeMorphMagnitude[c]          * !horizontalMarks[forwardMorphScene[c]].test(op);
    float morphedConnection = connectionA + connectionB;
    modClickGain[op][threeToFour[op][mod]][c] = clickFilterEnabled ? modClickFilters[op][threeToFour[op][mod]][c].process(sampleTime, morphedConnection) : morphedConnection;
    return inputVoltage * modClickGain[op][threeToFour[op][mod]][c];
}

float AlgomorphSmall::routeDiagonalB(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connectionA = algoName[centerMorphScene[c]].test(op * 3 + mod)   * (1.f - relativeMorphMagnitude[c]);
    float connectionB = algoName[forwardMorphScene[c]].test(op * 3 + mod)  * relativeMorphMagnitude[c];
    float morphedConnection = connectionA + connectionB;
    modClickGain[op][threeToFour[op][mod]][c] = clickFilterEnabled ? modClickFilters[op][threeToFour[op][mod]][c].process(sampleTime, morphedConnection) : morphedConnection;
    return inputVoltage * modClickGain[op][threeToFour[op][mod]][c];
}

float AlgomorphSmall::routeDiagonalRing(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connection = algoName[backwardMorphScene[c]].test(op * 3 + mod) * relativeMorphMagnitude[c] * !horizontalMarks[backwardMorphScene[c]].test(op);
    modRingClickGain[op][threeToFour[op][mod]][c] = clickFilterEnabled ? modRingClickFilters[op][threeToFour[op][mod]][c].process(sampleTime, connection) : connection; 
    return -inputVoltage * modRingClickGain[op][threeToFour[op][mod]][c];
}

float AlgomorphSmall::routeDiagonalRingB(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connection = algoName[backwardMorphScene[c]].test(op * 3 + mod) * relativeMorphMagnitude[c];
    modRingClickGain[op][threeToFour[op][mod]][c] = clickFilterEnabled ? modRingClickFilters[op][threeToFour[op][mod]][c].process(sampleTime, connection) : connection; 
    return -inputVoltage * modRingClickGain[op][threeToFour[op][mod]][c];
}

float AlgomorphSmall::routeSum(float sampleTime, float inputVoltage, int op, int c) {
    float connection    =   carrier[centerMorphScene[c]].test(op)     * (1.f - relativeMorphMagnitude[c])  * !horizontalMarks[centerMorphScene[c]].test(op)
                        +   carrier[forwardMorphScene[c]].test(op)    * relativeMorphMagnitude[c]          * !horizontalMarks[forwardMorphScene[c]].test(op);
    sumClickGain[op][c] = clickFilterEnabled ? sumClickFilters[op][c].process(sampleTime, connection) : connection;
    return inputVoltage * sumClickGain[op][c];
}

float AlgomorphSmall::routeSumB(float sampleTime, float inputVoltage, int op, int c) {
    float connection    =   carrier[centerMorphScene[c]].test(op)     * (1.f - relativeMorphMagnitude[c])
                        +   carrier[forwardMorphScene[c]].test(op)    * relativeMorphMagnitude[c];
    sumClickGain[op][c] = clickFilterEnabled ? sumClickFilters[op][c].process(sampleTime, connection) : connection;
    return inputVoltage * sumClickGain[op][c];
}

float AlgomorphSmall::routeSumRing(float sampleTime, float inputVoltage, int op, int c) {
    float connection = carrier[backwardMorphScene[c]].test(op) * relativeMorphMagnitude[c] * !horizontalMarks[backwardMorphScene[c]].test(op);
    sumRingClickGain[op][c] = clickFilterEnabled ? sumRingClickFilters[op][c].process(sampleTime, connection) : connection;
    return -inputVoltage * sumRingClickGain[op][c];
}

float AlgomorphSmall::routeSumRingB(float sampleTime, float inputVoltage, int op, int c) {
    float connection = carrier[backwardMorphScene[c]].test(op) * relativeMorphMagnitude[c];
    sumRingClickGain[op][c] = clickFilterEnabled ? sumRingClickFilters[op][c].process(sampleTime, connection) : connection;
    return -inputVoltage * sumRingClickGain[op][c];
}

void AlgomorphSmall::updateSceneBrightnesses() {
    if (modeB) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                //Op lights
                //Purple lights
                sceneBrightnesses[i][j][0] = getInputBrightness(AlgomorphSmall::InputIds::OPERATOR_INPUTS + j);

                //Mod lights
                //Purple lights
                sceneBrightnesses[i][j + 4][0] = getOutputBrightness(AlgomorphSmall::OutputIds::MODULATOR_OUTPUTS + j);

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
                    sceneBrightnesses[i][j][0] = getInputBrightness(AlgomorphSmall::InputIds::OPERATOR_INPUTS + j);
                    //Red lights
                    sceneBrightnesses[i][j][2] = 0.f;

                    //Mod lights
                    //Purple lights
                    sceneBrightnesses[i][j + 4][0] = getOutputBrightness(AlgomorphSmall::OutputIds::MODULATOR_OUTPUTS + j);

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
                    sceneBrightnesses[i][j + 4][0] = getOutputBrightness(AlgomorphSmall::OutputIds::MODULATOR_OUTPUTS + j);

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

json_t* AlgomorphSmall::dataToJson() {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "Config Enabled", json_boolean(configMode));
    json_object_set_new(rootJ, "Config Mode", json_integer(configOp));
    json_object_set_new(rootJ, "Config Scene", json_integer(configScene));
    json_object_set_new(rootJ, "Current Scene", json_integer(baseScene));
    json_object_set_new(rootJ, "Horizontal Allowed", json_boolean(modeB));
    json_object_set_new(rootJ, "Ring Morph", json_boolean(ringMorph));
    json_object_set_new(rootJ, "Randomize Ring Morph", json_boolean(randomRingMorph));
    json_object_set_new(rootJ, "Auto Exit", json_boolean(exitConfigOnConnect));
    json_object_set_new(rootJ, "Click Filter Enabled", json_boolean(clickFilterEnabled));
    // json_object_set_new(rootJ, "Glowing Ink", json_boolean(glowingInk));
    json_object_set_new(rootJ, "VU Lights", json_boolean(vuLights));
    json_object_set_new(rootJ, "Mod Gain", json_real(gain));
    json_object_set_new(rootJ, "Morph CV 1 Multiplier", json_real(morphMult[0]));
    json_object_set_new(rootJ, "Morph CV 2 Multiplier", json_real(morphMult[1]));

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
        json_t* sceneForcedCarriersJ = json_object();
        json_object_set_new(sceneForcedCarriersJ, (std::string("Algorithm ") + std::to_string(scene)).c_str(), json_integer(forcedCarriers[scene].to_ullong()));
        json_array_append_new(forcedCarriersJ, sceneForcedCarriersJ);
    }
    json_object_set_new(rootJ, "Algorithms: Forced Carriers", forcedCarriersJ);

    return rootJ;
}

void AlgomorphSmall::dataFromJson(json_t* rootJ) {
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
        
    auto ringMorph = json_object_get(rootJ, "Ring Morph");
    if (ringMorph)
        this->ringMorph = json_boolean_value(ringMorph);

    auto randomRingMorph = json_object_get(rootJ, "Randomize Ring Morph");
    if (randomRingMorph)
        this->randomRingMorph = json_boolean_value(randomRingMorph);

    auto exitConfigOnConnect = json_object_get(rootJ, "Auto Exit");
    if (exitConfigOnConnect)
        this->exitConfigOnConnect = json_boolean_value(exitConfigOnConnect);

    auto clickFilterEnabled = json_object_get(rootJ, "Click Filter Enabled");
    if (clickFilterEnabled)
        this->clickFilterEnabled = json_boolean_value(clickFilterEnabled);

    // auto glowingInk = json_object_get(rootJ, "Glowing Ink");
    // if (glowingInk)
    //     this->glowingInk = json_boolean_value(glowingInk);

    auto vuLights = json_object_get(rootJ, "VU Lights");
    if (vuLights)
        this->vuLights = json_boolean_value(vuLights);

    auto gain = json_object_get(rootJ, "Mod Gain");
    if (gain)
        this->gain = json_real_value(gain);

    auto morphMult1 = json_object_get(rootJ, "Morph CV 1 Multiplier");
    if (morphMult1)
        this->morphMult[0] = json_real_value(morphMult1);

    auto morphMult2 = json_object_get(rootJ, "Morph CV 2 Multiplier");
    if (morphMult2)
        this->morphMult[1] = json_real_value(morphMult2);

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
        json_t* sceneForcedCarriers; size_t sceneIndex;
        json_array_foreach(forcedCarriersJ, sceneIndex, sceneForcedCarriers) {
            forcedCarriers[sceneIndex] = json_integer_value(json_object_get(sceneForcedCarriers, (std::string("Algorithm ") + std::to_string(sceneIndex)).c_str()));
        }
    }

    // Update disabled status, carriers, and display algorithm
    for (int scene = 0; scene < 3; scene++) {
        updateOpsDisabled(scene);
        updateCarriers(scene);
        updateDisplayAlgo(scene);
    }

    graphDirty = true;
}

float AlgomorphSmall::getInputBrightness(int portID) {
    if (vuLights)
        return std::max(    {   inputs[portID].plugLights[0].getBrightness(),
                                inputs[portID].plugLights[1].getBrightness() * 4,
                                inputs[portID].plugLights[2].getBrightness()          }   );
    else
        return 1.f;
}

float AlgomorphSmall::getOutputBrightness(int portID) {
    if (vuLights)
        return std::max(    {   outputs[portID].plugLights[0].getBrightness(),
                                outputs[portID].plugLights[1].getBrightness() * 4,
                                outputs[portID].plugLights[2].getBrightness()          }   );
    else
        return 1.f;
}


///// Panel Widget

AlgomorphSmallWidget::SetGainLevelAction::SetGainLevelAction() {
    name = "Delexander Algomorph set gain level";
}

void AlgomorphSmallWidget::SetGainLevelAction::undo() {
    rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
    assert(mw);
    AlgomorphSmall* m = dynamic_cast<AlgomorphSmall*>(mw->module);
    assert(m);
    
    m->gain = oldGain;
}

void AlgomorphSmallWidget::SetGainLevelAction::redo() {
    rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
    assert(mw);
    AlgomorphSmall* m = dynamic_cast<AlgomorphSmall*>(mw->module);
    assert(m);
    
    m->gain = newGain;
}

void AlgomorphSmallWidget::SetGainLevelItem::onAction(const rack::event::Action &e) {
    if (module->gain != gain) {
        // History
        SetGainLevelAction* h = new SetGainLevelAction;
        h->moduleId = module->id;
        h->oldGain = module->gain;
        h->newGain = gain;
        
        module->gain = gain;

        APP->history->push(h);
    }
}

Menu* AlgomorphSmallWidget::GainLevelMenuItem::createChildMenu() {
    Menu* menu = new Menu;
    createGainLevelMenu(module, menu);
    return menu;
}

void AlgomorphSmallWidget::GainLevelMenuItem::createGainLevelMenu(AlgomorphSmall* module, rack::ui::Menu* menu) {
    menu->addChild(construct<SetGainLevelItem>(&MenuItem::text, "+12 dB", &SetGainLevelItem::module, module, &SetGainLevelItem::gain, 4.f, &SetGainLevelItem::rightText, CHECKMARK(module->gain == 4.f)));
    menu->addChild(construct<SetGainLevelItem>(&MenuItem::text, "+6 dB", &SetGainLevelItem::module, module, &SetGainLevelItem::gain, 2.f, &SetGainLevelItem::rightText, CHECKMARK(module->gain == 2.f)));
    menu->addChild(construct<SetGainLevelItem>(&MenuItem::text, "0 dB", &SetGainLevelItem::module, module, &SetGainLevelItem::gain, 1.f, &SetGainLevelItem::rightText, CHECKMARK(module->gain == 1.f)));
    menu->addChild(construct<SetGainLevelItem>(&MenuItem::text, "-6 dB", &SetGainLevelItem::module, module, &SetGainLevelItem::gain, 0.5f, &SetGainLevelItem::rightText, CHECKMARK(module->gain == 0.5f)));
    menu->addChild(construct<SetGainLevelItem>(&MenuItem::text, "-12 dB", &SetGainLevelItem::module, module, &SetGainLevelItem::gain, 0.25f, &SetGainLevelItem::rightText, CHECKMARK(module->gain == 0.25f)));
}

AlgomorphSmallWidget::SetMorphMultAction::SetMorphMultAction() {
    name = "Delexander Algomorph set morph multiplier";
}

void AlgomorphSmallWidget::SetMorphMultAction::undo() {
    rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
    assert(mw);
    AlgomorphSmall* m = dynamic_cast<AlgomorphSmall*>(mw->module);
    assert(m);
    
    m->morphMult[inputId] = oldMult;
}

void AlgomorphSmallWidget::SetMorphMultAction::redo() {
    rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
    assert(mw);
    AlgomorphSmall* m = dynamic_cast<AlgomorphSmall*>(mw->module);
    assert(m);
    
    m->morphMult[inputId] = newMult;
}

void AlgomorphSmallWidget::SetMorphMultItem::onAction(const rack::event::Action &e) {
    if (module->morphMult[inputId] != morphMult) {
        // History
        SetMorphMultAction* h = new SetMorphMultAction;
        h->moduleId = module->id;
        h->oldMult = module->morphMult[inputId];
        h->newMult = morphMult;
        
        module->morphMult[inputId] = morphMult;

        APP->history->push(h);
    }
}

Menu* AlgomorphSmallWidget::MorphMultMenuItem::createChildMenu() {
    Menu* menu = new Menu;
    createMorphMultMenu(module, menu, inputId);
    return menu;
}

void AlgomorphSmallWidget::MorphMultMenuItem::createMorphMultMenu(AlgomorphSmall* module, rack::ui::Menu* menu, int inputId) {
    menu->addChild(construct<SetMorphMultItem>(&MenuItem::text, "1x", &SetMorphMultItem::module, module, &SetMorphMultItem::inputId, inputId, &SetMorphMultItem::morphMult, 1.f, &SetMorphMultItem::rightText, CHECKMARK(module->morphMult[inputId] == 1.f)));
    menu->addChild(construct<SetMorphMultItem>(&MenuItem::text, "2x", &SetMorphMultItem::module, module, &SetMorphMultItem::inputId, inputId, &SetMorphMultItem::morphMult, 2.f, &SetMorphMultItem::rightText, CHECKMARK(module->morphMult[inputId] == 2.f)));
    menu->addChild(construct<SetMorphMultItem>(&MenuItem::text, "3x", &SetMorphMultItem::module, module, &SetMorphMultItem::inputId, inputId, &SetMorphMultItem::morphMult, 3.f, &SetMorphMultItem::rightText, CHECKMARK(module->morphMult[inputId] == 3.f)));
}

AlgomorphSmallWidget::AlgomorphSmallWidget(AlgomorphSmall* module) {
    setModule(module);
    
    setPanel(APP->window->loadSvg(rack::asset::plugin(pluginInstance, "res/AlgomorphSmall.svg")));

    addChild(rack::createWidget<DLXGameBitBlack>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(rack::createWidget<DLXGameBitBlack>(Vec(box.size.x - RACK_GRID_WIDTH * 2, 0)));
    addChild(rack::createWidget<DLXGameBitBlack>(Vec(RACK_GRID_WIDTH, 365)));
    addChild(rack::createWidget<DLXGameBitBlack>(Vec(box.size.x - RACK_GRID_WIDTH * 2, 365)));

    // ink = rack::createWidget<AlgomorphSmallGlowingInk>(Vec(0,0));
    // if (!module->glowingInk)
    //     ink->hide();
    // addChild(ink);

    AlgomorphDisplayWidget* screenWidget = new AlgomorphDisplayWidget(module);
    screenWidget->box.pos = mm2px(Vec(6.252, 11.631));
    screenWidget->box.size = mm2px(Vec(38.295, 31.590));
    addChild(screenWidget);
    //Place backlight _above_ in scene in order for brightness to affect screenWidget
    addChild(createBacklight<DLXScreenMultiLight>(mm2px(Vec(6.252, 11.631)), mm2px(Vec(38.295, 31.590)), module, AlgomorphSmall::DISPLAY_BACKLIGHT));

    addChild(createRingLightCentered<DLXMultiLight>(mm2px(Vec(25.468, 61.936)), module, AlgomorphSmall::SCREEN_BUTTON_RING_LIGHT));
    addParam(rack::createParamCentered<DLXPurpleButton>(mm2px(Vec(25.468, 61.936)), module, AlgomorphSmall::SCREEN_BUTTON));
    addChild(rack::createParamCentered<DLXScreenButtonLight>(mm2px(Vec(25.468, 61.936)), module, AlgomorphSmall::SCREEN_BUTTON));

    addChild(createRingLightCentered<DLXMultiLight>(SceneButtonCenters[0], module, AlgomorphSmall::SCENE_LIGHTS + 0));
    addChild(createRingIndicatorCentered<Algomorph>(SceneButtonCenters[0], module, AlgomorphSmall::SCENE_INDICATORS + 0));
    addParam(rack::createParamCentered<rack::componentlibrary::TL1105>(SceneButtonCenters[0], module, AlgomorphSmall::SCENE_BUTTONS + 0));
    addChild(rack::createParamCentered<DLX1ButtonLight>(SceneButtonCenters[0], module, AlgomorphSmall::SCENE_BUTTONS + 0));

    addChild(createRingLightCentered<DLXMultiLight>(SceneButtonCenters[1], module, AlgomorphSmall::SCENE_LIGHTS + 3));
    addChild(createRingIndicatorCentered<Algomorph>(SceneButtonCenters[1], module, AlgomorphSmall::SCENE_INDICATORS + 3));
    addParam(rack::createParamCentered<rack::componentlibrary::TL1105>(SceneButtonCenters[1], module, AlgomorphSmall::SCENE_BUTTONS + 1));
    addChild(rack::createParamCentered<DLX2ButtonLight>(SceneButtonCenters[1], module, AlgomorphSmall::SCENE_BUTTONS + 1));

    addChild(createRingLightCentered<DLXMultiLight>(SceneButtonCenters[2], module, AlgomorphSmall::SCENE_LIGHTS + 6));
    addChild(createRingIndicatorCentered<Algomorph>(SceneButtonCenters[2], module, AlgomorphSmall::SCENE_INDICATORS + 6));
    addParam(rack::createParamCentered<rack::componentlibrary::TL1105>(SceneButtonCenters[2], module, AlgomorphSmall::SCENE_BUTTONS + 2));
    addChild(rack::createParamCentered<DLX3ButtonLight>(SceneButtonCenters[2], module, AlgomorphSmall::SCENE_BUTTONS + 2));

    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(7.415, 52.477)), module, AlgomorphSmall::WILDCARD_INPUT));

    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(9.687, 111.785)), module, AlgomorphSmall::MORPH_INPUTS + 0));
    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(41.012, 111.785)), module, AlgomorphSmall::MORPH_INPUTS + 1));

    addParam(rack::createParamCentered<DLXMediumLightKnob>(mm2px(Vec(25.412, 112.356)), module, AlgomorphSmall::MORPH_KNOB));

    DLXMediumLightKnob* morphAttenLightKnob = rack::createParamCentered<DLXMediumLightKnob>(mm2px(Vec(25.412, 112.356)), module, AlgomorphSmall::MORPH_ATTEN_KNOB);
    morphAttenLightKnob->hide();
    addParam(morphAttenLightKnob);

    addOutput(rack::createOutputCentered<DLXPJ301MPort>(mm2px(Vec(43.386, 52.477)), module, AlgomorphSmall::CARRIER_SUM_OUTPUT));

    addChild(createRingLightCentered<DLXYellowLight>(mm2px(Vec(25.468, 101.489)), module, AlgomorphSmall::EDIT_LIGHT));
    addChild(rack::createParamCentered<DLXPurpleButton>(mm2px(Vec(25.468, 101.489)), module, AlgomorphSmall::EDIT_BUTTON));
    addChild(rack::createParamCentered<DLXPencilButtonLight>(mm2px(Vec(25.468, 101.489)), module, AlgomorphSmall::EDIT_BUTTON));

    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(7.284, 66.351)), module, AlgomorphSmall::OPERATOR_INPUTS + 3));
    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(7.284, 76.673)), module, AlgomorphSmall::OPERATOR_INPUTS + 2));
    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(7.284, 86.994)), module, AlgomorphSmall::OPERATOR_INPUTS + 1));
    addInput(rack::createInputCentered<DLXPJ301MPort>(mm2px(Vec(7.284, 97.315)), module, AlgomorphSmall::OPERATOR_INPUTS + 0));

    addOutput(rack::createOutputCentered<DLXPJ301MPort>(mm2px(Vec(43.386, 66.351)), module, AlgomorphSmall::MODULATOR_OUTPUTS + 3));
    addOutput(rack::createOutputCentered<DLXPJ301MPort>(mm2px(Vec(43.386, 76.673)), module, AlgomorphSmall::MODULATOR_OUTPUTS + 2));
    addOutput(rack::createOutputCentered<DLXPJ301MPort>(mm2px(Vec(43.386, 86.994)), module, AlgomorphSmall::MODULATOR_OUTPUTS + 1));
    addOutput(rack::createOutputCentered<DLXPJ301MPort>(mm2px(Vec(43.386, 97.315)), module, AlgomorphSmall::MODULATOR_OUTPUTS + 0));

    ConnectionBgWidget* connectionBgWidget = new ConnectionBgWidget(OpButtonCenters, ModButtonCenters, module);
    connectionBgWidget->box.pos = OpButtonCenters[3];
    connectionBgWidget->box.size = ModButtonCenters[0].minus(OpButtonCenters[3]);
    addChild(connectionBgWidget);

    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[3], ModButtonCenters[3], module, AlgomorphSmall::H_CONNECTION_LIGHTS + 9));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[2], ModButtonCenters[2], module, AlgomorphSmall::H_CONNECTION_LIGHTS + 6));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[1], ModButtonCenters[1], module, AlgomorphSmall::H_CONNECTION_LIGHTS + 3));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[0], ModButtonCenters[0], module, AlgomorphSmall::H_CONNECTION_LIGHTS + 0));

    addChild(createLineLight<DLXRedLight>(OpButtonCenters[0], ModButtonCenters[3], module, AlgomorphSmall::D_DISABLE_LIGHTS + 2));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[0], ModButtonCenters[2], module, AlgomorphSmall::D_DISABLE_LIGHTS + 1));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[0], ModButtonCenters[1], module, AlgomorphSmall::D_DISABLE_LIGHTS + 0));

    addChild(createLineLight<DLXRedLight>(OpButtonCenters[1], ModButtonCenters[3], module, AlgomorphSmall::D_DISABLE_LIGHTS + 5));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[1], ModButtonCenters[2], module, AlgomorphSmall::D_DISABLE_LIGHTS + 4));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[1], ModButtonCenters[0], module, AlgomorphSmall::D_DISABLE_LIGHTS + 3));

    addChild(createLineLight<DLXRedLight>(OpButtonCenters[2], ModButtonCenters[3], module, AlgomorphSmall::D_DISABLE_LIGHTS + 8));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[2], ModButtonCenters[1], module, AlgomorphSmall::D_DISABLE_LIGHTS + 7));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[2], ModButtonCenters[0], module, AlgomorphSmall::D_DISABLE_LIGHTS + 6));

    addChild(createLineLight<DLXRedLight>(OpButtonCenters[3], ModButtonCenters[2], module, AlgomorphSmall::D_DISABLE_LIGHTS + 11));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[3], ModButtonCenters[1], module, AlgomorphSmall::D_DISABLE_LIGHTS + 10));
    addChild(createLineLight<DLXRedLight>(OpButtonCenters[3], ModButtonCenters[0], module, AlgomorphSmall::D_DISABLE_LIGHTS + 9));

    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[0], ModButtonCenters[3], module, AlgomorphSmall::CONNECTION_LIGHTS + 6));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[0], ModButtonCenters[2], module, AlgomorphSmall::CONNECTION_LIGHTS + 3));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[0], ModButtonCenters[1], module, AlgomorphSmall::CONNECTION_LIGHTS + 0));

    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[1], ModButtonCenters[3], module, AlgomorphSmall::CONNECTION_LIGHTS + 15));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[1], ModButtonCenters[2], module, AlgomorphSmall::CONNECTION_LIGHTS + 12));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[1], ModButtonCenters[0], module, AlgomorphSmall::CONNECTION_LIGHTS + 9));

    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[2], ModButtonCenters[3], module, AlgomorphSmall::CONNECTION_LIGHTS + 24));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[2], ModButtonCenters[1], module, AlgomorphSmall::CONNECTION_LIGHTS + 21));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[2], ModButtonCenters[0], module, AlgomorphSmall::CONNECTION_LIGHTS + 18));

    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[3], ModButtonCenters[2], module, AlgomorphSmall::CONNECTION_LIGHTS + 33));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[3], ModButtonCenters[1], module, AlgomorphSmall::CONNECTION_LIGHTS + 30));
    addChild(createLineLight<DLXMultiLight>(OpButtonCenters[3], ModButtonCenters[0], module, AlgomorphSmall::CONNECTION_LIGHTS + 27));
    
    addChild(createRingLightCentered<DLXMultiLight>(OpButtonCenters[3], module, AlgomorphSmall::OPERATOR_LIGHTS + 9));
    addChild(createRingLightCentered<DLXMultiLight>(OpButtonCenters[2], module, AlgomorphSmall::OPERATOR_LIGHTS + 6));
    addChild(createRingLightCentered<DLXMultiLight>(OpButtonCenters[1], module, AlgomorphSmall::OPERATOR_LIGHTS + 3));
    addChild(createRingLightCentered<DLXMultiLight>(OpButtonCenters[0], module, AlgomorphSmall::OPERATOR_LIGHTS + 0));

    addChild(createRingIndicatorCentered<Algomorph>(OpButtonCenters[3], module, AlgomorphSmall::CARRIER_INDICATORS + 9));
    addChild(createRingIndicatorCentered<Algomorph>(OpButtonCenters[2], module, AlgomorphSmall::CARRIER_INDICATORS + 6));
    addChild(createRingIndicatorCentered<Algomorph>(OpButtonCenters[1], module, AlgomorphSmall::CARRIER_INDICATORS + 3));
    addChild(createRingIndicatorCentered<Algomorph>(OpButtonCenters[0], module, AlgomorphSmall::CARRIER_INDICATORS + 0));

    addChild(createRingLightCentered<DLXMultiLight>(ModButtonCenters[3], module, AlgomorphSmall::MODULATOR_LIGHTS + 9));
    addChild(createRingLightCentered<DLXMultiLight>(ModButtonCenters[2], module, AlgomorphSmall::MODULATOR_LIGHTS + 6));
    addChild(createRingLightCentered<DLXMultiLight>(ModButtonCenters[1], module, AlgomorphSmall::MODULATOR_LIGHTS + 3));
    addChild(createRingLightCentered<DLXMultiLight>(ModButtonCenters[0], module, AlgomorphSmall::MODULATOR_LIGHTS + 0));

    addParam(rack::createParamCentered<DLXPurpleButton>(OpButtonCenters[3], module, AlgomorphSmall::OPERATOR_BUTTONS + 3));
    addParam(rack::createParamCentered<DLXPurpleButton>(OpButtonCenters[2], module, AlgomorphSmall::OPERATOR_BUTTONS + 2));
    addParam(rack::createParamCentered<DLXPurpleButton>(OpButtonCenters[1], module, AlgomorphSmall::OPERATOR_BUTTONS + 1));
    addParam(rack::createParamCentered<DLXPurpleButton>(OpButtonCenters[0], module, AlgomorphSmall::OPERATOR_BUTTONS + 0));

    addParam(rack::createParamCentered<DLXPurpleButton>(ModButtonCenters[3], module, AlgomorphSmall::MODULATOR_BUTTONS + 3));
    addParam(rack::createParamCentered<DLXPurpleButton>(ModButtonCenters[2], module, AlgomorphSmall::MODULATOR_BUTTONS + 2));
    addParam(rack::createParamCentered<DLXPurpleButton>(ModButtonCenters[1], module, AlgomorphSmall::MODULATOR_BUTTONS + 1));
    addParam(rack::createParamCentered<DLXPurpleButton>(ModButtonCenters[0], module, AlgomorphSmall::MODULATOR_BUTTONS + 0));
}

void AlgomorphSmallWidget::appendContextMenu(Menu* menu) {
    AlgomorphSmall* module = dynamic_cast<AlgomorphSmall*>(this->module);

    menu->addChild(new rack::ui::MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Audio Settings"));

    menu->addChild(construct<GainLevelMenuItem>(&MenuItem::text, "Modulator Gain adjustment…", &MenuItem::rightText, std::string(module->gain == 1.f ? "0dB " : module->gain == 2.f ? "+6dB " : module->gain == 4.f ? "+12dB " : module->gain == 0.5f ? "-6dB " : module->gain == 0.25f ? "-12dB " : "err ") + RIGHT_ARROW, &GainLevelMenuItem::module, module));

    menu->addChild(construct<ClickFilterMenuItem>(&MenuItem::text, "Click Filter…", &MenuItem::rightText, (module->clickFilterEnabled ? "Enabled ▸" : "Disabled ▸"), &ClickFilterMenuItem::module, module));

    RingMorphItem *ringMorphItem = rack::createMenuItem<RingMorphItem>("Enable Ring Morph", CHECKMARK(module->ringMorph));
    ringMorphItem->module = module;
    menu->addChild(ringMorphItem);

    menu->addChild(new rack::ui::MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Interaction Settings"));

    menu->addChild(construct<MorphMultMenuItem>(&MenuItem::text, "CV A multiplier…", &MenuItem::rightText, std::to_string((int)module->morphMult[0]) + "x " + RIGHT_ARROW, &MorphMultMenuItem::module, module, &MorphMultMenuItem::inputId, 0));
    menu->addChild(construct<MorphMultMenuItem>(&MenuItem::text, "CV B multiplier…", &MenuItem::rightText, std::to_string((int)module->morphMult[1]) + "x " + RIGHT_ARROW, &MorphMultMenuItem::module, module, &MorphMultMenuItem::inputId, 1));

    ToggleModeBItem *toggleModeBItem = rack::createMenuItem<ToggleModeBItem>("Alter Ego", CHECKMARK(module->modeB));
    toggleModeBItem->module = module;
    menu->addChild(toggleModeBItem);
    
    ExitConfigItem *exitConfigItem = rack::createMenuItem<ExitConfigItem>("Exit Edit Mode after connection", CHECKMARK(module->exitConfigOnConnect));
    exitConfigItem->module = module;
    menu->addChild(exitConfigItem);

    menu->addChild(new rack::ui::MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Visual Settings"));


    VULightsItem *vuLightsItem = rack::createMenuItem<VULightsItem>("Disable VU lighting", CHECKMARK(!module->vuLights));
    vuLightsItem->module = module;
    menu->addChild(vuLightsItem);
    
    // GlowingInkItem *glowingInkItem = rack::createMenuItem<GlowingInkItem>("Enable glowing panel ink", CHECKMARK(module->glowingInk));
    // glowingInkItem->module = module;
    // menu->addChild(glowingInkItem);

    // SaveVisualSettingsItem *saveVisualSettingsItem = rack::createMenuItem<SaveVisualSettingsItem>("Save visual settings as default", CHECKMARK(module->glowingInk == pluginSettings.glowingInkDefault && module->vuLights == pluginSettings.vuLightsDefault));
    SaveVisualSettingsItem *saveVisualSettingsItem = rack::createMenuItem<SaveVisualSettingsItem>("Save visual settings as default", CHECKMARK(module->vuLights == pluginSettings.vuLightsDefault));
    saveVisualSettingsItem->module = module;
    menu->addChild(saveVisualSettingsItem);

    // menu->addChild(new rack::ui::MenuSeparator());

    // DebugItem *debugItem = rack::createMenuItem<DebugItem>("The system is down", CHECKMARK(module->debug));
    // debugItem->module = module;
    // menu->addChild(debugItem);
}

void AlgomorphSmallWidget::step() {
    if (module) {
        AlgomorphSmall* m = dynamic_cast<AlgomorphSmall*>(module);
        // ink->visible = m->glowingInk == 1;
        if (m->inputs[AlgomorphSmall::MORPH_INPUTS + 0].isConnected() || m->inputs[AlgomorphSmall::MORPH_INPUTS + 1].isConnected()) {
            if (morphKnobShown) {
                DLXMediumLightKnob* morphKnob = dynamic_cast<DLXMediumLightKnob*>(getParam(AlgomorphSmall::MORPH_KNOB));
                morphKnob->hide();
                // morphKnob->sibling->hide();
                DLXMediumLightKnob* morphAtten = dynamic_cast<DLXMediumLightKnob*>(getParam(AlgomorphSmall::MORPH_ATTEN_KNOB));
                morphAtten->show();
                // morphAtten->sibling->show();
                morphKnobShown = false;
            }
        }
        else {
            if (!morphKnobShown) {
                DLXMediumLightKnob* morphAtten = dynamic_cast<DLXMediumLightKnob*>(getParam(AlgomorphSmall::MORPH_ATTEN_KNOB));
                morphAtten->hide();
                // morphAtten->sibling->hide();
                DLXMediumLightKnob* morphKnob = dynamic_cast<DLXMediumLightKnob*>(getParam(AlgomorphSmall::MORPH_KNOB));
                morphKnob->show();
                // morphKnob->sibling->show();
                morphKnobShown = true;
            }
        }
    }
    ModuleWidget::step();
}

Model* modelAlgomorphSmall = createModel<AlgomorphSmall, AlgomorphSmallWidget>("AlgomorphSmall");
