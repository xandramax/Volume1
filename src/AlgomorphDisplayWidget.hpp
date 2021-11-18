#pragma once
#include "plugin.hpp"
#include "Algomorph.hpp"
#include "AlgomorphDisplayWidget.hpp"
#include <rack.hpp>
using rack::math::crossfade;


template < int OPS = 4, int SCENES = 3 >
struct AlgomorphDisplayWidget : rack::widget::FramebufferWidget {
    struct AlgoDrawWidget : rack::app::LightWidget {
        Algomorph<OPS, SCENES>* module;
        alGraph graphs[SCENES];
        int translatedAlgoName[SCENES] = {0};
        std::bitset<OPS> horizontalMarks[SCENES] = {0};
        std::bitset<OPS> forcedCarriers[SCENES] = {0};
        int scene = std::floor((OPS + 1)/ 2.f);
        int morphScene = scene;
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
        const NVGcolor TEXT_COLOR = DLXExtraLightPurple;
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

        AlgoDrawWidget(Algomorph<OPS, SCENES>* module) {
            this->module = module;
            fontPath = "res/MiriamLibre-Regular.ttf";
        };

        void drawNodes(NVGcontext* ctx, alGraph source, alGraph destination, float morph) {
            if (source.numNodes >= destination.numNodes)
                renderNodes(ctx, source, destination, morph, false);
            else
                renderNodes(ctx, destination, source, morph, true);
        };

