#pragma once
#include <rack.hpp>
using rack::history::ModuleAction;


/// Undo/Redo History

struct AlgorithmDiagonalChangeAction : ModuleAction {
    int scene, op, mod;

	AlgorithmDiagonalChangeAction();
	void undo() override;
	void redo() override;
};

struct AlgorithmHorizontalChangeAction : ModuleAction {
    int scene, op;

	AlgorithmHorizontalChangeAction();
	void undo() override;
	void redo() override;
};

struct KnobModeAction : ModuleAction {
    int oldKnobMode, newKnobMode;

	KnobModeAction();
	void undo() override;
	void redo() override;
};

struct AlgorithmForcedCarrierChangeAction : ModuleAction {
    int scene, op;

	AlgorithmForcedCarrierChangeAction();
	void undo() override;
	void redo() override;
};

struct AlgorithmSceneChangeAction : ModuleAction {
    int oldScene, newScene;

	AlgorithmSceneChangeAction();
	void undo() override;
	void redo() override;
};

struct AuxInputSetAction : ModuleAction {
    int auxIndex, mode, channels;

	AuxInputSetAction();
	void undo() override;
	void redo() override;
};

struct AuxInputSwitchAction : ModuleAction {
	int auxIndex, oldMode, newMode, channels;

	AuxInputSwitchAction();
	void undo() override;
	void redo() override;
};

struct AuxInputUnsetAction : ModuleAction {
    int auxIndex, mode, channels;

	AuxInputUnsetAction();
	void undo() override;
	void redo() override;
};

struct AllowMultipleModesAction : ModuleAction {
	int auxIndex;
	AllowMultipleModesAction();
	void undo() override;
	void redo() override;
};

struct ResetSceneAction : ModuleAction {
	int oldResetScene, newResetScene;

	ResetSceneAction();
	void undo() override;
	void redo() override;
};

struct ToggleModeBAction : ModuleAction {
	ToggleModeBAction();
	void undo() override;
	void redo() override;
};

struct ToggleRingMorphAction : ModuleAction {
	ToggleRingMorphAction();
	void undo() override;
	void redo() override;
};

struct ToggleRandomizeRingMorphAction : ModuleAction {
	ToggleRandomizeRingMorphAction();
	void undo() override;
	void redo() override;
};

struct ToggleExitConfigOnConnectAction : ModuleAction {
	ToggleExitConfigOnConnectAction();
	void undo() override;
	void redo() override;
};

struct ToggleCCWSceneSelectionAction : ModuleAction {
	ToggleCCWSceneSelectionAction();
	void undo() override;
	void redo() override;
};

struct ToggleWildModSumAction : ModuleAction {
	ToggleWildModSumAction();
	void undo() override;
	void redo() override;
};

struct ToggleClickFilterAction : ModuleAction {
	ToggleClickFilterAction();
	void undo() override;
	void redo() override;
};

struct ToggleGlowingInkAction : ModuleAction {
	ToggleGlowingInkAction();
	void undo() override;
	void redo() override;
};

struct ToggleVULightsAction : ModuleAction {
	ToggleVULightsAction();
    void undo() override;
    void redo() override;
};

struct ToggleResetOnRunAction : ModuleAction {
	ToggleResetOnRunAction();
    void undo() override;
    void redo() override;
};

struct ToggleRunSilencerAction : ModuleAction {
	ToggleRunSilencerAction();
	void undo() override;
	void redo() override;
};

struct SetClickFilterAction : ModuleAction {
	float oldSlew, newSlew;

	SetClickFilterAction();
	void undo() override;
	void redo() override;
};

struct RandomizeCurrentAlgorithmAction : ModuleAction {
	int oldAlgoName, oldHorizontalMarks, oldOpsDisabled, oldForcedCarriers;
	int newAlgoName, newHorizontalMarks, newOpsDisabled, newForcedCarriers;
	int scene;

	RandomizeCurrentAlgorithmAction();
    void undo() override;
    void redo() override;
};

struct RandomizeAllAlgorithmsAction : ModuleAction {
	int oldAlgorithm[3], oldHorizontalMarks[3], oldOpsDisabled[3], oldForcedCarriers[3];
	int newAlgorithm[3], newHorizontalMarks[3], newOpsDisabled[3], newForcedCarriers[3];

	RandomizeAllAlgorithmsAction();
	void undo() override;
	void redo() override;
};

struct InitializeCurrentAlgorithmAction : ModuleAction {
	int oldAlgoName, oldHorizontalMarks, oldOpsDisabled, oldForcedCarriers, scene;

	InitializeCurrentAlgorithmAction();
	void undo() override;
	void redo() override;
};

struct InitializeAllAlgorithmsAction : ModuleAction {
	int oldAlgorithm[3], oldHorizontalMarks[3], oldOpsDisabled[3], oldForcedCarriers[3];

	InitializeAllAlgorithmsAction();
	void undo() override;
	void redo() override;
};
