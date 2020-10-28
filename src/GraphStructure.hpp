#pragma once
#include "plugin.hpp"
#include "GraphData.hpp"

struct Node {
    Vec coords;
    int id;
};

struct Edge {
    Vec moveCoords;
    Vec curve[15][3];
    int curveLength = 0;

    bool operator> (const Edge &e) {
        return curveLength > e.curveLength; 
    }
    bool operator< (const Edge &e) {
        return curveLength < e.curveLength; 
    }
    bool operator>= (const Edge &e) {
        return curveLength >= e.curveLength; 
    }
    bool operator<= (const Edge &e) {
        return curveLength <= e.curveLength; 
    }
    bool operator== (const Edge &e) {
        return curveLength == e.curveLength; 
    }
};

struct Arrow { 
    Vec moveCoords;
    Vec lines[9];
};

struct alGraph {
    Node nodes[4];
    Edge edges[9];
    Arrow arrows[9];
    int numEdges = 0;
    int numNodes = 0;
    bool mystery = false;

	alGraph() {
		for (int i = 0; i < 4; i++) {
            int id = -xNodeData[0][i*2+1];
			nodes[id - 1].coords = Vec(xNodeData[0][i*2 + 2], yNodeData[0][i*2+2]);
        }
	}

	alGraph(int graphId) {
        for (int i = 0; i < 4; i++)
            nodes[i].id = 404;      // Initialize
		for (int i = 0; i < 4; i++) {
            int nodeId = -xNodeData[graphId][i*2+1];
            if (nodeId != 404) {
                numNodes++;
                nodes[nodeId - 1].id = nodeId;
                nodes[nodeId - 1].coords = Vec(xNodeData[graphId][i * 2 + 2], yNodeData[graphId][i + 1]);
            }
        }
        int curveDataIndex = 1;
        for (int i = 0; i < 9; i++) {
            if (moveCurveData[graphId][i].x != -404) {
                numEdges++;
                edges[i].moveCoords = moveCurveData[graphId][i];
                for (int j = 0; j < 15; j++) {
                    if (xCurveData[graphId][curveDataIndex] == -1) {
                        curveDataIndex++;
                        break;
                    }
                    else if (xCurveData[graphId][curveDataIndex] == -404) 
                        break;
                    else {
                        edges[i].curveLength++; 
                        for (int k = 0; k < 3; k++)
                            edges[i].curve[j][k] = Vec(xCurveData[graphId][curveDataIndex + k], yCurveData[graphId][curveDataIndex + k]);
                        curveDataIndex += 3;
                    }
                }
                arrows[i].moveCoords = Vec(xPolygonData[graphId][i * 10], yPolygonData[graphId][i * 10]);
                for (int j = 1; j < 10; j++) {
                    if (xPolygonData[graphId][j + i * 10] == -404)
                        break;
                    else
                        arrows[i].lines[j - 1] = Vec(xPolygonData[graphId][j + i * 10], yPolygonData[graphId][j + i * 10]);
                }
            }
            else
                break;
        }
	}

    bool operator> (const alGraph &g) {
        return numEdges > g.numEdges; 
    }
    bool operator< (const alGraph &g) {
        return numEdges < g.numEdges; 
    }
    bool operator>= (const alGraph &g) {
        return numEdges >= g.numEdges; 
    }
    bool operator<= (const alGraph &g) {
        return numEdges <= g.numEdges; 
    }
    bool operator== (const alGraph &g) {
        return numEdges == g.numEdges; 
    }
};
