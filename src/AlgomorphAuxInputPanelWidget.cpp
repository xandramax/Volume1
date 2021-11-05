#include "AlgomorphAuxInputPanelWidget.hpp"
#include "AlgomorphLarge.hpp"
#include "plugin.hpp" // For rack::pluginInstance


AlgomorphAuxInputPanelWidget::AlgoDrawWidget::AlgoDrawWidget(rack::engine::Module* module) {
    this->module = module;
    fontPath = "res/MiriamLibre-Regular.ttf"; 
}

void AlgomorphAuxInputPanelWidget::AlgoDrawWidget::drawLayer(const Widget::DrawArgs& args, int layer) {
    if (!module) return;

    if (layer == 1) {
        font = APP->window->loadFont(rack::asset::plugin(pluginInstance, fontPath));
        
        // Draw labels
        nvgBeginPath(args.vg);
        nvgFontSize(args.vg, 10.f);
        nvgFontFaceId(args.vg, font->handle);
        nvgFillColor(args.vg, textColor);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER);
        for (int i = 0; i < 5; i++) {
            std::string label = reinterpret_cast<AlgomorphLarge*>(module)->auxInput[i]->shortLabel;
            char const *id = label.c_str();
            nvgTextBounds(args.vg, LABEL_BOUNDS[i].x, LABEL_BOUNDS[i].y, id, id + label.length(), textBounds);
            float xOffset = 1.15f;//(textBounds[2] - textBounds[0]) / 2.f;
            float yOffset = -35.f;//(textBounds[3] - textBounds[1]) / 3.25f;
            nvgText(args.vg, LABEL_BOUNDS[i].x + xOffset, LABEL_BOUNDS[i].y + yOffset, id, id + label.length());
        }
    }

    TransparentWidget::drawLayer(args, layer);
}

AlgomorphAuxInputPanelWidget::AlgomorphAuxInputPanelWidget(rack::engine::Module* module) {
    this->module = module;
    w = new AlgoDrawWidget(module);
    addChild(w);
}

void AlgomorphAuxInputPanelWidget::step() {
    if (module && reinterpret_cast<AlgomorphLarge*>(module)->auxPanelDirty) {
        FramebufferWidget::dirty = true;
        w->box.size = box.size;
        reinterpret_cast<AlgomorphLarge*>(module)->auxPanelDirty = false;
    }
    FramebufferWidget::step();
}
