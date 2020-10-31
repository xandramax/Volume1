#pragma once
#include <rack.hpp>
#include <bitset>
using namespace rack;
extern Plugin* pluginInstance;
extern Model* modelAlgomorph4;

struct bitsetCompare {
    bool operator() (const std::bitset<16> &b1, const std::bitset<16> &b2) const {
        return b1.to_ulong() < b2.to_ulong();
	}
};

static const NVGcolor DLXDarkPurple = nvgRGB(0x14, 0x0d, 0x13);
static const NVGcolor DLXLightPurple = nvgRGB(139, 112, 162);
static const NVGcolor DLXRed = nvgRGB(0xae, 0x34, 0x58);
static const NVGcolor DLXYellow = nvgRGB(0xa9, 0xa9, 0x83);


/// Constants

static constexpr float BLINK_INTERVAL = 0.42857142857f;
static constexpr float DEF_CLICK_FILTER_SLEW = 3750.f;
static constexpr float FIVE_D_TWO = 5.f / 2.f;
static constexpr float FIVE_D_THREE = 5.f / 3.f;
static constexpr float CLOCK_IGNORE_DURATION = 0.001f;     // disable clock on powerup and reset for 1 ms (so that the first step plays)
static constexpr float DEF_RED_BRIGHTNESS = 0.4975f;
static constexpr float INDICATOR_BRIGHTNESS = 0.325f;

/// Auxiliary input and knob

struct AuxSourceModes {
	static const int MORPH = 0;
	static const int MORPH_ATTEN = 1;
	static const int CLICK_FILTER = 2;
	static const int DOUBLE_MORPH = 3;
	static const int TRIPLE_MORPH = 4;
	static const int NUM_MODES = 5;
};

struct AuxInputModes : AuxSourceModes {
	static const int SUM_ATTEN = 		AuxSourceModes::NUM_MODES;
	static const int MOD_ATTEN = 		AuxSourceModes::NUM_MODES + 1;
	static const int CLOCK = 			AuxSourceModes::NUM_MODES + 2;
	static const int REVERSE_CLOCK = 	AuxSourceModes::NUM_MODES + 3;
	static const int RESET = 			AuxSourceModes::NUM_MODES + 4;
	static const int RUN = 				AuxSourceModes::NUM_MODES + 5;
	static const int SCENE_OFFSET = 	AuxSourceModes::NUM_MODES + 6;
	static const int WILDCARD_MOD = 	AuxSourceModes::NUM_MODES + 7;
	static const int WILDCARD_SUM = 	AuxSourceModes::NUM_MODES + 8;
	static const int SHADOW = 			AuxSourceModes::NUM_MODES + 9;
	// 4 shadow modes
	static const int NUM_MODES = AuxSourceModes::NUM_MODES + 13;
};

//Order must match above
static const std::string AuxInputModeLabels[AuxInputModes::NUM_MODES] = {	"Morph CV",
																			"Morph CV Attenuverter",
																			"Click Filter Strength",
																			"Double Morph CV",
																			"Triple Morph CV",
																			"Sum Output Attenuverter",
																			"Mod Output Attenuverter",
																			"Clock",
																			"Reverse Clock",
																			"Reset",
																			"Run",
																			"Algorithm Offset",
																			"Wildcard Modulator",
																			"Carrier",
																			"Shadow -> 1",
																			"Shadow -> 2",
																			"Shadow -> 3",
																			"Shadow -> 4"};

struct AuxKnobModes : AuxSourceModes {
	static const int SUM_GAIN = 		AuxSourceModes::NUM_MODES;
	static const int MOD_GAIN = 		AuxSourceModes::NUM_MODES + 1;
	static const int OP_GAIN = 			AuxSourceModes::NUM_MODES + 2;
	static const int UNI_MORPH = 		AuxSourceModes::NUM_MODES + 3;
	static const int ENDLESS_MORPH = 	AuxSourceModes::NUM_MODES + 4;
	static const int NUM_MODES = 		AuxSourceModes::NUM_MODES + 5;
};

//Order must match above
static const std::string AuxKnobModeLabels[AuxKnobModes::NUM_MODES] = {		"Morph",
																			"Morph CV Attenuverter",
																			"Click Filter Strength",
																			"Double Morph",
																			"Triple Morph",
																			"Sum Output Gain",
																			"Mod Output Gain",
																			"Op Input Gain",
																			"Unipolar Morph Plus",
																			"Endless Morph"};

/// Undo/Redo History

template < typename MODULE >
struct AlgorithmDiagonalChangeAction : history::ModuleAction {
    int scene, op, mod;
	std::bitset<16> displayAlgoName;

