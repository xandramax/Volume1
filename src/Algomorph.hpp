#pragma once
#include "Components.hpp" // For RingIndicatorRotor
#include "plugin.hpp" // For constants
#include <bitset>
#include <rack.hpp>
using rack::event::Action;
using rack::history::ModuleAction;
using rack::ui::Menu;


template < int OPS = 4, int SCENES = 3 >
struct Algomorph : rack::engine::Module {
    float morph[CHANNELS] = {0.f};                                      // Range -1.f -> 1.f
    float relativeMorphMagnitude[CHANNELS] = { morph[0] };              // Range 0.f -> 1.f
    rack::dsp::RingBuffer<float, 4> displayMorph;

    int baseScene = -1;                                                 // Center the Morph knob on saved algorithm 0, 1, or 2
    int centerMorphScene[CHANNELS]    = { baseScene };
    int forwardMorphScene[CHANNELS]   = { (baseScene + 1) % 3 };
    int backwardMorphScene[CHANNELS]  = { (baseScene + 2) % 3 };
    rack::dsp::RingBuffer<int, 4> displayScene;
    rack::dsp::RingBuffer<int, 4> displayMorphScene;

    std::bitset<OPS*OPS> algoName[SCENES]         = {0};                            // 16-bit IDs of the three stored algorithms

    std::bitset<OPS> horizontalMarks[SCENES]   = {0};                               // If the user creates a horizontal connection, mark it here
    std::bitset<OPS> forcedCarriers[SCENES]    = {0};                               // If the user forces an operator to act as a carrier, mark it here 
    std::bitset<OPS> carriers[SCENES]           = {0xF, 0xF, 0xF};                   // If an operator is acting as a carrier, whether forced or automatically, mark it here
    std::bitset<OPS> opsDisabled[SCENES]       = {0};                               // If an operator is disabled, whether forced or automatically, mark it here
    rack::dsp::RingBuffer<std::bitset<OPS>, 4> displayHorizontalMarks[SCENES];
    rack::dsp::RingBuffer<std::bitset<OPS>, 4> displayForcedCarriers[SCENES];
    
    rack::dsp::RingBuffer<std::bitset<OPS*OPS>, 4> displayAlgoName[SCENES];         // When operators are disabled, remove their mod destinations from here
                                                                                    // If a disabled operator is a mod destination, set it to enabled here
    std::bitset<OPS*OPS> tempDisplayAlgoName = 0;

    rack::dsp::ClockDivider cvDivider;
    rack::dsp::BooleanTrigger sceneButtonTrigger[SCENES];
    rack::dsp::BooleanTrigger editTrigger;
    rack::dsp::BooleanTrigger operatorTrigger[OPS];
    rack::dsp::BooleanTrigger modulatorTrigger[OPS];

    // [op][mod][channel]
    rack::dsp::SlewLimiter modClickFilters[OPS][OPS][CHANNELS];
    rack::dsp::SlewLimiter modRingClickFilters[OPS][OPS][CHANNELS];
    float modClickGain[OPS][OPS][CHANNELS] = {{{0.f}}};
    float modRingClickGain[OPS][OPS][CHANNELS] = {{{0.f}}};

    // [op][channel]
    rack::dsp::SlewLimiter sumClickFilters[OPS][CHANNELS];
    rack::dsp::SlewLimiter sumRingClickFilters[OPS][CHANNELS];
    float sumClickGain[OPS][CHANNELS] = {{0.f}};
    float sumRingClickGain[OPS][CHANNELS] = {{0.f}};

    rack::dsp::ClockDivider clickFilterDivider;

    rack::dsp::ClockDivider lightDivider;
    float sceneBrightnesses[SCENES][OPS*(OPS-1)][3] = {{{}}};       // [scene][light][color]
    float blinkTimer = BLINK_INTERVAL;
    bool blinkStatus = true;
    RingIndicatorRotor rotor;

    bool configMode = true;
    int configScene = -1;
    int configOp = -1;              // Set to 0-3 when configuring mod destinations for operators 1-4

    bool graphDirty = true;
    bool debug = false;

    int graphAddressTranslation[0xFFFF];        // Graph ID conversion
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

    int relToAbs[OPS][OPS-1] = {{0}};    // Modulator ID conversion ([op][x] = y, where x is 0..2 and y is 0..3)
    int absToRel[OPS][OPS] = {{0}};      // Modulator ID conversion ([op][x] = y, where x is 0..3 and y is 0..2)

    //User settings
    bool clickFilterEnabled = true;
    bool ringMorph = false;
    bool randomRingMorph = false;
    bool exitConfigOnConnect = false;
    bool glowingInk = false;
    bool vuLights = true;
    bool modeB = false;
    float clickFilterSlew = DEF_CLICK_FILTER_SLEW;

