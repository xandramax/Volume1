#pragma once
#include <rack.hpp>


// AuxInput and AuxKnob modes:

struct AuxSourceModes {
	static const int MORPH = 0;
	static const int MORPH_ATTEN = 1;
	static const int CLICK_FILTER = 2;
	static const int DOUBLE_MORPH = 3;
	static const int TRIPLE_MORPH = 4;
	static const int NUM_MODES = 5;
};

// AuxInput-only modes:

struct AuxInputModes : AuxSourceModes {
	static const int SUM_ATTEN = 		AuxSourceModes::NUM_MODES;
	static const int MOD_ATTEN = 		AuxSourceModes::NUM_MODES + 1;
	static const int CLOCK = 			AuxSourceModes::NUM_MODES + 2;
	static const int REVERSE_CLOCK = 	AuxSourceModes::NUM_MODES + 3;
	static const int RESET = 			AuxSourceModes::NUM_MODES + 4;
	static const int RUN = 				AuxSourceModes::NUM_MODES + 5;
	static const int SCENE_OFFSET = 	AuxSourceModes::NUM_MODES + 6;
	static const int WILDCARD_MOD = 	AuxSourceModes::NUM_MODES + 7;
	static const int WILDCARD_SUM = 	AuxSourceModes::NUM_MODES + 8;
	static const int SHADOW = 			AuxSourceModes::NUM_MODES + 9;
	// 4 shadow modes
	static const int DOUBLE_MORPH_ATTEN = AuxSourceModes::NUM_MODES + 13;
	static const int TRIPLE_MORPH_ATTEN = AuxSourceModes::NUM_MODES + 14;
	static const int NUM_MODES = AuxSourceModes::NUM_MODES + 15;
};

//Order must match above
static const std::string AuxInputModeLabels[AuxInputModes::NUM_MODES] = {	"Morph",
																			"Morph CV Attenuverter",
																			"Click Filter Strength",
																			"Double Morph",
																			"Triple Morph",
																			"Sum Attenuverter",
																			"Modulator Attenuverter",
																			"Clock",
																			"Reverse Clock",
																			"Reset",
																			"Run",
																			"Algorithm Offset",
																			"Wildcard Modulator",
																			"Carrier",
																			"Operator 1",
																			"Operator 2",
																			"Operator 3",
																			"Operator 4",
																			"Morph CV Double Ampliverter",
																			"Morph CV Triple Ampliverter"};

//Order must match above
static const std::string AuxInputModeShortLabels[AuxInputModes::NUM_MODES] = {	"CV",
																				"CV%",
																				"CLICK",
																				"CVx2",
																				"CVx3",
																				"SUM%",
																				"MOD%",
																				"CLOCK",
																				"BTTF",
																				"RESET",
																				"RUN",
																				"ADDR",
																				"WILD",
																				"CARR",
																				"OP 1",
																				"OP 2",
																				"OP 3",
																				"OP 4",
																				"CV%x2",
																				"CV%x3"	};

//Order must match above
static const std::string AuxInputModeDescriptions[AuxInputModes::NUM_MODES] = {	"CV input for modulating Morph state",
																				"CV input for attenuating/inverting Morph modulation",
																				"CV input for modulating the strength of the Click Filter",
																				"2x CV input for modulating Morph state",
																				"3x CV input for modulating Morph state",
																				"CV input for attenuating/inverting the Carrier and Modulator Sum outputs",
																				"CV input for attenuating/inverting the Modulator outputs",
																				"Trigger input for jumping to the next scene",
																				"Trigger input for jumping to the previous scene",
																				"Trigger input for resetting to the default scene",
																				"Trigger input to Stop or Run the module's clock inputs",
																				"CV input for offsetting from the current scene",
																				"Wildcard input, routed to all Modulator outputs",
																				"Carrier input, routed to the Carrier Sum output",
																				"Operator 1 input, routed to match Operator 1's destination",
																				"Operator 2 input, routed to match Operator 2's destination",
																				"Operator 3 input, routed to match Operator 3's destination",
																				"Operator 4 input, routed to match Operator 4's destination",
																				"2x CV input for attenuating/inverting Morph modulation",
																				"3x CV input for attenuating/inverting Morph modulation" };

