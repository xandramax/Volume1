#pragma once
#include "rack.hpp"

/// Auxiliary input and knob

struct AuxSourceModes {
	static const int MORPH = 0;
	static const int MORPH_ATTEN = 1;
	static const int CLICK_FILTER = 2;
	static const int DOUBLE_MORPH = 3;
	static const int TRIPLE_MORPH = 4;
	static const int NUM_MODES = 5;
};

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
																			"Sum Output Attenuverter",
																			"Mod Output Attenuverter",
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


template < typename MODULE >
struct AuxInput {
    MODULE* module;
    int id = -1;
    bool connected = false;
    int channels = 0;
    
    float voltage[AuxInputModes::NUM_MODES][16];
    float defVoltage[AuxInputModes::NUM_MODES] = { 0.f };

    bool modeIsActive[AuxInputModes::NUM_MODES] = {false};
    bool allowMultipleModes = false;
    int activeModes = 0;
    int lastSetMode = 0;

    rack::dsp::SchmittTrigger runCVTrigger;
    rack::dsp::SchmittTrigger sceneAdvCVTrigger;
    rack::dsp::SchmittTrigger reverseSceneAdvCVTrigger;

    rack::dsp::SlewLimiter shadowClickFilter[4];                  // [op]
    
    rack::dsp::SlewLimiter wildcardModClickFilter[16];
    float wildcardModClickGain = 0.f;
    rack::dsp::SlewLimiter wildcardSumClickFilter[16];
    float wildcardSumClickGain = 0.f;

    AuxInput(int id, MODULE* module);
    void resetVoltages();
    void setMode(int newMode);
    void unsetAuxMode(int oldMode);
    void clearAuxModes();
    void updateVoltage();
};