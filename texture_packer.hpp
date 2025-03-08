#ifndef TEXTURE_PACKER_HPP
#define TEXTURE_PACKER_HPP

#include <map>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <set>
#include <unordered_set>
#include <vector>
#include <string>
#include <filesystem>
#include <glm/glm.hpp>

#include <iostream>
#include <vector>
#include <map>

#include "sbpt_generated_includes.hpp"
#include "split_packer.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

/// new VVV

struct TextureBlock {
    TextureBlock(int width, int height, const std::string &file) : block(width, height), texture_path(file) {};
    Block block;
    std::string texture_path;
    std::map<std::string, std::map<std::string, float>> subtextures;
};

struct PackedTextureContainer {
    std::shared_ptr<SplitPacker> packer;
    std::vector<TextureBlock> packed_texture_blocks;
};

/// new ^^^

// TODO was working on consructing a function which gives back you texture index
// rename texture index to packed texture index bounding shit
// and then in main use that and store that data into IVPTP shit and then
// when drawing upload that extra data and things should work? try it out on gypsum factor.

struct PackedTextureSubTexture {
    int packed_texture_bounding_box_index;
    int packed_texture_index;
    std::vector<glm::vec2> texture_coordinates;
    std::map<std::string, PackedTextureSubTexture> sub_atlas;
    int top_left_x;
    int top_left_y;
    int width;
    int height;

    friend std::ostream &operator<<(std::ostream &os, const PackedTextureSubTexture &pts) {
        os << "PackedTextureSubTexture {"
           << "\n  Bounding Box Index: " << pts.packed_texture_bounding_box_index
           << "\n  Texture Index: " << pts.packed_texture_index << "\n  Top Left: (" << pts.top_left_x << ", "
           << pts.top_left_y << ")"
           << "\n  Size: " << pts.width << "x" << pts.height << "\n  Texture Coordinates: [";

        for (size_t i = 0; i < pts.texture_coordinates.size(); ++i) {
            os << "(" << pts.texture_coordinates[i].x << ", " << pts.texture_coordinates[i].y << ")";
            if (i < pts.texture_coordinates.size() - 1)
                os << ", ";
        }

        os << "]\n  Sub-Atlas: {";
        for (const auto &[key, sub_texture] : pts.sub_atlas) {
            os << "\n    \"" << key << "\": " << sub_texture;
        }
        os << "\n  }\n}";

        return os;
    }
};

class TexturePacker {
  public:
    TexturePacker(const std::filesystem::path &textures_directory, const std::filesystem::path &output_dir,
                  int container_side_length);

    void regenerate(const std::vector<std::string> &new_texture_paths = {});

    std::vector<Block> collect_textures_data_from_dir(const std::string &directory, const std::string &output_dir,
                                                      const std::set<std::string> &currently_packed_texture_paths);

    void pack_textures(const std::vector<std::string> &texture_paths, const std::filesystem::path &output_dir,
                       int container_side_length);

    std::vector<PackedTextureContainer> pack_texture_blocks_into_containers(std::vector<TextureBlock> &texture_blocks,
                                                                            int container_size);

    std::vector<std::string> get_texture_paths(const std::filesystem::path &directory,
                                               const std::filesystem::path &output_dir);

    std::vector<TextureBlock>
    construct_texture_blocks_from_texture_paths(const std::vector<std::string> &texture_paths);

    PackedTextureSubTexture get_packed_texture_sub_texture(const std::string &file_path);

    int get_packed_texture_index_of_texture(const std::string &file_path);
    int get_packed_texture_bounding_box_index_of_texture(const std::string &texture_path);

    glm::vec2 get_packed_texture_coordinate(const std::string &file_path, const glm::vec2 &texture_coordinate);

    std::vector<glm::vec2> get_packed_texture_coordinates(const std::string &file_path,
                                                          const std::vector<glm::vec2> &texture_coordinate);

    PackedTextureSubTexture get_packed_texture_sub_texture_atlas(const std::string &file_path,
                                                                 const std::string &sub_texture_name);

    size_t get_atlas_size_of_sub_texture(const std::string &file_path);

    void bind_texture_array();

    std::vector<std::string> currently_held_texture_paths;
    const std::filesystem::path &textures_directory;
    const std::filesystem::path &output_dir;
    int container_side_length;

    std::vector<glm::vec4> texture_index_to_bounding_box;

  private:
    void set_file_path_to_packed_texture_map(const std::filesystem::path &file_path, unsigned int atlas_width,
                                             unsigned int atlas_height);

    void populate_texture_index_to_bounding_box();

    PackedTextureSubTexture parse_sub_texture(const nlohmann::json &sub_texture_json, int atlas_width, int atlas_height,
                                              int texture_index);
    GLuint packed_texture_array_gl_id;
    GLuint packed_texture_bounding_boxes_gl_id;
    std::map<std::string, PackedTextureSubTexture> file_path_to_packed_texture_info;
    // a texture index is simply a unique identifier given to each texture path
    // note that it has nothing ot do with a packed index or anything like that
};

#endif // TEXTURE_PACKER_HPP
