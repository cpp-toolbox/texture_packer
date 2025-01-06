#ifndef TEXTURE_PACKER_HPP
#define TEXTURE_PACKER_HPP

#include <map>
#include <nlohmann/json_fwd.hpp>
#include <vector>
#include <string>
#include <glm/glm.hpp>

#include "sbpt_generated_includes.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

struct PackedTextureSubTexture {
    unsigned int packed_texture_index;
    std::vector<glm::vec2> texture_coordinates;
    // this nesting can occur when we pack a texture atlas, it is only ever one level deep
    std::map<std::string, PackedTextureSubTexture> sub_atlas;
};

class TexturePacker {
  public:
    TexturePacker(const std::string &packed_texture_json_path);
    void regenerate(const std::string &packed_texture_json_path);
    PackedTextureSubTexture get_packed_texture_sub_texture(const std::string &file_path);
    int get_packed_texture_index_of_texture(const std::string &file_path);
    glm::vec2 get_packed_texture_coordinate(const std::string &file_path, const glm::vec2 &texture_coordinate);
    std::vector<glm::vec2> get_packed_texture_coordinates(const std::string &file_path,
                                                          const std::vector<glm::vec2> &texture_coordinate);
    PackedTextureSubTexture get_packed_texture_sub_texture_atlas(const std::string &file_path,
                                                                 const std::string &sub_texture_name);
    size_t get_atlas_size_of_sub_texture(const std::string &file_path);

    void bind_texture_array();

  private:
    void parse_json_file(const std::string &file_path, unsigned int atlas_width, unsigned int atlas_height);
    PackedTextureSubTexture parse_sub_texture(const nlohmann::json &sub_texture_json, int atlas_width,
                                              int atlas_height);
    GLuint texture_array;
    std::map<std::string, PackedTextureSubTexture> file_path_to_packed_texture_info;
};

#endif // TEXTURE_PACKER_HPP