	AlgorithmDiagonalChangeAction() {
		name = "Delexander Algomorph diagonal connection";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->opDestinations[scene][op][mod] ^= true;
        m->algoName[scene].flip(op * 3 + mod);
		if (m->modeB || !m->horizontalDestinations[scene][op])
        	m->displayAlgoName[scene].flip(op * 3 + mod);
		if (!m->modeB) {
			// If the mod output in question corresponds to a disabled operator
			if (!m->horizontalDestinations[scene][op] && m->horizontalDestinations[scene][m->threeToFour[op][mod]]) {
				// If a connection has been made, enable that operator visually
				if (m->opDestinations[scene][op][mod]) {
					m->displayAlgoName[scene].set(12 + m->threeToFour[op][mod], false);
				}
				// If a connection has been broken, 
				else {
					if (m->horizontalDestinations[scene][m->threeToFour[op][mod]]) {
						bool disabled = true;
						for (int j = 0; j < 4; j++) {
							if (j != op && j != m->threeToFour[op][mod] && m->opDestinations[scene][j][m->fourToThree[j][op]])
								disabled = false;
						}
						if (disabled)
							m->displayAlgoName[scene].set(12 + m->threeToFour[op][mod], true);
						else
							m->displayAlgoName[scene].set(12 + m->threeToFour[op][mod], false);
					}
				}
			}
		}
		m->graphDirty = true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->opDestinations[scene][op][mod] ^= true;
        m->algoName[scene].flip(op * 3 + mod);
		if (m->modeB || !m->horizontalDestinations[scene][op])
        	m->displayAlgoName[scene].flip(op * 3 + mod);
		if (!m->modeB) {
			// If the mod output in question corresponds to a disabled operator
			if (!m->horizontalDestinations[scene][op] && m->horizontalDestinations[scene][m->threeToFour[op][mod]]) {
				// If a connection has been made, enable that operator visually
				if (m->opDestinations[scene][op][mod]) {
					m->displayAlgoName[scene].set(12 + m->threeToFour[op][mod], false);
				}
				// If a connection has been broken, 
				else {
					if (m->horizontalDestinations[scene][m->threeToFour[op][mod]]) {
						bool disabled = true;
						for (int j = 0; j < 4; j++) {
							if (j != op && j != m->threeToFour[op][mod] && m->opDestinations[scene][j][m->fourToThree[j][op]])
								disabled = false;
						}
						if (disabled)
							m->displayAlgoName[scene].set(12 + m->threeToFour[op][mod], true);
						else
							m->displayAlgoName[scene].set(12 + m->threeToFour[op][mod], false);
					}	
				}
			}
		}
		m->graphDirty = true;
	}
};

template < typename MODULE >
struct AlgorithmHorizontalChangeAction : history::ModuleAction {
    int scene, op;

	AlgorithmHorizontalChangeAction() {
		name = "Delexander Algomorph horizontal connection";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->horizontalDestinations[scene][op] ^= true;
		if (!m->modeB) {
			m->algoName[scene].flip(12 + op);
			m->displayAlgoName[scene].set(12 + op, m->algoName[scene][12 + op]);
			for (int j = 0; j < 4; j++) {
				if (m->horizontalDestinations[scene][j]) {
					bool fullDisable = true;
					for (int i = 0; i < 3; i++)     // Set all of the disabled operators' destinations fo false
						m->displayAlgoName[scene].set(j * 3 + i, false);
					for (int i = 0; i < 4; i++) {     // Check if any other operators are modulating this operator
						if (i != j && !m->horizontalDestinations[scene][i] && m->opDestinations[scene][i][m->fourToThree[i][j]])
							fullDisable = false;
					}
					// If anything is modulating the operator, set it enabled in the display. Otherwise, disable it in the display.
					if (fullDisable)
						m->displayAlgoName[scene].set(12 + j, true);
					else
						m->displayAlgoName[scene].set(12 + j, false);
				}
				else {
					if (j == op) {
						// Reenable destinations in the display and handle the consequences
						for (int i = 0; i < 3; i++) {
							if (m->opDestinations[scene][op][i]) {
								m->displayAlgoName[scene].set(op * 3 + i, true);
								// the consequences
								if (m->horizontalDestinations[scene][m->threeToFour[op][i]])
									m->displayAlgoName[scene].set(12 + m->threeToFour[op][i], false);
							}
						}  
					}
				}
			}
		}
		m->graphDirty = true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->horizontalDestinations[scene][op] ^= true;
		if (!m->modeB) {
			m->algoName[scene].flip(12 + op);
			m->displayAlgoName[scene].set(12 + op, m->algoName[scene][12 + op]);
			for (int j = 0; j < 4; j++) {
				if (m->horizontalDestinations[scene][j]) {
					bool fullDisable = true;
					for (int i = 0; i < 3; i++)     // Set all of the disabled operators' destinations fo false
						m->displayAlgoName[scene].set(j * 3 + i, false);
					for (int i = 0; i < 4; i++) {     // Check if any other operators are modulating this operator
						if (i != j && !m->horizontalDestinations[scene][i] && m->opDestinations[scene][i][m->fourToThree[i][j]])
							fullDisable = false;
					}
					// If anything is modulating the operator, set it enabled in the display. Otherwise, disable it in the display.
					if (fullDisable)
						m->displayAlgoName[scene].set(12 + j, true);
					else
						m->displayAlgoName[scene].set(12 + j, false);
				}
				else {
					if (j == op) {
						// Reenable destinations in the display and handle the consequences
						for (int i = 0; i < 3; i++) {
							if (m->opDestinations[scene][op][i]) {
								m->displayAlgoName[scene].set(op * 3 + i, true);
								// the consequences
								if (m->horizontalDestinations[scene][m->threeToFour[op][i]])
									m->displayAlgoName[scene].set(12 + m->threeToFour[op][i], false);
							}
						}  
					}
				}
			}
		}
		m->graphDirty = true;
	}
};