        void renderNodes(NVGcontext* ctx, alGraph mostNodes, alGraph leastNodes, float morph, bool flipped) {
            for (int op = 0; op < OPS; op++) {
                Node nodes[2];
                float nodeX[2] = {0.f};
                float nodeY[2] = {0.f};
                float nodeAlpha[2] = {0.f};
                float textAlpha[2] = {0.f};
                nodeFillColor = NODE_FILL_COLOR;
                feedbackFillColor = FEEDBACK_NODE_COLOR;
                nodeStrokeColor = NODE_STROKE_COLOR;
                textColor = TEXT_COLOR;
                bool draw = true;

                nvgBeginPath(ctx);
                if (mostNodes.nodes[op].id == 404) {
                    if (leastNodes.nodes[op].id == 404)
                        draw = false;
                    else {
                        nodes[0] = leastNodes.nodes[op];
                        nodes[1] = leastNodes.nodes[op];
                        if (!flipped) {
                            nodeAlpha[0] = 0.f;
                            textAlpha[0] = 0.f;
                            nodeAlpha[1] = NODE_FILL_COLOR.a;
                            textAlpha[1] = TEXT_COLOR.a;
                            nodeX[0] = xOrigin;
                            nodeY[0] = yOrigin;
                            nodeX[1] = nodes[1].coords.x;
                            nodeY[1] = nodes[1].coords.y;
                        }
                        else {
                            nodeAlpha[0] = NODE_FILL_COLOR.a;
                            textAlpha[0] = TEXT_COLOR.a;
                            nodeAlpha[1] = 0.f;
                            textAlpha[1] = 0.f;
                            nodeX[0] = nodes[0].coords.x;
                            nodeY[0] = nodes[0].coords.y;
                            nodeX[1] = xOrigin;
                            nodeY[1] = yOrigin;
                        }
                    }
                }
                else if (leastNodes.nodes[op].id == 404) {
                    nodes[0] = mostNodes.nodes[op];
                    nodes[1] = mostNodes.nodes[op];
                    if (!flipped) {
                        nodeAlpha[0] = NODE_FILL_COLOR.a;
                        textAlpha[0] = TEXT_COLOR.a;
                        nodeAlpha[1] = 0.f;
                        textAlpha[1] = 0.f;
                        nodeX[0] = nodes[0].coords.x;
                        nodeY[0] = nodes[0].coords.y;
                        nodeX[1] = xOrigin;
                        nodeY[1] = yOrigin;
                    }
                    else {
                        nodeAlpha[0] = 0.f;
                        textAlpha[0] = 0.f;
                        nodeAlpha[1] = NODE_FILL_COLOR.a;
                        textAlpha[1] = TEXT_COLOR.a;
                        nodeX[0] = xOrigin;
                        nodeY[0] = yOrigin;
                        nodeX[1] = nodes[0].coords.x;
                        nodeY[1] = nodes[0].coords.y;
                    }
                }
                else {
                    if (!flipped) {
                        nodes[0] = mostNodes.nodes[op];
                        nodes[1] = leastNodes.nodes[op];
                    }
                    else {
                        nodes[0] = leastNodes.nodes[op];
                        nodes[1] = mostNodes.nodes[op];
                    }
                    nodeAlpha[0] = NODE_FILL_COLOR.a;
                    textAlpha[0] = TEXT_COLOR.a;
                    nodeAlpha[1] = NODE_FILL_COLOR.a;
                    textAlpha[1] = TEXT_COLOR.a;
                    nodeX[0] = nodes[0].coords.x;
                    nodeX[1] = nodes[1].coords.x;
                    nodeY[0] = nodes[0].coords.y;
                    nodeY[1] = nodes[1].coords.y;
                }
                if (draw) {
                    nodeFillColor.a = crossfade(nodeAlpha[0], nodeAlpha[1], morph);
                    nodeStrokeColor.a = crossfade(nodeAlpha[0], nodeAlpha[1], morph);
                    textColor.a = crossfade(textAlpha[0], textAlpha[1], morph);

                    nvgCircle(ctx,  crossfade(nodeX[0], nodeX[1], morph),
                                    crossfade(nodeY[0], nodeY[1], morph),
                                    radius);
                    if (module->modeB && (horizontalMarks[scene].test(op) || horizontalMarks[morphScene].test(op))) {
                        feedbackFillColor.a = crossfade(nodeAlpha[0], nodeAlpha[1], morph);
                        NVGcolor sceneColor = horizontalMarks[scene].test(op) ? feedbackFillColor : nodeFillColor;
                        NVGcolor morphColor = horizontalMarks[morphScene].test(op) ? feedbackFillColor : nodeFillColor;
                        NVGcolor fillColor = crossfadeColor(sceneColor, morphColor, morph);
                        nvgFillColor(ctx, fillColor);
                    }
                    else
                        nvgFillColor(ctx, nodeFillColor);
                    nvgFill(ctx);
                    nvgStrokeColor(ctx, nodeStrokeColor);
                    nvgStrokeWidth(ctx, nodeStroke);
                    nvgStroke(ctx);

                    bool sceneCarrierValue = forcedCarriers[scene].test(op) && !graphs[scene].mystery;
                    bool morphCarrierValue = forcedCarriers[morphScene].test(op) && !graphs[morphScene].mystery;
                    if (sceneCarrierValue || morphCarrierValue)  {
                        float xOffset = module->rotor.getXoffset(radius);
                        float yOffset = module->rotor.getYoffset(radius);
                        nvgBeginPath(ctx);
                        nvgCircle(ctx,  crossfade(nodeX[0] + xOffset, nodeX[1] + xOffset, morph),
                                        crossfade(nodeY[0] + yOffset, nodeY[1] + yOffset, morph),
                                        radius / 10.f);
                        NVGcolor carrierColor = color::mult(DLXExtraLightPurple, crossfade(sceneCarrierValue, morphCarrierValue, morph));
                        nvgFillColor(ctx, carrierColor);
                        nvgFill(ctx);
                    }

                    nvgBeginPath(ctx);
                    nvgFontSize(ctx, 11.f);
                    nvgFontFaceId(ctx, font->handle);
                    nvgFillColor(ctx, textColor);
                    std::string s = std::to_string(op + 1);
                    char const *id = s.c_str();
                    nvgTextBounds(ctx, nodes[0].coords.x, nodes[0].coords.y, id, id + 1, textBounds);
                    float xOffset = (textBounds[2] - textBounds[0]) / 2.f;
                    float yOffset = (textBounds[3] - textBounds[1]) / 3.25f;
                    nvgText(ctx,    crossfade(nodeX[0], nodeX[1], morph) - xOffset,
                                    crossfade(nodeY[0], nodeY[1], morph) + yOffset,
                                    id, id + 1);
                }
            }
        };

