#pragma once

struct FMDelexanderSettings {
	bool glowingInkDefault = false;
	bool vuLightsDefault = true;
	bool allowMultipleModes[4] = {false};
	// AuxInputModes::NUM_MODES = 18
	bool auxInputDefaults[4][18] = {{false}};

	FMDelexanderSettings();
	void saveToJson();
	void readFromJson();
};