template < typename MODULE, typename MODULEWIDGET >
struct KnobModeAction : history::ModuleAction {
    int oldKnobMode, newKnobMode;

	KnobModeAction() {
		name = "Delexander Algomorph knob mode";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		dynamic_cast<MODULEWIDGET*>(mw)->setKnobMode(oldKnobMode);
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		dynamic_cast<MODULEWIDGET*>(mw)->setKnobMode(newKnobMode);
	}
};

template < typename MODULE >
struct AlgorithmForcedCarrierChangeAction : history::ModuleAction {
    int scene, op;

	AlgorithmForcedCarrierChangeAction() {
		name = "Delexander Algomorph forced carrier";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->forcedCarrier[scene][op] ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->forcedCarrier[scene][op] ^= true;
	}
};

template < typename MODULE >
struct AlgorithmSceneChangeAction : history::ModuleAction {
    int oldScene, newScene;

	AlgorithmSceneChangeAction() {
		name = "Delexander Algomorph base scene";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->baseScene = oldScene;
		m->graphDirty = true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->baseScene = newScene;
		m->graphDirty = true;
	}
};

template < typename MODULE >
struct AuxInputSetAndRememberAction : history::ModuleAction {
    int mode, channels;
	float voltage[16], scaledVoltage[16];

	AuxInputSetAndRememberAction() {
		name = "Delexander Algomorph AUX In mode set & remember";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.unsetMode(mode);
		for (int c = 0; c < channels; c++) {
			m->auxInput.voltage[mode][c] = voltage[c];
			m->scaledAuxVoltage[mode][c] = scaledVoltage[c];
		}
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.setMode(mode);
	}
};

template < typename MODULE >
struct AuxInputSetAndForgetAction : history::ModuleAction {
    int mode, channels;

	AuxInputSetAndForgetAction() {
		name = "Delexander Algomorph AUX In mode set & forget";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.unsetMode(mode);
		for (int c = 0; c < channels; c++)
			m->auxInput.voltage[mode][c] = m->auxInput.defVoltage[mode];
		m->rescaleVoltage(mode);
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.setMode(mode);
	}
};

template < typename MODULE >
struct AuxInputSwitchAndRememberAction : history::ModuleAction {
    int oldMode, newMode, channels;
	float oldVoltage[16], newVoltage[16], oldScaledVoltage[16], newScaledVoltage[16];

	AuxInputSwitchAndRememberAction() {
		name = "Delexander Algomorph AUX In mode switch & remember";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.unsetMode(newMode);
		for (int c = 0; c < 16; c++) {
			m->auxInput.voltage[newMode][c] = newVoltage[c];
			m->scaledAuxVoltage[newMode][c] = newScaledVoltage[c];
		}
		m->auxInput.setMode(oldMode);
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.unsetMode(oldMode);
		for (int c = 0; c < 16; c++) {
			m->auxInput.voltage[oldMode][c] = oldVoltage[c];
			m->scaledAuxVoltage[oldMode][c] = oldScaledVoltage[c];
		}
		m->auxInput.setMode(newMode);
	}
};

template < typename MODULE >
struct AuxInputSwitchAndForgetAction : history::ModuleAction {
    bool forgetOldVoltage, forgetNewVoltage;
	int oldMode, newMode, channels;
	float oldVoltage[16], oldScaledVoltage[16], newVoltage[16], newScaledVoltage[16];

	AuxInputSwitchAndForgetAction() {
		name = "Delexander Algomorph AUX In mode switch & forget";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.unsetMode(newMode);
		if (forgetNewVoltage) {
			for (int c = 0; c < channels; c++)
				m->auxInput.voltage[newMode][c] = m->auxInput.defVoltage[newMode];
			m->rescaleVoltage(newMode);
		}
		else {
			for (int c = 0; c < channels; c++) {
				m->auxInput.voltage[newMode][c] = newVoltage[c];
				m->scaledAuxVoltage[newMode][c] = newScaledVoltage[c];
			}
		}
		m->auxInput.setMode(oldMode);
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.unsetMode(oldMode);
		if (forgetOldVoltage) {
			for (int c = 0; c < channels; c++)
				m->auxInput.voltage[oldMode][c] = m->auxInput.defVoltage[oldMode];
			m->rescaleVoltage(oldMode);
		}
		else {
			for (int c = 0; c < channels; c++) {
				m->auxInput.voltage[oldMode][c] = oldVoltage[c];
				m->scaledAuxVoltage[oldMode][c] = oldScaledVoltage[c];
			}
		}
		m->auxInput.setMode(newMode);
	}
};

template < typename MODULE >
struct AuxInputUnsetAndRememberAction : history::ModuleAction {
    int mode, channels;
	float voltage[16], scaledVoltage[16];

	AuxInputUnsetAndRememberAction() {
		name = "Delexander Algomorph AUX In mode unset & remember";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.setMode(mode);
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.unsetMode(mode);
		for (int c = 0; c < channels; c++) {
			m->auxInput.voltage[mode][c] = voltage[c];
			m->scaledAuxVoltage[mode][c] = scaledVoltage[c];
		}
	}
};

template < typename MODULE >
struct AuxInputUnsetAndForgetAction : history::ModuleAction {
    int mode, channels;

