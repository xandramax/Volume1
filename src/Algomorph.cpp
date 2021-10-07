#include "plugin.hpp"
#include "Algomorph.hpp"
#include "AlgomorphDisplayWidget.hpp"
#include "GraphStructure.hpp"
#include <bitset>

Algomorph::Algomorph() {
    for (int i = 0; i < 4; i++) {
        for (int c = 0; c < 16; c++) {
            sumClickFilters[i][c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
            sumRingClickFilters[i][c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
            for (int j = 0; j < 4; j++) {
                modClickFilters[i][j][c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
                modRingClickFilters[i][j][c].setRiseFall(DEF_CLICK_FILTER_SLEW, DEF_CLICK_FILTER_SLEW);
            }
        }
    }

    clickFilterDivider.setDivision(128);
    lightDivider.setDivision(64);
    cvDivider.setDivision(32);

    // Map 3-bit operator-relative mod output indices to 4-bit generalized equivalents
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (i != j) {
                threeToFour[i][i > j ? j : j - 1] = j;
                fourToThree[i][j] = i > j ? j : j - 1;
            }
            else
                fourToThree[i][j] = -1;
        }
    }

    // Initialize graphAddressTranslation[] to -1, then index local ids by generalized 16-bit equivalents
    for (int i = 0; i < 0xFFFF; i++) {
        graphAddressTranslation[i] = -1;
    }
    for (int i = 0; i < 1980; i++) {
        graphAddressTranslation[(int)GRAPH_DATA.xNodeData[i][0]] = i;
    }

    Algomorph::onReset();
}

void Algomorph::onReset() {
    for (int scene = 0; scene < 3; scene++) {
        algoName[scene].reset();
        for (int op = 0; op < 4; op++) {
            horizontalMarks[scene].set(op, false);
            forcedCarriers[scene].set(op, false);
            carrier[scene].set(op, true);
            opsDisabled[scene].set(op, false);
        }
        updateDisplayAlgo(scene);
    }

    configMode = false;
    configOp = -1;
    configScene = 1;
    baseScene = 1;

    for (int c = 0; c < 16; c++) {
        centerMorphScene[c]    = baseScene;
        forwardMorphScene[c]   = (baseScene + 1) % 3;
        backwardMorphScene[c]  = (baseScene + 2) % 3;
    }

    displayMorph.push(0.f);
    displayScene.push(baseScene);
    displayMorphScene.push(forwardMorphScene[0]);

    displayMorph.clear();
    displayScene.clear();
    displayMorphScene.clear();

    clickFilterEnabled = true;
    clickFilterSlew = DEF_CLICK_FILTER_SLEW;
    ringMorph = false;
    randomRingMorph = false;
    exitConfigOnConnect = false;
    modeB = false;

    glowingInk = pluginSettings.glowingInkDefault;
    vuLights = pluginSettings.vuLightsDefault;

    blinkStatus = true;
    blinkTimer = BLINK_INTERVAL;
    graphDirty = true;
}

void Algomorph::randomizeAlgorithm(int scene) {
    bool noCarrier = true;
    algoName[scene].reset();    //Initialize
    horizontalMarks[scene].reset();   //Initialize
    if (modeB)
        forcedCarriers[scene].reset();    //Initialize
    for (int op = 0; op < 4; op++) {
        if (modeB) {
            bool disabled = true;           //Initialize
            forcedCarriers[scene].set(op, false);   //Initialize
            if (random::uniform() > .5f) {
                forcedCarriers[scene].set(op, true);
                noCarrier = false;
                disabled = false;
            }
            if (random::uniform() > .5f) {
                horizontalMarks[scene].set(op, true);
                //Do not set algoName, because the operator is not disabled
                disabled = false;
            }
            for (int mod = 0; mod < 3; mod++) {
                if (random::uniform() > .5f) {
                    algoName[scene].set(op * 3 + mod, true);
                    disabled = false;
                }
            }
            if (disabled)
                algoName[scene].set(12 + op, true);
        }
        else {
            forcedCarriers[scene].set(op, false);   //Disable
            if (random::uniform() > .5f) {  //If true, operator is a carrier
                noCarrier = false;
                for (int mod = 0; mod < 3; mod++)
                    algoName[scene].set(op * 3 + mod, false);
            }
            else {
                if (random::uniform() > .5) {   //If true, operator is disabled
                    horizontalMarks[scene].set(op, true);
                    algoName[scene].set(12 + op, true);
                    for (int mod = 0; mod < 3; mod++ )
                        algoName[scene].set(op * 3 + mod, false);
                }
                else {
                    for (int mod = 0; mod < 3; mod++) {
                        if (random::uniform() > .5f)
                            algoName[scene].set(op * 3 + mod, true);    
                    }
                }
            }
        }
    }
    if (noCarrier) {
        int shortStraw = std::floor(random::uniform() * 4);
        while (shortStraw == 4)
            shortStraw = std::floor(random::uniform() * 4);
        if (modeB) {
            forcedCarriers[scene].set(shortStraw, true);
            algoName[scene].set(12 + shortStraw, false);
        }
        else {
            horizontalMarks[scene].set(shortStraw, false);
            algoName[scene].set(12 + shortStraw, false);
            for (int mod = 0; mod < 3; mod++)
                algoName[scene].set(shortStraw * 3 + mod, false);
        }
    }
    updateCarriers(scene);
    updateOpsDisabled(scene);
    updateDisplayAlgo(scene);
}

void Algomorph::onRandomize() {
    //If in config mode, only randomize the current algorithm,
    //do not change the base scene and do not enable/disable ring morph
    if (configMode)
        randomizeAlgorithm(configScene);
    else {
        for (int scene = 0; scene < 3; scene++)
            randomizeAlgorithm(scene);
    }
    graphDirty = true;
}

float Algomorph::routeHorizontal(float sampleTime, float inputVoltage, int op, int c) {
    float connectionA = horizontalMarks[centerMorphScene[c]].test(op) * (1.f - relativeMorphMagnitude[c]);
    float connectionB = horizontalMarks[forwardMorphScene[c]].test(op) * (relativeMorphMagnitude[c]);
    float morphedConnection = connectionA + connectionB;
    modClickGain[op][op][c] = clickFilterEnabled ? modClickFilters[op][op][c].process(sampleTime, morphedConnection) : morphedConnection;
    return inputVoltage * modClickGain[op][op][c];
}

float Algomorph::routeHorizontalRing(float sampleTime, float inputVoltage, int op, int c) {
    float connection = horizontalMarks[backwardMorphScene[c]].test(op) * relativeMorphMagnitude[c];
    modRingClickGain[op][op][c] = clickFilterEnabled ? modRingClickFilters[op][op][c].process(sampleTime, connection) : connection;
    return -inputVoltage * modRingClickGain[op][op][c];
}

float Algomorph::routeDiagonal(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connectionA = algoName[centerMorphScene[c]].test(op * 3 + mod)   * (1.f - relativeMorphMagnitude[c])  * !horizontalMarks[centerMorphScene[c]].test(op);
    float connectionB = algoName[forwardMorphScene[c]].test(op * 3 + mod)  * relativeMorphMagnitude[c]          * !horizontalMarks[forwardMorphScene[c]].test(op);
    float morphedConnection = connectionA + connectionB;
    modClickGain[op][threeToFour[op][mod]][c] = clickFilterEnabled ? modClickFilters[op][threeToFour[op][mod]][c].process(sampleTime, morphedConnection) : morphedConnection;
    return inputVoltage * modClickGain[op][threeToFour[op][mod]][c];
}

float Algomorph::routeDiagonalB(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connectionA = algoName[centerMorphScene[c]].test(op * 3 + mod)   * (1.f - relativeMorphMagnitude[c]);
    float connectionB = algoName[forwardMorphScene[c]].test(op * 3 + mod)  * relativeMorphMagnitude[c];
    float morphedConnection = connectionA + connectionB;
    modClickGain[op][threeToFour[op][mod]][c] = clickFilterEnabled ? modClickFilters[op][threeToFour[op][mod]][c].process(sampleTime, morphedConnection) : morphedConnection;
    return inputVoltage * modClickGain[op][threeToFour[op][mod]][c];
}

float Algomorph::routeDiagonalRing(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connection = algoName[backwardMorphScene[c]].test(op * 3 + mod) * relativeMorphMagnitude[c] * !horizontalMarks[backwardMorphScene[c]].test(op);
    modRingClickGain[op][threeToFour[op][mod]][c] = clickFilterEnabled ? modRingClickFilters[op][threeToFour[op][mod]][c].process(sampleTime, connection) : connection; 
    return -inputVoltage * modRingClickGain[op][threeToFour[op][mod]][c];
}

float Algomorph::routeDiagonalRingB(float sampleTime, float inputVoltage, int op, int mod, int c) {
    float connection = algoName[backwardMorphScene[c]].test(op * 3 + mod) * relativeMorphMagnitude[c];
    modRingClickGain[op][threeToFour[op][mod]][c] = clickFilterEnabled ? modRingClickFilters[op][threeToFour[op][mod]][c].process(sampleTime, connection) : connection; 
    return -inputVoltage * modRingClickGain[op][threeToFour[op][mod]][c];
}

float Algomorph::routeSum(float sampleTime, float inputVoltage, int op, int c) {
    float connection    =   carrier[centerMorphScene[c]].test(op)     * (1.f - relativeMorphMagnitude[c])  * !horizontalMarks[centerMorphScene[c]].test(op)
                        +   carrier[forwardMorphScene[c]].test(op)    * relativeMorphMagnitude[c]          * !horizontalMarks[forwardMorphScene[c]].test(op);
    sumClickGain[op][c] = clickFilterEnabled ? sumClickFilters[op][c].process(sampleTime, connection) : connection;
    return inputVoltage * sumClickGain[op][c];
}

float Algomorph::routeSumB(float sampleTime, float inputVoltage, int op, int c) {
    float connection    =   carrier[centerMorphScene[c]].test(op)     * (1.f - relativeMorphMagnitude[c])
                        +   carrier[forwardMorphScene[c]].test(op)    * relativeMorphMagnitude[c];
    sumClickGain[op][c] = clickFilterEnabled ? sumClickFilters[op][c].process(sampleTime, connection) : connection;
    return inputVoltage * sumClickGain[op][c];
}

float Algomorph::routeSumRing(float sampleTime, float inputVoltage, int op, int c) {
    float connection = carrier[backwardMorphScene[c]].test(op) * relativeMorphMagnitude[c] * !horizontalMarks[backwardMorphScene[c]].test(op);
    sumRingClickGain[op][c] = clickFilterEnabled ? sumRingClickFilters[op][c].process(sampleTime, connection) : connection;
    return -inputVoltage * sumRingClickGain[op][c];
}

float Algomorph::routeSumRingB(float sampleTime, float inputVoltage, int op, int c) {
    float connection = carrier[backwardMorphScene[c]].test(op) * relativeMorphMagnitude[c];
    sumRingClickGain[op][c] = clickFilterEnabled ? sumRingClickFilters[op][c].process(sampleTime, connection) : connection;
    return -inputVoltage * sumRingClickGain[op][c];
}

void Algomorph::toggleHorizontalDestination(int scene, int op) {
    horizontalMarks[scene].flip(op);
    if (!modeB) {
        toggleDisabled(scene, op);
        if (horizontalMarks[scene].test(op))
            carrier[scene].set(op, forcedCarriers[scene].test(op));
        else
            carrier[scene].set(op, isCarrier(scene, op));
    }
    else {
        bool mismatch = opsDisabled[scene].test(op) ^ isDisabled(scene, op);
        if (mismatch)
            toggleDisabled(scene, op);
    }
}

void Algomorph::toggleDiagonalDestination(int scene, int op, int mod) {
    algoName[scene].flip(op * 3 + mod);
    if (!modeB) {
        if (algoName[scene].test(op * 3 + mod))
            carrier[scene].set(op, forcedCarriers[scene].test(op));
        else
            carrier[scene].set(op, isCarrier(scene, op));
    }
    else {   
        bool mismatch = opsDisabled[scene].test(op) ^ isDisabled(scene, op);
        if (mismatch)
            toggleDisabled(scene, op);
    }
    if (!opsDisabled[scene].test(op))
        updateDisplayAlgo(scene);
}

bool Algomorph::isCarrier(int scene, int op) {
    bool isCarrier = true;
    if (modeB)
        isCarrier = false;
    else {
        for (int mod = 0; mod < 3; mod++) {
            if (algoName[scene].test(op * 3 + mod))
                isCarrier = false;
        }
        if (horizontalMarks[scene].test(op))
            isCarrier = false;
    }
    isCarrier |= forcedCarriers[scene].test(op);
    return isCarrier;
}

void Algomorph::updateCarriers(int scene) {
    for (int op = 0; op < 4; op++)
        carrier[scene].set(op, isCarrier(scene, op));
}

bool Algomorph::isDisabled(int scene, int op) {
    if (!modeB) {
        if (horizontalMarks[scene].test(op))
            return true;
        else
            return false;
    }
    else {
        if (forcedCarriers[scene].test(op))
            return false;
        else {
            if (horizontalMarks[scene].test(op))
                return false;
            else {
                for (int mod = 0; mod < 3; mod++) {
                    if (algoName[scene].test(op * 3 + mod))
                        return false;
                }
                return true;
            }
        }
    }
}

void Algomorph::toggleDisabled(int scene, int op) {
    algoName[scene].flip(12 + op);
    opsDisabled[scene].flip(op);
    updateDisplayAlgo(scene);
}

void Algomorph::updateOpsDisabled(int scene) {
    for (int op = 0; op < 4; op++) {
        opsDisabled[scene].set(op, isDisabled(scene, op));
    }
}

void Algomorph::updateDisplayAlgo(int scene) {
    tempDisplayAlgoName = algoName[scene];
    // Set display algorithm
    for (int op = 0; op < 4; op++) {
        if (opsDisabled[scene].test(op)) {
            // Set all destinations to false
            for (int mod = 0; mod < 3; mod++)
                tempDisplayAlgoName.set(op * 3 + mod, false);
            // Check if any operators are modulating this operator
            bool fullDisable = true;
            for (int i = 0; i < 4; i++) {     
                if (!opsDisabled[scene].test(i) && algoName[scene].test(i * 3 + fourToThree[i][op]))
                    fullDisable = false;
            }
            if (fullDisable) {
                tempDisplayAlgoName.set(12 + op, true);
            }
            else
                tempDisplayAlgoName.set(12 + op, false);
        }
        else {
            // Enable destinations in the display and handle the consequences
            for (int i = 0; i < 3; i++) {
                if (algoName[scene].test(op * 3 + i)) {
                    tempDisplayAlgoName.set(op * 3 + i, true);
                    // the consequences
                    if (opsDisabled[scene].test(threeToFour[op][i]))
                        tempDisplayAlgoName.set(12 + threeToFour[op][i], false);
                }
            }  
        }
    }
    displayAlgoName[scene].push(tempDisplayAlgoName);
}

void Algomorph::toggleModeB() {
    modeB ^= true;
    if (modeB) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                carrier[i].set(j, forcedCarriers[i].test(j));
                bool mismatch = opsDisabled[i].test(j) ^ isDisabled(i, j);
                if (mismatch)
                    toggleDisabled(i, j);
            }
        }
    }
    else {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                bool mismatch = opsDisabled[i].test(j) ^ isDisabled(i, j);
                if (mismatch)
                    toggleDisabled(i, j);
            }
            // Check carrier status after above loop is complete
            for (int op = 0; op < 4; op++)
                carrier[i].set(op, isCarrier(i, op));
        }
    }
}

