#include "plugin.hpp"
#include "GraphStructure.hpp"

bool Edge::operator> (const Edge &e) {
    return curveLength > e.curveLength; 
}
bool Edge::operator< (const Edge &e) {
    return curveLength < e.curveLength; 
}
bool Edge::operator>= (const Edge &e) {
    return curveLength >= e.curveLength; 
}
bool Edge::operator<= (const Edge &e) {
    return curveLength <= e.curveLength; 
}
bool Edge::operator== (const Edge &e) {
    return curveLength == e.curveLength; 
}

alGraph::alGraph() {
    for (int i = 0; i < 4; i++) {
        int id = -GRAPH_DATA.xNodeData[0][i*2+1];
        nodes[id - 1].coords = Vec(GRAPH_DATA.xNodeData[0][i*2 + 2], GRAPH_DATA.yNodeData[0][i*2+2]);
    }
}

alGraph::alGraph(int graphId) {
    for (int i = 0; i < 4; i++)
        nodes[i].id = 404;      // Initialize
    for (int i = 0; i < 4; i++) {
        int nodeId = -GRAPH_DATA.xNodeData[graphId][i*2+1];
        if (nodeId != 404) {
            numNodes++;
            nodes[nodeId - 1].id = nodeId;
            nodes[nodeId - 1].coords = Vec(GRAPH_DATA.xNodeData[graphId][i * 2 + 2], GRAPH_DATA.yNodeData[graphId][i + 1]);
        }
    }
    int curveDataIndex = 1;
    for (int i = 0; i < 9; i++) {
        if (GRAPH_DATA.moveCurveData[graphId][i].x != -404) {
            numEdges++;
            edges[i].moveCoords = GRAPH_DATA.moveCurveData[graphId][i];
            for (int j = 0; j < 15; j++) {
                if (GRAPH_DATA.xCurveData[graphId][curveDataIndex] == -1) {
                    curveDataIndex++;
                    break;
                }
                else if (GRAPH_DATA.xCurveData[graphId][curveDataIndex] == -404) 
                    break;
                else {
                    edges[i].curveLength++; 
                    for (int k = 0; k < 3; k++)
                        edges[i].curve[j][k] = Vec(GRAPH_DATA.xCurveData[graphId][curveDataIndex + k], GRAPH_DATA.yCurveData[graphId][curveDataIndex + k]);
                    curveDataIndex += 3;
                }
            }
            arrows[i].moveCoords = Vec(GRAPH_DATA.xPolygonData[graphId][i * 10], GRAPH_DATA.yPolygonData[graphId][i * 10]);
            for (int j = 1; j < 10; j++) {
                if (GRAPH_DATA.xPolygonData[graphId][j + i * 10] == -404)
                    break;
                else
                    arrows[i].lines[j - 1] = Vec(GRAPH_DATA.xPolygonData[graphId][j + i * 10], GRAPH_DATA.yPolygonData[graphId][j + i * 10]);
            }
        }
        else
            break;
    }
}

bool alGraph::operator> (const alGraph &g) {
    return numEdges > g.numEdges; 
}
bool alGraph::operator< (const alGraph &g) {
    return numEdges < g.numEdges; 
}
bool alGraph::operator>= (const alGraph &g) {
    return numEdges >= g.numEdges; 
}
bool alGraph::operator<= (const alGraph &g) {
    return numEdges <= g.numEdges; 
}
bool alGraph::operator== (const alGraph &g) {
    return numEdges == g.numEdges; 
}
