#include "split_packer.hpp"

PackedRectNode::PackedRectNode(int x, int y, int w, int h) : top_left_x(x), top_left_y(y), w(w), h(h) {}
Block::Block(int w, int h) : w(w), h(h) {}
SplitPacker::SplitPacker(int width, int height) { root = std::make_shared<PackedRectNode>(0, 0, width, height); }

std::shared_ptr<PackedRectNode> SplitPacker::find_node_with_enough_space_rec(std::shared_ptr<PackedRectNode> node,
                                                                             int width, int height) {
    if (!node)
        return nullptr;

    if (node->used) {
        auto right_node = find_node_with_enough_space_rec(node->right, width, height);
        return right_node ? right_node : find_node_with_enough_space_rec(node->left, width, height);
    } else if (width <= node->w && height <= node->h) {
        node->used = true;
        node->right =
            std::make_shared<PackedRectNode>(node->top_left_x + width, node->top_left_y, node->w - width, height);
        node->left =
            std::make_shared<PackedRectNode>(node->top_left_x, node->top_left_y + height, node->w, node->h - height);
        return node;
    } else {
        return nullptr;
    }
}

void SplitPacker::fit(std::vector<Block> &blocks) {
    for (auto &block : blocks) {
        fit(block);
    }
}

void SplitPacker::fit(Block &block) {
    auto node = find_node_with_enough_space_rec(root, block.w, block.h);
    if (node) {
        block.packed_placement = *node;
    } else {
        block.packed_placement = std::nullopt;
    }
}
