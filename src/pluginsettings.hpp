#pragma once

struct FMDelexandraSettings {
	bool glowingInkDefault = false;
	bool vuLightsDefault = true;
	bool allowMultipleModes[5] = {false};		// AlgomorphLarge::NUM_AUX_INPUTS = 5
	bool auxInputDefaults[5][19] = {{false}};	// AuxInputModes::NUM_MODES = 19
	// TODO: Use AuxInputModes::NUM_MODES
	// TODO: Use AlgomorphLarge::NUM_AUX_INPUTS

	FMDelexandraSettings();
	void saveToJson();
	void readFromJson();
};
