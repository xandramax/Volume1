#include <rack.hpp>
#include "pluginsettings.hpp"
#include "AuxSources.hpp"


FMDelexanderSettings pluginSettings;

FMDelexanderSettings::FMDelexanderSettings() {
    initDefaults();
}

void FMDelexanderSettings::initDefaults() {
    // Initialize to Defaults
    auxInputDefaults[0][AuxInputModes::RESET] = true;
    auxInputDefaults[1][AuxInputModes::CLOCK] = true;
    auxInputDefaults[2][AuxInputModes::WILDCARD_MOD] = true;
    auxInputDefaults[3][AuxInputModes::MORPH] = true;
    auxInputDefaults[4][AuxInputModes::MORPH] = true;
}

void FMDelexanderSettings::saveToJson() {
    json_t* settingsJ = json_object();

    json_object_set_new(settingsJ, "glowingInkDefault", json_boolean(glowingInkDefault));
    
    json_object_set_new(settingsJ, "vuLightsDefault", json_boolean(vuLightsDefault));

    json_t* allowMultipleModesJ = json_array();
    for (int auxIndex = 0; auxIndex < 5; auxIndex++) {
        json_t* allowanceJ = json_object();
        json_object_set_new(allowanceJ, (std::string("Aux Input ") + std::to_string(auxIndex).c_str() + ": " + "Multimode Allowed").c_str(), json_boolean(allowMultipleModes[auxIndex]));
        json_array_append_new(allowMultipleModesJ, allowanceJ);
    }
    json_object_set_new(settingsJ, "Aux Inputs: Allow Multiple Modes", allowMultipleModesJ);

    json_t* auxInputDefaultsJ = json_array();
    for (int auxIndex = 0; auxIndex < 5; auxIndex++) {
        for (int mode = 0; mode < AuxInputModes::NUM_MODES; mode++) {
            json_t* auxDefaultJ = json_object();
            json_object_set_new(auxDefaultJ, (std::string("Aux Input ") + std::to_string(auxIndex).c_str() + " Default: " + AuxInputModeLabels[mode]).c_str(), json_boolean(auxInputDefaults[auxIndex][mode]));
            json_array_append_new(auxInputDefaultsJ, auxDefaultJ);
        }
    }
    json_object_set_new(settingsJ, "Aux Inputs: Default Modes", auxInputDefaultsJ);

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
        initDefaults();
        saveToJson();
        return;
    }

    json_error_t error;
    json_t* settingsJ = json_loadf(file, 0, &error);
    if (!settingsJ) {
        // invalid setting json file
        fclose(file);
        initDefaults();
        saveToJson();
        return;
    }

    json_t* glowingInkDefaultJ = json_object_get(settingsJ, "glowingInkDefault");
    if (glowingInkDefaultJ)
        glowingInkDefault = json_boolean_value(glowingInkDefaultJ);

    json_t* vuLightsDefaultJ = json_object_get(settingsJ, "vuLightsDefault");
    if (vuLightsDefaultJ)
        vuLightsDefault = json_boolean_value(vuLightsDefaultJ);

    json_t* allowMultipleModesJ = json_object_get(settingsJ, "Aux Inputs: Allow Multiple Modes");
    if (allowMultipleModesJ) {
        json_t* allowanceJ; size_t allowanceIndex;
        json_array_foreach(allowMultipleModesJ, allowanceIndex, allowanceJ)
            allowMultipleModes[allowanceIndex] = json_boolean_value(json_object_get(allowanceJ, (std::string("Aux Input ") + std::to_string(allowanceIndex).c_str() + ": " + "Multimode Allowed").c_str()));
    }

    json_t* auxInputDefaultsJ = json_object_get(settingsJ, "Aux Inputs: Default Modes");
    if (auxInputDefaultsJ) {
        json_t* auxDefaultJ; size_t auxDefaultsIndex;
        int auxIndex = 0; int mode = 0;
        json_array_foreach(auxInputDefaultsJ, auxDefaultsIndex, auxDefaultJ) {
            auxInputDefaults[auxIndex][mode] = json_boolean_value(json_object_get(auxDefaultJ, (std::string("Aux Input ") + std::to_string(auxIndex).c_str() + " Default: " + AuxInputModeLabels[mode]).c_str()));
            mode++;
            if (mode > AuxInputModes::NUM_MODES - 1) {
                mode = 0;
                auxIndex++;
            }
        }
    }

    fclose(file);
    json_decref(settingsJ);
}
