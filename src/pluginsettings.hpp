#pragma once
#include "AuxSources.hpp"

struct DelexanderVol1Settings {
	bool glowingInkDefault = false;
	bool vuLightsDefault = true;
	//NUM_AUX_INPUTS = 5
	bool allowMultipleModes[5] = {false};
	bool auxInputDefaults[5][AuxInputModes::NUM_MODES] = {{false}};

	DelexanderVol1Settings();
	void initDefaults();
	void saveToJson();
	void readFromJson();
};
