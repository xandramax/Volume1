#pragma once

struct FMDelexandraSettings {
	bool glowingInkDefault = false;
	bool vuLightsDefault = true;
	int auxInputDefaults[4] = {	0,
								7,
								9,
								0};

	void saveToJson();
	void readFromJson();
};
