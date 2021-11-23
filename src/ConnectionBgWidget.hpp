#pragma once
#include "plugin.hpp" // For constants
#include "Algomorph.hpp"
#include <vector>
#include <rack.hpp>
using rack::math::Vec;
using rack::ui::MenuItem;
using rack::event::Action;
using rack::construct;


// Menu

template < int OPS = 4, int SCENES = 3 >
struct InitializeCurrentAlgorithmItem : MenuItem {
    Algomorph<OPS, SCENES>* module;

    void onAction(const Action &e) override {
		int scene = module->configMode ? module->configScene : module->centerMorphScene[0];

		// History
		InitializeCurrentAlgorithmAction<OPS, SCENES>* h = new InitializeCurrentAlgorithmAction<OPS, SCENES>;
		h->moduleId = module->id;
		h->scene = scene;
		h->oldAlgoName = module->algoName[scene].to_ullong();
		h->oldHorizontalMarks = module->horizontalMarks[scene].to_ullong();
		h->oldForcedCarriers = module->forcedCarriers[scene].to_ullong();
		h->oldOpsDisabled = module->opsDisabled[scene].to_ullong();

		module->initializeAlgorithm(scene);
		module->graphDirty = true;

		APP->history->push(h);
	};
};

template < int OPS = 4, int SCENES = 3 >
struct InitializeAllAlgorithmsItem : MenuItem {
    Algomorph<OPS, SCENES>* module;

    void onAction(const Action &e) override {
		// History
		InitializeAllAlgorithmsAction<OPS, SCENES>* h = new InitializeAllAlgorithmsAction<OPS, SCENES>;
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
	};
};

template < int OPS = 4, int SCENES = 3 >
struct RandomizeCurrentAlgorithmItem : MenuItem {
    Algomorph<OPS, SCENES>* module;

    void onAction(const Action &e) override {
		int scene = module->configMode ? module->configScene : module->centerMorphScene[0];

		RandomizeCurrentAlgorithmAction<OPS, SCENES>* h = new RandomizeCurrentAlgorithmAction<OPS, SCENES>();
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
	};
};

template < int OPS = 4, int SCENES = 3 >
struct RandomizeAllAlgorithmsItem : MenuItem {
    Algomorph<OPS, SCENES>* module;

    void onAction(const Action &e) override {
		// History
		RandomizeAllAlgorithmsAction<OPS, SCENES>* h = new RandomizeAllAlgorithmsAction<OPS, SCENES>();
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
	};
};


// Widget

struct Line {
	Vec left, right;
	Vec size;
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
		if (flipped) {
			left = Vec(RING_RADIUS * std::cos(startAngle) + a.x, RING_RADIUS * std::sin(startAngle) + a.y);
			right = Vec(b.x - RING_RADIUS * std::cos(startAngle),  b.y - RING_RADIUS * std::sin(startAngle));
		}
		else {
			left = Vec(RING_RADIUS * std::cos(endAngle) + a.x, RING_RADIUS * std::sin(endAngle) + a.y);
			right = Vec(b.x - RING_RADIUS * std::cos(endAngle),  b.y - RING_RADIUS * std::sin(endAngle));
		}
	};
};

template < int OPS = 4, int SCENES = 3 >
struct ConnectionBgWidget : rack::widget::OpaqueWidget {
	Algomorph<OPS, SCENES>* module;
	std::vector<Line> lines;

	ConnectionBgWidget(std::vector<Vec> left, std::vector<Vec> right, Algomorph<OPS, SCENES>* module) {
		this->module = module;	
		for (unsigned i = 0; i < left.size(); i++) {
			for (unsigned j = 0; j < right.size(); j++) {
				lines.push_back(Line(left[i], right[j]));
			}
		}
	};
	void draw(const Widget::DrawArgs& args) override {
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
	};

	void onButton(const rack::event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			createContextMenu();
			e.consume(this);
		}
		OpaqueWidget::onButton(e);
	};

	void createContextMenu() {
		std::string currentAlgo = std::to_string((module->configMode ? module->configScene : module->centerMorphScene[0]) + 1);

		rack::ui::Menu* menu = rack::createMenu();
		menu->addChild(construct<RandomizeCurrentAlgorithmItem<OPS, SCENES>>(&MenuItem::text, "Randomize Algorithm " +  currentAlgo, &RandomizeCurrentAlgorithmItem<OPS, SCENES>::module, module));
		menu->addChild(construct<InitializeCurrentAlgorithmItem<OPS, SCENES>>(&MenuItem::text, "Initialize Algorithm " + currentAlgo, &InitializeCurrentAlgorithmItem<OPS, SCENES>::module, module));
		menu->addChild(new rack::ui::MenuSeparator());
		menu->addChild(construct<RandomizeAllAlgorithmsItem<OPS, SCENES>>(&MenuItem::text, "Randomize All Algorithms", &RandomizeAllAlgorithmsItem<OPS, SCENES>::module, module));
		menu->addChild(construct<InitializeAllAlgorithmsItem<OPS, SCENES>>(&MenuItem::text, "Initialize All Algorithms", &InitializeAllAlgorithmsItem<OPS, SCENES>::module, module));
	};
};
