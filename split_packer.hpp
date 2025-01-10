#ifndef SPLIT_PACKER_HPP
#define SPLIT_PACKER_HPP

#include <vector>
#include <memory>
#include <optional>

struct PackedRectNode {
    int top_left_x, top_left_y, w, h;
    bool used = false;
    std::shared_ptr<PackedRectNode> left = nullptr;
    std::shared_ptr<PackedRectNode> right = nullptr;
    PackedRectNode(int x, int y, int w, int h);
};

class Block {
  public:
    int w, h;
    std::optional<PackedRectNode> packed_placement;
    Block(int w, int h);
};

class SplitPacker {
  public:
    SplitPacker(int width, int height);
    void fit(std::vector<Block> &blocks);
    void fit(Block &block);

  private:
    std::shared_ptr<PackedRectNode> root;
    std::shared_ptr<PackedRectNode> find_node_with_enough_space_rec(std::shared_ptr<PackedRectNode> root, int width,
                                                                    int height);
};

#endif // SPLIT_PACKER_HPP
