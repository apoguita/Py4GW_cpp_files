#pragma once
#include "Headers.h"


struct Node {
    uint32_t type; // XNode = 0, YNode = 1, SinkNode = 2
    uint32_t id;

    Node() : type(0), id(0) {}
};

struct XNode : Node {
    std::pair<float, float> pos; // Position (x, y)
    std::pair<float, float> dir; // Direction vector (x, y)
    uint32_t left_id;  // Left node ID
    uint32_t right_id; // Right node ID

    XNode() : pos({ 0.0f, 0.0f }), dir({ 0.0f, 0.0f }), left_id(UINT32_MAX), right_id(UINT32_MAX) {
        type = 0; // Set type to XNode
    }
};

struct YNode : Node {
    std::pair<float, float> pos; // Position (x, y)
    uint32_t left_id;  // Left node ID
    uint32_t right_id; // Right node ID

    YNode() : pos({ 0.0f, 0.0f }), left_id(UINT32_MAX), right_id(UINT32_MAX) {
        type = 1; // Set type to YNode
    }
};

struct SinkNode : Node {
    std::vector<uint32_t> trapezoid_ids; // IDs of associated trapezoids

    SinkNode() {
        type = 2; // Set type to SinkNode
    }
};

struct Portal {
    uint16_t left_layer_id;
    uint16_t right_layer_id;
    uint32_t h0004;
    uint32_t pair_index;  // Index of the paired portal
	uint32_t count; // Number of trapezoids connected by this portal
    std::vector<uint32_t> trapezoid_indices; // IDs of associated trapezoids

    Portal() : left_layer_id(0xFFFF), right_layer_id(0xFFFF), h0004(0), pair_index(UINT32_MAX) {}
};



