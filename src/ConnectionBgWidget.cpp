#include "ConnectionBgWidget.hpp"
#include "AlgomorphHistory.hpp"
#include "Algomorph.hpp"
#include "plugin.hpp" // For constants
#include <rack.hpp>
using rack::construct;


Line::Line(Vec a, Vec b) {
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
    if (flipped) {
        left = Vec(RING_RADIUS * std::cos(startAngle) + a.x, RING_RADIUS * std::sin(startAngle) + a.y);
        right = Vec(b.x - RING_RADIUS * std::cos(startAngle),  b.y - RING_RADIUS * std::sin(startAngle));
    }
    else {
        left = Vec(RING_RADIUS * std::cos(endAngle) + a.x, RING_RADIUS * std::sin(endAngle) + a.y);
        right = Vec(b.x - RING_RADIUS * std::cos(endAngle),  b.y - RING_RADIUS * std::sin(endAngle));
    }
}

ConnectionBgWidget::ConnectionBgWidget(std::vector<Vec> left, std::vector<Vec> right, Algomorph* module) {
    this->module = module;	
    for (unsigned i = 0; i < left.size(); i++) {
        for (unsigned j = 0; j < right.size(); j++) {
            lines.push_back(Line(left[i], right[j]));
        }
    }
}

void ConnectionBgWidget::draw(const Widget::DrawArgs& args) {
    //Colors from rack::GrayModuleLightWidget
    for (Line line : lines) {
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, line.left.x - box.pos.x, line.left.y - box.pos.y);
        nvgLineTo(args.vg, line.right.x - box.pos.x, line.right.y - box.pos.y);
        nvgStrokeWidth(args.vg, 1.1f);
        // Background
        nvgStrokeColor(args.vg, nvgRGB(0x5a, 0x5a, 0x5a));
        nvgStroke(args.vg);
        // Border
        nvgStrokeWidth(args.vg, 0.6);
        nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 0x60));
        nvgStroke(args.vg);
    }

    OpaqueWidget::draw(args);
}

void ConnectionBgWidget::onButton(const rack::event::Button& e) {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
        createContextMenu();
        e.consume(this);
    }
    OpaqueWidget::onButton(e);
}

void ConnectionBgWidget::createContextMenu() {
    std::string currentAlgo = std::to_string((module->configMode ? module->configScene : module->centerMorphScene[0]) + 1);

    rack::ui::Menu* menu = rack::createMenu();
    menu->addChild(construct<RandomizeCurrentAlgorithmItem>(&MenuItem::text, "Randomize Algorithm " +  currentAlgo, &RandomizeCurrentAlgorithmItem::module, module));
    menu->addChild(construct<InitializeCurrentAlgorithmItem>(&MenuItem::text, "Initialize Algorithm " + currentAlgo, &InitializeCurrentAlgorithmItem::module, module));
    menu->addChild(new rack::ui::MenuSeparator());
    menu->addChild(construct<RandomizeAllAlgorithmsItem>(&MenuItem::text, "Randomize All Algorithms", &RandomizeAllAlgorithmsItem::module, module));
    menu->addChild(construct<InitializeAllAlgorithmsItem>(&MenuItem::text, "Initialize All Algorithms", &InitializeAllAlgorithmsItem::module, module));
}

void InitializeCurrentAlgorithmItem::onAction(const rack::event::Action &e) {
    int scene = module->configMode ? module->configScene : module->centerMorphScene[0];

    // History
    InitializeCurrentAlgorithmAction* h = new InitializeCurrentAlgorithmAction;
    h->moduleId = module->id;
    h->scene = scene;
    h->oldAlgoName = module->algoName[scene].to_ullong();
    h->oldHorizontalMarks = module->horizontalMarks[scene].to_ullong();
    h->oldForcedCarriers = module->forcedCarriers[scene].to_ullong();
    h->oldOpsDisabled = module->opsDisabled[scene].to_ullong();

    module->initializeAlgorithm(scene);
    module->graphDirty = true;

    APP->history->push(h);
}

void InitializeAllAlgorithmsItem::onAction(const rack::event::Action &e) {
    // History
    InitializeAllAlgorithmsAction* h = new InitializeAllAlgorithmsAction;
    h->moduleId = module->id;
    for (int scene = 0; scene < 3; scene++) {
        h->oldAlgorithm[scene] = module->algoName[scene].to_ullong();
        h->oldHorizontalMarks[scene] = module->horizontalMarks[scene].to_ullong();
        h->oldForcedCarriers[scene] = module->forcedCarriers[scene].to_ullong();
        h->oldOpsDisabled[scene] = module->opsDisabled[scene].to_ullong();
        module->initializeAlgorithm(scene);
    }

    module->graphDirty = true;

    APP->history->push(h);
}

void RandomizeCurrentAlgorithmItem::onAction(const rack::event::Action &e) {
    int scene = module->configMode ? module->configScene : module->centerMorphScene[0];

    RandomizeCurrentAlgorithmAction* h = new RandomizeCurrentAlgorithmAction();
    h->moduleId = module->id;
    h->scene = scene;

    h->oldAlgoName = module->algoName[scene].to_ullong();
    h->oldHorizontalMarks = module->horizontalMarks[scene].to_ullong();
    h->oldOpsDisabled = module->opsDisabled[scene].to_ullong();
    h->oldForcedCarriers = module->forcedCarriers[scene].to_ullong();
    
    module->randomizeAlgorithm(module->configMode ? module->configScene : module->centerMorphScene[0]);
    module->graphDirty = true;

    h->newAlgoName = module->algoName[scene].to_ullong();
    h->newHorizontalMarks = module->horizontalMarks[scene].to_ullong();
    h->newOpsDisabled = module->opsDisabled[scene].to_ullong();
    h->newForcedCarriers = module->forcedCarriers[scene].to_ullong();

    APP->history->push(h);
}

void RandomizeAllAlgorithmsItem::onAction(const rack::event::Action &e) {
    // History
    RandomizeAllAlgorithmsAction* h = new RandomizeAllAlgorithmsAction();
    h->moduleId = module->id;
    for (int scene = 0; scene < 3; scene++) {
        h->oldAlgorithm[scene] = module->algoName[scene].to_ullong();
        h->oldHorizontalMarks[scene] = module->horizontalMarks[scene].to_ullong();
        h->oldOpsDisabled[scene] = module->opsDisabled[scene].to_ullong();
        h->oldForcedCarriers[scene] = module->forcedCarriers[scene].to_ullong();
        module->randomizeAlgorithm(scene);
    }

    for (int scene = 0; scene < 3; scene++)
        module->randomizeAlgorithm(scene);
    module->graphDirty = true;

    for (int scene = 0; scene < 3; scene++) {
        h->newAlgorithm[scene] = module->algoName[scene].to_ullong();
        h->newHorizontalMarks[scene] = module->horizontalMarks[scene].to_ullong();
        h->newOpsDisabled[scene] = module->opsDisabled[scene].to_ullong();
        h->newForcedCarriers[scene] = module->forcedCarriers[scene].to_ullong();
    }

    APP->history->push(h);
}