	AuxInputUnsetAndForgetAction() {
		name = "Delexander Algomorph AUX In mode unset & forget";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.setMode(mode);
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.unsetMode(mode);
		for (int c = 0; c < channels; c++)
			m->auxInput.voltage[mode][c] = m->auxInput.defVoltage[mode];
		m->rescaleVoltage(mode);
	}
};

template < typename MODULE >
struct RememberAuxVoltageAction : history::ModuleAction {
	RememberAuxVoltageAction() {
		name = "Delexander Algomorph remember option voltage";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.forgetVoltage = true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.forgetVoltage = false;
	}
};

template < typename MODULE >
struct AllowMultipleModesAction : history::ModuleAction {
	AllowMultipleModesAction() {
		name = "Delexander Algomorph allow multiple modes";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.allowMultipleModes = false;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->auxInput.allowMultipleModes = true;
	}
};

template < typename MODULE >
struct ResetSceneAction : history::ModuleAction {
	int oldResetScene, newResetScene;

	ResetSceneAction() {
		name = "Delexander Algomorph change reset scene";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->resetScene = oldResetScene;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->resetScene = newResetScene;
	}
};

template < typename MODULE >
struct ToggleModeBAction : history::ModuleAction {
	std::bitset<16> algoName[3];
	std::bitset<16> displayAlgoName[3];

	ToggleModeBAction() {
		name = "Delexander Algomorph toggle mode B";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->modeB ^= true;
		for (int i = 0; i < 3; i++) {
			m->algoName[i] = algoName[i];
			m->displayAlgoName[i] = displayAlgoName[i];
		}
		m->graphDirty = true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->modeB ^= true;
		if (m->modeB) {
			for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 4; j++)
                    m->algoName[i].set(12 + j, false);
				m->displayAlgoName[i] = m->algoName[i];
            }
		}
		else {
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 4; j++) {
					if (m->horizontalDestinations[i][j]) {
						m->algoName[i].set(12 + j, true);
						m->displayAlgoName[i] = m->algoName[i];
						for (int k = 0; k < 3; k++)
							m->displayAlgoName[i].set(j * 3 + k, false);
					}
				}
			}
		}
		m->graphDirty = true;
	}
};

template < typename MODULE >
struct ToggleRingMorphAction : history::ModuleAction {
	ToggleRingMorphAction() {
		name = "Delexander Algomorph toggle ring morph";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->ringMorph ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->ringMorph ^= true;
	}
};

template < typename MODULE >
struct ToggleRandomizeRingMorphAction : history::ModuleAction {
	ToggleRandomizeRingMorphAction() {
		name = "Delexander Algomorph toggle randomize ring morph";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->randomRingMorph ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->randomRingMorph ^= true;
	}
};

template < typename MODULE >
struct ToggleExitConfigOnConnectAction : history::ModuleAction {
	ToggleExitConfigOnConnectAction() {
		name = "Delexander Algomorph toggle exit config on connect";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->exitConfigOnConnect ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->exitConfigOnConnect ^= true;
	}
};

template < typename MODULE >
struct ToggleCCWSceneSelectionAction : history::ModuleAction {
	ToggleCCWSceneSelectionAction() {
		name = "Delexander Algomorph toggle CCW scene selection";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->ccwSceneSelection ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->ccwSceneSelection ^= true;
	}
};

template < typename MODULE >
struct ToggleClickFilterAction : history::ModuleAction {
	ToggleClickFilterAction() {
		name = "Delexander Algomorph toggle click filter";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->clickFilterEnabled ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->clickFilterEnabled ^= true;
	}
};

template < typename MODULE >
struct ToggleGlowingInkAction : history::ModuleAction {
	ToggleGlowingInkAction() {
		name = "Delexander Algomorph toggle glowing ink";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->glowingInk ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->glowingInk ^= true;
	}
};

template < typename MODULE >
struct ToggleVULightsAction : history::ModuleAction {
	ToggleVULightsAction() {
		name = "Delexander Algomorph toggle VU lights";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->vuLights ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->vuLights ^= true;
	}
};

template < typename MODULE >
struct ToggleResetOnRunAction : history::ModuleAction {
	ToggleResetOnRunAction() {
		name = "Delexander Algomorph toggle reset on run";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->resetOnRun ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->resetOnRun ^= true;
	}
};

template < typename MODULE >
struct ToggleRunSilencerAction : history::ModuleAction {
	ToggleRunSilencerAction() {
		name = "Delexander Algomorph toggle run silencer";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->runSilencer ^= true;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->runSilencer ^= true;
	}
};

template < typename MODULE >
struct SetClickFilterAction : history::ModuleAction {
	float oldSlew, newSlew;

	SetClickFilterAction() {
		name = "Delexander Algomorph set click filter slew";
	}

	void undo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->clickFilterSlew = oldSlew;
	}

	void redo() override {
		app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
		assert(mw);
		MODULE* m = dynamic_cast<MODULE*>(mw->module);
		assert(m);
		m->clickFilterSlew = newSlew;
	}
};


/// Params

struct DLXPortPoly : SvgPort {
	DLXPortPoly() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXPort16.svg")));
		// shadow->opacity = 0.f;
	}
};

