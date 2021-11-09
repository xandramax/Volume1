#pragma once


NVGcolor crossfadeColor(NVGcolor c1, NVGcolor c2, float t);

struct AlgomorphDisplayWidget : rack::widget::FramebufferWidget {
    struct AlgoDrawWidget : rack::app::LightWidget {
        Algomorph* module;
        alGraph graphs[3];
        int translatedAlgoName[3] = {0};
        std::bitset<4> horizontalMarks[3] = {0};
        std::bitset<4> forcedCarriers[3] = {0};
        int scene = 1;
        int morphScene = 1;
        float morph = 0.f;
        bool firstRun = true;
        std::string fontPath = "";
        std::shared_ptr<rack::window::Font> font;
        float textBounds[4];

        float xOrigin = box.size.x / 2.f;
        float yOrigin = box.size.y / 2.f;
   
        const NVGcolor NODE_FILL_COLOR = DLXMediumDarkPurple;
        const NVGcolor FEEDBACK_NODE_COLOR = DLXPurple;
        const NVGcolor NODE_STROKE_COLOR = nvgRGB(26, 26, 26);
        const NVGcolor TEXT_COLOR = nvgRGBA(0xb2, 0xa9, 0xb9, 0xff);
        const NVGcolor EDGE_COLOR = nvgRGBA(0x9a,0x9a,0x6f,0xff);

        NVGcolor nodeFillColor = NODE_FILL_COLOR;
        NVGcolor nodeStrokeColor = NODE_STROKE_COLOR;
        NVGcolor feedbackFillColor = FEEDBACK_NODE_COLOR;
        NVGcolor textColor = TEXT_COLOR;
        NVGcolor edgeColor = EDGE_COLOR;
        float borderStroke = 0.45f;
        float labelStroke = 0.5f;
        float nodeStroke = 0.75f;
        float radius = 8.35425f;
        float edgeStroke = 0.925f;
        static constexpr float arrowStroke1 = (2.65f/4.f) + (1.f/3.f);
        static constexpr float arrowStroke2 = (7.65f/4.f) + (1.f/3.f);

        AlgoDrawWidget(Algomorph* module);
        void drawNodes(NVGcontext* ctx, alGraph source, alGraph destination, float morph);
        void renderNodes(NVGcontext* ctx, alGraph mostNodes, alGraph leastNodes, float morph, bool flipped);
        void drawEdges(NVGcontext* ctx, alGraph source, alGraph destination, float morph);
        void renderEdges(NVGcontext* ctx, alGraph mostEdges, alGraph leastEdges, float morph, bool flipped);
        void reticulateEdge(NVGcontext* ctx, Edge mostCurved, Edge leastCurved, float morph, bool flipped);
        void reticulateArrow(NVGcontext* ctx, Arrow mostGregarious, Arrow leastGregarious, float morph, bool flipped);
        void drawLayer(const Widget::DrawArgs& args, int layer) override;
    };

    Algomorph* module;
    AlgoDrawWidget* w;

    AlgomorphDisplayWidget(Algomorph* module);
    void step() override;
    void draw(const DrawArgs& args) override;
};
