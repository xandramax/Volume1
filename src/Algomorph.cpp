#include "plugin.hpp"
#include "GraphData.hpp"
#include <bitset>

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
    bool opDisabled[3][4];          // [scene][op]
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
    int configMode = -1;    // Set to 0-3 when configuring mod destinations for operators 1-4
    int baseScene = 1;      // Center the Morph knob on saved algorithm 0, 1, or 2

    bool graphDirty = true;
	// bool debug = false;

    //User settings
    bool clickFilterEnabled = true;
    bool ringMorph = false;
    bool exitConfigOnConnect = true;
    bool ccwSceneSelection = true;      // Default true to interface with rising ramp LFO at Morph CV input

    dsp::BooleanTrigger sceneButtonTrigger[3];
    dsp::BooleanTrigger sceneAdvButtonTrigger;
    dsp::SchmittTrigger sceneAdvCVTrigger;
    dsp::BooleanTrigger operatorTrigger[4];
    dsp::BooleanTrigger modulatorTrigger[4];

    dsp::SlewLimiter clickFilters[2][4][4][16];    // [noRing/ring][op][legal mod][channel], [op][3] = Sum output

    dsp::ClockDivider lightDivider;

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

        //  Initialize opDisabled[] and opDestinations[] to false;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                opDisabled[i][j] = false;
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
                opDisabled[i][j] = false;
                for (int k = 0; k < 3; k++) {
                    opDestinations[i][j][k] = false;
                }
            }
        }
        configMode = -1;
        baseScene = 1;
        clickFilterEnabled = true;
        ringMorph = false;
        exitConfigOnConnect = true;
        ccwSceneSelection = true;
		graphDirty = true;
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

		// if (debug)
		// 	int x = 0;

        if (lightDivider.process()) {
            if (configMode > -1 || morph[0] == 0.f) {  //Display state without morph
                //Set scene lights
                for (int i = 0; i < 3; i++) {
                    if (i == baseScene)
                        lights[SCENE_LIGHTS + i].setBrightness(1.f);
                    else
                        lights[SCENE_LIGHTS + i].setBrightness(0.f);
                }
                //Set connection lights
                for (int i = 0; i < 12; i++) {
					// if (debug)
					// 	int debugA = i / 3, debugB = i % 3; 
                    lights[CONNECTION_LIGHTS + i].setBrightness(opDestinations[baseScene][i / 3][i % 3] ? 1.f : 0.f);
				}
                //Set disable lights
                for (int i = 0; i < 4; i++)
                    lights[DISABLE_LIGHTS + i].setBrightness(opDisabled[baseScene][i] ? 1.f : 0.f);
                /* //Set op/mod lights
                if (configMode == -1) {
                    for (int i = 0; i < 4; i++) {
                        if (opDisabled[baseScene][i]) {
                            lights[OPERATOR_LIGHTS + i].setBrightness(0.f);
                            lights[MODULATOR_LIGHTS + i].setBrightness(0.f);
                        }
                        else {
                            lights[OPERATOR_LIGHTS + i].setBrightness(0.4f);
                            lights[MODULATOR_LIGHTS + i].setBrightness(0.4f);
                        }
                    }
                } */
            }
            else {  //Display morphed state
                float brightness;
                if (morph[0] > 0.f) {
                    //Set scene lights
                    // lights[SCENE_LIGHTS + baseScene].setBrightness(1.f - morph[0]);
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
                        if (opDisabled[baseScene][i])
                            brightness += 1.f - morph[0];
                        if (opDisabled[(baseScene + 1) % 3][i])
                            brightness += morph[0];
                        lights[DISABLE_LIGHTS + i].setBrightness(brightness);
                    }
                    /* //Set op/mod lights
                    for (int i = 0; i < 4; i++) {
                        brightness = 0.f;
                        if (!opDisabled[baseScene][i])
                            brightness += 1.f - morph[0];
                        if (!opDisabled[(baseScene + 1) % 3][i])
                            brightness += morph[0];
                        brightness *= 0.4f;
                        lights[OPERATOR_LIGHTS + i].setBrightness(brightness);
                        lights[MODULATOR_LIGHTS + i].setBrightness(brightness);
                    } */
                }
                else {
                    //Set scene lights
                    // lights[SCENE_LIGHTS + baseScene].setBrightness(1.f - (morph[0] * -1.f));
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
                        if (opDisabled[baseScene][i])
                            brightness += 1.f - (morph[0] * -1.f);
                        if (opDisabled[(baseScene + 2) % 3][i])
                            brightness += morph[0] * -1.f;
                        lights[DISABLE_LIGHTS + i].setBrightness(brightness);
                    }
                    /* //Set op/mod lights
                    for (int i = 0; i < 4; i++) {
                        brightness = 0.f;
                        if (!opDisabled[baseScene][i])
                            brightness += 1.f - (morph[0] * -1.f);
                        if (!opDisabled[(baseScene + 2) % 3][i])
                            brightness += morph[0] * -1.f;
                        brightness *= 0.4f;
                        lights[OPERATOR_LIGHTS + i].setBrightness(brightness);
                        lights[MODULATOR_LIGHTS + i].setBrightness(brightness);
                    } */
                }
            }
        }

        //Check to change scene
        //Scene buttons
        for (int i = 0; i < 3; i++) {
            if (sceneButtonTrigger[i].process(params[SCENE_BUTTONS + i].getValue() > 0.f)) {
                //If not changing to a new scene
                if (baseScene == i) {
                    //Reset scene morph knob
                    params[MORPH_KNOB].setValue(0.f);
					graphDirty = true;
                    //Turn off config mode if necessary
                    if (configMode > -1) {
                        lights[OPERATOR_LIGHTS + configMode].setBrightness(0.f);
                        for (int j = 0; j < 4; j++) {
                            lights[MODULATOR_LIGHTS + j].setBrightness(0.f);
                        }
                        configMode = -1;
                    }
                }
                else {
                    //Adjust scene lights
                    lights[SCENE_LIGHTS + baseScene].setBrightness(0.f);
                    lights[SCENE_LIGHTS + i].setBrightness(1.f);
                    //Switch scene
                    baseScene = i;
                    //Turn off config mode if necessary
                    if (configMode > -1) {
                        lights[OPERATOR_LIGHTS + configMode].setBrightness(0.f);
                        for (int j = 0; j < 4; j++) {
                            lights[MODULATOR_LIGHTS + j].setBrightness(0.f);
                        }
                        configMode = -1;
                    }
                    //Adjust connection lights
                    for (int j = 0; j < 12; j++)
                        lights[CONNECTION_LIGHTS + j].setBrightness(opDestinations[baseScene][j / 3][j % 3] ? 1.f : 0.f);
                    //Adjust disable lights
                    for (int j = 0; j < 4; j++)
                        lights[DISABLE_LIGHTS + j].setBrightness(opDisabled[baseScene][j] ? 1.f : 0.f);
					graphDirty = true;
                }
            }
        }
        //Scene advance button & trigger input
        if (configMode == -1 && sceneAdvCVTrigger.process(inputs[SCENE_ADV_INPUT].getVoltage())) {
            //Adjust scene lights, advance base scene
            lights[SCENE_LIGHTS + baseScene].setBrightness(0.f);
            if (!ccwSceneSelection)
               baseScene = (baseScene + 1) % 3;
            else
               baseScene = (baseScene + 2) % 3;
            lights[SCENE_LIGHTS + baseScene].setBrightness(1.f);
            //Adjust connection lights
            for (int j = 0; j < 12; j++)
                lights[CONNECTION_LIGHTS + j].setBrightness(opDestinations[baseScene][j / 3][j % 3] ? 1.f : 0.f);
            //Adjust disable lights
            for (int j = 0; j < 4; j++)
                lights[DISABLE_LIGHTS + j].setBrightness(opDisabled[baseScene][j] ? 1.f : 0.f);
            graphDirty = true;
        }

        //Check to enter config mode
        for (int i = 0; i < 4; i++) {
            if (operatorTrigger[i].process(params[OPERATOR_BUTTONS + i].getValue() > 0.f)) {
			    if (configMode == i) {  //Exit config mode
                    lights[OPERATOR_LIGHTS + configMode].setBrightness(0.f);
                    for (int j = 0; j < 4; j++) {
                        lights[MODULATOR_LIGHTS + j].setBrightness(0.f);
                    }
                    configMode = -1;
                }
                else {
                    if (configMode > -1)    //Turn off previous if switching from existing config mode
                        lights[OPERATOR_LIGHTS + configMode].setBrightness(0.f);

                    configMode = i;
                    lights[OPERATOR_LIGHTS + configMode].setBrightness(1.f);

                    for (int j = 0; j < 3; j++) {
                        lights[MODULATOR_LIGHTS + threeToFour[configMode][j]].setBrightness(opDestinations[baseScene][configMode][j] ? 1.f : 0.f);
                    }
                }
                graphDirty = true;
                break;
            }
        }

        //Check for config mode destination selection
        if (configMode > -1) {
			if (modulatorTrigger[configMode].process(params[MODULATOR_BUTTONS + configMode].getValue() > 0.f)) {  //Op is connected to itself
				lights[MODULATOR_LIGHTS + configMode].setBrightness(opDisabled[baseScene][configMode] ? 0.f : 1.f); 
				opDisabled[baseScene][configMode] ^= true;

				if (exitConfigOnConnect) {
					lights[OPERATOR_LIGHTS + configMode].setBrightness(0.f);
					for (int j = 0; j < 4; j++) {
						lights[MODULATOR_LIGHTS + j].setBrightness(0.f);
					}
					configMode = -1;
				}
                
                graphDirty = true;
			}
			else {
				for (int i = 0; i < 3; i++) {
					if (modulatorTrigger[threeToFour[configMode][i]].process(params[MODULATOR_BUTTONS + threeToFour[configMode][i]].getValue() > 0.f)) {
						// if (debug)
						// 	int x = 0;
						lights[MODULATOR_LIGHTS + threeToFour[configMode][i]].setBrightness((opDestinations[baseScene][configMode][i]) ? 0.f : 1.f); 

						opDestinations[baseScene][configMode][i] ^= true;
						algoName[baseScene].flip(configMode * 3 + i);

						if (exitConfigOnConnect) {
							lights[OPERATOR_LIGHTS + configMode].setBrightness(0.f);
							for (int j = 0; j < 4; j++) {
								lights[MODULATOR_LIGHTS + j].setBrightness(0.f);
							}
							configMode = -1;
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
                            bool connected = opDestinations[baseScene][i][j] && !opDisabled[baseScene][i];
                            carrier[baseScene][i] = connected ? false : carrier[baseScene][i];
                            gain[0][i][j][c] = clickFilterEnabled ? clickFilters[0][i][j][c].process(args.sampleTime, connected) : connected;
                            modOut[threeToFour[i][j]][c] += in[c] * gain[0][i][j][c];
                        }
                        bool sumConnected = carrier[baseScene][i] && !opDisabled[baseScene][i];
                        gain[0][i][3][c] = clickFilterEnabled ? clickFilters[0][i][3][c].process(args.sampleTime, sumConnected) : sumConnected;
                        sumOut[c] += in[c] * gain[0][i][3][c];
                    }
                    //Check current algorithm and the one to the right
                    else if (morph[c] > 0.f) {
						for (int j = 0; j < 3; j++) {
                            if (ringMorph) {
                                float ringConnection = opDestinations[(baseScene + 2) % 3][i][j] * morph[c] * !opDisabled[(baseScene + 2) % 3][i];
                                carrier[(baseScene + 2) % 3][i] = ringConnection > 0.f ? false : carrier[(baseScene + 2) % 3][i];
                                gain[1][i][j][c] = clickFilterEnabled ? clickFilters[1][i][j][c].process(args.sampleTime, ringConnection) : ringConnection; 
                            }
                            float connectionA = opDestinations[baseScene][i][j]             * (1.f - morph[c])  * !opDisabled[baseScene][i];
                            float connectionB = opDestinations[(baseScene + 1) % 3][i][j]   * morph[c]          * !opDisabled[(baseScene + 1) % 3][i];
                            carrier[baseScene][i]           = connectionA > 0.f ? false : carrier[baseScene][i];
                            carrier[(baseScene + 1) % 3][i] = connectionB > 0.f ? false : carrier[(baseScene + 1) % 3][i];
                            gain[0][i][j][c] = clickFilterEnabled ? clickFilters[0][i][j][c].process(args.sampleTime, connectionA + connectionB) : connectionA + connectionB;
                            modOut[threeToFour[i][j]][c] += in[c] * gain[0][i][j][c] - in[c] * gain[1][i][j][c];
                        }
                        if (ringMorph) {
                            float ringSumQuantity = carrier[(baseScene + 2) % 3][i] * morph[c] * !opDisabled[(baseScene + 2) % 3][i];
                            gain[1][i][3][c] = clickFilterEnabled ? clickFilters[1][i][3][c].process(args.sampleTime, ringSumQuantity) : ringSumQuantity;
                        }
                        float sumQuantity =     carrier[baseScene][i]           * (1.f - morph[c])  * !opDisabled[baseScene][i]
                                            +   carrier[(baseScene + 1) % 3][i] * morph[c]          * !opDisabled[(baseScene + 1) % 3][i];
                        gain[0][i][3][c] = clickFilterEnabled ? clickFilters[0][i][3][c].process(args.sampleTime, sumQuantity) : sumQuantity;
                        sumOut[c] += in[c] * gain[0][i][3][c] - in[c] * gain[1][i][3][c];
                    }
                    //Check current algorithm and the one to the left
                    else {
						for (int j = 0; j < 3; j++) {
                            if (ringMorph) {
                                float ringConnection = opDestinations[(baseScene + 1) % 3][i][j] * morph[c] * !opDisabled[(baseScene + 1) % 3][i];
                                carrier[(baseScene + 1) % 3][i] = ringConnection > 0.f ? false : carrier[(baseScene + 1) % 3][i];
                                gain[1][i][j][c] = clickFilterEnabled ? clickFilters[1][i][j][c].process(args.sampleTime, ringConnection) : ringConnection;
                            }
                            float connectionA = opDestinations[baseScene][i][j]             * (1.f - (morph[c] * -1.f)) * !opDisabled[baseScene][i];
                            float connectionB = opDestinations[(baseScene + 2) % 3][i][j]   * (morph[c] * -1.f)         * !opDisabled[(baseScene + 2) % 3][i];
                            carrier[baseScene][i]           = connectionA > 0.f ? false : carrier[baseScene][i];
                            carrier[(baseScene + 2) % 3][i] = connectionB > 0.f ? false : carrier[(baseScene + 2) % 3][i];
                            gain[0][i][j][c] = clickFilterEnabled ? clickFilters[0][i][j][c].process(args.sampleTime, connectionA + connectionB) : connectionA + connectionB;
                            modOut[threeToFour[i][j]][c] += in[c] * gain[0][i][j][c] - in[c] * gain[1][i][j][c];
                        }
                        if (ringMorph) {
                            float ringSumQuantity = carrier[(baseScene + 1) % 3][i] * morph[c] * !opDisabled[(baseScene + 1) % 3][i];
                            gain[1][i][3][c] = clickFilterEnabled ? clickFilters[1][i][3][c].process(args.sampleTime, ringSumQuantity) : ringSumQuantity;
                        }
                        float sumQuantity =     carrier[baseScene][i]           * (1.f - (morph[c] * -1.f)) * !opDisabled[baseScene][i]
                                            +   carrier[(baseScene + 2) % 3][i] * (morph[c] * -1.f)         * !opDisabled[(baseScene + 2) % 3][i];
                        gain[0][i][3][c] = clickFilterEnabled ? clickFilters[0][i][3][c].process(args.sampleTime, sumQuantity) : sumQuantity;
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
        json_object_set_new(rootJ, "Config Mode", json_integer(configMode));
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
        json_t* opDisabledJ = json_array();
		for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                json_t* disabledJ = json_object();
                json_object_set_new(disabledJ, "Disabled Op", json_boolean(opDisabled[i][j]));
                json_array_append_new(opDisabledJ, disabledJ);
            }
		}
		json_object_set_new(rootJ, "Operators Disabled", opDisabledJ);
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
		configMode = json_integer_value(json_object_get(rootJ, "Config Mode"));
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
		json_t* opDisabledJ = json_object_get(rootJ, "Operators Disabled");
		json_t* disabledOpJ;
		size_t disabledOpIndex;
        i = j = 0;
		json_array_foreach(opDisabledJ, disabledOpIndex, disabledOpJ) {
            opDisabled[i][j] = json_boolean_value(json_object_get(disabledOpJ, "Disabled Op"));
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
   
        NVGcolor fillColor = nvgRGBA(149, 122, 172, 160);
        NVGcolor strokeColor = nvgRGB(26, 26, 26);
        NVGcolor edgeColor = nvgRGBA(0x9a,0x9a,0x6f,0xff);
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
                        edgeColor = nvgRGBA(0x9a, 0x9a, 0x6f, crossfade(0xff, 0x00, morph));
                    }
                    else {
                        edge[0] = leastEdges.edges[i];
                        edge[1] = mostEdges.edges[i];
                        nvgMoveTo(ctx, crossfade(xOrigin, edge[1].moveCoords.x, morph), crossfade(yOrigin, edge[1].moveCoords.y, morph));
                        edgeColor = nvgRGBA(0x9a, 0x9a, 0x6f, crossfade(0x00, 0xff, morph));
                    }
                    // if (module->debug) {
                    //     float debugA = edge[0].moveCoords.x, debugB = edge[0].moveCoords.y, debugC = xOrigin, debugD = yOrigin;
                    //     int x = 0;
                    // }
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
                    // if (module->debug) {
                    //     float debugA = edge[0].moveCoords.x, debugB = edge[1].moveCoords.x, debugC = edge[0].moveCoords.y, debugD = edge[1].moveCoords.y;
                    //     int x = 0;
                    // }
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
                    edgeColor = nvgRGBA(0x9a, 0x9a, 0x6f, 0xff);
                    // if (module->debug) {
                    //     float debugA = edge[0].moveCoords.x, debugB = edge[1].moveCoords.x, debugC = edge[0].moveCoords.y, debugD = edge[1].moveCoords.y;
                    //     int x = 0;
                    // }
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
                    // if (module->debug) {
                    //     float   debugA = mostCurved.curve[j][0].x, 
                    //             debugB = mostCurved.curve[j][0].y,
                    //             debugC = xOrigin,
                    //             debugD = yOrigin,
                    //             debugE = mostCurved.curve[j][1].x,
                    //             debugF = mostCurved.curve[j][1].y,
                    //             debugG = xOrigin,
                    //             debugH = yOrigin,
                    //             debugI = mostCurved.curve[j][2].x,
                    //             debugJ = mostCurved.curve[j][2].y,
                    //             debugK = xOrigin,
                    //             debugL = yOrigin,
                    //             debugM = crossfade(mostCurved.curve[j][0].x, xOrigin, morph),
                    //             debugN = crossfade(mostCurved.curve[j][0].y, yOrigin, morph),
                    //             debugO = crossfade(mostCurved.curve[j][1].x, xOrigin, morph),
                    //             debugP = crossfade(mostCurved.curve[j][1].y, yOrigin, morph),
                    //             debugQ = crossfade(mostCurved.curve[j][2].x, xOrigin, morph),
                    //             debugR = crossfade(mostCurved.curve[j][2].y, yOrigin, morph);
                    //     int x = 0;
                    // }
                }
                else if (j < leastCurved.curveLength) {
                    if (!flipped)
                        nvgBezierTo(ctx, crossfade(mostCurved.curve[j][0].x, leastCurved.curve[j][0].x, morph), crossfade(mostCurved.curve[j][0].y, leastCurved.curve[j][0].y, morph), crossfade(mostCurved.curve[j][1].x, leastCurved.curve[j][1].x, morph), crossfade(mostCurved.curve[j][1].y, leastCurved.curve[j][1].y, morph), crossfade(mostCurved.curve[j][2].x, leastCurved.curve[j][2].x, morph), crossfade(mostCurved.curve[j][2].y, leastCurved.curve[j][2].y, morph));
                    else
                        nvgBezierTo(ctx, crossfade(leastCurved.curve[j][0].x, mostCurved.curve[j][0].x, morph), crossfade(leastCurved.curve[j][0].y, mostCurved.curve[j][0].y, morph), crossfade(leastCurved.curve[j][1].x, mostCurved.curve[j][1].x, morph), crossfade(leastCurved.curve[j][1].y, mostCurved.curve[j][1].y, morph), crossfade(leastCurved.curve[j][2].x, mostCurved.curve[j][2].x, morph), crossfade(leastCurved.curve[j][2].y, mostCurved.curve[j][2].y, morph));
                    // if (module->debug) {
                    //     float   debugA = mostCurved.curve[j][0].x, 
                    //             debugB = mostCurved.curve[j][0].y,
                    //             debugC = leastCurved.curve[j][0].x,
                    //             debugD = leastCurved.curve[j][0].y,
                    //             debugE = mostCurved.curve[j][1].x,
                    //             debugF = mostCurved.curve[j][1].y,
                    //             debugG = leastCurved.curve[j][1].x,
                    //             debugH = leastCurved.curve[j][1].y,
                    //             debugI = mostCurved.curve[j][2].x,
                    //             debugJ = mostCurved.curve[j][2].y,
                    //             debugK = leastCurved.curve[j][2].x,
                    //             debugL = leastCurved.curve[j][2].y,
                    //             debugM = crossfade(mostCurved.curve[j][0].x, leastCurved.curve[j][0].x, morph),
                    //             debugN = crossfade(mostCurved.curve[j][0].y, leastCurved.curve[j][0].y, morph),
                    //             debugO = crossfade(mostCurved.curve[j][1].x, leastCurved.curve[j][1].x, morph),
                    //             debugP = crossfade(mostCurved.curve[j][1].y, leastCurved.curve[j][1].y, morph),
                    //             debugQ = crossfade(mostCurved.curve[j][2].x, leastCurved.curve[j][2].x, morph),
                    //             debugR = crossfade(mostCurved.curve[j][2].y, leastCurved.curve[j][2].y, morph);
                    //     int x = 0;
                    // }
                }
                else {
                    if (!flipped)
                        nvgBezierTo(ctx, crossfade(mostCurved.curve[j][0].x, leastCurved.curve[leastCurved.curveLength - 1][0].x, morph), crossfade(mostCurved.curve[j][0].y, leastCurved.curve[leastCurved.curveLength - 1][0].y, morph), crossfade(mostCurved.curve[j][1].x, leastCurved.curve[leastCurved.curveLength - 1][1].x, morph), crossfade(mostCurved.curve[j][1].y, leastCurved.curve[leastCurved.curveLength - 1][1].y, morph), crossfade(mostCurved.curve[j][2].x, leastCurved.curve[leastCurved.curveLength - 1][2].x, morph), crossfade(mostCurved.curve[j][2].y, leastCurved.curve[leastCurved.curveLength - 1][2].y, morph));
                    else
                        nvgBezierTo(ctx, crossfade(leastCurved.curve[leastCurved.curveLength - 1][0].x, mostCurved.curve[j][0].x, morph), crossfade(leastCurved.curve[leastCurved.curveLength - 1][0].y, mostCurved.curve[j][0].y, morph), crossfade(leastCurved.curve[leastCurved.curveLength - 1][1].x, mostCurved.curve[j][1].x, morph), crossfade(leastCurved.curve[leastCurved.curveLength - 1][1].y, mostCurved.curve[j][1].y, morph), crossfade(leastCurved.curve[leastCurved.curveLength - 1][2].x, mostCurved.curve[j][2].x, morph), crossfade(leastCurved.curve[leastCurved.curveLength - 1][2].y, mostCurved.curve[j][2].y, morph));
                    // if (module->debug) {
                    //     float   debugA = mostCurved.curve[j][0].x,
                    //             debugB = mostCurved.curve[j][0].y,
                    //             debugC = leastCurved.curve[leastCurved.curveLength - 1][0].x,
                    //             debugD = leastCurved.curve[leastCurved.curveLength - 1][0].y,
                    //             debugE = mostCurved.curve[j][1].x,
                    //             debugF = mostCurved.curve[j][1].y,
                    //             debugG = leastCurved.curve[leastCurved.curveLength - 1][1].x,
                    //             debugH = leastCurved.curve[leastCurved.curveLength - 1][1].y,
                    //             debugI = mostCurved.curve[j][2].x,
                    //             debugJ = mostCurved.curve[j][2].y,
                    //             debugK = leastCurved.curve[leastCurved.curveLength - 1][2].x,
                    //             debugL = leastCurved.curve[leastCurved.curveLength - 1][2].y,
                    //             debugM = crossfade(mostCurved.curve[j][0].x, leastCurved.curve[leastCurved.curveLength - 1][0].x, morph),
                    //             debugN = crossfade(mostCurved.curve[j][0].y, leastCurved.curve[leastCurved.curveLength - 1][0].y, morph),
                    //             debugO = crossfade(mostCurved.curve[j][1].x, leastCurved.curve[leastCurved.curveLength - 1][1].x, morph),
                    //             debugP = crossfade(mostCurved.curve[j][1].y, leastCurved.curve[leastCurved.curveLength - 1][1].y, morph),
                    //             debugQ = crossfade(mostCurved.curve[j][2].x, leastCurved.curve[leastCurved.curveLength - 1][2].x, morph),
                    //             debugR = crossfade(mostCurved.curve[j][2].y, leastCurved.curve[leastCurved.curveLength - 1][2].y, morph);
                    //     int x = 0;
                    // }
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
                    // if (module->debug) {
                    //     float   debugA = mostGregarious.lines[j].x, 
                    //             debugB = mostGregarious.lines[j].y,
                    //             debugC = xOrigin,
                    //             debugD = yOrigin,
                    //             debugE = mostGregarious.lines[j].x,
                    //             debugF = mostGregarious.lines[j].y,
                    //             debugG = xOrigin,
                    //             debugH = yOrigin,
                    //             debugI = mostGregarious.lines[j].x,
                    //             debugJ = mostGregarious.lines[j].y,
                    //             debugK = xOrigin,
                    //             debugL = yOrigin,
                    //             debugM = crossfade(mostGregarious.lines[j].x, xOrigin, morph),
                    //             debugN = crossfade(mostGregarious.lines[j].y, yOrigin, morph),
                    //             debugO = crossfade(mostGregarious.lines[j].x, xOrigin, morph),
                    //             debugP = crossfade(mostGregarious.lines[j].y, yOrigin, morph),
                    //             debugQ = crossfade(mostGregarious.lines[j].x, xOrigin, morph),
                    //             debugR = crossfade(mostGregarious.lines[j].y, yOrigin, morph);
                    //     int x = 0;
                    // }
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
                    // if (module->debug) {
                    //     float   debugA = mostGregarious.lines[j].x, 
                    //             debugB = mostGregarious.lines[j].y,
                    //             debugC = leastGregarious.lines[j].x,
                    //             debugD = leastGregarious.lines[j].y,
                    //             debugE = mostGregarious.lines[j].x,
                    //             debugF = mostGregarious.lines[j].y,
                    //             debugG = leastGregarious.lines[j].x,
                    //             debugH = leastGregarious.lines[j].y,
                    //             debugI = mostGregarious.lines[j].x,
                    //             debugJ = mostGregarious.lines[j].y,
                    //             debugK = leastGregarious.lines[j].x,
                    //             debugL = leastGregarious.lines[j].y,
                    //             debugM = crossfade(mostGregarious.lines[j].x, leastGregarious.lines[j].x, morph),
                    //             debugN = crossfade(mostGregarious.lines[j].y, leastGregarious.lines[j].y, morph),
                    //             debugO = crossfade(mostGregarious.lines[j].x, leastGregarious.lines[j].x, morph),
                    //             debugP = crossfade(mostGregarious.lines[j].y, leastGregarious.lines[j].y, morph),
                    //             debugQ = crossfade(mostGregarious.lines[j].x, leastGregarious.lines[j].x, morph),
                    //             debugR = crossfade(mostGregarious.lines[j].y, leastGregarious.lines[j].y, morph);
                    //     int x = 0;
                    // }
                }
            }
        }

        void draw(const Widget::DrawArgs& args) override {
            if (!module) return;

            xOrigin = box.size.x / 2.f;
            yOrigin = box.size.y / 2.f;

			for (int i = 0; i < 3; i++) {
			// 	auto search = module->graphStore.find(module->algoName[i]);
			// 	if (search != module->graphStore.end())
			// 		graphs[i] = search->second;
				// if (module->debug) {
				// 	long long debugA = module->algoName[i].to_ullong();
				// 	int debugB = module->sixteenToTwelve[module->algoName[i].to_ullong()];
				// 	std::printf("%lld, %d", debugA, debugB);
				// }
                int name = module->sixteenToTwelve[module->algoName[i].to_ullong()];
                if (name != -1)
    				graphs[i] = alGraph(module->sixteenToTwelve[(int)module->algoName[i].to_ullong()]);
                else
                    graphs[i] = alGraph(0);
    		}

            bool noMorph = false;                
			float radius = 8.35425f;

            if (module->morph[0] == 0.f || module->configMode > -1)
                noMorph = true;

			nvgBeginPath(args.vg);
			nvgRect(args.vg, box.getTopLeft().x, box.getTopLeft().y, box.size.x, box.size.y);
            nvgStrokeWidth(args.vg, borderStroke);
			nvgStroke(args.vg);

            // Draw node numbers
            nvgBeginPath(args.vg);
            nvgFontSize(args.vg, 10.f);
            nvgFontFaceId(args.vg, font->handle);
            for (int i = 0; i < 4; i++) {
                std::string s = std::to_string(i + 1);
                char const *id = s.c_str();
                nvgTextBounds(args.vg, graphs[module->baseScene].nodes[i].coords.x, graphs[module->baseScene].nodes[i].coords.y, id, id + 1, textBounds);
                float xOffset = (textBounds[2] - textBounds[0]) / 2.f;
                float yOffset = (textBounds[3] - textBounds[1]) / 3.25f;
                // if (module->debug)
                //     int x = 0;
                if (module->morph[0] == 0.f || module->configMode > -1)    //Display state without morph
                    nvgText(args.vg, graphs[module->baseScene].nodes[i].coords.x - xOffset, graphs[module->baseScene].nodes[i].coords.y + yOffset, id, id + 1);
                else {  //Display moprhed state
                    if (module->morph[0] > 0.f) {
                        nvgText(args.vg,  crossfade(graphs[module->baseScene].nodes[i].coords.x, graphs[(module->baseScene + 1) % 3].nodes[i].coords.x, module->morph[0]) - xOffset,
                                            crossfade(graphs[module->baseScene].nodes[i].coords.y, graphs[(module->baseScene + 1) % 3].nodes[i].coords.y, module->morph[0]) + yOffset,
                                            id, id + 1);
                    }
                    else {
                        nvgText(args.vg,  crossfade(graphs[module->baseScene].nodes[i].coords.x, graphs[(module->baseScene + 2) % 3].nodes[i].coords.x, -module->morph[0]) - xOffset,
                                            crossfade(graphs[module->baseScene].nodes[i].coords.y, graphs[(module->baseScene + 2) % 3].nodes[i].coords.y, -module->morph[0]) + yOffset,
                                            id, id + 1);
                    }
                }
            }
            nvgStrokeColor(args.vg, strokeColor);
            nvgStrokeWidth(args.vg, labelStroke);
            nvgStroke(args.vg);
            nvgFillColor(args.vg, fillColor);
            nvgFill(args.vg);

            // Draw nodes
            nvgBeginPath(args.vg);
            for (int i = 0; i < 4; i++) {
                if (noMorph)    //Display state without morph
                    nvgCircle(args.vg, graphs[module->baseScene].nodes[i].coords.x, graphs[module->baseScene].nodes[i].coords.y, radius);
                else {  //Display moprhed state
                    if (module->morph[0] > 0.f) {
                        nvgCircle(args.vg,  crossfade(graphs[module->baseScene].nodes[i].coords.x, graphs[(module->baseScene + 1) % 3].nodes[i].coords.x, module->morph[0]),
                                            crossfade(graphs[module->baseScene].nodes[i].coords.y, graphs[(module->baseScene + 1) % 3].nodes[i].coords.y, module->morph[0]),
                                            radius);
                    }
                    else {
                        nvgCircle(args.vg,  crossfade(graphs[module->baseScene].nodes[i].coords.x, graphs[(module->baseScene + 2) % 3].nodes[i].coords.x, -module->morph[0]),
                                            crossfade(graphs[module->baseScene].nodes[i].coords.y, graphs[(module->baseScene + 2) % 3].nodes[i].coords.y, -module->morph[0]),
                                            radius);
                    }
                }
            }
            nvgFillColor(args.vg, fillColor);
            nvgFill(args.vg);
            nvgStrokeColor(args.vg, strokeColor);
            nvgStrokeWidth(args.vg, nodeStroke);
            nvgStroke(args.vg);

            // Draw edges +/ arrows
            if (noMorph) {
                // Draw edges
                nvgBeginPath(args.vg);
                for (int i = 0; i < graphs[module->baseScene].numEdges; i++) {
                    Edge edge = graphs[module->baseScene].edges[i];
                    nvgMoveTo(args.vg, edge.moveCoords.x, edge.moveCoords.y);
                    for (int j = 0; j < edge.curveLength; j++) {
                        nvgBezierTo(args.vg, edge.curve[j][0].x, edge.curve[j][0].y, edge.curve[j][1].x, edge.curve[j][1].y, edge.curve[j][2].x, edge.curve[j][2].y);
                        // if (module->debug) {
                        //     float   debugA = edge.curve[j][0].x, 
                        //             debugB = edge.curve[j][1].x, 
                        //             debugC = edge.curve[j][2].x, 
                        //             debugD = edge.curve[j][0].y, 
                        //             debugE = edge.curve[j][1].y, 
                        //             debugF = edge.curve[j][2].y,
                        //             debugG = edge.moveCoords.x,
                        //             debugH = edge.moveCoords.y;
                        //     int x = 0;
                        // }
                    }
                }
                edgeColor = nvgRGBA(0x9a, 0x9a, 0x6f, 0xff);
                nvgStrokeColor(args.vg, edgeColor);
                nvgStrokeWidth(args.vg, edgeStroke);
                nvgStroke(args.vg);
                // Draw arrows
                for (int i = 0; i < graphs[module->baseScene].numEdges; i++) {
                    nvgBeginPath(args.vg);
                    nvgMoveTo(args.vg, graphs[module->baseScene].arrows[i].moveCoords.x, graphs[module->baseScene].arrows[i].moveCoords.y);
                    for (int j = 0; j < 9; j++) {
                        nvgLineTo(args.vg, graphs[module->baseScene].arrows[i].lines[j].x, graphs[module->baseScene].arrows[i].lines[j].y);
                        // if (module->debug) {
                        //     auto debugA = graphs[module->baseScene].arrows[i]; 
                        //     int x = 0;
                        // }
                    }
                    // nvgStrokeColor(args.vg, strokeColor);
                    // nvgStrokeWidth(args.vg, arrowStroke2);
                    // nvgStroke(args.vg);
                    edgeColor = nvgRGBA(0x9a, 0x9a, 0x6f, 0xff);
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
                    drawEdges(args.vg, graphs[module->baseScene], graphs[(module->baseScene + 1) % 3], module->morph[0]);
                else
                    drawEdges(args.vg, graphs[module->baseScene], graphs[(module->baseScene + 2) % 3], -module->morph[0]);
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
    template <typename TBase = GrayModuleLightWidget>
	LineLight<TBase>* createLineLight(Vec a, Vec b, engine::Module* module, int firstLightId) {
        LineLight<TBase>* o = new LineLight<TBase>(a, b);
        o->box.pos = a;
        o->module = module;
        o->firstLightId = firstLightId;
        return o;
    }

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