void Algomorph::toggleForcedCarrier(int scene, int op) {
    forcedCarriers[scene].flip(op);
    if (forcedCarriers[scene].test(op))
        carrier[scene].set(op, true);
    else
        carrier[scene].set(op, isCarrier(scene, op));
    if (modeB) {
        bool mismatch = opsDisabled[scene].test(op) ^ isDisabled(scene, op);
        if (mismatch)
            toggleDisabled(scene, op);
    }
}


///// Panel Widget

/// Menu Items

void AlgomorphWidget::ToggleModeBItem::onAction(const event::Action &e) {
    // History
    ToggleModeBAction<Algomorph>* h = new ToggleModeBAction<Algomorph>;
    h->moduleId = module->id;

    module->toggleModeB();

    APP->history->push(h);

    module->graphDirty = true;
}

void AlgomorphWidget::RingMorphItem::onAction(const event::Action &e) {
    // History
    ToggleRingMorphAction<Algomorph>* h = new ToggleRingMorphAction<Algomorph>;
    h->moduleId = module->id;

    module->ringMorph ^= true;

    APP->history->push(h);
}

void AlgomorphWidget::ExitConfigItem::onAction(const event::Action &e) {
    // History
    ToggleExitConfigOnConnectAction<Algomorph>* h = new ToggleExitConfigOnConnectAction<Algomorph>;
    h->moduleId = module->id;

    module->exitConfigOnConnect ^= true;

    APP->history->push(h);
}

