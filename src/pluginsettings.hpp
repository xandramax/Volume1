#pragma once
#include "AuxSources.hpp"

struct FMDelexanderSettings {
	bool glowingInkDefault = false;
	bool vuLightsDefault = true;
	//NUM_AUX_INPUTS = 5
	bool allowMultipleModes[5] = {false};
	bool auxInputDefaults[5][AuxInputModes::NUM_MODES] = {{false}};

	FMDelexanderSettings();
	void initDefaults();
	void saveToJson();
	void readFromJson();
};