        void drawEdges(NVGcontext* ctx, alGraph source, alGraph destination, float morph) {
            if (source >= destination)
                renderEdges(ctx, source, destination, morph, false);
            else
                renderEdges(ctx, destination, source, morph, true);
        };

        void renderEdges(NVGcontext* ctx, alGraph mostEdges, alGraph leastEdges, float morph, bool flipped) {
            for (int i = 0; i < mostEdges.numEdges; i++) {
                Edge edge[2];
                Arrow arrow[2];
                nvgBeginPath(ctx);
                if (leastEdges.numEdges == 0) {
                    if (!flipped) {
                        edge[0] = mostEdges.edges[i];
                        edge[1] = leastEdges.edges[i];
                        nvgMoveTo(ctx, crossfade(edge[0].moveCoords.x, xOrigin, morph), crossfade(edge[0].moveCoords.y, yOrigin, morph));
                        edgeColor.a = crossfade(EDGE_COLOR.a, 0x00, morph);
                    }
                    else {
                        edge[0] = leastEdges.edges[i];
                        edge[1] = mostEdges.edges[i];
                        nvgMoveTo(ctx, crossfade(xOrigin, edge[1].moveCoords.x, morph), crossfade(yOrigin, edge[1].moveCoords.y, morph));
                        edgeColor.a = crossfade(0x00, EDGE_COLOR.a, morph);
                    }
                    arrow[0] = mostEdges.arrows[i];
                    arrow[1] = leastEdges.arrows[i];
                }
                else if (i < leastEdges.numEdges) {
                    if (!flipped) {
                        edge[0] = mostEdges.edges[i];
                        edge[1] = leastEdges.edges[i];
                    }
                    else {
                        edge[0] = leastEdges.edges[i];
                        edge[1] = mostEdges.edges[i];
                    }
                    nvgMoveTo(ctx, crossfade(edge[0].moveCoords.x, edge[1].moveCoords.x, morph), crossfade(edge[0].moveCoords.y, edge[1].moveCoords.y, morph));
                    edgeColor = EDGE_COLOR;
                    arrow[0] = mostEdges.arrows[i];
                    arrow[1] = leastEdges.arrows[i];
                }
                else {
                    if (!flipped) {
                        edge[0] = mostEdges.edges[i];
                        edge[1] = leastEdges.edges[std::max(0, leastEdges.numEdges - 1)];
                    }
                    else {
                        edge[0] = leastEdges.edges[std::max(0, leastEdges.numEdges - 1)];
                        edge[1] = mostEdges.edges[i];
                    }
                    nvgMoveTo(ctx, crossfade(edge[0].moveCoords.x, edge[1].moveCoords.x, morph), crossfade(edge[0].moveCoords.y, edge[1].moveCoords.y, morph));
                    edgeColor = EDGE_COLOR;
                    arrow[0] = mostEdges.arrows[i];
                    arrow[1] = leastEdges.arrows[std::max(0, leastEdges.numEdges - 1)];
                }
                if (edge[0] >= edge[1]) {
                    reticulateEdge(ctx, edge[0], edge[1], morph, false);
                }
                else {
                    reticulateEdge(ctx, edge[1], edge[0], morph, true);
                }

                nvgStrokeColor(ctx, edgeColor);
                nvgStrokeWidth(ctx, edgeStroke);
                nvgStroke(ctx);

                nvgBeginPath(ctx);
                reticulateArrow(ctx, arrow[0], arrow[1], morph, flipped);
                nvgFillColor(ctx, edgeColor);
                nvgFill(ctx);
                nvgStrokeColor(ctx, edgeColor);
                nvgStrokeWidth(ctx, arrowStroke1);
                nvgStroke(ctx);
            }
        };