struct DLXPortPolyOut : SvgPort {
	DLXPortPolyOut() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXPort16b.svg")));
		// shadow->opacity = 0.f;
	}
};

struct DLXPortG : SvgPort {
	DLXPortG() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXPortG.svg")));
		// shadow->opacity = 0.f;
	}
};

struct DLXPurpleButton : rack::app::SvgSwitch {
	int state = 0;

	DLXPurpleButton() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_Button_0c.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_Button_1c.svg")));
	}
};

struct DLXTL1105B : rack::app::SvgSwitch {
	int state = 0;

	DLXTL1105B() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_TL1105B_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_TL1105B_1.svg")));
	}
};


/// Lights

template <typename TBase = GrayModuleLightWidget>
struct TDlxPurpleLight : TBase {
	TDlxPurpleLight() {
		this->addBaseColor(DLXLightPurple);
	}
};
typedef TDlxPurpleLight<> DLXPurpleLight;

template <typename TBase = GrayModuleLightWidget>
struct TDlxRedLight : TBase {
	TDlxRedLight() {
		this->addBaseColor(DLXRed);
	}
};
typedef TDlxRedLight<> DLXRedLight;

template <typename TBase = GrayModuleLightWidget>
struct TDlxYellowLight : TBase {
	TDlxYellowLight() {
		this->addBaseColor(DLXYellow);
	}
};
typedef TDlxYellowLight<> DLXYellowLight;

template <typename TBase = GrayModuleLightWidget>
struct TDlxMultiLight : TBase {
	TDlxMultiLight() {
		this->addBaseColor(DLXLightPurple);
		this->addBaseColor(DLXYellow);
		this->addBaseColor(DLXRed);
	}
};
typedef TDlxMultiLight<> DLXMultiLight;

template <typename TBase = GrayModuleLightWidget>
struct TDlxScreenMultiLight : TBase {
	TDlxScreenMultiLight() {
		this->addBaseColor(DLXDarkPurple);
		this->addBaseColor(DLXYellow);
		this->addBaseColor(DLXRed);
	}
};
typedef TDlxMultiLight<> DLXScreenMultiLight;

struct Line {
	Vec left, right;
	Vec pos, size;
	bool flipped;
	Line(Vec a, Vec b) {
		flipped = a.y > b.y;
		size = Vec(b.x - a.x, std::fabs(b.y - a.y));
		float startAngle, endAngle;
		if (flipped) {
			startAngle = std::atan2(-size.y, size.x);
			endAngle = std::atan2(size.x, -size.y);
		}
		else {
			startAngle = std::atan2(size.x, size.y);
			endAngle = std::atan2(size.y, size.x);
		}
		float radius = 8.462 + 1.7;		//ringLight radius + strokeWidth
		if (flipped) {
			left = Vec(radius * std::cos(startAngle) + a.x, radius * std::sin(startAngle) + a.y);
			right = Vec(b.x - radius * std::cos(startAngle),  b.y - radius * std::sin(startAngle));
		}
		else {
			left = Vec(radius * std::cos(endAngle) + a.x, radius * std::sin(endAngle) + a.y);
			right = Vec(b.x - radius * std::cos(endAngle),  b.y - radius * std::sin(endAngle));
		}
	}
};

struct ConnectionBgWidget : TransparentWidget {
	std::vector<Line> lines;
	ConnectionBgWidget(std::vector<Vec> left, std::vector<Vec> right) {
		for (unsigned i = 0; i < left.size(); i++) {
			for (unsigned j = 0; j < right.size(); j++) {
				lines.push_back(Line(left[i], right[j]));
			}
		}
	}

	void draw(const widget::Widget::DrawArgs& args) override {
        //Colors from GrayModuleLightWidget
		for (Line line : lines) {
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, line.left.x, line.left.y);
			nvgLineTo(args.vg, line.right.x, line.right.y);
			nvgStrokeWidth(args.vg, 1.1f);
			// Background
			nvgStrokeColor(args.vg, nvgRGB(0x5a, 0x5a, 0x5a));
			nvgStroke(args.vg);
			// Border
			nvgStrokeWidth(args.vg, 0.6);
			nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 0x60));
			nvgStroke(args.vg);
        }
    }
};

template < typename TBase = GrayModuleLightWidget >
struct TBacklight : TBase {
    void drawLight(const widget::Widget::DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, this->box.size.x, this->box.size.y, 3.675f);
		if (this->color.a > 0.0) {
			nvgFillColor(args.vg, this->color);
			nvgFill(args.vg);
		}
	}
	void drawHalo(const widget::Widget::DrawArgs& args) override {
		// float owidth = 1.2275f * this->box.size.x;
		// float oheight = 1.2275f * this->box.size.y;
		float xradius = 56.537f, yradius = 46.638f;
		float oxradius = xradius * 3.65f, oyradius = yradius * 3.65f;

		nvgBeginPath(args.vg);
		nvgRect(args.vg, xradius - oxradius, yradius - oyradius, 2.f * oxradius, 2.f * oyradius);
		
		NVGpaint p;
		float rx = (xradius + oxradius)*0.5f;
		float ry = (yradius + oyradius)*0.5f;
		float fx = (oxradius - xradius);
		NVG_NOTUSED(args.vg);
		memset(&p, 0, sizeof(p));
		nvgTransformIdentity(p.xform);
		p.xform[4] = xradius;
		p.xform[5] = yradius;
		p.extent[0] = rx;
		p.extent[1] = ry;
		p.radius = rx;
		p.feather = std::max(1.0f, fx);
		p.innerColor = color::mult(this->color, 0.07);
		p.outerColor = nvgRGB(0, 0, 0);
		
		nvgFillPaint(args.vg, p);
		nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
		nvgFill(args.vg);
	}
};
typedef TBacklight<> Backlight;