    Algomorph() {
        for (int op = 0; op < OPS; op++) {
            for (int c = 0; c < CHANNELS; c++) {
                sumClickFilters[op][c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
                sumRingClickFilters[op][c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
                for (int mod = 0; mod < OPS; mod++) {
                    modClickFilters[op][mod][c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
                    modRingClickFilters[op][mod][c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
                }
            }
        }

        clickFilterDivider.setDivision(128);
        lightDivider.setDivision(64);
        cvDivider.setDivision(32);

        // Map 3-bit operator-relative mod output indices to 4-bit generalized equivalents
        for (int op = 0; op < OPS; op++) {
            for (int mod = 0; mod < OPS; mod++) {
                if (op != mod) {
                    relToAbs[op][op > mod ? mod : mod - 1] = mod;
                    absToRel[op][mod] = op > mod ? mod : mod - 1;
                }
                else
                    absToRel[op][mod] = -1;
            }
        }

        // TODO: Not OPS-agnostic, move to subclass
        // Initialize graphAddressTranslation[] to -1, then index local ids by generalized 16-bit equivalents
        for (int i = 0; i < 0xFFFF; i++) {
            graphAddressTranslation[i] = -1;
        }
        for (int i = 0; i < 1980; i++) {
            graphAddressTranslation[(int)GRAPH_DATA.xNodeData[i][0]] = i;
        }

        Algomorph<OPS, SCENES>::onReset();
    };

    void onReset() override {
        for (int scene = 0; scene < SCENES; scene++) {
            algoName[scene].reset();
            for (int op = 0; op < OPS; op++) {
                horizontalMarks[scene].set(op, false);
                forcedCarriers[scene].set(op, false);
                carriers[scene].set(op, true);
                opsDisabled[scene].set(op, false);
            }
            updateDisplayAlgo(scene);
        }

        configMode = false;
        configOp = -1;
        configScene = std::floor(SCENES + 1 / 2.f);
        baseScene = 1;

        for (int c = 0; c < CHANNELS; c++) {
            centerMorphScene[c]    = baseScene;
            forwardMorphScene[c]   = (baseScene + 1) % SCENES;
            backwardMorphScene[c]  = (baseScene + SCENES - 1) % SCENES;
        }

        displayMorph.push(0.f);
        displayScene.push(baseScene);
        displayMorphScene.push(forwardMorphScene[0]);

        displayMorph.clear();
        displayScene.clear();
        displayMorphScene.clear();

        clickFilterEnabled = true;
        clickFilterSlew = DEF_CLICK_FILTER_SLEW;
        ringMorph = false;
        randomRingMorph = false;
        exitConfigOnConnect = false;
        modeB = false;

        glowingInk = pluginSettings.glowingInkDefault;
        vuLights = pluginSettings.vuLightsDefault;

        blinkStatus = true;
        blinkTimer = BLINK_INTERVAL;
        graphDirty = true;
    };

    void randomizeAlgorithm(int scene) {
        bool noCarrier = true;
        algoName[scene].reset();    //Initialize
        horizontalMarks[scene].reset();   //Initialize
        if (modeB)
            forcedCarriers[scene].reset();    //Initialize
        for (int op = 0; op < OPS; op++) {
            if (modeB) {
                bool disabled = true;           //Initialize
                forcedCarriers[scene].set(op, false);   //Initialize
                if (rack::random::uniform() > .5f) {
                    forcedCarriers[scene].set(op, true);
                    noCarrier = false;
                    disabled = false;
                }
                if (rack::random::uniform() > .5f) {
                    horizontalMarks[scene].set(op, true);
                    //Do not set algoName, because the operator is not disabled
                    disabled = false;
                }
                for (int mod = 0; mod < OPS - 1; mod++) {
                    if (rack::random::uniform() > .5f) {
                        algoName[scene].set(op * (OPS - 1) + mod, true);
                        disabled = false;
                    }
                }
                if (disabled)
                    algoName[scene].set(12 + op, true);
            }
            else {
                forcedCarriers[scene].set(op, false);   //Disable
                if (rack::random::uniform() > .5f) {  //If true, operator is a carrier
                    noCarrier = false;
                    for (int mod = 0; mod < OPS - 1; mod++)
                        algoName[scene].set(op * (OPS - 1) + mod, false);
                }
                else {
                    if (rack::random::uniform() > .5) {   //If true, operator is disabled
                        horizontalMarks[scene].set(op, true);
                        algoName[scene].set(12 + op, true);
                        for (int mod = 0; mod < OPS - 1; mod++ )
                            algoName[scene].set(op * (OPS - 1) + mod, false);
                    }
                    else {
                        for (int mod = 0; mod < OPS - 1; mod++) {
                            if (rack::random::uniform() > .5f)
                                algoName[scene].set(op * (OPS - 1) + mod, true);    
                        }
                    }
                }
            }
        }
        if (noCarrier) {
            int shortStraw = std::floor(rack::random::uniform() * 4);
            while (shortStraw == 4)
                shortStraw = std::floor(rack::random::uniform() * 4);
            if (modeB) {
                forcedCarriers[scene].set(shortStraw, true);
                algoName[scene].set(12 + shortStraw, false);
            }
            else {
                horizontalMarks[scene].set(shortStraw, false);
                algoName[scene].set(12 + shortStraw, false);
                for (int mod = 0; mod < OPS - 1; mod++)
                    algoName[scene].set(shortStraw * 3 + mod, false);
            }
        }
        updateCarriers(scene);
        updateOpsDisabled(scene);
        updateDisplayAlgo(scene);
    };

    void onRandomize() override {
        //If in config mode, only randomize the current algorithm,
        //do not change the base scene and do not enable/disable ring morph
        if (configMode)
            randomizeAlgorithm(configScene);
        else {
            for (int scene = 0; scene < SCENES; scene++)
                randomizeAlgorithm(scene);
        }
        graphDirty = true;
    };

    void initializeAlgorithm(int scene) {
        algoName[scene].reset();
        horizontalMarks[scene].reset();
        opsDisabled[scene].reset();
        forcedCarriers[scene].reset();
        updateDisplayAlgo(scene);
        graphDirty = true;
    };

    float routeHorizontal(float sampleTime, float inputVoltage, int op, int c) {
        float connectionA = horizontalMarks[centerMorphScene[c]].test(op) * (1.f - relativeMorphMagnitude[c]);
        float connectionB = horizontalMarks[forwardMorphScene[c]].test(op) * (relativeMorphMagnitude[c]);
        float morphedConnection = connectionA + connectionB;
        modClickGain[op][op][c] = clickFilterEnabled ? modClickFilters[op][op][c].process(sampleTime, morphedConnection) : morphedConnection;
        return inputVoltage * modClickGain[op][op][c];
    };

    float routeHorizontalRing(float sampleTime, float inputVoltage, int op, int c) {
        float connection = horizontalMarks[backwardMorphScene[c]].test(op) * relativeMorphMagnitude[c];
        modRingClickGain[op][op][c] = clickFilterEnabled ? modRingClickFilters[op][op][c].process(sampleTime, connection) : connection;
        return -inputVoltage * modRingClickGain[op][op][c];
    };

    float routeDiagonal(float sampleTime, float inputVoltage, int op, int mod, int c) {
        float connectionA = algoName[centerMorphScene[c]].test(op * (OPS - 1) + mod)   * (1.f - relativeMorphMagnitude[c])  * !horizontalMarks[centerMorphScene[c]].test(op);
        float connectionB = algoName[forwardMorphScene[c]].test(op * (OPS - 1) + mod)  * relativeMorphMagnitude[c]          * !horizontalMarks[forwardMorphScene[c]].test(op);
        float morphedConnection = connectionA + connectionB;
        modClickGain[op][relToAbs[op][mod]][c] = clickFilterEnabled ? modClickFilters[op][relToAbs[op][mod]][c].process(sampleTime, morphedConnection) : morphedConnection;
        return inputVoltage * modClickGain[op][relToAbs[op][mod]][c];
    };

    float routeDiagonalB(float sampleTime, float inputVoltage, int op, int mod, int c) {
        float connectionA = algoName[centerMorphScene[c]].test(op * (OPS - 1) + mod)   * (1.f - relativeMorphMagnitude[c]);
        float connectionB = algoName[forwardMorphScene[c]].test(op * (OPS - 1) + mod)  * relativeMorphMagnitude[c];
        float morphedConnection = connectionA + connectionB;
        modClickGain[op][relToAbs[op][mod]][c] = clickFilterEnabled ? modClickFilters[op][relToAbs[op][mod]][c].process(sampleTime, morphedConnection) : morphedConnection;
        return inputVoltage * modClickGain[op][relToAbs[op][mod]][c];
    };

    float routeDiagonalRing(float sampleTime, float inputVoltage, int op, int mod, int c) {
        float connection = algoName[backwardMorphScene[c]].test(op * (OPS - 1) + mod) * relativeMorphMagnitude[c] * !horizontalMarks[backwardMorphScene[c]].test(op);
        modRingClickGain[op][relToAbs[op][mod]][c] = clickFilterEnabled ? modRingClickFilters[op][relToAbs[op][mod]][c].process(sampleTime, connection) : connection; 
        return -inputVoltage * modRingClickGain[op][relToAbs[op][mod]][c];
    };

    float routeDiagonalRingB(float sampleTime, float inputVoltage, int op, int mod, int c) {
        float connection = algoName[backwardMorphScene[c]].test(op * (OPS - 1) + mod) * relativeMorphMagnitude[c];
        modRingClickGain[op][relToAbs[op][mod]][c] = clickFilterEnabled ? modRingClickFilters[op][relToAbs[op][mod]][c].process(sampleTime, connection) : connection; 
        return -inputVoltage * modRingClickGain[op][relToAbs[op][mod]][c];
    };

    float routeSum(float sampleTime, float inputVoltage, int op, int c) {
        float connection    =   carriers[centerMorphScene[c]].test(op)     * (1.f - relativeMorphMagnitude[c])  * !horizontalMarks[centerMorphScene[c]].test(op)
                            +   carriers[forwardMorphScene[c]].test(op)    * relativeMorphMagnitude[c]          * !horizontalMarks[forwardMorphScene[c]].test(op);
        sumClickGain[op][c] = clickFilterEnabled ? sumClickFilters[op][c].process(sampleTime, connection) : connection;
        return inputVoltage * sumClickGain[op][c];
    };

    float routeSumB(float sampleTime, float inputVoltage, int op, int c) {
        float connection    =   carriers[centerMorphScene[c]].test(op)     * (1.f - relativeMorphMagnitude[c])
                            +   carriers[forwardMorphScene[c]].test(op)    * relativeMorphMagnitude[c];
        sumClickGain[op][c] = clickFilterEnabled ? sumClickFilters[op][c].process(sampleTime, connection) : connection;
        return inputVoltage * sumClickGain[op][c];
    };

    float routeSumRing(float sampleTime, float inputVoltage, int op, int c) {
        float connection = carriers[backwardMorphScene[c]].test(op) * relativeMorphMagnitude[c] * !horizontalMarks[backwardMorphScene[c]].test(op);
        sumRingClickGain[op][c] = clickFilterEnabled ? sumRingClickFilters[op][c].process(sampleTime, connection) : connection;
        return -inputVoltage * sumRingClickGain[op][c];
    };

    float routeSumRingB(float sampleTime, float inputVoltage, int op, int c) {
        float connection = carriers[backwardMorphScene[c]].test(op) * relativeMorphMagnitude[c];
        sumRingClickGain[op][c] = clickFilterEnabled ? sumRingClickFilters[op][c].process(sampleTime, connection) : connection;
        return -inputVoltage * sumRingClickGain[op][c];
    };

    void toggleHorizontalDestination(int scene, int op) {
        horizontalMarks[scene].flip(op);
        if (!modeB) {
            toggleDisabled(scene, op);
            if (horizontalMarks[scene].test(op))
                carriers[scene].set(op, forcedCarriers[scene].test(op));
            else
                carriers[scene].set(op, isCarrier(scene, op));
        }
        else {
            bool mismatch = opsDisabled[scene].test(op) ^ isDisabled(scene, op);
            if (mismatch)
                toggleDisabled(scene, op);
        }
        displayHorizontalMarks[scene].push(horizontalMarks[scene]);
        displayForcedCarriers[scene].push(forcedCarriers[scene]);
    };

    void toggleDiagonalDestination(int scene, int op, int mod) {
        algoName[scene].flip(op * (OPS - 1) + mod);
        if (!modeB) {
            if (algoName[scene].test(op * (OPS - 1) + mod))
                carriers[scene].set(op, forcedCarriers[scene].test(op));
            else
                carriers[scene].set(op, isCarrier(scene, op));
        }
        else {   
            bool mismatch = opsDisabled[scene].test(op) ^ isDisabled(scene, op);
            if (mismatch)
                toggleDisabled(scene, op);
        }
        if (!opsDisabled[scene].test(op))
            updateDisplayAlgo(scene);
    };

    bool isCarrier(int scene, int op) {
        bool isCarrier = true;
        if (modeB)
            isCarrier = false;
        else {
            for (int mod = 0; mod < OPS - 1; mod++) {
                if (algoName[scene].test(op * (OPS - 1) + mod))
                    isCarrier = false;
            }
            if (horizontalMarks[scene].test(op))
                isCarrier = false;
        }
        isCarrier |= forcedCarriers[scene].test(op);
        return isCarrier;
    };

    void updateCarriers(int scene) {
        for (int op = 0; op < OPS; op++)
            carriers[scene].set(op, isCarrier(scene, op));
        displayForcedCarriers[scene].push(forcedCarriers[scene]);
    };

    bool isDisabled(int scene, int op) {
        if (!modeB) {
            if (horizontalMarks[scene].test(op))
                return true;
            else
                return false;
        }
        else {
            if (forcedCarriers[scene].test(op))
                return false;
            else {
                if (horizontalMarks[scene].test(op))
                    return false;
                else {
                    for (int mod = 0; mod < OPS - 1; mod++) {
                        if (algoName[scene].test(op * (OPS - 1) + mod))
                            return false;
                    }
                    return true;
                }
            }
        }
    };

    void toggleDisabled(int scene, int op) {
        algoName[scene].flip(OPS * (OPS - 1) + op);
        opsDisabled[scene].flip(op);
        updateDisplayAlgo(scene);
    };

    void updateOpsDisabled(int scene) {
        for (int op = 0; op < OPS; op++) {
            opsDisabled[scene].set(op, isDisabled(scene, op));
        }
    };

    void updateDisplayAlgo(int scene) {
        tempDisplayAlgoName = algoName[scene];
        // Set display algorithm
        for (int op = 0; op < OPS; op++) {
            if (opsDisabled[scene].test(op)) {
                // Set all destinations to false
                for (int mod = 0; mod < OPS - 1; mod++)
                    tempDisplayAlgoName.set(op * (OPS - 1) + mod, false);
                // Check if any operators are modulating this operator
                bool fullDisable = true;
                for (int i = 0; i < 4; i++) {     
                    if (!opsDisabled[scene].test(i) && algoName[scene].test(i * 3 + absToRel[i][op]))
                        fullDisable = false;
                }
                if (fullDisable) {
                    tempDisplayAlgoName.set(12 + op, true);
                }
                else
                    tempDisplayAlgoName.set(12 + op, false);
            }
            else {
                // Enable destinations in the display and handle the consequences
                for (int mod = 0; mod < 3; mod++) {
                    if (algoName[scene].test(op * (OPS - 1) + mod)) {
                        tempDisplayAlgoName.set(op * (OPS - 1) + mod, true);
                        // the consequences
                        if (opsDisabled[scene].test(relToAbs[op][mod]))
                            tempDisplayAlgoName.set(12 + relToAbs[op][mod], false);
                    }
                }  
            }
        }
        displayAlgoName[scene].push(tempDisplayAlgoName);
        displayHorizontalMarks[scene].push(horizontalMarks[scene]);
        displayForcedCarriers[scene].push(forcedCarriers[scene]);
    };

    void toggleModeB() {
        modeB ^= true;
        if (modeB) {
            for (int scene = 0; scene < SCENES; scene++) {
                for (int op = 0; op < OPS; op++) {
                    carriers[scene].set(op, forcedCarriers[scene].test(op));
                    bool mismatch = opsDisabled[scene].test(op) ^ isDisabled(scene, op);
                    if (mismatch)
                        toggleDisabled(scene, op);
                }
            }
        }
        else {
            for (int scene = 0; scene < SCENES; scene++) {
                for (int op = 0; op < OPS; op++) {
                    bool mismatch = opsDisabled[scene].test(op) ^ isDisabled(scene, op);
                    if (mismatch)
                        toggleDisabled(scene, op);
                }
                // Check carrier status after above loop is complete
                for (int op = 0; op < OPS; op++)
                    carriers[scene].set(op, isCarrier(scene, op));
            }
        }
    };

    void toggleForcedCarrier(int scene, int op) {
        forcedCarriers[scene].flip(op);
        if (forcedCarriers[scene].test(op))
            carriers[scene].set(op, true);
        else
            carriers[scene].set(op, isCarrier(scene, op));
        if (modeB) {
            bool mismatch = opsDisabled[scene].test(op) ^ isDisabled(scene, op);
            if (mismatch)
                toggleDisabled(scene, op);
        }
        displayForcedCarriers[scene].push(forcedCarriers[scene]);
    };
};


/// Undo/Redo History

template < int OPS = 4, int SCENES = 3 >
struct AlgorithmDiagonalChangeAction : ModuleAction {
    int scene, op, mod;

	AlgorithmDiagonalChangeAction() {
		name = "Delexander Algomorph diagonal connection";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);

		m->toggleDiagonalDestination(scene, op, mod);

		m->graphDirty = true;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);

		m->toggleDiagonalDestination(scene, op, mod);

		m->graphDirty = true;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct AlgorithmHorizontalChangeAction : ModuleAction {
    int scene, op;

	AlgorithmHorizontalChangeAction() {
		name = "Delexander Algomorph horizontal connection";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);

		m->toggleHorizontalDestination(scene, op);

		m->graphDirty = true;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);

		m->toggleHorizontalDestination(scene, op);

		m->graphDirty = true;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct AlgorithmForcedCarrierChangeAction : ModuleAction {
    int scene, op;

	AlgorithmForcedCarrierChangeAction() {
		name = "Delexander Algomorph forced carrier";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);

		m->toggleForcedCarrier(scene, op);
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);

		m->toggleForcedCarrier(scene, op);
	};
};

template < int OPS = 4, int SCENES = 3 >
struct AlgorithmSceneChangeAction : ModuleAction {
    int oldScene, newScene;

	AlgorithmSceneChangeAction() {
		name = "Delexander Algomorph base scene";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->baseScene = oldScene;
		m->graphDirty = true;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->baseScene = newScene;
		m->graphDirty = true;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct ToggleModeBAction : ModuleAction {
	ToggleModeBAction() {
		name = "Delexander Algomorph toggle mode B";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		
		m->toggleModeB();

		m->graphDirty = true;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		
		m->toggleModeB();

		m->graphDirty = true;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct ToggleRingMorphAction : ModuleAction {
	ToggleRingMorphAction() {
		name = "Delexander Algomorph toggle ring morph";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->ringMorph ^= true;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->ringMorph ^= true;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct ToggleRandomizeRingMorphAction : ModuleAction {
	ToggleRandomizeRingMorphAction() {
		name = "Delexander Algomorph toggle randomize ring morph";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->randomRingMorph ^= true;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->randomRingMorph ^= true;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct ToggleExitConfigOnConnectAction : ModuleAction {
	ToggleExitConfigOnConnectAction() {
		name = "Delexander Algomorph toggle exit config on connect";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->exitConfigOnConnect ^= true;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->exitConfigOnConnect ^= true;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct ToggleClickFilterAction : ModuleAction {
	ToggleClickFilterAction() {
		name = "Delexander Algomorph toggle click filter";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->clickFilterEnabled ^= true;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->clickFilterEnabled ^= true;
	};

};

template < int OPS = 4, int SCENES = 3 >
struct ToggleGlowingInkAction : ModuleAction {
	ToggleGlowingInkAction() {
		name = "Delexander Algomorph toggle glowing ink";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->glowingInk ^= true;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->glowingInk ^= true;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct ToggleVULightsAction : ModuleAction {
	ToggleVULightsAction() {
		name = "Delexander Algomorph toggle VU lights";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->vuLights ^= true;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->vuLights ^= true;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct SetClickFilterAction : ModuleAction {
	float oldSlew, newSlew;

	SetClickFilterAction() {
		name = "Delexander Algomorph set click filter slew";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->clickFilterSlew = oldSlew;
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->clickFilterSlew = newSlew;
	};
};

template < int OPS = 4, int SCENES = 3 >
struct RandomizeCurrentAlgorithmAction : ModuleAction {
	int oldAlgoName, oldHorizontalMarks, oldOpsDisabled, oldForcedCarriers;
	int newAlgoName, newHorizontalMarks, newOpsDisabled, newForcedCarriers;
	int scene;

	RandomizeCurrentAlgorithmAction() {
		name = "Delexander Algomorph randomize current algorithm";
	};
    void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->algoName[scene] = oldAlgoName;
		m->horizontalMarks[scene] = oldHorizontalMarks;
		m->opsDisabled[scene] = oldOpsDisabled;
		m->forcedCarriers[scene] = oldForcedCarriers;
		m->updateDisplayAlgo(scene);
	};
    void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->algoName[scene] = newAlgoName;
		m->horizontalMarks[scene] = newHorizontalMarks;
		m->opsDisabled[scene] = newOpsDisabled;
		m->forcedCarriers[scene] = newForcedCarriers;
		m->updateDisplayAlgo(scene);
	};
};

template < int OPS = 4, int SCENES = 3 >
struct RandomizeAllAlgorithmsAction : ModuleAction {
	int oldAlgorithm[3], oldHorizontalMarks[3], oldOpsDisabled[3], oldForcedCarriers[3];
	int newAlgorithm[3], newHorizontalMarks[3], newOpsDisabled[3], newForcedCarriers[3];

	RandomizeAllAlgorithmsAction() {
		name = "Delexander Algomorph randomize all algorithms";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		for (int scene = 0; scene < 3; scene++) {
			m->algoName[scene] = oldAlgorithm[scene];
			m->horizontalMarks[scene] = oldHorizontalMarks[scene];
			m->opsDisabled[scene] = oldOpsDisabled[scene];
			m->forcedCarriers[scene] = oldForcedCarriers[scene];
			m->updateDisplayAlgo(scene);
		}
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		for (int scene = 0; scene < 3; scene++) {
			m->algoName[scene] = newAlgorithm[scene];
			m->horizontalMarks[scene] = newHorizontalMarks[scene];
			m->opsDisabled[scene] = newOpsDisabled[scene];
			m->forcedCarriers[scene] = newForcedCarriers[scene];
			m->updateDisplayAlgo(scene);
		}
	};
};

template < int OPS = 4, int SCENES = 3 >
struct InitializeCurrentAlgorithmAction : ModuleAction {
	int oldAlgoName, oldHorizontalMarks, oldOpsDisabled, oldForcedCarriers, scene;

	InitializeCurrentAlgorithmAction() {
		name = "Delexander Algomorph initialize current algorithm";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->algoName[scene] = oldAlgoName;
		m->horizontalMarks[scene] = oldHorizontalMarks;
		m->opsDisabled[scene] = oldOpsDisabled;
		m->forcedCarriers[scene] = oldForcedCarriers;
		m->updateDisplayAlgo(scene);
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		m->algoName[scene].reset();
		m->horizontalMarks[scene].reset();
		m->opsDisabled[scene].reset();
		m->forcedCarriers[scene].reset();
		m->updateDisplayAlgo(scene);
	};
};

template < int OPS = 4, int SCENES = 3 >
struct InitializeAllAlgorithmsAction : ModuleAction {
	int oldAlgorithm[3], oldHorizontalMarks[3], oldOpsDisabled[3], oldForcedCarriers[3];

	InitializeAllAlgorithmsAction() {
		name = "Delexander Algomorph initialize all algorithms";
	};
	void undo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		for (int i = 0; i < 3; i++) {
			m->algoName[i] = oldAlgorithm[i];
			m->horizontalMarks[i] = oldHorizontalMarks[i];
			m->opsDisabled[i] = oldOpsDisabled[i];
			m->forcedCarriers[i] = oldForcedCarriers[i];
			m->updateDisplayAlgo(i);
		}
	};
	void redo() override {
		rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		Algomorph<OPS, SCENES>* m = dynamic_cast<Algomorph<OPS, SCENES>*>(mw->module);
		assert(m);
		for (int i = 0; i < 3; i++) {
			m->algoName[i].reset();
			m->horizontalMarks[i].reset();
			m->opsDisabled[i].reset();
			m->forcedCarriers[i].reset();
			m->updateDisplayAlgo(i);
		}
	};
};


// Module Widget


template < int OPS = 4, int SCENES = 3 >
struct AlgomorphMenuItem : rack::ui::MenuItem {
    Algomorph<OPS, SCENES>* module;
};

template < int OPS = 4, int SCENES = 3 >
struct AlgomorphWidget : rack::app::ModuleWidget {
    // Menu items
    struct ToggleModeBItem : AlgomorphMenuItem<OPS, SCENES> {
        void onAction(const Action &e) override {
            // History
            ToggleModeBAction<OPS, SCENES>* h = new ToggleModeBAction<OPS, SCENES>;
            h->moduleId = this->module->id;

            this->module->toggleModeB();

            APP->history->push(h);

            this->module->graphDirty = true;
        };
    };
    struct RingMorphItem : AlgomorphMenuItem<OPS, SCENES> {
        void onAction(const Action &e) override {
            // History
            ToggleRingMorphAction<OPS, SCENES>* h = new ToggleRingMorphAction<OPS, SCENES>;
            h->moduleId = this->module->id;

            this->module->ringMorph ^= true;

            APP->history->push(h);
        };
    };
    struct ExitConfigItem : AlgomorphMenuItem<OPS, SCENES> {
        void onAction(const Action &e) override {
            // History
            ToggleExitConfigOnConnectAction<OPS, SCENES>* h = new ToggleExitConfigOnConnectAction<OPS, SCENES>;
            h->moduleId = this->module->id;

            this->module->exitConfigOnConnect ^= true;

            APP->history->push(h);
        };
    };

    struct ClickFilterEnabledItem : AlgomorphMenuItem<OPS, SCENES> {
        void onAction(const Action &e) override {
            // History
            ToggleClickFilterAction<OPS, SCENES>* h = new ToggleClickFilterAction<OPS, SCENES>;
            h->moduleId = this->module->id;

            this->module->clickFilterEnabled ^= true;

            APP->history->push(h);
        };
    };

    struct VULightsItem : AlgomorphMenuItem<OPS, SCENES> {
        void onAction(const Action &e) override {
            // History
            ToggleVULightsAction<OPS, SCENES>* h = new ToggleVULightsAction<OPS, SCENES>;
            h->moduleId = this->module->id;

            this->module->vuLights ^= true;

            APP->history->push(h);
        };
    };
    struct GlowingInkItem : AlgomorphMenuItem<OPS, SCENES> {
        void onAction(const Action &e) override {
            // History
            ToggleGlowingInkAction<OPS, SCENES>* h = new ToggleGlowingInkAction<OPS, SCENES>;
            h->moduleId = this->module->id;

            this->module->glowingInk ^= true;

            APP->history->push(h);
        };
    };
    struct DebugItem : AlgomorphMenuItem<OPS, SCENES> {
        void onAction(const Action &e) override {
            this->module->debug ^= true;
        };
    };
    struct SaveVisualSettingsItem : AlgomorphMenuItem<OPS, SCENES> {
        void onAction(const Action &e) override {
            // pluginSettings.glowingInkDefault = module->glowingInk;
            pluginSettings.vuLightsDefault = this->module->vuLights;
            pluginSettings.saveToJson();
        };
    };

    // Sub-menus
    struct AudioSettingsMenuItem : AlgomorphMenuItem<OPS, SCENES> {
        void createAudioSettingsMenu(Algomorph<OPS, SCENES>* module, Menu* menu) {
            menu->addChild(rack::construct<ClickFilterMenuItem>(&MenuItem::text, "Click Filter…", &MenuItem::rightText, (this->module->clickFilterEnabled ? "Enabled ▸" : "Disabled ▸"), &ClickFilterMenuItem::module, module));

            RingMorphItem *ringMorphItem = rack::createMenuItem<RingMorphItem>("Enable Ring Morph", CHECKMARK(this->module->ringMorph));
            ringMorphItem->module = module;
            menu->addChild(ringMorphItem);
        };
        Menu* createChildMenu() override {
            Menu* menu = new Menu;
            createAudioSettingsMenu(this->module, menu);
            return menu;
        };
    };
    struct ClickFilterMenuItem : AlgomorphMenuItem<OPS, SCENES> {
        void createClickFilterMenu(Algomorph<OPS, SCENES>* module, Menu* menu) {
            ClickFilterEnabledItem *clickFilterEnabledItem = rack::createMenuItem<ClickFilterEnabledItem>("Disable Click Filter", CHECKMARK(!this->module->clickFilterEnabled));
            clickFilterEnabledItem->module = module;
            menu->addChild(clickFilterEnabledItem);
            ClickFilterSlider* clickFilterSlider = new ClickFilterSlider(module);
            clickFilterSlider->box.size.x = 200.0;
            menu->addChild(clickFilterSlider);
        };
        Menu* createChildMenu() override {
            Menu* menu = new Menu;
            createClickFilterMenu(this->module, menu);
            return menu;
        };
    };

    struct ClickFilterSlider : rack::ui::Slider {
        float oldValue = DEF_CLICK_FILTER_SLEW;
        Algomorph<OPS, SCENES>* module;

        struct ClickFilterQuantity : rack::Quantity {
            Algomorph<OPS, SCENES>* module;
            float v = -1.f;

            ClickFilterQuantity(Algomorph<OPS, SCENES>* m) {
                module = m;
            };
            void setValue(float value) override {
                v = rack::math::clamp(value, 16.f, 7500.f);
                this->module->clickFilterSlew = v;
            };
            float getValue() override {
                v = this->module->clickFilterSlew;
                return v;
            };
            float getDefaultValue() override {
                return DEF_CLICK_FILTER_SLEW;
            };
            float getMinValue() override {
                return 16.f;
            };
            float getMaxValue() override {
                return 7500.f;
            };
            float getDisplayValue() override {
                return getValue();
            };
            std::string getDisplayValueString() override {
                int i = int(getValue());
                return rack::string::f("%i", i);
            };
            void setDisplayValue(float displayValue) override {
                setValue(displayValue);
            };
            std::string getLabel() override {
                return "Click Filter Slew";
            };
            std::string getUnit() override {
                return "Hz";
            };
        };

        ClickFilterSlider(Algomorph<OPS, SCENES>* m) {
            quantity = new ClickFilterQuantity(m);
            module = m;
        };
        ~ClickFilterSlider() {
            delete quantity;
        };
        void onDragStart(const rack::event::DragStart& e) override {
            if (quantity)
                oldValue = quantity->getValue();
        };
        void onDragMove(const rack::event::DragMove& e) override {
            if (quantity)
                quantity->moveScaledValue(0.002f * e.mouseDelta.x);
        };
        void onDragEnd(const rack::event::DragEnd& e) override {
            if (quantity) {
                // History
                SetClickFilterAction<OPS, SCENES>* h = new SetClickFilterAction<OPS, SCENES>;
                h->moduleId = this->module->id;
                h->oldSlew = oldValue;
                h->newSlew = quantity->getValue();

                APP->history->push(h);
            }    
        };
    };
};
