#pragma once
#include "Algomorph.hpp"
#include <vector>
#include <rack.hpp>
using rack::math::Vec;
using rack::ui::MenuItem;
using rack::event::Action;


struct Line {
	Vec left, right;
	Vec size;
	bool flipped;
	Line(Vec a, Vec b);
};

struct ConnectionBgWidget : rack::widget::OpaqueWidget {
	Algomorph* module;
	std::vector<Line> lines;

	ConnectionBgWidget(std::vector<Vec> left, std::vector<Vec> right, Algomorph* module);
	void draw(const Widget::DrawArgs& args) override;
	void onButton(const rack::event::Button& e) override;
	void createContextMenu();

};

struct InitializeCurrentAlgorithmItem : MenuItem {
    Algomorph* module;

    void onAction(const Action &e) override;
};

struct InitializeAllAlgorithmsItem : MenuItem {
    Algomorph* module;

    void onAction(const Action &e) override;
};

struct RandomizeCurrentAlgorithmItem : MenuItem {
    Algomorph* module;

    void onAction(const Action &e) override;
};

struct RandomizeAllAlgorithmsItem : MenuItem {
    Algomorph* module;

    void onAction(const Action &e) override;
};
