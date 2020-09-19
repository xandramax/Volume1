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
        ENUMS(SCENE_LIGHTS, 3),
        ENUMS(OPERATOR_LIGHTS, 4),
        ENUMS(MODULATOR_LIGHTS, 4),
        ENUMS(CONNECTION_LIGHTS, 12),
		ENUMS(DISABLE_LIGHTS, 4),
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
    bool exitConfigOnConnect = false;
    bool ccwSceneSelection = true;      // Default true to interface with rising ramp LFO at Morph CV input

    dsp::BooleanTrigger sceneButtonTrigger[3];
    dsp::BooleanTrigger sceneAdvButtonTrigger;
    dsp::SchmittTrigger sceneAdvCVTrigger;
    dsp::BooleanTrigger operatorTrigger[4];
    dsp::BooleanTrigger modulatorTrigger[4];

    dsp::SlewLimiter clickFilters[2][4][4][16];    // [noRing/ring][op][legal mod][channel], [op][3] = Sum output

    dsp::ClockDivider lightDivider;
    float blinkTimer = BLINK_INTERVAL;
    bool shine = true;

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
                    clickFilters[0][i][j][c].setRiseFall(400.f, 400.f);
                    clickFilters[1][i][j][c].setRiseFall(400.f, 400.f);
                }
            }
        }
        lightDivider.setDivision(512);

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
        exitConfigOnConnect = false;
        ccwSceneSelection = true;
        shine = true;
        blinkTimer = BLINK_INTERVAL;
		graphDirty = true;
	}

    void onRandomize() override {
        bool carrier[3][4];
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
        ringMorph = random::uniform() > .5f;
        graphDirty = true;
        Module::onRandomize();
    }

	void process(const ProcessArgs& args) override {
		float in[16] = {0.f};
        float gain[2][4][4][16] = {{{{0.f}}}};      // [noRing/ring][op][legal mod][channel], gain[x][y][3][z] = sum output
        float modOut[4][16] = {{0.f}};
        float sumOut[16] = {0.f};
        bool carrier[3][4] = {  {true, true, true, true},
                                {true, true, true, true},
                                {true, true, true, true} };
		int channels = 1;

        // Only redraw display if morph has changed
        float newMorph0 = clamp(inputs[MORPH_INPUT].getVoltage(0) / 5.f + params[MORPH_KNOB].getValue(), -1.f, 1.f);
        if (morph[0] != newMorph0) {
            morph[0] = newMorph0;
            graphDirty = true;
        }

        if (lightDivider.process()) {
            if (configMode) {   //Display state without morph, highlight configScene
                //Set scene lights
                for (int i = 0; i < 3; i++) {
                    lights[SCENE_LIGHTS + i].setBrightness(configScene == i ? shine : 0.f);
                }
                //Set op lights
                for (int i = 0; i < 4; i++)
                    lights[OPERATOR_LIGHTS + i].setBrightness(configOp == i ? shine : 0.f);
                //Check and update blink timer
                if (blinkTimer > BLINK_INTERVAL / lightDivider.getDivision()) {
                    shine ^= true;
                    blinkTimer = 0.f;
                }
                blinkTimer += args.sampleTime;
                //Set connection lights
                for (int i = 0; i < 12; i++)
                    lights[CONNECTION_LIGHTS + i].setBrightness(opDestinations[configScene][i / 3][i % 3] ? 1.f : 0.f);
                //Set disable lights
                for (int i = 0; i < 4; i++)
                    lights[DISABLE_LIGHTS + i].setBrightness(opEnabled[configScene][i] ? 0.f : 1.f);
                //Set mod lights
                if (configOp > -1) {
                    for (int i = 0; i < 3; i++) {
                        lights[MODULATOR_LIGHTS + threeToFour[configOp][i]].setBrightness(opDestinations[configScene][configOp][i] ? 1.f : 0.f);
                    }
                }
            } 
            else {
                //Set base scene light
                for (int i = 0; i < 3; i++)
                    lights[SCENE_LIGHTS + i].setBrightness(i == baseScene ? 1.f : 0.f);
                if (morph[0] == 0.f) {  //Display state without morph
                    //Set connection lights
                    for (int i = 0; i < 12; i++)
                        lights[CONNECTION_LIGHTS + i].setBrightness(opDestinations[baseScene][i / 3][i % 3] ? 1.f : 0.f);
                    //Set disable lights
                    for (int i = 0; i < 4; i++)
                        lights[DISABLE_LIGHTS + i].setBrightness(opEnabled[baseScene][i] ? 0.f : 1.f);
                }
                else {  //Display morphed state
                    float brightness;
                    if (morph[0] > 0.f) {
                        //Set scene lights
                        lights[SCENE_LIGHTS + (baseScene + 1) % 3].setBrightness(morph[0]);
                        //Set connection lights
                        for (int i = 0; i < 12; i++) {
                            brightness = 0.f;
                            if (opDestinations[baseScene][i / 3][i % 3])
                                brightness += 1.f - morph[0];
                            if (opDestinations[(baseScene + 1) % 3][i / 3][i % 3])
                                brightness += morph[0];
                            lights[CONNECTION_LIGHTS + i].setBrightness(brightness);
                        }
                        //Set disable lights
                        for (int i = 0; i < 4; i++) {
                            brightness = 0.f;
                            if (!opEnabled[baseScene][i])
                                brightness += 1.f - morph[0];
                            if (!opEnabled[(baseScene + 1) % 3][i])
                                brightness += morph[0];
                            lights[DISABLE_LIGHTS + i].setBrightness(brightness);
                        }
                    }
                    else {
                        //Set scene lights
                        lights[SCENE_LIGHTS + (baseScene + 2) % 3].setBrightness(morph[0] * -1.f);
                        //Set connection lights
                        for (int i = 0; i < 12; i++) {
                            brightness = 0.f;
                            if (opDestinations[baseScene][i / 3][i % 3])
                                brightness += 1.f - (morph[0] * -1.f);
                            if (opDestinations[(baseScene + 2) % 3][i / 3][i % 3])
                                brightness += morph[0] * -1.f;
                            lights[CONNECTION_LIGHTS + i].setBrightness(brightness);
                        }
                        //Set disable lights
                        for (int i = 0; i < 4; i++) {
                            brightness = 0.f;
                            if (!opEnabled[baseScene][i])
                                brightness += 1.f - (morph[0] * -1.f);
                            if (!opEnabled[(baseScene + 2) % 3][i])
                                brightness += morph[0] * -1.f;
                            lights[DISABLE_LIGHTS + i].setBrightness(brightness);
                        }
                    }
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
                        //Turn off op/mod lights
                        for (int i = 0; i < 4; i++) {
                            lights[MODULATOR_LIGHTS + i].setBrightness(0.f);
                            lights[OPERATOR_LIGHTS + i].setBrightness(0.f);
                        }
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
                }
                else if (configOp == i) {  
                    //Deselect operator
                    lights[OPERATOR_LIGHTS + i].setBrightness(0.f);
                    for (int j = 0; j < 4; j++) {
                        lights[MODULATOR_LIGHTS + j].setBrightness(0.f);
                    }
                    configOp = -1;
                    configMode = false;
                }
                else {
                    configOp = i;
                }
                graphDirty = true;
                break;
            }
        }

        //Check for config mode destination selection
        if (configMode && configOp > -1) {
			if (modulatorTrigger[configOp].process(params[MODULATOR_BUTTONS + configOp].getValue() > 0.f)) {  //Op is connected to itself
				lights[MODULATOR_LIGHTS + configOp].setBrightness(opEnabled[configScene][configOp] ? 1.f : 0.f); 
				opEnabled[configScene][configOp] ^= true;

				if (exitConfigOnConnect) {
					lights[OPERATOR_LIGHTS + configOp].setBrightness(0.f);
					for (int j = 0; j < 4; j++) {
						lights[MODULATOR_LIGHTS + j].setBrightness(0.f);
					}
					configMode = false;
				}
                
                graphDirty = true;
			}
			else {
				for (int i = 0; i < 3; i++) {
					if (modulatorTrigger[threeToFour[configOp][i]].process(params[MODULATOR_BUTTONS + threeToFour[configOp][i]].getValue() > 0.f)) {
						lights[MODULATOR_LIGHTS + threeToFour[configOp][i]].setBrightness((opDestinations[configScene][configOp][i]) ? 0.f : 1.f); 

						opDestinations[configScene][configOp][i] ^= true;
						algoName[configScene].flip(configOp * 3 + i);

						if (exitConfigOnConnect) {
							lights[OPERATOR_LIGHTS + configOp].setBrightness(0.f);
							for (int j = 0; j < 4; j++) {
								lights[MODULATOR_LIGHTS + j].setBrightness(0.f);
							}
							configMode = false;
						}

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
                morph[c] = inputs[MORPH_INPUT].getVoltage(c) / 5.f + params[MORPH_KNOB].getValue();
                morph[c] = clamp(morph[c], -1.f, 1.f);
            }

            for (int i = 0; i < 4; i++) {
                if (inputs[OPERATOR_INPUTS + i].isConnected()) {
                    in[c] = inputs[OPERATOR_INPUTS + i].getVoltage(c);
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
	}

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "Config Enabled", json_boolean(configMode));
        json_object_set_new(rootJ, "Config Mode", json_integer(configOp));
        json_object_set_new(rootJ, "Config Scene", json_integer(configOp));
        json_object_set_new(rootJ, "Current Scene", json_integer(baseScene));
        json_object_set_new(rootJ, "Ring Morph", json_boolean(ringMorph));
        json_object_set_new(rootJ, "Auto Exit", json_boolean(exitConfigOnConnect));
        json_object_set_new(rootJ, "CCW Scene Selection", json_boolean(ccwSceneSelection));
        json_object_set_new(rootJ, "Click Filter Enabled", json_boolean(clickFilterEnabled));
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
		exitConfigOnConnect = json_boolean_value(json_object_get(rootJ, "Auto Exit"));
		ccwSceneSelection = json_boolean_value(json_object_get(rootJ, "CCW Scene Selection"));
		clickFilterEnabled = json_boolean_value(json_object_get(rootJ, "Click Filter Enabled"));
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
    struct AlgoDrawWidget : OpaqueWidget {
        MODULE* module;
        alGraph graphs[3];
		bool firstRun = true;
        std::shared_ptr<Font> font;
        float textBounds[4];

        float xOrigin = box.size.x / 2.f;
        float yOrigin = box.size.y / 2.f;
   
        const NVGcolor NODE_FILL_COLOR = nvgRGBA(149, 122, 172, 120);
        const NVGcolor NODE_STROKE_COLOR = nvgRGB(26, 26, 26);
        const NVGcolor TEXT_COLOR = nvgRGBA(0xcc, 0xcc, 0xcc, 255);
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
			nvgRect(args.vg, box.getTopLeft().x, box.getTopLeft().y, box.size.x, box.size.y);
            nvgStrokeWidth(args.vg, borderStroke);
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
                // if (module->debug)
                //     int x = 0;
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

// struct DebugItem : MenuItem {
// 	Algomorph4 *module;
// 	void onAction(const event::Action &e) override {
// 		module->debug ^= true;
// 	}
// };

struct Algomorph4Widget : ModuleWidget {
	Algomorph4Widget(Algomorph4* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Algomorph.svg")));

        Vec OpButtonCenter[4] = {   {59.785, 247.736},
                                    {59.785, 278.458},
                                    {59.785, 309.180},
                                    {59.785, 339.902} };
        Vec ModButtonCenter[4] = {  {104.207, 247.736},
                                    {104.207, 278.458},
                                    {104.207, 309.180},
                                    {104.207, 339.902} };

		addChild(createWidget<ScrewBlack>(Vec(15, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 30, 0)));
		addChild(createWidget<ScrewBlack>(Vec(15, 365)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 30, 365)));

        AlgoScreenWidget<Algomorph4>* screenWidget = new AlgoScreenWidget<Algomorph4>(module);
        screenWidget->box.pos = Vec(25.962f, 30.763f);
        screenWidget->box.size = Vec(113.074f, 93.277f);
		addChild(screenWidget);

        addChild(createParamCentered<TL1105>(Vec(37.414, 158.528), module, Algomorph4::SCENE_BUTTONS + 0));
        addChild(createParamCentered<TL1105>(Vec(62.755, 158.528), module, Algomorph4::SCENE_BUTTONS + 1));
        addChild(createParamCentered<TL1105>(Vec(88.097, 158.528), module, Algomorph4::SCENE_BUTTONS + 2));

        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(37.414, 148.051), module, Algomorph4::SCENE_LIGHTS + 0));
        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(62.755, 148.051), module, Algomorph4::SCENE_LIGHTS + 1));
        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(88.097, 148.051), module, Algomorph4::SCENE_LIGHTS + 2));

		addInput(createInputCentered<DLXPortPoly>(Vec(125.350, 158.528), module, Algomorph4::SCENE_ADV_INPUT));

		addInput(createInput<DLXPortPoly>(Vec(30.234, 190.601), module, Algomorph4::MORPH_INPUT));

		addChild(createParam<DLXKnob>(Vec(65.580, 185.682), module, Algomorph4::MORPH_KNOB));

        addOutput(createOutput<DLXPortPolyOut>(Vec(115.420, 190.601), module, Algomorph4::SUM_OUTPUT));

        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(11.181, 247.736), module, Algomorph4::OPERATOR_LIGHTS + 0));
        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(11.181, 278.458), module, Algomorph4::OPERATOR_LIGHTS + 1));
        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(11.181, 309.180), module, Algomorph4::OPERATOR_LIGHTS + 2));
        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(11.181, 339.902), module, Algomorph4::OPERATOR_LIGHTS + 3));

        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(152.820, 247.736), module, Algomorph4::MODULATOR_LIGHTS + 0));
        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(152.820, 278.458), module, Algomorph4::MODULATOR_LIGHTS + 1));
        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(152.820, 309.180), module, Algomorph4::MODULATOR_LIGHTS + 2));
        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(152.820, 339.902), module, Algomorph4::MODULATOR_LIGHTS + 3));

		addInput(createInput<DLXPortPoly>(Vec(21.969, 237.806), module, Algomorph4::OPERATOR_INPUTS + 0));
		addInput(createInput<DLXPortPoly>(Vec(21.969, 268.528), module, Algomorph4::OPERATOR_INPUTS + 1));
		addInput(createInput<DLXPortPoly>(Vec(21.969, 299.250), module, Algomorph4::OPERATOR_INPUTS + 2));
		addInput(createInput<DLXPortPoly>(Vec(21.969, 329.972), module, Algomorph4::OPERATOR_INPUTS + 3));

		addOutput(createOutput<DLXPortPolyOut>(Vec(122.164, 237.806), module, Algomorph4::MODULATOR_OUTPUTS + 0));
		addOutput(createOutput<DLXPortPolyOut>(Vec(122.164, 268.528), module, Algomorph4::MODULATOR_OUTPUTS + 1));
		addOutput(createOutput<DLXPortPolyOut>(Vec(122.164, 299.250), module, Algomorph4::MODULATOR_OUTPUTS + 2));
		addOutput(createOutput<DLXPortPolyOut>(Vec(122.164, 329.972), module, Algomorph4::MODULATOR_OUTPUTS + 3));
	
        addChild(createLineLight<PurpleLight>(OpButtonCenter[0], ModButtonCenter[1], module, Algomorph4::CONNECTION_LIGHTS + 0));
        addChild(createLineLight<PurpleLight>(OpButtonCenter[0], ModButtonCenter[2], module, Algomorph4::CONNECTION_LIGHTS + 1));
        addChild(createLineLight<PurpleLight>(OpButtonCenter[0], ModButtonCenter[3], module, Algomorph4::CONNECTION_LIGHTS + 2));

        addChild(createLineLight<PurpleLight>(OpButtonCenter[1], ModButtonCenter[0], module, Algomorph4::CONNECTION_LIGHTS + 3));
        addChild(createLineLight<PurpleLight>(OpButtonCenter[1], ModButtonCenter[2], module, Algomorph4::CONNECTION_LIGHTS + 4));
        addChild(createLineLight<PurpleLight>(OpButtonCenter[1], ModButtonCenter[3], module, Algomorph4::CONNECTION_LIGHTS + 5));

        addChild(createLineLight<PurpleLight>(OpButtonCenter[2], ModButtonCenter[0], module, Algomorph4::CONNECTION_LIGHTS + 6));
        addChild(createLineLight<PurpleLight>(OpButtonCenter[2], ModButtonCenter[1], module, Algomorph4::CONNECTION_LIGHTS + 7));
        addChild(createLineLight<PurpleLight>(OpButtonCenter[2], ModButtonCenter[3], module, Algomorph4::CONNECTION_LIGHTS + 8));

        addChild(createLineLight<PurpleLight>(OpButtonCenter[3], ModButtonCenter[0], module, Algomorph4::CONNECTION_LIGHTS + 9));
        addChild(createLineLight<PurpleLight>(OpButtonCenter[3], ModButtonCenter[1], module, Algomorph4::CONNECTION_LIGHTS + 10));
        addChild(createLineLight<PurpleLight>(OpButtonCenter[3], ModButtonCenter[2], module, Algomorph4::CONNECTION_LIGHTS + 11));

        addChild(createLineLight<RedLight>(OpButtonCenter[0], ModButtonCenter[0], module, Algomorph4::DISABLE_LIGHTS + 0));
        addChild(createLineLight<RedLight>(OpButtonCenter[1], ModButtonCenter[1], module, Algomorph4::DISABLE_LIGHTS + 1));
        addChild(createLineLight<RedLight>(OpButtonCenter[2], ModButtonCenter[2], module, Algomorph4::DISABLE_LIGHTS + 2));
        addChild(createLineLight<RedLight>(OpButtonCenter[3], ModButtonCenter[3], module, Algomorph4::DISABLE_LIGHTS + 3));

		addParam(createParamCentered<TL1105>(OpButtonCenter[0], module, Algomorph4::OPERATOR_BUTTONS + 0));
		addParam(createParamCentered<TL1105>(OpButtonCenter[1], module, Algomorph4::OPERATOR_BUTTONS + 1));
		addParam(createParamCentered<TL1105>(OpButtonCenter[2], module, Algomorph4::OPERATOR_BUTTONS + 2));
		addParam(createParamCentered<TL1105>(OpButtonCenter[3], module, Algomorph4::OPERATOR_BUTTONS + 3));

		addParam(createParamCentered<TL1105>(ModButtonCenter[0], module, Algomorph4::MODULATOR_BUTTONS + 0));
		addParam(createParamCentered<TL1105>(ModButtonCenter[1], module, Algomorph4::MODULATOR_BUTTONS + 1));
		addParam(createParamCentered<TL1105>(ModButtonCenter[2], module, Algomorph4::MODULATOR_BUTTONS + 2));
		addParam(createParamCentered<TL1105>(ModButtonCenter[3], module, Algomorph4::MODULATOR_BUTTONS + 3));
    }

    void appendContextMenu(Menu* menu) override {
		Algomorph4* module = dynamic_cast<Algomorph4*>(this->module);

		menu->addChild(new MenuSeparator());

		RingMorphItem *ringMorphItem = createMenuItem<RingMorphItem>("Enable Ring Morph", CHECKMARK(module->ringMorph));
		ringMorphItem->module = module;
		menu->addChild(ringMorphItem);
		
        ExitConfigItem *exitConfigItem = createMenuItem<ExitConfigItem>("Exit Config Mode after Connection", CHECKMARK(module->exitConfigOnConnect));
		exitConfigItem->module = module;
		menu->addChild(exitConfigItem);
        
        CCWScenesItem *ccwScenesItem = createMenuItem<CCWScenesItem>("Trigger input advances counter-clockwise", CHECKMARK(module->ccwSceneSelection));
		ccwScenesItem->module = module;
		menu->addChild(ccwScenesItem);
        
        ClickFilterEnabledItem *clickFilterEnabledItem = createMenuItem<ClickFilterEnabledItem>("Enable click filter", CHECKMARK(module->clickFilterEnabled));
		clickFilterEnabledItem->module = module;
		menu->addChild(clickFilterEnabledItem);

		// DebugItem *debugItem = createMenuItem<DebugItem>("The system is down", CHECKMARK(module->debug));
		// debugItem->module = module;
		// menu->addChild(debugItem);
	}
};


Model* modelAlgomorph4 = createModel<Algomorph4, Algomorph4Widget>("Algomorph4");