        void reticulateEdge(NVGcontext* ctx, Edge mostCurved, Edge leastCurved, float morph, bool flipped) {
            for (int j = 0; j < mostCurved.curveLength; j++) {
                if (leastCurved.curveLength == 0) {
                    if (!flipped)
                        nvgBezierTo(ctx, crossfade(mostCurved.curve[j][0].x, xOrigin, morph), crossfade(mostCurved.curve[j][0].y, yOrigin, morph), crossfade(mostCurved.curve[j][1].x, xOrigin, morph), crossfade(mostCurved.curve[j][1].y, yOrigin, morph), crossfade(mostCurved.curve[j][2].x, xOrigin, morph), crossfade(mostCurved.curve[j][2].y, yOrigin, morph));
                    else
                        nvgBezierTo(ctx, crossfade(xOrigin, mostCurved.curve[j][0].x, morph), crossfade(yOrigin, mostCurved.curve[j][0].y, morph), crossfade(xOrigin, mostCurved.curve[j][1].x, morph), crossfade(yOrigin, mostCurved.curve[j][1].y, morph), crossfade(xOrigin, mostCurved.curve[j][2].x, morph), crossfade(yOrigin, mostCurved.curve[j][2].y, morph));
                }
                else if (j < leastCurved.curveLength) {
                    if (!flipped)
                        nvgBezierTo(ctx, crossfade(mostCurved.curve[j][0].x, leastCurved.curve[j][0].x, morph), crossfade(mostCurved.curve[j][0].y, leastCurved.curve[j][0].y, morph), crossfade(mostCurved.curve[j][1].x, leastCurved.curve[j][1].x, morph), crossfade(mostCurved.curve[j][1].y, leastCurved.curve[j][1].y, morph), crossfade(mostCurved.curve[j][2].x, leastCurved.curve[j][2].x, morph), crossfade(mostCurved.curve[j][2].y, leastCurved.curve[j][2].y, morph));
                    else
                        nvgBezierTo(ctx, crossfade(leastCurved.curve[j][0].x, mostCurved.curve[j][0].x, morph), crossfade(leastCurved.curve[j][0].y, mostCurved.curve[j][0].y, morph), crossfade(leastCurved.curve[j][1].x, mostCurved.curve[j][1].x, morph), crossfade(leastCurved.curve[j][1].y, mostCurved.curve[j][1].y, morph), crossfade(leastCurved.curve[j][2].x, mostCurved.curve[j][2].x, morph), crossfade(leastCurved.curve[j][2].y, mostCurved.curve[j][2].y, morph));
                }
                else {
                    if (!flipped)
                        nvgBezierTo(ctx, crossfade(mostCurved.curve[j][0].x, leastCurved.curve[leastCurved.curveLength - 1][0].x, morph), crossfade(mostCurved.curve[j][0].y, leastCurved.curve[leastCurved.curveLength - 1][0].y, morph), crossfade(mostCurved.curve[j][1].x, leastCurved.curve[leastCurved.curveLength - 1][1].x, morph), crossfade(mostCurved.curve[j][1].y, leastCurved.curve[leastCurved.curveLength - 1][1].y, morph), crossfade(mostCurved.curve[j][2].x, leastCurved.curve[leastCurved.curveLength - 1][2].x, morph), crossfade(mostCurved.curve[j][2].y, leastCurved.curve[leastCurved.curveLength - 1][2].y, morph));
                    else
                        nvgBezierTo(ctx, crossfade(leastCurved.curve[leastCurved.curveLength - 1][0].x, mostCurved.curve[j][0].x, morph), crossfade(leastCurved.curve[leastCurved.curveLength - 1][0].y, mostCurved.curve[j][0].y, morph), crossfade(leastCurved.curve[leastCurved.curveLength - 1][1].x, mostCurved.curve[j][1].x, morph), crossfade(leastCurved.curve[leastCurved.curveLength - 1][1].y, mostCurved.curve[j][1].y, morph), crossfade(leastCurved.curve[leastCurved.curveLength - 1][2].x, mostCurved.curve[j][2].x, morph), crossfade(leastCurved.curve[leastCurved.curveLength - 1][2].y, mostCurved.curve[j][2].y, morph));
                }
            }
        };

