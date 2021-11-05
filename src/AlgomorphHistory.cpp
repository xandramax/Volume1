#include "AlgomorphHistory.hpp"
#include "Algomorph.hpp"
#include "AlgomorphLarge.hpp"


AlgorithmDiagonalChangeAction::AlgorithmDiagonalChangeAction() {
	name = "Delexander Algomorph diagonal connection";
}

void AlgorithmDiagonalChangeAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);

	m->toggleDiagonalDestination(scene, op, mod);

	m->graphDirty = true;
}

void AlgorithmDiagonalChangeAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);

	m->toggleDiagonalDestination(scene, op, mod);

	m->graphDirty = true;
}

AlgorithmHorizontalChangeAction::AlgorithmHorizontalChangeAction() {
	name = "Delexander Algomorph horizontal connection";
}

void AlgorithmHorizontalChangeAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);

	m->toggleHorizontalDestination(scene, op);

	m->graphDirty = true;
}

void AlgorithmHorizontalChangeAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);

	m->toggleHorizontalDestination(scene, op);

	m->graphDirty = true;
}

AlgorithmForcedCarrierChangeAction::AlgorithmForcedCarrierChangeAction() {
	name = "Delexander Algomorph forced carrier";
}

void AlgorithmForcedCarrierChangeAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);

	m->toggleForcedCarrier(scene, op);
}

void AlgorithmForcedCarrierChangeAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);

	m->toggleForcedCarrier(scene, op);
}

AlgorithmSceneChangeAction::AlgorithmSceneChangeAction() {
	name = "Delexander Algomorph base scene";
}

void AlgorithmSceneChangeAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->baseScene = oldScene;
	m->graphDirty = true;
}

void AlgorithmSceneChangeAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->baseScene = newScene;
	m->graphDirty = true;
}

ToggleModeBAction::ToggleModeBAction() {
	name = "Delexander Algomorph toggle mode B";
}

void ToggleModeBAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	
	m->toggleModeB();

	m->graphDirty = true;
}

void ToggleModeBAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	
	m->toggleModeB();

	m->graphDirty = true;
}

ToggleRingMorphAction::ToggleRingMorphAction() {
	name = "Delexander Algomorph toggle ring morph";
}

void ToggleRingMorphAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->ringMorph ^= true;
}

void ToggleRingMorphAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->ringMorph ^= true;
}

ToggleRandomizeRingMorphAction::ToggleRandomizeRingMorphAction() {
	name = "Delexander Algomorph toggle randomize ring morph";
}

void ToggleRandomizeRingMorphAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->randomRingMorph ^= true;
}

void ToggleRandomizeRingMorphAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->randomRingMorph ^= true;
}

ToggleExitConfigOnConnectAction::ToggleExitConfigOnConnectAction() {
	name = "Delexander Algomorph toggle exit config on connect";
}

void ToggleExitConfigOnConnectAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->exitConfigOnConnect ^= true;
}

void ToggleExitConfigOnConnectAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->exitConfigOnConnect ^= true;
}

ToggleClickFilterAction::ToggleClickFilterAction() {
	name = "Delexander Algomorph toggle click filter";
}

void ToggleClickFilterAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->clickFilterEnabled ^= true;
}

void ToggleClickFilterAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->clickFilterEnabled ^= true;
}

ToggleGlowingInkAction::ToggleGlowingInkAction() {
	name = "Delexander Algomorph toggle glowing ink";
}

void ToggleGlowingInkAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->glowingInk ^= true;
}

void ToggleGlowingInkAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->glowingInk ^= true;
}

ToggleVULightsAction::ToggleVULightsAction() {
	name = "Delexander Algomorph toggle VU lights";
}

void ToggleVULightsAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->vuLights ^= true;
}

void ToggleVULightsAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->vuLights ^= true;
}

ToggleResetOnRunAction::ToggleResetOnRunAction() {
	name = "Delexander Algomorph toggle reset on run";
}

SetClickFilterAction::SetClickFilterAction() {
	name = "Delexander Algomorph set click filter slew";
}

void SetClickFilterAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->clickFilterSlew = oldSlew;
}

void SetClickFilterAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->clickFilterSlew = newSlew;
}

RandomizeCurrentAlgorithmAction::RandomizeCurrentAlgorithmAction() {
	name = "Delexander Algomorph randomize current algorithm";
}

void RandomizeCurrentAlgorithmAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->algoName[scene] = oldAlgoName;
	m->horizontalMarks[scene] = oldHorizontalMarks;
	m->opsDisabled[scene] = oldOpsDisabled;
	m->forcedCarriers[scene] = oldForcedCarriers;
	m->updateDisplayAlgo(scene);
}

void RandomizeCurrentAlgorithmAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->algoName[scene] = newAlgoName;
	m->horizontalMarks[scene] = newHorizontalMarks;
	m->opsDisabled[scene] = newOpsDisabled;
	m->forcedCarriers[scene] = newForcedCarriers;
	m->updateDisplayAlgo(scene);
}

RandomizeAllAlgorithmsAction::RandomizeAllAlgorithmsAction() {
	name = "Delexander Algomorph randomize all algorithms";
}

void RandomizeAllAlgorithmsAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	for (int scene = 0; scene < 3; scene++) {
		m->algoName[scene] = oldAlgorithm[scene];
		m->horizontalMarks[scene] = oldHorizontalMarks[scene];
		m->opsDisabled[scene] = oldOpsDisabled[scene];
		m->forcedCarriers[scene] = oldForcedCarriers[scene];
		m->updateDisplayAlgo(scene);
	}
}

void RandomizeAllAlgorithmsAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	for (int scene = 0; scene < 3; scene++) {
		m->algoName[scene] = newAlgorithm[scene];
		m->horizontalMarks[scene] = newHorizontalMarks[scene];
		m->opsDisabled[scene] = newOpsDisabled[scene];
		m->forcedCarriers[scene] = newForcedCarriers[scene];
		m->updateDisplayAlgo(scene);
	}
}

InitializeCurrentAlgorithmAction::InitializeCurrentAlgorithmAction() {
	name = "Delexander Algomorph initialize current algorithm";
}

void InitializeCurrentAlgorithmAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->algoName[scene] = oldAlgoName;
	m->horizontalMarks[scene] = oldHorizontalMarks;
	m->opsDisabled[scene] = oldOpsDisabled;
	m->forcedCarriers[scene] = oldForcedCarriers;
	m->updateDisplayAlgo(scene);
}

void InitializeCurrentAlgorithmAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	m->algoName[scene].reset();
	m->horizontalMarks[scene].reset();
	m->opsDisabled[scene].reset();
	m->forcedCarriers[scene].reset();
	m->updateDisplayAlgo(scene);
}

InitializeAllAlgorithmsAction::InitializeAllAlgorithmsAction() {
	name = "Delexander Algomorph initialize all algorithms";
}

void InitializeAllAlgorithmsAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	for (int i = 0; i < 3; i++) {
		m->algoName[i] = oldAlgorithm[i];
		m->horizontalMarks[i] = oldHorizontalMarks[i];
		m->opsDisabled[i] = oldOpsDisabled[i];
		m->forcedCarriers[i] = oldForcedCarriers[i];
		m->updateDisplayAlgo(i);
	}
}

void InitializeAllAlgorithmsAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	Algomorph* m = dynamic_cast<Algomorph*>(mw->module);
	assert(m);
	for (int i = 0; i < 3; i++) {
		m->algoName[i].reset();
		m->horizontalMarks[i].reset();
		m->opsDisabled[i].reset();
		m->forcedCarriers[i].reset();
		m->updateDisplayAlgo(i);
	}
}

AuxInputSetAction::AuxInputSetAction() {
	name = "Delexander Algomorph AUX In mode set";
}


// AlgomorphLarge:

void AuxInputSetAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->unsetAuxMode(auxIndex, mode);
	for (int c = 0; c < channels; c++)
		m->auxInput[auxIndex]->voltage[mode][c] = m->auxInput[auxIndex]->defVoltage[mode];
	m->rescaleVoltage(mode, channels);
}

KnobModeAction::KnobModeAction() {
name = "Delexander Algomorph knob mode";
}

void KnobModeAction::undo() {
rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
assert(mw);
AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
assert(m);
m->knobMode = oldKnobMode;
}

void KnobModeAction::redo() {
rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
assert(mw);
AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
assert(m);
m->knobMode = newKnobMode;
}

void AuxInputSetAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->auxInput[auxIndex]->setMode(mode);
	m->rescaleVoltage(mode, channels);
}

AuxInputSwitchAction::AuxInputSwitchAction() {
	name = "Delexander Algomorph AUX In mode switch";
}

void AuxInputSwitchAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->unsetAuxMode(auxIndex, newMode);
	for (int c = 0; c < channels; c++)
		m->auxInput[auxIndex]->voltage[newMode][c] = m->auxInput[auxIndex]->defVoltage[newMode];
	m->rescaleVoltage(newMode, channels);
	m->auxInput[auxIndex]->setMode(oldMode);
	m->rescaleVoltage(oldMode, channels);
}

void AuxInputSwitchAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->unsetAuxMode(auxIndex, oldMode);
	for (int c = 0; c < channels; c++)
		m->auxInput[auxIndex]->voltage[oldMode][c] = m->auxInput[auxIndex]->defVoltage[oldMode];
	m->rescaleVoltage(oldMode, channels);
	m->auxInput[auxIndex]->setMode(newMode);
	m->rescaleVoltage(newMode, channels);
}

AuxInputUnsetAction::AuxInputUnsetAction() {
	name = "Delexander Algomorph AUX In mode unset";
}

void AuxInputUnsetAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->auxInput[auxIndex]->setMode(mode);
	m->rescaleVoltage(mode, channels);
}

void AuxInputUnsetAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->unsetAuxMode(auxIndex, mode);
	for (int c = 0; c < channels; c++)
		m->auxInput[auxIndex]->voltage[mode][c] = m->auxInput[auxIndex]->defVoltage[mode];
	m->rescaleVoltage(mode, channels);
}

AllowMultipleModesAction::AllowMultipleModesAction() {
	name = "Delexander Algomorph allow multiple modes";
}

void AllowMultipleModesAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->auxInput[auxIndex]->allowMultipleModes = false;
}

void AllowMultipleModesAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->auxInput[auxIndex]->allowMultipleModes = true;
}

ResetSceneAction::ResetSceneAction() {
	name = "Delexander Algomorph change reset scene";
}

void ResetSceneAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->resetScene = oldResetScene;
}

void ResetSceneAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->resetScene = newResetScene;
}

ToggleCCWSceneSelectionAction::ToggleCCWSceneSelectionAction() {
	name = "Delexander Algomorph toggle CCW scene selection";
}

void ToggleCCWSceneSelectionAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->ccwSceneSelection ^= true;
}

void ToggleCCWSceneSelectionAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->ccwSceneSelection ^= true;
}

ToggleWildModSumAction::ToggleWildModSumAction() {
	name = "Delexander Algomorph toggle wildcard mod summing";
}

void ToggleWildModSumAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->wildModIsSummed ^= true;
}

void ToggleWildModSumAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->wildModIsSummed ^= true;
}

void ToggleResetOnRunAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->resetOnRun ^= true;
}

void ToggleResetOnRunAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->resetOnRun ^= true;
}

ToggleRunSilencerAction::ToggleRunSilencerAction() {
	name = "Delexander Algomorph toggle run silencer";
}

void ToggleRunSilencerAction::undo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->runSilencer ^= true;
}

void ToggleRunSilencerAction::redo() {
	rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
	assert(mw);
	AlgomorphLarge* m = dynamic_cast<AlgomorphLarge*>(mw->module);
	assert(m);
	m->runSilencer ^= true;
}
