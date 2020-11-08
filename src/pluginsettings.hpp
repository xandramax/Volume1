#pragma once

struct FMDelexanderSettings {
	bool glowingInkDefault = false;
	bool vuLightsDefault = true;
	int auxInputDefaults[4] = {	0,
								7,
								9,
								0};

	void saveToJson();
	void readFromJson();
};