        void reticulateArrow(NVGcontext* ctx, Arrow mostGregarious, Arrow leastGregarious, float morph, bool flipped) {
            if (leastGregarious.moveCoords.x == 0) {
                if (!flipped)
                    nvgMoveTo(ctx, crossfade(mostGregarious.moveCoords.x, xOrigin, morph), crossfade(mostGregarious.moveCoords.y, yOrigin, morph));
                else
                    nvgMoveTo(ctx, crossfade(xOrigin, mostGregarious.moveCoords.x, morph), crossfade(yOrigin, mostGregarious.moveCoords.y, morph));
                for (int j = 0; j < 9; j++) {
                    if (!flipped)
                        nvgLineTo(ctx, crossfade(mostGregarious.lines[j].x, xOrigin, morph), crossfade(mostGregarious.lines[j].y, yOrigin, morph));
                    else
                        nvgLineTo(ctx, crossfade(xOrigin, mostGregarious.lines[j].x, morph), crossfade(yOrigin, mostGregarious.lines[j].y, morph));
                }
            }
            else {
                if (!flipped)
                    nvgMoveTo(ctx, crossfade(mostGregarious.moveCoords.x, leastGregarious.moveCoords.x, morph), crossfade(mostGregarious.moveCoords.y, leastGregarious.moveCoords.y, morph));
                else
                    nvgMoveTo(ctx, crossfade(leastGregarious.moveCoords.x, mostGregarious.moveCoords.x, morph), crossfade(leastGregarious.moveCoords.y, mostGregarious.moveCoords.y, morph));
                for (int j = 0; j < 9; j++) {
                    if (!flipped)
                        nvgLineTo(ctx, crossfade(mostGregarious.lines[j].x, leastGregarious.lines[j].x, morph), crossfade(mostGregarious.lines[j].y, leastGregarious.lines[j].y, morph));
                    else
                        nvgLineTo(ctx, crossfade(leastGregarious.lines[j].x, mostGregarious.lines[j].x, morph), crossfade(leastGregarious.lines[j].y, mostGregarious.lines[j].y, morph));
                }
            }
        };

