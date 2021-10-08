#include "AlgomorphLarge.hpp"

struct AlgomorphAuxInputPanelWidget : FramebufferWidget {
    struct AlgoDrawWidget : LightWidget {
        const Vec LABEL_BOUNDS[5] = {   mm2px(Vec(4.291, 14.643)).minus(box.pos),
                                        mm2px(Vec(4.291, 26.682)).minus(box.pos),
                                        mm2px(Vec(4.291, 39.159)).minus(box.pos),
                                        mm2px(Vec(4.291, 51.634)).minus(box.pos),
                                        mm2px(Vec(4.291, 64.178)).minus(box.pos)    };

        AlgomorphLarge* module;
        int auxInputModes[5] = {0};
        std::string fontPath = "";
        std::shared_ptr<Font> font;
        float textBounds[5] = {0.f};
   
        const NVGcolor TEXT_COLOR = nvgRGBA(0xcc, 0xcc, 0xcc, 0xff);

        NVGcolor textColor = TEXT_COLOR;
        float labelStroke = 0.5f;

        AlgoDrawWidget(AlgomorphLarge* module);
        void draw(const Widget::DrawArgs& args) override;
    };

    AlgomorphLarge* module;
    AlgoDrawWidget* w;

    AlgomorphAuxInputPanelWidget(AlgomorphLarge* module);
    void step() override;
};
