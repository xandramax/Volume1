#include "plugin.hpp"


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
        ENUMS(CONNECTION_LIGHTS, 16),
		NUM_LIGHTS
	};
    bool opDestinations[3][4][4];
    int currentScene = 1;
    int configMode = -1;     //Set to 0-3 when configuring mod destinations for operators 1-4

    dsp::BooleanTrigger sceneTrigger[3];
    dsp::BooleanTrigger operatorTrigger[4];
    dsp::BooleanTrigger modulatorTrigger[4];

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
                for (int k = 0; k < 4; k++) {
                    opDestinations[i][j][k] = false;
                }
            }
        }
	}

	void process(const ProcessArgs& args) override {
		float in[16] = {0.f};
        float modOut[4][16] = {0.f};
        float sumOut[16] = {0.f};
        bool carrier[3][4] = {  {true, true, true, true},
                                {true, true, true, true},
                                {true, true, true, true} };
		int channels = 1;
        float morph = inputs[MORPH_CV].getVoltage() / 5.f + params[MORPH_KNOB].getValue();
        printf("morph: %f\n", morph);
        clamp(morph, -1.f, 1.f);

        if (configMode == -1) {  //Display morph state
            //Set scene lights
            if (morph == 0.f) {
                for (int i = 0; i < 3; i++) {
                    if (i == currentScene)
                        lights[SCENE_LIGHTS + i].setBrightness(1.f);
                    else
                        lights[SCENE_LIGHTS + i].setBrightness(0.f);
                }
            }
            else if (morph > 0.f) {
                lights[SCENE_LIGHTS + currentScene].setBrightness(1.f - morph);
                lights[SCENE_LIGHTS + (currentScene + 1) % 3].setBrightness(morph);
            }
            else {
                lights[SCENE_LIGHTS + currentScene].setBrightness(1.f - (morph * -1.f));
                lights[SCENE_LIGHTS + (currentScene + 2) % 3].setBrightness(morph * -1.f);
            }
            //Set connection lights
            if (morph == 0.f) {
                for (int i = 0; i < 16; i++)
                    lights[CONNECTION_LIGHTS + i].setBrightness(opDestinations[currentScene][i / 4][i % 4] ? 1.f : 0.f);
            }
            else if (morph > 0.f) {
                float brightness;
                for (int i = 0; i < 16; i++) {
                    brightness = 0.f;
                    if (opDestinations[currentScene][i / 4][i % 4])
                        brightness += 1.f - morph;
                    if (opDestinations[(currentScene + 1) % 3][i / 4][i % 4])
                        brightness += morph;
                    lights[CONNECTION_LIGHTS + i].setBrightness(brightness);
                }
            }
            else {
                float brightness;
                for (int i = 0; i < 16; i++) {
                    brightness = 0.f;
                    if (opDestinations[currentScene][i / 4][i % 4])
                        brightness += 1.f - (morph * -1.f);
                    if (opDestinations[(currentScene + 2) % 3][i / 4][i % 4])
                        brightness += morph * -1.f;
                    lights[CONNECTION_LIGHTS + i].setBrightness(brightness);
                }
            }
        }
        else {  //Display current state without morph
            //Set scene lights
            for (int i = 0; i < 3; i++) {
                if (i == currentScene)
                    lights[SCENE_LIGHTS + i].setBrightness(1.f);
                else
                    lights[SCENE_LIGHTS + i].setBrightness(0.f);
            }
            //Set connection lights
            for (int i = 0; i < 16; i++)
                lights[CONNECTION_LIGHTS + i].setBrightness(opDestinations[currentScene][i / 4][i % 4] ? 1.f : 0.f);
        }

        //Check to change scene
        for (int i = 0; i < 3; i++) {
            if (sceneTrigger[i].process(params[SCENE_BUTTONS + i].getValue() > 0.f)) {
                //If changing to a new scene
                if (currentScene == i) {
                    //Reset scene morph knob
                    params[MORPH_KNOB].setValue(0.f);
                }
                else {
                    //Adjust scene lights
                    lights[SCENE_LIGHTS + currentScene].setBrightness(0.f);
                    lights[SCENE_LIGHTS + i].setBrightness(1.f);
                    currentScene = i;
                    //Turn off config mode if necessary
                    if (configMode > -1) {
                        lights[OPERATOR_LIGHTS + configMode].setBrightness(0.f);
                        for (int j = 0; j < 4; j++) {
                            lights[MODULATOR_LIGHTS + j].setBrightness(0.f);
                        }
                        configMode = -1;
                    }
                    //Adjust connection lights
                    for (int j = 0; j < 16; j++)
                        lights[CONNECTION_LIGHTS + j].setBrightness(opDestinations[currentScene][j / 4][j % 4] ? 1.f : 0.f);
                    //Reset scene morph knob
                    params[MORPH_KNOB].setValue(0.f);
                }
            }
        }

        //Check to enter config mode
        for (int i = 0; i < 4; i++) {
            if (operatorTrigger[i].process(params[OPERATOR_BUTTONS + i].getValue() > 0.f)) {
			    if (configMode == i) {
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

                    for (int j = 0; j < 4; j++) {
                        lights[MODULATOR_LIGHTS + j].setBrightness(opDestinations[currentScene][configMode][j] ? 1.f : 0.f);
                    }
                }

                break;
            }
        }

        //Check for config mode destination selection
        if (configMode > -1) {
            for (int i = 0; i < 4; i++) {
                if (modulatorTrigger[i].process(params[MODULATOR_BUTTONS + i].getValue() > 0.f)) {
                    //Toggle the light before toggling the opDestination...?
                    lights[MODULATOR_LIGHTS + i].setBrightness((opDestinations[currentScene][configMode][i]) ? 0.f : 1.f); 

                    opDestinations[currentScene][configMode][i] = !opDestinations[currentScene][configMode][i];

                    break;
                }
            }
        }

        // Get operator input, route to modulation output, additionally route carriers to sum output
		for (int i = 0; i < 4; i++) {
			if (inputs[OPERATOR_INPUTS + i].isConnected()) {
				if (channels < inputs[OPERATOR_INPUTS + i].getChannels())
                    channels = inputs[OPERATOR_INPUTS + i].getChannels();

				inputs[OPERATOR_INPUTS + i].readVoltages(in);

                if (morph == 0.f) {
                    for (int j = 0; j < 4; j++) {
                        if (opDestinations[currentScene][i][j]) {
                            for (int c = 0; c < channels; c++) {
                                modOut[j][c] += in[c];
                            }
                            carrier[currentScene][i] = false;
                        }
                    }

                    if (carrier[currentScene][i]) {
                        for (int c = 0; c < channels; c++) {
                            sumOut[c] += in[c];
                        }
                    }
                }
                else if (morph > 0.f) {
                    for (int j = 0; j < 4; j++) {
                        if (opDestinations[currentScene][i][j]) {
                            for (int c = 0; c < channels; c++)
                                modOut[j][c] += in[c] * (1.f - morph);
                            carrier[currentScene][i] = false;
                        }
                        if (opDestinations[(currentScene + 1) % 3][i][j]) {
                            for (int c = 0; c < channels; c++)
                                modOut[j][c] += in[c] * morph;
                            carrier[(currentScene + 1) % 3][i] = false;
                        }
                    }

                    if (carrier[currentScene][i]) {
                        for (int c = 0; c < channels; c++)
                            sumOut[c] += in[c] * (1.f - morph);
                    }
                    if (carrier[(currentScene + 1) % 3][i]) {
                        for (int c = 0; c < channels; c++)
                            sumOut[c] += in[c] * morph;
                    }
                }
                else {
                    for (int j = 0; j < 4; j++) {
                        if (opDestinations[currentScene][i][j]) {
                            for (int c = 0; c < channels; c++)
                                modOut[j][c] += in[c] * (1.f - (morph * -1.f));
                            carrier[currentScene][i] = false;
                        }
                        if (opDestinations[(currentScene + 2) % 3][i][j]) {
                            for (int c = 0; c < channels; c++)
                                modOut[j][c] += in[c] * (morph * -1.f);
                            carrier[(currentScene + 2) % 3][i] = false;
                        }
                    }

                    if (carrier[currentScene][i]) {
                        for (int c = 0; c < channels; c++)
                            sumOut[c] += in[c] * (1.f - (morph * -1.f));
                    }
                    if (carrier[(currentScene + 2) % 3][i]) {
                        for (int c = 0; c < channels; c++)
                            sumOut[c] += in[c] * (morph * -1.f);
                    }
                }
			}			
		}
        // Set output
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
};