template <typename TBase = DLXScreenMultiLight>
TBacklight<TBase>* createBacklight(Vec pos, Vec size, engine::Module* module, int firstLightId) {
	TBacklight<TBase>* o = new TBacklight<TBase>();
	o->box.pos = pos;
	o->box.size = size;
	o->module = module;
	o->firstLightId = firstLightId;
	return o;
}

template <typename TBase = GrayModuleLightWidget>
struct TLineLight : TBase {
	bool flipped = false;
	TLineLight(Vec a, Vec b) {
		flipped = a.y > b.y;
		/* if (!flipped)
			a.y *= -1; */
		this->box.size = Vec(b.x - a.x, std::fabs(b.y - a.y));
		float angle = std::atan2(this->box.size.y, this->box.size.x);
		float radius = 8.462 + 1.7;		//ringLight radius + strokeWidth
		Vec start, end;
		if (flipped) {
			start = Vec(a.x + radius * std::cos(angle), a.y - radius * std::sin(angle));
			end = Vec(b.x - radius * std::cos(angle),  b.y + radius * std::sin(angle));
		}
		else {
			start = Vec(a.x + radius * std::cos(angle), a.y + radius * std::sin(angle));
			end = Vec(b.x - radius * std::cos(angle),  b.y - radius * std::sin(angle));
		}
		this->box.pos = start;
		if (flipped)
			this->box.pos.y = end.y;
		this->box.size = Vec(end.x - start.x, std::fabs(end.y - start.y));
		// this->box.pos.y *= -1;
	}
	void drawLight(const widget::Widget::DrawArgs& args) override {
		nvgBeginPath(args.vg);
		if (flipped) {
			nvgMoveTo(args.vg, 0.f, this->box.size.y);
			nvgLineTo(args.vg, this->box.size.x, 0.f);
		}
		else {
			nvgMoveTo(args.vg, 0.f, 0.f);
			nvgLineTo(args.vg, this->box.size.x, this->box.size.y);
		}
		nvgStrokeWidth(args.vg, .975f);
		// Foreground
		if (this->color.a > 0.0) {
			nvgStrokeColor(args.vg, this->color);
			nvgStroke(args.vg);
		}
	}
	void drawHalo(const widget::Widget::DrawArgs& args) override {
		/* float radius = std::min(this->box.size.x, this->box.size.y) / 2.0;
		float oradius = 3.0 * radius;

		nvgBeginPath(args.vg);
		if (flipped)
			nvgRect(args.vg, radius - oradius, radius - oradius - this->box.size.y, 2 * oradius, 2 * oradius);
		else
			nvgRect(args.vg, radius - oradius, radius - oradius, 2 * oradius, 2 * oradius);

		NVGpaint paint;
		NVGcolor icol = color::mult(this->color, 0.07);
		NVGcolor ocol = nvgRGB(0, 0, 0);
		if (flipped)
			paint = nvgRadialGradient(args.vg, radius, radius - this->box.size.y, radius, oradius, icol, ocol);
		else
			paint = nvgRadialGradient(args.vg, radius, radius, radius, oradius, icol, ocol);
		nvgFillPaint(args.vg, paint);
		nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
		nvgFill(args.vg); */
	}
};
typedef TLineLight<> LineLight;

template <typename TBase = DLXPurpleLight>
TLineLight<TBase>* createLineLight(Vec a, Vec b, engine::Module* module, int firstLightId) {
	TLineLight<TBase>* o = new TLineLight<TBase>(a, b);
	o->module = module;
	o->firstLightId = firstLightId;
	return o;
}

template <typename TBase = GrayModuleLightWidget>
struct TRingLight : TBase {
	float radius = 1.f;
	float strokeSize = 0.f;
	TRingLight(float r, float s = 0) {
		radius = r;
		strokeSize = s;
	}
	void drawLight(const widget::Widget::DrawArgs& args) override {
			// Adapted from LightWidget::drawLight, with no fill
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, radius, radius, radius);
			nvgStrokeWidth(args.vg, 1.2 + strokeSize);

			// Background
			if (this->bgColor.a > 0.0) {
				nvgStrokeColor(args.vg, this->bgColor);
				nvgStroke(args.vg);
				// nvgFillColor(args.vg, this->color);
				// nvgFill(args.vg);
			}

			// Foreground
			if (this->color.a > 0.0) {
				nvgStrokeWidth(args.vg, 1.3 + strokeSize);
				nvgStrokeColor(args.vg, this->color);
				nvgStroke(args.vg);
			}

			// Border
			if (this->borderColor.a > 0.0) {
				nvgStrokeWidth(args.vg, 0.5);
				nvgStrokeColor(args.vg, this->borderColor);
				nvgStroke(args.vg);
			}
	}
};
typedef TRingLight<> RingLight;