void AlgomorphWidget::ClickFilterEnabledItem::onAction(const event::Action &e) {
    // History
    ToggleClickFilterAction<Algomorph>* h = new ToggleClickFilterAction<Algomorph>;
    h->moduleId = module->id;

    module->clickFilterEnabled ^= true;

    APP->history->push(h);
}

void AlgomorphWidget::VULightsItem::onAction(const event::Action &e) {
    // History
    ToggleVULightsAction<Algomorph>* h = new ToggleVULightsAction<Algomorph>;
    h->moduleId = module->id;

    module->vuLights ^= true;

    APP->history->push(h);
}

void AlgomorphWidget::GlowingInkItem::onAction(const event::Action &e) {
    // History
    ToggleGlowingInkAction<Algomorph>* h = new ToggleGlowingInkAction<Algomorph>;
    h->moduleId = module->id;

    module->glowingInk ^= true;

    APP->history->push(h);
}

void AlgomorphWidget::DebugItem::onAction(const event::Action &e) {
    module->debug ^= true;
}

/// Click Filter Menu

Menu* AlgomorphWidget::ClickFilterMenuItem::createChildMenu() {
    Menu* menu = new Menu;
    createClickFilterMenu(module, menu);
    return menu;
}

void AlgomorphWidget::ClickFilterMenuItem::createClickFilterMenu(Algomorph* module, ui::Menu* menu) {
    ClickFilterEnabledItem *clickFilterEnabledItem = createMenuItem<ClickFilterEnabledItem>("Disable Click Filter", CHECKMARK(!module->clickFilterEnabled));
    clickFilterEnabledItem->module = module;
    menu->addChild(clickFilterEnabledItem);
    ClickFilterSlider* clickFilterSlider = new ClickFilterSlider(module);
    clickFilterSlider->box.size.x = 200.0;
    menu->addChild(clickFilterSlider);
}