        void drawLayer(const Widget::DrawArgs& args, int layer) override {
            if (!module) return;

            if (layer == 1) {
                font = APP->window->loadFont(rack::asset::plugin(pluginInstance, fontPath));

                //Origin must be updated
                xOrigin = box.size.x / 2.f;
                yOrigin = box.size.y / 2.f;

                for (int scene = 0; scene < SCENES; scene++) {
                    if (!module->displayAlgoName[scene].empty()) {
                        translatedAlgoName[scene] = module->graphAddressTranslation[module->displayAlgoName[scene].shift().to_ullong()];
                        if (translatedAlgoName[scene] != -1)
                            graphs[scene] = alGraph(translatedAlgoName[scene]);
                        else {
                            graphs[scene] = alGraph(1979);
                            graphs[scene].mystery = true;
                        }
                    }
                    if (!module->displayHorizontalMarks[scene].empty())
                        horizontalMarks[scene] = module->displayHorizontalMarks[scene].shift();
                    if (!module->displayForcedCarriers[scene].empty())
                        forcedCarriers[scene] = module->displayForcedCarriers[scene].shift();
                }

                if (!module->displayScene.empty()) {
                    scene = module->displayScene.shift();
                    if (scene != -1) {
                        if (!module->displayMorphScene.empty())
                            morphScene = module->displayMorphScene.shift();
                        if (!module->displayMorph.empty())
                            morph = module->displayMorph.shift();
                    }
                }
                nvgBeginPath(args.vg);
                nvgRoundedRect(args.vg, box.getTopLeft().x, box.getTopLeft().y, box.size.x, box.size.y, 3.675f);
                nvgStrokeWidth(args.vg, borderStroke);
                nvgStroke(args.vg);

                if (module->configMode) {   //Display state without morph
                    if (graphs[scene].numNodes > 0) {
                        // Draw nodes
                        if (module->modeB && horizontalMarks[scene].any()){
                            for (int op = 0; op < OPS; op++) {
                                if (graphs[scene].nodes[op].id != 404) {
                                    nvgBeginPath(args.vg);
                                    nvgCircle(args.vg, graphs[scene].nodes[op].coords.x, graphs[scene].nodes[op].coords.y, radius);
                                    if (horizontalMarks[scene].test(op))
                                        nvgFillColor(args.vg, feedbackFillColor);
                                    else
                                        nvgFillColor(args.vg, nodeFillColor);
                                    nvgFill(args.vg);
                                    nvgStrokeColor(args.vg, nodeStrokeColor);
                                    nvgStrokeWidth(args.vg, nodeStroke);
                                    nvgStroke(args.vg);
                                }
                            }
                        }
                        else {
                            nvgBeginPath(args.vg);
                            for (int op = 0; op < OPS; op++) {
                                if (graphs[scene].nodes[op].id != 404)
                                    nvgCircle(args.vg, graphs[scene].nodes[op].coords.x, graphs[scene].nodes[op].coords.y, radius);
                            }
                            nvgFillColor(args.vg, nodeFillColor);
                            nvgFill(args.vg);
                            nvgStrokeColor(args.vg, nodeStrokeColor);
                            nvgStrokeWidth(args.vg, nodeStroke);
                            nvgStroke(args.vg);
                        }

                        if (forcedCarriers[scene].any()) {
                            float xOffset = module->rotor.getXoffset(radius);
                            float yOffset = module->rotor.getYoffset(radius);
                            nvgBeginPath(args.vg);
                            for (int op = 0; op < 4; op++) {
                                if (forcedCarriers[scene].test(op)) {
                                    nvgCircle(args.vg,  graphs[scene].nodes[op].coords.x + xOffset,
                                                    graphs[scene].nodes[op].coords.y + yOffset,
                                                    radius / 10.f);
                                }
                            }
                            nvgFillColor(args.vg, DLXExtraLightPurple);
                            nvgFill(args.vg);
                        }

                        // Draw numbers
                        nvgBeginPath(args.vg);
                        nvgFontSize(args.vg, 11.f);
                        nvgFontFaceId(args.vg, font->handle);
                        nvgFillColor(args.vg, textColor);
                        for (int op = 0; op < 4; op++) {
                            if (graphs[scene].nodes[op].id != 404) {
                                std::string s = std::to_string(op + 1);
                                char const *id = s.c_str();
                                nvgTextBounds(args.vg, graphs[scene].nodes[op].coords.x, graphs[scene].nodes[op].coords.y, id, id + 1, textBounds);
                                float xOffset = (textBounds[2] - textBounds[0]) / 2.f;
                                float yOffset = (textBounds[3] - textBounds[1]) / 3.25f;
                                nvgText(args.vg, graphs[scene].nodes[op].coords.x - xOffset, graphs[scene].nodes[op].coords.y + yOffset, id, id + 1);
                            }
                        }
                    }
                }
                else {
                    // Draw nodes and numbers
                    nvgBeginPath(args.vg);
                    drawNodes(args.vg, graphs[scene], graphs[morphScene], morph);
                }

                // Draw error display
                if (module->configMode) {
                    if (graphs[scene].mystery) {
                        // Draw question mark
                        nvgBeginPath(args.vg);
                        nvgFontSize(args.vg, 92.f);
                        nvgFontFaceId(args.vg, font->handle);
                        textColor = TEXT_COLOR;
                        nvgFillColor(args.vg, textColor);
                        std::string s = "?";
                        char const *id = s.c_str();
                        nvgTextBounds(args.vg, xOrigin, yOrigin, id, id + 1, textBounds);
                        float xOffset = (textBounds[2] - textBounds[0]) / 2.f + 1.f;
                        float yOffset = (textBounds[3] - textBounds[1]) / 3.925f + 1.f;
                        nvgText(args.vg, xOrigin - xOffset, yOrigin + yOffset, id, id + 1);

                        // Draw message
                        nvgBeginPath(args.vg);
                        nvgFontSize(args.vg, 11.f);
                        nvgFontFaceId(args.vg, font->handle);
                        textColor = TEXT_COLOR;
                        nvgFillColor(args.vg, textColor);
                        s = "cannot visualize";
                        id = s.c_str();
                        nvgTextBounds(args.vg, xOrigin, yOrigin, id, id + s.length(), textBounds);
                        xOffset = (textBounds[2] - textBounds[0]) / 2.f - 0.5f;
                        yOffset = (textBounds[3] - textBounds[1]) * 1.315f;
                        nvgText(args.vg, xOrigin - xOffset, yOffset, id, id + s.length());
                        s = "no natural carrier";
                        id = s.c_str();
                        nvgTextBounds(args.vg, xOrigin, yOrigin, id, id + s.length(), textBounds);
                        xOffset = (textBounds[2] - textBounds[0]) / 2.f - 0.5f;
                        yOffset = (textBounds[3] - textBounds[1]) / 1.345f;
                        nvgText(args.vg, xOrigin - xOffset, this->getBox().getBottom() - yOffset, id, id + s.length());
                    }
                }
                else {
                    if (graphs[scene].mystery || graphs[morphScene].mystery) {
                        nvgBeginPath(args.vg);
                        nvgFontSize(args.vg, 92.f);
                        nvgFontFaceId(args.vg, font->handle);
                        if (graphs[scene].mystery && graphs[morphScene].mystery)
                            textColor = TEXT_COLOR;
                        else if (graphs[scene].mystery)
                            textColor.a = crossfade(TEXT_COLOR.a, 0x00, morph);
                        else
                            textColor.a = crossfade(0x00, TEXT_COLOR.a, morph);
                        nvgFillColor(args.vg, textColor);
                        std::string s = "?";
                        char const *id = s.c_str();
                        nvgTextBounds(args.vg, xOrigin, yOrigin, id, id + 1, textBounds);
                        float xOffset = (textBounds[2] - textBounds[0]) / 2.f + 1.f;
                        float yOffset = (textBounds[3] - textBounds[1]) / 3.925f + 1.f;
                        nvgText(args.vg, xOrigin - xOffset, yOrigin + yOffset, id, id + 1);
                    }
                }

                // Draw edges +/ arrows
                if (module->configMode) {
                    // Draw edges
                    nvgBeginPath(args.vg);
                    for (int i = 0; i < graphs[scene].numEdges; i++) {
                        Edge edge = graphs[scene].edges[i];
                        nvgMoveTo(args.vg, edge.moveCoords.x, edge.moveCoords.y);
                        for (int j = 0; j < edge.curveLength; j++) {
                            nvgBezierTo(args.vg, edge.curve[j][0].x, edge.curve[j][0].y, edge.curve[j][1].x, edge.curve[j][1].y, edge.curve[j][2].x, edge.curve[j][2].y);
                        }
                    }
                    edgeColor = EDGE_COLOR;
                    nvgStrokeColor(args.vg, edgeColor);
                    nvgStrokeWidth(args.vg, edgeStroke);
                    nvgStroke(args.vg);
                    // Draw arrows
                    for (int i = 0; i < graphs[scene].numEdges; i++) {
                        nvgBeginPath(args.vg);
                        nvgMoveTo(args.vg, graphs[scene].arrows[i].moveCoords.x, graphs[scene].arrows[i].moveCoords.y);
                        for (int j = 0; j < 9; j++)
                            nvgLineTo(args.vg, graphs[scene].arrows[i].lines[j].x, graphs[scene].arrows[i].lines[j].y);
                        edgeColor = EDGE_COLOR;
                        nvgFillColor(args.vg, edgeColor);
                        nvgFill(args.vg);
                        nvgStrokeColor(args.vg, edgeColor);
                        nvgStrokeWidth(args.vg, arrowStroke1);
                        nvgStroke(args.vg);
                    }
                }
                else {
                    // Draw edges AND arrows
                    drawEdges(args.vg, graphs[scene], graphs[morphScene], morph);
                }
            }

            LightWidget::drawLayer(args, layer);
        };
    };

    Algomorph<OPS, SCENES>* module;
    AlgoDrawWidget* w;

    AlgomorphDisplayWidget(Algomorph<OPS, SCENES>* module) {
        this->module = module;
        w = new AlgoDrawWidget(module);
        addChild(w);
    };

    void step() override {
        if (module && module->graphDirty) {
            FramebufferWidget::dirty = true;
            w->box.size = box.size;
            module->graphDirty = false;
        }
        FramebufferWidget::step();
    };
};
