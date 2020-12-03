#pragma once
#include "plugin.hpp"

struct Algomorph : Module {
    float morph[16] = {0.f};                                    // Range -1.f -> 1.f
    float relativeMorphMagnitude[16] = { morph[0] };            // Range 0.f -> 1.f

    int baseScene = -1;                                         // Center the Morph knob on saved algorithm 0, 1, or 2
    int centerMorphScene[16]    = { baseScene };
    int forwardMorphScene[16]   = { (baseScene + 1) % 3 };
    int backwardMorphScene[16]  = { (baseScene + 2) % 3 };

    std::bitset<16> algoName[3]         = {0};                  // 16-bit IDs of the three stored algorithms

    std::bitset<4> horizontalMarks[3]   = {0};                  // If the user creates a horizontal connection, mark it here
    std::bitset<4> forcedCarriers[3]    = {0};                  // If the user forces an operator to act as a carrier, mark it here 
    std::bitset<4> carrier[3]           = {0xF, 0xF, 0xF};      // If an operator is acting as a carrier, whether forced or automatically, mark it here
    std::bitset<4> opsDisabled[3]       = {0};                  // If an operator is disabled, whether forced or automatically, mark it here
    
    std::bitset<16> displayAlgoName[3]  = {0};                  // When operators are disabled, remove their mod destinations from here
                                                                // If a disabled operator is a mod destination, set it to enabled here

    dsp::ClockDivider cvDivider;
    dsp::BooleanTrigger sceneButtonTrigger[3];
    dsp::BooleanTrigger editTrigger;
    dsp::BooleanTrigger operatorTrigger[4];
    dsp::BooleanTrigger modulatorTrigger[4];

    // [noRing/ring][op][mod][channel]
    dsp::SlewLimiter modClickFilters[4][4][16];
    dsp::SlewLimiter modRingClickFilters[4][4][16];
    float modClickGain[4][4][16] = {{{0.f}}};
    float modRingClickGain[4][4][16] = {{{0.f}}};

    // [op][channel]
    dsp::SlewLimiter sumClickFilters[4][16];
    dsp::SlewLimiter sumRingClickFilters[4][16];
    float sumClickGain[4][16] = {{0.f}};
    float sumRingClickGain[4][16] = {{0.f}};

    dsp::ClockDivider clickFilterDivider;

    dsp::ClockDivider lightDivider;
    float sceneBrightnesses[3][12][3] = {{{}}};
    float blinkTimer = BLINK_INTERVAL;
    bool blinkStatus = true;
    RingIndicatorRotor rotor;

    bool configMode = true;
    int configScene = -1;
    int configOp = -1;              // Set to 0-3 when configuring mod destinations for operators 1-4

    bool graphDirty = true;
    bool debug = false;

    int graphAddressTranslation[0xFFFF];        // Graph ID conversion
                                                // The algorithm graph data are stored with IDs in 12-bit space:
                                                //       0000 000 000 000 000 -> 1111 000 000 000 000
                                                // The first 4 bits mark which operators are disabled (1) vs enabled (0).
                                                // Each set of 3 bits corresponds to an operator.
                                                // Each bit represents one of an oscillator's "legal" mod destinations.
                                                // At least one operator is a carrier (having no mod destinations, i.e. all bits zero).
                                                // However, the algorithms are accessed via 20-bit IDs:
                                                //       0000 0000 0000 0000 0000 -> 1111 0000 0000 0000 0000
                                                // In 16-bit space, the feedback destinations are included but never equal 1.  
                                                // graphAddressTranslation is indexed by 20-bit ID and returns equivalent 12-bit ID.

    int threeToFour[4][3] = {{0}};      // Modulator ID conversion ([op][x] = y, where x is 0..2 and y is 0..3)
    int fourToThree[4][4] = {{0}};      // Modulator ID conversion ([op][x] = y, where x is 0..3 and y is 0..2)

    //User settings
    bool clickFilterEnabled = true;
    bool ringMorph = false;
    bool randomRingMorph = false;
    bool exitConfigOnConnect = false;
    bool glowingInk = false;
    bool vuLights = true;
    bool modeB = false;
    float clickFilterSlew = DEF_CLICK_FILTER_SLEW;

    Algomorph();
    void onReset() override;
    void randomizeAlgorithm(int scene);
    void onRandomize() override;
    float routeHorizontal(float sampleTime, float inputVoltage, int op, int c);
    float routeHorizontalRing(float sampleTime, float inputVoltage, int op, int c);
    float routeDiagonal(float sampleTime, float inputVoltage, int op, int mod, int c);
    float routeDiagonalB(float sampleTime, float inputVoltage, int op, int mod, int c);
    float routeDiagonalRing(float sampleTime, float inputVoltage, int op, int mod, int c);
    float routeDiagonalRingB(float sampleTime, float inputVoltage, int op, int mod, int c);
    float routeSum(float sampleTime, float inputVoltage, int op, int c);
    float routeSumB(float sampleTime, float inputVoltage, int op, int c);
    float routeSumRing(float sampleTime, float inputVoltage, int op, int c);
    float routeSumRingB(float sampleTime, float inputVoltage, int op, int c);
    void toggleHorizontalDestination(int scene, int op);
    void toggleDiagonalDestination(int scene, int op, int mod);
    bool isCarrier(int scene, int op);
    void updateCarriers(int scene);
    bool isDisabled(int scene, int op);
    void toggleDisabled(int scene, int op);
    void updateOpsDisabled(int scene);
    void updateDisplayAlgo(int scene);
    void toggleModeB();
    void toggleForcedCarrier(int scene, int op);
};

struct AlgomorphWidget : ModuleWidget {
    // Menu items
    struct AlgomorphMenuItem : MenuItem {
        Algomorph* module;
    };
    struct ToggleModeBItem : AlgomorphMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct RingMorphItem : AlgomorphMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct ExitConfigItem : AlgomorphMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct ClickFilterEnabledItem : AlgomorphMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct VULightsItem : AlgomorphMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct GlowingInkItem : AlgomorphMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct DebugItem : AlgomorphMenuItem {
        void onAction(const event::Action &e) override;
    };
    struct SaveVisualSettingsItem : AlgomorphMenuItem {
        void onAction(const event::Action &e) override;
    };

    // Sub-menus
    struct AudioSettingsMenuItem : AlgomorphMenuItem {
        void createAudioSettingsMenu(Algomorph* module, ui::Menu* menu);
        Menu* createChildMenu() override;
    };
    struct ClickFilterMenuItem : AlgomorphMenuItem {
        void createClickFilterMenu(Algomorph* module, ui::Menu* menu);
        Menu* createChildMenu() override;
    };

    struct ClickFilterSlider : ui::Slider {
        float oldValue = DEF_CLICK_FILTER_SLEW;
        Algomorph* module;

        struct ClickFilterQuantity : Quantity {
            Algomorph* module;
            float v = -1.f;

            ClickFilterQuantity(Algomorph* m);
            void setValue(float value) override;
            float getValue() override;
            float getDefaultValue() override;
            float getMinValue() override;
            float getMaxValue() override;
            float getDisplayValue() override;
            std::string getDisplayValueString() override;
            void setDisplayValue(float displayValue) override;
            std::string getLabel() override;
            std::string getUnit() override;
        };

        ClickFilterSlider(Algomorph* m);
        ~ClickFilterSlider();
        void onDragStart(const event::DragStart& e) override;
        void onDragMove(const event::DragMove& e) override;
        void onDragEnd(const event::DragEnd& e) override;
    };
};
