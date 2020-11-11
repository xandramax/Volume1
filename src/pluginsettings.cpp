#include "rack.hpp"
#include "pluginsettings.hpp"


FMDelexanderSettings pluginSettings;

FMDelexanderSettings::FMDelexanderSettings() {
    // Initialize to Defaults
    auxInputDefaults[2][0] = true;
    auxInputDefaults[1][5] = true;
    auxInputDefaults[3][9] = true;
    auxInputDefaults[0][0] = true;
}

void FMDelexanderSettings::saveToJson() {
    json_t* settingsJ = json_object();

    json_object_set_new(settingsJ, "glowingInkDefault", json_boolean(glowingInkDefault));
    
    json_object_set_new(settingsJ, "vuLightsDefault", json_boolean(vuLightsDefault));

    json_t* allowMultipleModesJ = json_array();
    for (int i = 0; i < 4; i++) {
        json_t* allowanceJ = json_object();
        json_object_set_new(allowanceJ, "Multimode Allowance", json_boolean(allowMultipleModes[i]));
        json_array_append_new(allowMultipleModesJ, allowanceJ);
    }
    json_object_set_new(settingsJ, "Allow Multiple Modes", allowMultipleModesJ);

    json_t* auxInputDefaultsJ = json_array();
    for (int i = 0; i < 4; i++) {
        // AuxInputModes::NUM_MODES = 18
        for (int mode = 0; mode < 18; mode++) {
            json_t* auxDefaultJ = json_object();
            json_object_set_new(auxDefaultJ, "Aux Default", json_boolean(auxInputDefaults[i][mode]));
            json_array_append_new(auxInputDefaultsJ, auxDefaultJ);
        }
    }
    json_object_set_new(settingsJ, "Default Aux Input Modes", auxInputDefaultsJ);

    std::string settingsFilename = rack::asset::user("FM-Delexander.json");
    FILE* file = fopen(settingsFilename.c_str(), "w");
    if (file) {
        json_dumpf(settingsJ, file, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
        fclose(file);
    }
    json_decref(settingsJ);
}

void FMDelexanderSettings::readFromJson() {
    std::string settingsFilename = rack::asset::user("FM-Delexander.json");
    FILE* file = fopen(settingsFilename.c_str(), "r");
    if (!file) {
        saveToJson();
        return;
    }

    json_error_t error;
    json_t* settingsJ = json_loadf(file, 0, &error);
    if (!settingsJ) {
        // invalid setting json file
        fclose(file);
        saveToJson();
        return;
    }

    json_t* glowingInkDefaultJ = json_object_get(settingsJ, "glowingInkDefault");
    if (glowingInkDefaultJ) glowingInkDefault = json_integer_value(glowingInkDefaultJ);

    json_t* vuLightsDefaultJ = json_object_get(settingsJ, "vuLightsDefault");
    if (vuLightsDefaultJ) vuLightsDefault = json_integer_value(vuLightsDefaultJ);

    json_t* allowMultipleModesJ = json_object_get(settingsJ, "Allow Multiple Modes");
    json_t* allowanceJ; size_t allowanceIndex;
    json_array_foreach(allowMultipleModesJ, allowanceIndex, allowanceJ)
        allowMultipleModes[allowanceIndex] = json_boolean_value(json_object_get(allowanceJ, "Multimode Allowance"));

    json_t* auxInputDefaultsJ = json_object_get(settingsJ, "Default Aux Input Modes");
    json_t* auxDefaultJ; size_t auxDefaultsIndex;
    int auxIndex = 0; int mode = 0;
    json_array_foreach(auxInputDefaultsJ, auxDefaultsIndex, auxDefaultJ) {
        auxInputDefaults[auxIndex][mode] = json_boolean_value(json_object_get(auxDefaultJ, "Aux Default"));
        mode++;
        // AuxInputModes::NUM_MODES = 18
        if (mode > 18)
            auxIndex++;
    }

    fclose(file);
    json_decref(settingsJ);
}