// AuxKnob-only modes:

struct AuxKnobModes : AuxSourceModes {
	static const int SUM_GAIN = 			AuxSourceModes::NUM_MODES;
	static const int MOD_GAIN = 			AuxSourceModes::NUM_MODES + 1;
	static const int OP_GAIN = 				AuxSourceModes::NUM_MODES + 2;
	static const int UNI_MORPH = 			AuxSourceModes::NUM_MODES + 3;
	static const int ENDLESS_MORPH = 		AuxSourceModes::NUM_MODES + 4;
	static const int DOUBLE_MORPH_ATTEN = 	AuxSourceModes::NUM_MODES + 5;
	static const int TRIPLE_MORPH_ATTEN = 	AuxSourceModes::NUM_MODES + 6;
	static const int WILDCARD_MOD_GAIN = 	AuxSourceModes::NUM_MODES + 7;
	static const int NUM_MODES = 			AuxSourceModes::NUM_MODES + 8;
};

//Order must match above
static const std::string AuxKnobModeLabels[AuxKnobModes::NUM_MODES] = {		"Morph",
																			"Morph CV Attenuverter",
																			"Click Filter Strength",
																			"Double Morph",
																			"Triple Morph",
																			"Sum Outputs Gain",
																			"Mod Outputs Gain",
																			"Op Inputs Gain",
																			"Unipolar Triple Morph",
																			"Endless Morph",
																			"Morph CV Double Ampliverter",
																			"Morph CV Triple Ampliverter",
																			"Wildcard Mod Gain"};

//Order must match above
static constexpr float DEF_KNOB_VALUES[AuxKnobModes::NUM_MODES] = {	0.f,
                                                                    0.f,
                                                                    1.f,
                                                                    0.f,
                                                                    0.f,
                                                                    1.f,
                                                                    1.f,
                                                                    1.f,
                                                                    0.f,
                                                                    0.f,
																	1.f,
																	1.f,
																	1.f	};


// AuxInput Structure

struct AuxInput {
    rack::engine::Module* module;
    int id = -1;
    bool connected = false;
    int channels = 0;
    
    float voltage[AuxInputModes::NUM_MODES][16];
    float defVoltage[AuxInputModes::NUM_MODES] = { 0.f };

    bool modeIsActive[AuxInputModes::NUM_MODES] = {false};
    bool allowMultipleModes = false;
    int activeModes = 0;
    int lastSetMode = 0;

	std::string label = "";
	std::string shortLabel = "";
	std::string description = "";

    rack::dsp::SchmittTrigger runCVTrigger;
    rack::dsp::SchmittTrigger sceneAdvCVTrigger;
    rack::dsp::SchmittTrigger reverseSceneAdvCVTrigger;

    rack::dsp::SlewLimiter shadowClickFilter[4];                  // [op]
    
    rack::dsp::SlewLimiter wildcardModClickFilter[16];
    float wildcardModClickGain = 0.f;
    rack::dsp::SlewLimiter wildcardSumClickFilter[16];
    float wildcardSumClickGain = 0.f;

    AuxInput(int id, rack::engine::Module* module);
    void resetVoltages();
    void setMode(int newMode);
    void unsetAuxMode(int oldMode);
    void clearAuxModes();
    void updateVoltage();
	void updateLabel();
};

// Dynamic port tooltips

struct AuxInputInfo : rack::engine::PortInfo {
	AuxInput* input;

	std::string getName() override;
	std::string getDescription() override;
};

AuxInputInfo* configAuxInput(int portId, AuxInput* input, rack::engine::Module* module);