AlgomorphWidget::ClickFilterSlider::ClickFilterQuantity::ClickFilterQuantity(Algomorph* m) {
    module = m;
}

void AlgomorphWidget::ClickFilterSlider::ClickFilterQuantity::setValue(float value) {
    v = clamp(value, 16.f, 7500.f);
    module->clickFilterSlew = v;
}

float AlgomorphWidget::ClickFilterSlider::ClickFilterQuantity::getValue() {
    v = module->clickFilterSlew;
    return v;
}

float AlgomorphWidget::ClickFilterSlider::ClickFilterQuantity::getDefaultValue() {
    return DEF_CLICK_FILTER_SLEW;
}

float AlgomorphWidget::ClickFilterSlider::ClickFilterQuantity::getMinValue() {
    return 16.f;
}

float AlgomorphWidget::ClickFilterSlider::ClickFilterQuantity::getMaxValue() {
    return 7500.f;
}

float AlgomorphWidget::ClickFilterSlider::ClickFilterQuantity::getDisplayValue() {
    return getValue();
}

std::string AlgomorphWidget::ClickFilterSlider::ClickFilterQuantity::getDisplayValueString() {
    int i = int(getValue());
    return string::f("%i", i);
}

void AlgomorphWidget::ClickFilterSlider::ClickFilterQuantity::setDisplayValue(float displayValue) {
    setValue(displayValue);
}

