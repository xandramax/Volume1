#pragma once

struct FMDelexandraSettings {
	bool glowingInkDefault = false;
	bool vuLightsDefault = true;
	bool allowMultipleModes[4] = {false};
	// AuxInputModes::NUM_MODES = 18
	bool auxInputDefaults[4][18] = {{false}};

	FMDelexandraSettings();
	void saveToJson();
	void readFromJson();
};
