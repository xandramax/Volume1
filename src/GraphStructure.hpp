#pragma once

struct Node {
    Vec coords;
    int id;
};

struct Edge {
    Vec moveCoords;
    Vec curve[15][3];
    int curveLength = 0;

    bool operator> (const Edge &e);
    bool operator< (const Edge &e);
    bool operator>= (const Edge &e);
    bool operator<= (const Edge &e);
    bool operator== (const Edge &e);
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

	alGraph();
	alGraph(int graphId);
    bool operator> (const alGraph &g);
    bool operator< (const alGraph &g);
    bool operator>= (const alGraph &g);
    bool operator<= (const alGraph &g);
    bool operator== (const alGraph &g);
};
