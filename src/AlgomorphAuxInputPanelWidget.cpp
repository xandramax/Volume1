#include "AlgomorphLarge.hpp"
#include "AlgomorphAuxInputPanelWidget.hpp"

AlgomorphAuxInputPanelWidget::AlgoDrawWidget::AlgoDrawWidget(AlgomorphLarge* module) {
    this->module = module;
    fontPath = "res/MiriamLibre-Regular.ttf"; 
}

void AlgomorphAuxInputPanelWidget::AlgoDrawWidget::draw(const Widget::DrawArgs& args) {
    font = APP->window->loadFont(asset::plugin(pluginInstance, fontPath));
    if (!module) return;

    for (int i = 0; i < 5; i++) {
        int activeModes = module->auxInput[i]->activeModes;
        if (activeModes == 1)
            auxInputModes[i] = module->auxInput[i]->lastSetMode;
        else if (activeModes == 0)
            auxInputModes[i] = -1;
        else if (activeModes > 1) {
            auxInputModes[i] = -2;
        }
        else {
            //Error
            auxInputModes[i] = -3;
        }
    }
    
    nvgBeginPath(args.vg);

    // Draw numbers
    nvgBeginPath(args.vg);
    nvgFontSize(args.vg, 10.f);
    nvgFontFaceId(args.vg, font->handle);
    nvgFillColor(args.vg, textColor);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER);
    for (int i = 0; i < 5; i++) {
        if (auxInputModes[i] != -3) {
            std::string s;
            if (auxInputModes[i] > -1)
                s = AuxInputModeShortLabels[auxInputModes[i]];
            else if (auxInputModes[i] == -2)
                s = "MULTI";
            else if (auxInputModes[i] == -1)
                s = "NONE";
            else
                s = "ERROR";
            char const *id = s.c_str();
            nvgTextBounds(args.vg, LABEL_BOUNDS[i].x, LABEL_BOUNDS[i].y, id, id + s.length(), textBounds);
            float xOffset = 1.15f;//(textBounds[2] - textBounds[0]) / 2.f;
            float yOffset = -35.f;//(textBounds[3] - textBounds[1]) / 3.25f;
            nvgText(args.vg, LABEL_BOUNDS[i].x + xOffset, LABEL_BOUNDS[i].y + yOffset, id, id + s.length());
        }
    }        
}

AlgomorphAuxInputPanelWidget::AlgomorphAuxInputPanelWidget(AlgomorphLarge* module) {
    this->module = module;
    w = new AlgoDrawWidget(module);
    addChild(w);
}

void AlgomorphAuxInputPanelWidget::step() {
    if (module && module->auxPanelDirty) {
        FramebufferWidget::dirty = true;
        w->box.size = box.size;
        module->auxPanelDirty = false;
    }
    FramebufferWidget::step();
}