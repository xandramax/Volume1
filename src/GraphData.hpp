#pragma once
#include <rack.hpp>
using rack::math::Vec;


struct GraphData {
    static const float xNodeData[1980][9];
    static const float yNodeData[1980][9];
    static const Vec moveCurveData[1980][9];
    static const float xCurveData[1980][47];
    static const float yCurveData[1980][47];
    static const float xPolygonData[1980][90];
    static const float yPolygonData[1980][90];
};