template <typename TBase = DLXPurpleLight>
TRingLight<TBase>* createRingLight(Vec pos, float r, engine::Module* module, int firstLightId, float s = 0) {
	TRingLight<TBase>* o;
	if (s == 0)
		o = new TRingLight<TBase>(r);
	else
		o = new TRingLight<TBase>(r, s);
	o->box.pos = pos;
	o->module = module;
	o->firstLightId = firstLightId;
	return o;
}

template <typename TBase = DLXPurpleLight>
TRingLight<TBase>* createRingLightCentered(Vec pos, float r, engine::Module* module, int firstLightId, float s = 0) {
	TRingLight<TBase>* o = createRingLight<TBase>(pos, r, module, firstLightId, s);
	o->box.pos.x -= r;
	o->box.pos.y -= r;
	return o;
}

struct DLXRingIndicator : DLXMultiLight {
	float angle = 0.f;
	float transform[6];
	float radius = 1.f;
	float strokeSize = 0.f;
	
	DLXRingIndicator(float r, float s = 1.3) {
		radius = r;
		strokeSize = s;
	}

	void draw(const DrawArgs& args) override {
		if (angle >= 2.f * M_PI)
			angle = 0.f;
		else
			angle += 0.1f;
		math::Vec center = Vec(radius, radius);
		nvgTransformIdentity(transform);
		float t[6];
		nvgTransformTranslate(t, center.x, center.y);
		nvgTransformPremultiply(transform, t);
		nvgTransformRotate(t, angle);
		nvgTransformPremultiply(transform, t);
		nvgTransformTranslate(t, center.neg().x, center.neg().y);
		nvgTransformPremultiply(transform, t);
		nvgTransform(args.vg, transform[0], transform[1], transform[2], transform[3], transform[4], transform[5]);

		nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, 2.6f, 2.6f, strokeSize);
		nvgFillColor(args.vg, this->color);
		nvgFill(args.vg);

		Widget::draw(args);
	}
};

struct SvgLight : ModuleLightWidget {
	widget::SvgWidget* sw;

	SvgLight() {
		sw = new widget::SvgWidget;
		this->addChild(sw);
	};

	void draw(const DrawArgs& args) override {
		sw->draw(args);
		Widget::draw(args);
	}
};

template <class TSvgLight>
TSvgLight* createSvgLight(Vec pos, engine::Module* module, int firstLightId) {
	TSvgLight* o = new TSvgLight;
	o->box.pos = pos;
	o->module = module;
	if (module)
		o->firstLightId = firstLightId;
	return o;
}

struct SvgSwitchLight : SvgLight {
	std::vector<std::shared_ptr<Svg>> frames;
	engine::ParamQuantity* paramQuantity = NULL;
	float dirtyValue = NAN;
	/** Return to original position when released */
	bool momentary = false;
	/** Hysteresis state for momentary switch */
	bool momentaryPressed = false;
	bool momentaryReleased = false;

	/** Adds an SVG file to represent the next switch position */
	void addFrame(std::shared_ptr<Svg> svg) {
		frames.push_back(svg);
		// If this is our first frame, automatically set SVG and size
		if (!sw->svg) {
			sw->setSvg(svg);
			box.size = sw->box.size;
		}
	};
	void onChange(const event::Change& e) override {
		if (!frames.empty() && paramQuantity) {
			int index = (int) std::round(paramQuantity->getValue() - paramQuantity->getMinValue());
			index = math::clamp(index, 0, (int) frames.size() - 1);
			sw->setSvg(frames[index]);
		}
	};

	void step() override {
		if (momentaryPressed) {
			momentaryPressed = false;
			// Wait another frame.
		}
		else if (momentaryReleased) {
			momentaryReleased = false;
			if (paramQuantity) {
				// Set to minimum value
				paramQuantity->setMin();
			}
		}
		if (this->paramQuantity) {
			float value = paramQuantity->getValue();
			// Trigger change event when paramQuantity value changes
			if (value != dirtyValue) {
				dirtyValue = value;
				event::Change eChange;
				onChange(eChange);
			}
		}
		Widget::step();
	}

	void onDoubleClick(const event::DoubleClick& e) override {
		// Don't reset parameter on double-click
	}

	void onDragStart(const event::DragStart& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;

		if (momentary) {
			if (paramQuantity) {
				// Set to maximum value
				paramQuantity->setMax();
				momentaryPressed = true;
			}
		}
		else {
			if (paramQuantity) {
				float oldValue = paramQuantity->getValue();
				if (paramQuantity->isMax()) {
					// Reset value back to minimum
					paramQuantity->setMin();
				}
				else {
					// Increment value by 1
					paramQuantity->setValue(std::round(paramQuantity->getValue()) + 1.f);
				}

				float newValue = paramQuantity->getValue();
				if (oldValue != newValue) {
					// Push ParamChange history action
					history::ParamChange* h = new history::ParamChange;
					h->name = "move switch";
					h->moduleId = paramQuantity->module->id;
					h->paramId = paramQuantity->paramId;
					h->oldValue = oldValue;
					h->newValue = newValue;
					APP->history->push(h);
				}
			}
		}
	}

	void onDragEnd(const event::DragEnd& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;

		if (momentary) {
			momentaryReleased = true;
		}
	}

