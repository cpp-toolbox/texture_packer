#ifndef TEXTURE_PACKER_HPP
#define TEXTURE_PACKER_HPP

#include <map>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <set>
#include <unordered_set>
#include <vector>
#include <string>
#include <glm/glm.hpp>

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

struct PackedTextureSubTexture {
    unsigned int packed_texture_index;
    std::vector<glm::vec2> texture_coordinates;
    // this nesting can occur when we pack a texture atlas, it is only ever one level deep
    std::map<std::string, PackedTextureSubTexture> sub_atlas;
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

    std::vector<std::string> get_texture_paths(const std::string &directory, const std::string &output_dir);

    std::vector<TextureBlock>
    construct_texture_blocks_from_texture_paths(const std::vector<std::string> &texture_paths);

    PackedTextureSubTexture get_packed_texture_sub_texture(const std::string &file_path);

    int get_packed_texture_index_of_texture(const std::string &file_path);

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

  private:
    void set_file_path_to_packed_texture_map(const std::string &file_path, unsigned int atlas_width,
                                             unsigned int atlas_height);
    PackedTextureSubTexture parse_sub_texture(const nlohmann::json &sub_texture_json, int atlas_width,
                                              int atlas_height);
    GLuint texture_array;
    std::map<std::string, PackedTextureSubTexture> file_path_to_packed_texture_info;
};

#endif // TEXTURE_PACKER_HPP