std::string AlgomorphWidget::ClickFilterSlider::ClickFilterQuantity::getLabel() {
    return "Click Filter Slew";
}

std::string AlgomorphWidget::ClickFilterSlider::ClickFilterQuantity::getUnit() {
    return "Hz";
}

AlgomorphWidget::ClickFilterSlider::ClickFilterSlider(Algomorph* m) {
    quantity = new ClickFilterQuantity(m);
    module = m;
}

AlgomorphWidget::ClickFilterSlider::~ClickFilterSlider() {
    delete quantity;
}

void AlgomorphWidget::ClickFilterSlider::onDragStart(const event::DragStart& e) {
    if (quantity)
        oldValue = quantity->getValue();
}

void AlgomorphWidget::ClickFilterSlider::onDragMove(const event::DragMove& e) {
    if (quantity)
        quantity->moveScaledValue(0.002f * e.mouseDelta.x);
}

void AlgomorphWidget::ClickFilterSlider::onDragEnd(const event::DragEnd& e) {
    if (quantity) {
        // History
        SetClickFilterAction<Algomorph>* h = new SetClickFilterAction<Algomorph>;
        h->moduleId = module->id;
        h->oldSlew = oldValue;
        h->newSlew = quantity->getValue();

        APP->history->push(h);
    }    
}

/// Audio Settings Menu

Menu* AlgomorphWidget::AudioSettingsMenuItem::createChildMenu() {
    Menu* menu = new Menu;
    createAudioSettingsMenu(module, menu);
    return menu;
}

void AlgomorphWidget::AudioSettingsMenuItem::createAudioSettingsMenu(Algomorph* module, ui::Menu* menu) {
    menu->addChild(construct<ClickFilterMenuItem>(&MenuItem::text, "Click Filter…", &MenuItem::rightText, (module->clickFilterEnabled ? "Enabled ▸" : "Disabled ▸"), &ClickFilterMenuItem::module, module));

    RingMorphItem *ringMorphItem = createMenuItem<RingMorphItem>("Enable Ring Morph", CHECKMARK(module->ringMorph));
    ringMorphItem->module = module;
    menu->addChild(ringMorphItem);
}