struct Algomorph4Widget : ModuleWidget {
    LineLight* createLineLight(Vec a, Vec b, engine::Module* module, int firstLightId) {
        LineLight* o = new LineLight(a, b);
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

        addChild(createParamCentered<TL1105>(Vec(37.414, 163.627), module, Algomorph4::SCENE_BUTTONS + 0));
        addChild(createParamCentered<TL1105>(Vec(62.755, 163.627), module, Algomorph4::SCENE_BUTTONS + 1));
        addChild(createParamCentered<TL1105>(Vec(88.097, 163.627), module, Algomorph4::SCENE_BUTTONS + 2));

        addChild(createLightCentered<TinyLight<TPurpleLight>>(Vec(37.414, 175.271), module, Algomorph4::SCENE_LIGHTS + 0));
        addChild(createLightCentered<TinyLight<TPurpleLight>>(Vec(62.755, 175.271), module, Algomorph4::SCENE_LIGHTS + 1));
        addChild(createLightCentered<TinyLight<TPurpleLight>>(Vec(88.097, 175.271), module, Algomorph4::SCENE_LIGHTS + 2));

		addInput(createInput<DLXPortPoly>(Vec(30.234, 190.601), module, Algomorph4::MORPH_CV));

		addChild(createParam<DLXKnob>(Vec(65.580, 185.682), module, Algomorph4::MORPH_KNOB));

        addOutput(createOutput<DLXPortPolyOut>(Vec(115.420, 171.305), module, Algomorph4::SUM_OUTPUT));

        addChild(createLightCentered<TinyLight<TPurpleLight>>(Vec(11.181, 247.736), module, Algomorph4::OPERATOR_LIGHTS + 0));
        addChild(createLightCentered<TinyLight<TPurpleLight>>(Vec(11.181, 278.458), module, Algomorph4::OPERATOR_LIGHTS + 1));
        addChild(createLightCentered<TinyLight<TPurpleLight>>(Vec(11.181, 309.180), module, Algomorph4::OPERATOR_LIGHTS + 2));
        addChild(createLightCentered<TinyLight<TPurpleLight>>(Vec(11.181, 339.902), module, Algomorph4::OPERATOR_LIGHTS + 3));

        /* addChild(createLightCentered<TinyLight<TWhitePurpleLight>>(Vec(152.820, 247.736), module, Algomorph4::MODULATOR_0_LIGHTS + 0));
        addChild(createLightCentered<TinyLight<TWhitePurpleLight>>(Vec(152.820, 278.458), module, Algomorph4::MODULATOR_0_LIGHTS + 1));
        addChild(createLightCentered<TinyLight<TWhitePurpleLight>>(Vec(152.820, 309.180), module, Algomorph4::MODULATOR_0_LIGHTS + 2));
        addChild(createLightCentered<TinyLight<TWhitePurpleLight>>(Vec(152.820, 339.902), module, Algomorph4::MODULATOR_0_LIGHTS + 3)); */

        addChild(createLightCentered<TinyLight<TPurpleLight>>(Vec(152.820, 247.736), module, Algomorph4::MODULATOR_LIGHTS + 0));
        addChild(createLightCentered<TinyLight<TPurpleLight>>(Vec(152.820, 278.458), module, Algomorph4::MODULATOR_LIGHTS + 1));
        addChild(createLightCentered<TinyLight<TPurpleLight>>(Vec(152.820, 309.180), module, Algomorph4::MODULATOR_LIGHTS + 2));
        addChild(createLightCentered<TinyLight<TPurpleLight>>(Vec(152.820, 339.902), module, Algomorph4::MODULATOR_LIGHTS + 3));

		addInput(createInput<DLXPortPoly>(Vec(21.969, 237.806), module, Algomorph4::OPERATOR_INPUTS + 0));
		addInput(createInput<DLXPortPoly>(Vec(21.969, 268.528), module, Algomorph4::OPERATOR_INPUTS + 1));
		addInput(createInput<DLXPortPoly>(Vec(21.969, 299.250), module, Algomorph4::OPERATOR_INPUTS + 2));
		addInput(createInput<DLXPortPoly>(Vec(21.969, 329.972), module, Algomorph4::OPERATOR_INPUTS + 3));

		addOutput(createOutput<DLXPortPolyOut>(Vec(122.164, 237.806), module, Algomorph4::MODULATOR_OUTPUTS + 0));
		addOutput(createOutput<DLXPortPolyOut>(Vec(122.164, 268.528), module, Algomorph4::MODULATOR_OUTPUTS + 1));
		addOutput(createOutput<DLXPortPolyOut>(Vec(122.164, 299.250), module, Algomorph4::MODULATOR_OUTPUTS + 2));
		addOutput(createOutput<DLXPortPolyOut>(Vec(122.164, 329.972), module, Algomorph4::MODULATOR_OUTPUTS + 3));
	
        addChild(createLineLight(OpButtonCenter[0], ModButtonCenter[0], module, Algomorph4::CONNECTION_LIGHTS + 0));
        addChild(createLineLight(OpButtonCenter[0], ModButtonCenter[1], module, Algomorph4::CONNECTION_LIGHTS + 1));
        addChild(createLineLight(OpButtonCenter[0], ModButtonCenter[2], module, Algomorph4::CONNECTION_LIGHTS + 2));
        addChild(createLineLight(OpButtonCenter[0], ModButtonCenter[3], module, Algomorph4::CONNECTION_LIGHTS + 3));

        addChild(createLineLight(OpButtonCenter[1], ModButtonCenter[0], module, Algomorph4::CONNECTION_LIGHTS + 4));
        addChild(createLineLight(OpButtonCenter[1], ModButtonCenter[1], module, Algomorph4::CONNECTION_LIGHTS + 5));
        addChild(createLineLight(OpButtonCenter[1], ModButtonCenter[2], module, Algomorph4::CONNECTION_LIGHTS + 6));
        addChild(createLineLight(OpButtonCenter[1], ModButtonCenter[3], module, Algomorph4::CONNECTION_LIGHTS + 7));

        addChild(createLineLight(OpButtonCenter[2], ModButtonCenter[0], module, Algomorph4::CONNECTION_LIGHTS + 8));
        addChild(createLineLight(OpButtonCenter[2], ModButtonCenter[1], module, Algomorph4::CONNECTION_LIGHTS + 9));
        addChild(createLineLight(OpButtonCenter[2], ModButtonCenter[2], module, Algomorph4::CONNECTION_LIGHTS + 10));
        addChild(createLineLight(OpButtonCenter[2], ModButtonCenter[3], module, Algomorph4::CONNECTION_LIGHTS + 11));

        addChild(createLineLight(OpButtonCenter[3], ModButtonCenter[0], module, Algomorph4::CONNECTION_LIGHTS + 12));
        addChild(createLineLight(OpButtonCenter[3], ModButtonCenter[1], module, Algomorph4::CONNECTION_LIGHTS + 13));
        addChild(createLineLight(OpButtonCenter[3], ModButtonCenter[2], module, Algomorph4::CONNECTION_LIGHTS + 14));
        addChild(createLineLight(OpButtonCenter[3], ModButtonCenter[3], module, Algomorph4::CONNECTION_LIGHTS + 15));

		addParam(createParamCentered<TL1105>(OpButtonCenter[0], module, Algomorph4::OPERATOR_BUTTONS + 0));
		addParam(createParamCentered<TL1105>(OpButtonCenter[1], module, Algomorph4::OPERATOR_BUTTONS + 1));
		addParam(createParamCentered<TL1105>(OpButtonCenter[2], module, Algomorph4::OPERATOR_BUTTONS + 2));
		addParam(createParamCentered<TL1105>(OpButtonCenter[3], module, Algomorph4::OPERATOR_BUTTONS + 3));

		addParam(createParamCentered<TL1105>(ModButtonCenter[0], module, Algomorph4::MODULATOR_BUTTONS + 0));
		addParam(createParamCentered<TL1105>(ModButtonCenter[1], module, Algomorph4::MODULATOR_BUTTONS + 1));
		addParam(createParamCentered<TL1105>(ModButtonCenter[2], module, Algomorph4::MODULATOR_BUTTONS + 2));
		addParam(createParamCentered<TL1105>(ModButtonCenter[3], module, Algomorph4::MODULATOR_BUTTONS + 3));
    }
};


Model* modelAlgomorph4 = createModel<Algomorph4, Algomorph4Widget>("Algomorph4");