	void reset() {
		if (paramQuantity && !momentary) {
			paramQuantity->reset();
		}
	}
};

template <class TSvgSwitchLight>
TSvgSwitchLight* createSvgSwitchLight(Vec pos, engine::Module* module, int firstLightId, int paramId) {
	TSvgSwitchLight* o = new TSvgSwitchLight;
	o->box.pos = pos;
	o->module = module;
	if (module) {
		o->firstLightId = firstLightId;
		o->paramQuantity = module->paramQuantities[paramId];
	}
	return o;
}

template <class TSvgSwitchLight>
TSvgSwitchLight* createSvgSwitchLightCentered(Vec pos, engine::Module* module, int firstLightId, int paramId) {
	TSvgSwitchLight* o = createSvgSwitchLight<TSvgSwitchLight>(pos, module, firstLightId, paramId);
	o->box.pos = o->box.pos.minus(o->box.size.div(2));
	return o;
}

struct DLXPencilLight : SvgSwitchLight {
	int state = 0;

	DLXPencilLight() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_PencilLight_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_PencilLight_1.svg")));
	}
};

struct DLXScreenButtonLight : SvgSwitchLight {
	int state = 0;

	DLXScreenButtonLight() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_ScreenButton_light_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_ScreenButton_light_1.svg")));
	}
};

struct DLX1Light : SvgSwitchLight {
	int state = 0;

	DLX1Light() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_1b_light_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_1b_light_1.svg")));
	}
};

struct DLX2Light : SvgSwitchLight {
	int state = 0;

	DLX2Light() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_2b_light_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_2b_light_1.svg")));
	}
};

struct DLX3Light : SvgSwitchLight {
	int state = 0;

	DLX3Light() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_3b_light_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLX_3b_light_1.svg")));
	}
};

struct SvgKnobLight : SvgLight {
	float angle = 0;
	float transform[6];

	void draw(const DrawArgs& args) override {
		math::Vec center = Vec(sw->box.size.x / 2, sw->box.size.y / 2);
		nvgTransformIdentity(transform);
		float t[6];
		nvgTransformTranslate(t, center.x, center.y);
		nvgTransformPremultiply(transform, t);
		nvgTransformRotate(t, angle);
		nvgTransformPremultiply(transform, t);
		nvgTransformTranslate(t, center.neg().x, center.neg().y);
		nvgTransformPremultiply(transform, t);
		nvgTransform(args.vg, transform[0], transform[1], transform[2], transform[3], transform[4], transform[5]);
		sw->draw(args);
		Widget::draw(args);
	}
};

struct DLXKnobLight : SvgKnobLight {
	DLXKnobLight() {
		sw->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXKnobB_large_light.svg")));
	}
};

struct DLXSmallKnobLight : SvgKnobLight {
	DLXSmallKnobLight() {
		sw->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXKnobB_light.svg")));
	}
};

template <class TKnobLight = DLXKnobLight>
struct LightKnob : RoundKnob {
	TKnobLight* sibling;

	void setSibling(TKnobLight* kl) {
		sibling = kl;
	}

	void onChange(const event::Change& e) override {
		// Re-transform the widget::TransformWidget and pdate the sibling's transformation angle
		if (paramQuantity) {
			float angle;
			if (paramQuantity->isBounded()) {
				angle = math::rescale(paramQuantity->getScaledValue(), 0.f, 1.f, minAngle, maxAngle);
			}
			else {
				angle = math::rescale(paramQuantity->getValue(), -1.f, 1.f, minAngle, maxAngle);
			}
			angle = std::fmod(angle, 2 * M_PI);
			sibling->angle = angle;
			// Rotate SVG
			tw->identity();
			math::Vec center = sw->box.getCenter();
			tw->translate(center);
			tw->rotate(angle);
			tw->translate(center.neg());
			fb->dirty = true;
		}
		Knob::onChange(e);
	}
};

struct DLXLightKnob : LightKnob<DLXKnobLight> {
	DLXLightKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXKnobB_large.svg")));
	}
};

struct DLXSmallLightKnob : LightKnob<DLXSmallKnobLight> {
	DLXSmallLightKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXKnobB.svg")));
	}
};

template <class TKnobLight = DLXKnobLight, class TDLXLightKnob = DLXLightKnob>
TDLXLightKnob* createLightKnob(Vec pos, engine::Module* module, int paramId, TKnobLight* kl) {
	TDLXLightKnob* o = new TDLXLightKnob();
	o->box.pos = pos;
	if (module) {
		o->paramQuantity = module->paramQuantities[paramId];
	}
	o->setSibling(kl);
	return o;
}

template <class TKnobLight = DLXKnobLight, class TDLXLightKnob = DLXLightKnob>
TDLXLightKnob* createLightKnobCentered(Vec pos, engine::Module* module, int paramId, TKnobLight* kl) {
	TDLXLightKnob* o = createLightKnob<TKnobLight, TDLXLightKnob>(pos, module, paramId, kl);
	o->box.pos = o->box.pos.minus(o->box.size.div(2));
	return o;
}

///Glowing Ink

struct DLXGlowingInk : SvgLight {
	DLXGlowingInk() {
		sw->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/GlowingInk.svg")));
	}
};