#include "texture_packer.hpp"
#include <stb_image.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>

TexturePacker::TexturePacker(const std::string &packed_texture_json_path,
                             const std::vector<std::string> &packed_texture_paths) {
    // Load JSON configuration

    glGenTextures(1, &texture_array);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture_array);

    int width, height, nrChannels;
    unsigned char *data;
    int num_layers = static_cast<int>(packed_texture_paths.size());

    // assuming that it loads in upside down
    stbi_set_flip_vertically_on_load(1);

    // Assuming all textures are the same size; load the first texture to get dimensions
    data = stbi_load(packed_texture_paths[0].c_str(), &width, &height, &nrChannels, STBI_rgb_alpha);
    if (!data) {
        std::cerr << "Failed to load texture: " << packed_texture_paths[0] << std::endl;
        return;
    }
    stbi_image_free(data);

    parse_json_file(packed_texture_json_path, width, height);

    // Initialize the 2D texture array
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, width, height, num_layers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Load each texture layer
    for (int i = 0; i < num_layers; i++) {
        data = stbi_load(packed_texture_paths[i].c_str(), &width, &height, &nrChannels, STBI_rgb_alpha);
        if (data) {
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        } else {
            std::cerr << "Failed to load texture: " << packed_texture_paths[i] << std::endl;
        }
    }
}

std::vector<glm::vec2> compute_texture_coordinates(int x, int y, int width, int height, int atlas_width,
                                                   int atlas_height) {
    // we do one minus here because we flip our atlas
    float u1 = static_cast<float>(x) / atlas_width;
    float v1 = 1 - static_cast<float>(y + height) / atlas_height;
    float u2 = static_cast<float>(x + width) / atlas_width;
    float v2 = 1 - static_cast<float>(y) / atlas_height;

    return {{u1, v2}, {u1, v1}, {u2, v1}, {u2, v2}};
}

PackedTextureSubTexture TexturePacker::parse_sub_texture(const nlohmann::json &sub_texture_json, int atlas_width,
                                                         int atlas_height) {
    PackedTextureSubTexture sub_texture;
    int x = sub_texture_json.at("x").get<int>();
    int y = sub_texture_json.at("y").get<int>();
    int width = sub_texture_json.at("width").get<int>();
    int height = sub_texture_json.at("height").get<int>();
    int packed_texture_index = sub_texture_json.at("container_index").get<unsigned int>();

    sub_texture.texture_coordinates = compute_texture_coordinates(x, y, width, height, atlas_width, atlas_height);
    sub_texture.packed_texture_index = packed_texture_index;

    if (sub_texture_json.contains("sub_textures")) {
        for (auto &[sub_name, sub_atlas_json] : sub_texture_json.at("sub_textures").items()) {
            int sub_x = sub_atlas_json.at("x").get<int>();
            int sub_y = sub_atlas_json.at("y").get<int>();
            int sub_width = sub_atlas_json.at("width").get<int>();
            int sub_height = sub_atlas_json.at("height").get<int>();

            PackedTextureSubTexture sub_atlas_sub_texture;
            sub_atlas_sub_texture.packed_texture_index = packed_texture_index;
            sub_atlas_sub_texture.texture_coordinates =
                compute_texture_coordinates(sub_x, sub_y, sub_width, sub_height, atlas_width, atlas_height);
            sub_texture.sub_atlas[sub_name] = sub_atlas_sub_texture;
        }
    }

    return sub_texture;
}

void TexturePacker::parse_json_file(const std::string &file_path, unsigned int atlas_width, unsigned int atlas_height) {
    std::ifstream file(file_path);
    nlohmann::json j;
    file >> j;

    for (const auto &[path, texture_info] : j["sub_textures"].items()) {
        file_path_to_packed_texture_info[path] = parse_sub_texture(texture_info, atlas_width, atlas_height);
    }
}

PackedTextureSubTexture TexturePacker::get_packed_texture_sub_texture(const std::string &file_path) {
    if (file_path_to_packed_texture_info.contains(file_path)) {
        return file_path_to_packed_texture_info.at(file_path);
    }
    throw std::runtime_error("File path not found: " + file_path);
}

PackedTextureSubTexture TexturePacker::get_packed_texture_sub_texture_atlas(const std::string &file_path,
                                                                            const std::string &sub_texture_name) {
    if (file_path_to_packed_texture_info.contains(file_path)) {
        auto &texture = file_path_to_packed_texture_info.at(file_path);
        if (texture.sub_atlas.contains(sub_texture_name)) {
            return texture.sub_atlas.at(sub_texture_name);
        }
        throw std::runtime_error("Subtexture name not found: " + sub_texture_name);
    }
    throw std::runtime_error("File path not found: " + file_path);
}

void TexturePacker::bind_texture_array() { glBindTexture(GL_TEXTURE_2D_ARRAY, texture_array); }
