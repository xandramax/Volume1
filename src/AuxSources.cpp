#include "AuxSources.hpp"
#include "AlgomorphLarge.hpp"
#include "plugin.hpp" // For constants


AuxInput::AuxInput(int id, rack::engine::Module* module) {
    this->id = id;
    this->module = module;
    defVoltage[AuxInputModes::MOD_ATTEN] = 5.f;
    defVoltage[AuxInputModes::SUM_ATTEN] = 5.f;
    defVoltage[AuxInputModes::MORPH_ATTEN] = 5.f;
    defVoltage[AuxInputModes::DOUBLE_MORPH_ATTEN] = FIVE_D_TWO;
    defVoltage[AuxInputModes::TRIPLE_MORPH_ATTEN] = FIVE_D_THREE;
    resetVoltages();
}

void AuxInput::resetVoltages() {
    for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
        for (int j = 0; j < 16; j++) {
            voltage[i][j] = defVoltage[i];
        }
    }
}

void AuxInput::setMode(int newMode) {
    activeModes++;

    if (activeModes > 1 && !allowMultipleModes) {
        reinterpret_cast<AlgomorphLarge*>(module)->unsetAuxMode(id, lastSetMode);
    }

    modeIsActive[newMode] = true;
    lastSetMode = newMode;
    reinterpret_cast<AlgomorphLarge*>(module)->auxModeFlags[newMode] = true;

    updateLabel();

    reinterpret_cast<AlgomorphLarge*>(module)->auxPanelDirty = true;
}

void AuxInput::unsetAuxMode(int oldMode) {
    if (modeIsActive[oldMode]) {
        activeModes--;
        
        modeIsActive[oldMode] = false;

        updateLabel();
    }

    reinterpret_cast<AlgomorphLarge*>(module)->auxPanelDirty = true;
}

void AuxInput::clearAuxModes() {
    for (int mode = 0; mode < AuxInputModes::NUM_MODES; mode++)
        reinterpret_cast<AlgomorphLarge*>(module)->unsetAuxMode(id, mode);

    activeModes = 0;

    reinterpret_cast<AlgomorphLarge*>(module)->auxPanelDirty = true;
}

void AuxInput::updateVoltage() {
    for (int mode = 0; mode < AuxInputModes::NUM_MODES; mode++) {
        if (modeIsActive[mode]) {
            for (int c = 0; c < channels; c++)
                voltage[mode][c] = module->inputs[AlgomorphLarge::AUX_INPUTS + id].getPolyVoltage(c);
        }
    }
}

void AuxInput::updateLabel() {
    int displayCode;

    if (activeModes == 1)
        displayCode = lastSetMode;
    else if (activeModes == 0)
        displayCode = -1;
    else if (activeModes > 1) {
        displayCode = -2;
    }
    else {
        //Error
        displayCode = -3;
    }

    //Update label
    if (displayCode > -1) {
        shortLabel = AuxInputModeShortLabels[displayCode];
        label = AuxInputModeLabels[displayCode];
        description = AuxInputModeDescriptions[displayCode];
    }
    else if (displayCode == -2) {
        shortLabel = "MULTI";
        label = "Multimode Input";
        std::string description = "Multimode: ";
        int count = 0;
        for (int i = 0; i < AuxInputModes::NUM_MODES; i++) {
            if (modeIsActive[i]) {
                count++;
                description += AuxInputModeLabels[i];
            }
            if (count < activeModes)
                description += ", ";
            else
                break;
        }
    }
    else if (displayCode == -1)
    {
        shortLabel = "NONE";
        label = "Unassigned";
        description = "No mode is assigned";
    }
    else
    {
        shortLabel = "ERROR";
        label = "Error";
        description = "I am error";
    }
}

std::string AuxInputInfo::getName() {
    return this->input->label;
}

std::string AuxInputInfo::getDescription() {
    return this->input->description;
}

AuxInputInfo* configAuxInput(int portId, AuxInput* input, rack::engine::Module* module) {
	//From rack::engine::Module::configInput()
	assert(portId < (int) module->inputs.size() && portId < (int) module->inputInfos.size());
	if (module->inputInfos[portId])
		delete module->inputInfos[portId];

	AuxInputInfo* info = new AuxInputInfo();
	info->module = module;
	info->type = rack::engine::Port::INPUT;
	info->portId = portId;
	info->input = input;
	info->name = "";
	module->inputInfos[portId] = info;
	return info;
}; 
