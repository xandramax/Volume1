#include "plugin.hpp"
#include <bitset>

struct alGraph {
    Vec coords[4];

	alGraph() {
		for (int i = 0; i < 4; i++)
			coords[i] = Vec(graphData[0][i*2], graphData[0][i*2+1]);
	}

	alGraph(int g) {
		for (int i = 0; i < 4; i++)
			coords[i] = Vec(graphData[g][i*2+1], graphData[g][i*2+2]);
	}
};

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
        MORPH_CV,
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
    float morph[16];
    bool opDestinations[3][4][3];   //[scene][op][mod]
    bool opDisabled[3][4];          //[scene][op]
	int opModIndex[4][3];
    int baseScene = 1;
    bool graphDirty = true;

	bool debug = false;

    // std::map<std::bitset<16>, alGraph*, bitsetCompare> graphStore;
	// alGraph graphHeap[1675];
    std::bitset<12> algoName[3];
	int nameIndex[4088];

    //User settings
    bool ringMorph = false;
    bool exitConfigOnConnect = false;
    int configMode = -1;     //Set to 0-3 when configuring mod destinations for operators 1-4

    dsp::BooleanTrigger sceneTrigger[3];
    dsp::BooleanTrigger operatorTrigger[4];
    dsp::BooleanTrigger modulatorTrigger[4];
	// dsp::BooleanTrigger* opModTrigger[4][3]; // Operator-relative sets of modulatorTrigger outputs (i.e. those not belonging to the modulating operator) for graph indexing

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
        lightDivider.setDivision(16);

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                opDisabled[i][j] = false;
                for (int k = 0; k < 3; k++) {
                    opDestinations[i][j][k] = false;
                }
            }
        }

		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				if (i != j) {
					opModIndex[i][i > j ? j : j - 1] = j;
				}
			}
		}

		for (int i = 0; i < 1695; i++) {
			nameIndex[(int)graphData[i][0]] = i;
		}

        // for (int i = 0; i < 13560; i += 9) {
        //     for (int j = 0; j < 4; j++) {
        //         graphHeap[i/9].coords[j].x = graphData[i + 1 + j * 2];
        //         graphHeap[i/9].coords[j].y = graphData[i + 2 + j * 2];
        //     }
		// 	std::bitset<16> debug = std::bitset<16>((long long)graphData[i]);
        //     graphStore.insert({debug, &graphHeap[i/9]});
        // }
		
		graphDirty = true;
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
		graphDirty = true;
        configMode = -1;
        baseScene = 1;
	}

	void process(const ProcessArgs& args) override {
		float in[16] = {0.f};
        float modOut[4][16] = {0.f};
        float sumOut[16] = {0.f};
        bool carrier[3][4] = {  {true, true, true, true},
                                {true, true, true, true},
                                {true, true, true, true} };
		int channels = 1;
        float newMorph0 = clamp(inputs[MORPH_CV].getVoltage(0) / 5.f + params[MORPH_KNOB].getValue(), -1.f, 1.f);
        if (morph[0] != newMorph0) {
            morph[0] = newMorph0;
            graphDirty = true;
        }

		if (debug)
			int x = 0;

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
					if (debug)
						int debugA = i / 3, debugB = i % 3; 
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
                    lights[SCENE_LIGHTS + baseScene].setBrightness(1.f - morph[0]);
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
                    lights[SCENE_LIGHTS + baseScene].setBrightness(1.f - (morph[0] * -1.f));
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
        for (int i = 0; i < 3; i++) {
            if (sceneTrigger[i].process(params[SCENE_BUTTONS + i].getValue() > 0.f)) {
                //If not changing to a new scene
                if (baseScene == i) {
                    //Reset scene morph knob
                    params[MORPH_KNOB].setValue(0.f);
					graphDirty = true;
                }
                else {
                    //Adjust scene lights
                    lights[SCENE_LIGHTS + baseScene].setBrightness(0.f);
                    lights[SCENE_LIGHTS + i].setBrightness(1.f);
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
                    //Reset scene morph knob
                    params[MORPH_KNOB].setValue(0.f);
					graphDirty = true;
                }
            }
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
                        lights[MODULATOR_LIGHTS + opModIndex[configMode][j]].setBrightness(opDestinations[baseScene][configMode][j] ? 1.f : 0.f);
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
					if (modulatorTrigger[opModIndex[configMode][i]].process(params[MODULATOR_BUTTONS + opModIndex[configMode][i]].getValue() > 0.f)) {
						if (debug)
							int x = 0;
						lights[MODULATOR_LIGHTS + opModIndex[configMode][i]].setBrightness((opDestinations[baseScene][configMode][i]) ? 0.f : 1.f); 

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
                morph[c] = inputs[MORPH_CV].getVoltage(c) / 5.f + params[MORPH_KNOB].getValue();
                morph[c] = clamp(morph[c], -1.f, 1.f);
            }
  
            for (int i = 0; i < 4; i++) {
                if (inputs[OPERATOR_INPUTS + i].isConnected()) {
                    in[c] = inputs[OPERATOR_INPUTS + i].getVoltage(c);
                    //Simple case, do not check adjacent algorithms
                    if (morph[c] == 0.f) {
                        if (!opDisabled[baseScene][i]) {
                            for (int j = 0; j < 3; j++) {
                                if (opDestinations[baseScene][i][j]) {
                                    modOut[opModIndex[i][j]][c] += in[c];
                                    carrier[baseScene][i] = false;
                                }
                            }
                            if (carrier[baseScene][i]) {
                                    sumOut[c] += in[c];
                            }
                        }
                    }
                    //Check current algorithm and the one to the right
                    else if (morph[c] > 0.f) {
						for (int j = 0; j < 3; j++) {
                            if (opDestinations[baseScene][i][j] && !opDisabled[baseScene][i]) {
                                modOut[opModIndex[i][j]][c] += in[c] * (1.f - morph[c]);
                                carrier[baseScene][i] = false;
                            }
                            if (opDestinations[(baseScene + 1) % 3][i][j] && !opDisabled[(baseScene + 1) % 3][i]) {
                                modOut[opModIndex[i][j]][c] += in[c] * morph[c];
                                carrier[(baseScene + 1) % 3][i] = false;
                            }
                            if (ringMorph) {
                                //Also check algorithm to the left and invert its mod output
                                if (opDestinations[(baseScene + 2) % 3][i][j] && !opDisabled[(baseScene + 2) % 3][i]) {
                                    modOut[opModIndex[i][j]][c] += in[c] * (morph[c] * -1.f);
                                    carrier[(baseScene + 2) % 3][i] = false;
                                }
                            }
                        }
                        if (carrier[baseScene][i] && !opDisabled[baseScene][i]) {
                            sumOut[c] += in[c] * (1.f - morph[c]);
                        }
                        if (carrier[(baseScene + 1) % 3][i] && !opDisabled[(baseScene + 1) % 3][i]) {
                            sumOut[c] += in[c] * morph[c];
                        }
                        if (ringMorph) {
                            //Also chekc algo to the left and invert its carrier output
                            if (carrier[(baseScene + 2) % 3][i] && !opDisabled[(baseScene + 2) % 3][i]) {
                                sumOut[c] += in[c] * (morph[c] * -1.f);
                            }
                        }
                    }
                    //Check current algorithm and the one to the left
                    else {
						for (int j = 0; j < 3; j++) {
                            if (opDestinations[baseScene][i][j] && !opDisabled[baseScene][i]) {
                                modOut[opModIndex[i][j]][c] += in[c] * (1.f - (morph[c] * -1.f));
                                carrier[baseScene][i] = false;
                            }
                            if (opDestinations[(baseScene + 2) % 3][i][j] && !opDisabled[(baseScene + 2) % 3][i]) {
                                modOut[opModIndex[i][j]][c] += in[c] * (morph[c] * -1.f);
                                carrier[(baseScene + 2) % 3][i] = false;
                            }
                            if (ringMorph) {
                                //Also check algorithm to the right and invert its mod output
                                if (opDestinations[(baseScene + 1) % 3][i][j] && !opDisabled[(baseScene + 1) % 3][i]) {
                                    modOut[opModIndex[i][j]][c] += in[c] * morph[c];
                                    carrier[(baseScene + 1) % 3][i] = false;
                                }
                            }
                        }
                        if (carrier[baseScene][i] && !opDisabled[baseScene][i]) {
                            sumOut[c] += in[c] * (1.f - (morph[c] * -1.f));
                        }
                        if (carrier[(baseScene + 2) % 3][i] && !opDisabled[(baseScene + 2) % 3][i]) {
                            sumOut[c] += in[c] * (morph[c] * -1.f);
                        }
                        if (ringMorph) {
                            //Also chekc algo to the right and invert its carrier output
                            if (carrier[(baseScene + 1) % 3][i] && !opDisabled[(baseScene + 1) % 3][i]) {
                                sumOut[c] += in[c] * morph[c];
                            }
                        }
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

        AlgoDrawWidget(MODULE* module) {
            this->module = module;
            font = APP->window->loadFont(asset::plugin(pluginInstance, "res/terminal-grotesque.ttf"));
        }

        void draw(const Widget::DrawArgs& args) override {
            if (!module) return;

			for (int i = 0; i < 3; i++) {
			// 	auto search = module->graphStore.find(module->algoName[i]);
			// 	if (search != module->graphStore.end())
			// 		graphs[i] = search->second;
				if (module->debug) {
					long long debugA = module->algoName[i].to_ullong();
					int debugB = module->nameIndex[module->algoName[i].to_ullong()];
					std::printf("%lld, %d", debugA, debugB);
				}
				graphs[i] = alGraph(module->nameIndex[(int)module->algoName[i].to_ullong()]);
			}

            float stroke = 0.5f;
			float radius = 8.35425f;
			NVGcolor fillColor = nvgRGBA(149, 122, 172, 160);
			NVGcolor strokeColor = nvgRGB(26, 26, 26);

            nvgStrokeWidth(args.vg, stroke);
			nvgBeginPath(args.vg);
			nvgRect(args.vg, box.getTopLeft().x, box.getTopLeft().y, box.size.x, box.size.y);
			nvgStroke(args.vg);

            // Draw node numbers
            nvgBeginPath(args.vg);
            nvgFontSize(args.vg, 10.f);
            nvgFontFaceId(args.vg, font->handle);
            for (int i = 0; i < 4; i++) {
                std::string s = std::to_string(i + 1);
                char const *id = s.c_str();
                nvgTextBounds(args.vg, graphs[module->baseScene].coords[i].x, graphs[module->baseScene].coords[i].y, id, id + 1, textBounds);
                float xOffset = (textBounds[2] - textBounds[0]) / 2.f;
                float yOffset = (textBounds[3] - textBounds[1]) / 3.25f;
                if (module->debug)
                    int x = 0;
                if (module->morph[0] == 0.f || module->configMode > -1)    //Display state without morph
                    nvgText(args.vg, graphs[module->baseScene].coords[i].x - xOffset, graphs[module->baseScene].coords[i].y + yOffset, id, id + 1);
                else {  //Display moprhed state
                    if (module->morph[0] > 0.f) {
                        nvgText(args.vg,  crossfade(graphs[module->baseScene].coords[i].x, graphs[(module->baseScene + 1) % 3].coords[i].x, module->morph[0]) - xOffset,
                                            crossfade(graphs[module->baseScene].coords[i].y, graphs[(module->baseScene + 1) % 3].coords[i].y, module->morph[0]) + yOffset,
                                            id, id + 1);
                    }
                    else {
                        nvgText(args.vg,  crossfade(graphs[module->baseScene].coords[i].x, graphs[(module->baseScene + 2) % 3].coords[i].x, -module->morph[0]) - xOffset,
                                            crossfade(graphs[module->baseScene].coords[i].y, graphs[(module->baseScene + 2) % 3].coords[i].y, -module->morph[0]) + yOffset,
                                            id, id + 1);
                    }
                }
            }
            nvgStrokeColor(args.vg, strokeColor);
            nvgStroke(args.vg);
            nvgFillColor(args.vg, fillColor);
            nvgFill(args.vg);
            // Draw nodes
            nvgBeginPath(args.vg);
            for (int i = 0; i < 4; i++) {
                if (module->morph[0] == 0.f || module->configMode > -1)    //Display state without morph
                    nvgCircle(args.vg, graphs[module->baseScene].coords[i].x, graphs[module->baseScene].coords[i].y, radius);
                else {  //Display moprhed state
                    if (module->morph[0] > 0.f) {
                        nvgCircle(args.vg,  crossfade(graphs[module->baseScene].coords[i].x, graphs[(module->baseScene + 1) % 3].coords[i].x, module->morph[0]),
                                            crossfade(graphs[module->baseScene].coords[i].y, graphs[(module->baseScene + 1) % 3].coords[i].y, module->morph[0]),
                                            radius);
                    }
                    else {
                        nvgCircle(args.vg,  crossfade(graphs[module->baseScene].coords[i].x, graphs[(module->baseScene + 2) % 3].coords[i].x, -module->morph[0]),
                                            crossfade(graphs[module->baseScene].coords[i].y, graphs[(module->baseScene + 2) % 3].coords[i].y, -module->morph[0]),
                                            radius);
                    }
                }
            }
            nvgFillColor(args.vg, fillColor);
            nvgFill(args.vg);
            nvgStrokeColor(args.vg, strokeColor);
            nvgStroke(args.vg);
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

struct DebugItem : MenuItem {
	Algomorph4 *module;
	void onAction(const event::Action &e) override {
		module->debug ^= true;
	}
};

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

        addChild(createParamCentered<TL1105>(Vec(37.414, 163.627), module, Algomorph4::SCENE_BUTTONS + 0));
        addChild(createParamCentered<TL1105>(Vec(62.755, 163.627), module, Algomorph4::SCENE_BUTTONS + 1));
        addChild(createParamCentered<TL1105>(Vec(88.097, 163.627), module, Algomorph4::SCENE_BUTTONS + 2));

        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(37.414, 175.271), module, Algomorph4::SCENE_LIGHTS + 0));
        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(62.755, 175.271), module, Algomorph4::SCENE_LIGHTS + 1));
        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(88.097, 175.271), module, Algomorph4::SCENE_LIGHTS + 2));

		addInput(createInput<DLXPortPoly>(Vec(30.234, 190.601), module, Algomorph4::MORPH_CV));

		addChild(createParam<DLXKnob>(Vec(65.580, 185.682), module, Algomorph4::MORPH_KNOB));

        addOutput(createOutput<DLXPortPolyOut>(Vec(115.420, 171.305), module, Algomorph4::SUM_OUTPUT));

        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(11.181, 247.736), module, Algomorph4::OPERATOR_LIGHTS + 0));
        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(11.181, 278.458), module, Algomorph4::OPERATOR_LIGHTS + 1));
        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(11.181, 309.180), module, Algomorph4::OPERATOR_LIGHTS + 2));
        addChild(createLightCentered<TinyLight<PurpleLight>>(Vec(11.181, 339.902), module, Algomorph4::OPERATOR_LIGHTS + 3));

        /* addChild(createLightCentered<TinyLight<TWhitePurpleLight>>(Vec(152.820, 247.736), module, Algomorph4::MODULATOR_0_LIGHTS + 0));
        addChild(createLightCentered<TinyLight<TWhitePurpleLight>>(Vec(152.820, 278.458), module, Algomorph4::MODULATOR_0_LIGHTS + 1));
        addChild(createLightCentered<TinyLight<TWhitePurpleLight>>(Vec(152.820, 309.180), module, Algomorph4::MODULATOR_0_LIGHTS + 2));
        addChild(createLightCentered<TinyLight<TWhitePurpleLight>>(Vec(152.820, 339.902), module, Algomorph4::MODULATOR_0_LIGHTS + 3)); */

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
        
		DebugItem *debugItem = createMenuItem<DebugItem>("The system is down", CHECKMARK(module->debug));
		debugItem->module = module;
		menu->addChild(debugItem);
	}
};


Model* modelAlgomorph4 = createModel<Algomorph4, Algomorph4Widget>("Algomorph4");
