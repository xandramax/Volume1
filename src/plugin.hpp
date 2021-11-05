#pragma once
#include <rack.hpp>
#include <bitset>
#include "pluginsettings.hpp"
#include "GraphStructure.hpp"
#include "GraphData.hpp"
#include "AlgomorphHistory.hpp"


using namespace rack;

extern Plugin* pluginInstance;

extern DelexanderVol1Settings pluginSettings;

extern Model* modelAlgomorphLarge;
extern Model* modelAlgomorphSmall;


/// Constants:

const NVGcolor DLXDarkPurple = nvgRGB(0x14, 0x0d, 0x13);
const NVGcolor DLXLightPurple = nvgRGB(0x8b, 0x70, 0xa2);
const NVGcolor DLXRed = nvgRGB(0xae, 0x34, 0x58);
const NVGcolor DLXYellow = nvgRGB(0xa9, 0xa9, 0x83);
constexpr float BLINK_INTERVAL = 0.42857142857f;
constexpr float DEF_CLICK_FILTER_SLEW = 3750.f;
constexpr float FIVE_D_TWO = 5.f / 2.f;
constexpr float FIVE_D_THREE = 5.f / 3.f;
constexpr float CLOCK_IGNORE_DURATION = 0.001f;     // disable clock on powerup and reset for 1 ms (so that the first step plays)
constexpr float DEF_RED_BRIGHTNESS = 0.6475f;
constexpr float INDICATOR_BRIGHTNESS = 1.f;
constexpr float SVG_LIGHT_MIN_ALPHA = 4.f/9.f;
constexpr float RING_RADIUS = 8.752f;
constexpr float RING_LIGHT_STROKEWIDTH = 0.75f;
constexpr float RING_BG_STROKEWIDTH = 1.55f;
constexpr float RING_BORDER_STROKEWIDTH = 0.5825f;
constexpr float RING_STROKEWIDTH = RING_LIGHT_STROKEWIDTH + RING_BG_STROKEWIDTH + RING_BORDER_STROKEWIDTH;
constexpr float LINE_LIGHT_STROKEWIDTH = .975f;
const GraphData GRAPH_DATA;

///

struct bitsetCompare {
    bool operator() (const std::bitset<16> &b1, const std::bitset<16> &b2) const {
        return b1.to_ulong() < b2.to_ulong();
	}
};
