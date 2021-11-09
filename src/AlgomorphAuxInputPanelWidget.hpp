#pragma once
#include <rack.hpp>
using rack::math::Vec;
using rack::window::mm2px;


struct AlgomorphAuxInputPanelWidget : rack::widget::FramebufferWidget {
    struct AlgoDrawWidget : rack::widget::TransparentWidget {
        const Vec LABEL_BOUNDS[5] = {   mm2px(Vec(4.291, 13.374)).minus(box.pos),
                                        mm2px(Vec(4.291, 26.137)).minus(box.pos),
                                        mm2px(Vec(4.291, 38.901)).minus(box.pos),
                                        mm2px(Vec(4.291, 51.664)).minus(box.pos),
                                        mm2px(Vec(4.291, 64.428)).minus(box.pos)    };

        rack::engine::Module* module;
        std::string fontPath = "";
        std::shared_ptr<rack::window::Font> font;
        float textBounds[5] = {0.f};
   
        const NVGcolor DEF_TEXT_COLOR = nvgRGBA(0xcc, 0xcc, 0xcc, 0xff);

        NVGcolor textColor = DEF_TEXT_COLOR;
        float labelStroke = 0.5f;

        AlgoDrawWidget(rack::engine::Module* module);
        void drawLayer(const Widget::DrawArgs& args, int layer) override;
    };

    rack::engine::Module* module;
    AlgoDrawWidget* w;

    AlgomorphAuxInputPanelWidget(rack::engine::Module* module);
    void step() override;
};
