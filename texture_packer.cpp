#include "texture_packer.hpp"
#include <stb_image.h>
#include <stb_image_write.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <regex>
#include <set>

#include <iostream>
#include <unordered_set>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <string>
#include <map>
#include <stdexcept>
#include <glm/vec2.hpp>
#include <vector>

void create_directory_if_needed(const std::filesystem::path &output_dir) {
    // Check if the path is empty before proceeding
    if (output_dir.empty()) {
        std::cerr << "Error: Provided directory path is empty!" << std::endl;
        return;
    }

    // Check if the directory exists
    if (!std::filesystem::exists(output_dir)) {
        try {
            // Create the directory if it does not exist
            std::filesystem::create_directories(output_dir);
        } catch (const std::filesystem::filesystem_error &e) {
            std::cerr << "Error creating directory: " << e.what() << std::endl;
        }
    }
}

TexturePacker::TexturePacker(const std::filesystem::path &textures_directory, const std::filesystem::path &output_dir,
                             int container_side_length)
    : textures_directory(textures_directory), output_dir(output_dir), container_side_length(container_side_length) {

    create_directory_if_needed(output_dir);
    std::vector<std::string> initial_texture_paths = get_texture_paths(textures_directory, output_dir);
    regenerate(initial_texture_paths);
}

std::vector<std::string> TexturePacker::get_texture_paths(const std::filesystem::path &directory,
                                                          const std::filesystem::path &output_dir) {
    std::vector<std::string> image_file_paths;

    // Walk through the directory and subdirectories
    for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            std::string file = entry.path().filename().string();

            // Check if file is a supported image type
            if (file.size() > 4 &&
                (file.substr(file.size() - 4) == ".png" || file.substr(file.size() - 4) == ".jpg" ||
                 file.substr(file.size() - 4) == ".tga" || file.substr(file.size() - 5) == ".jpeg")) {
                std::string file_path = entry.path().string();

                // Skip files inside the output directory
                if (std::filesystem::equivalent(entry.path().parent_path(), output_dir)) {
                    std::cout << "Skipping file inside output directory: " << file_path << std::endl;
                    continue;
                }

                image_file_paths.push_back(file_path);
            }
        }
    }
    return image_file_paths;
}

bool is_power_of_two(int n) { return n > 0 && (n & (n - 1)) == 0; }

void TexturePacker::pack_textures(const std::vector<std::string> &texture_paths,
                                  const std::filesystem::path &output_dir, int container_side_length) {

    // Step 1: Construct texture blocks from the provided texture paths
    std::vector<TextureBlock> texture_blocks = construct_texture_blocks_from_texture_paths(texture_paths);
    for (const auto &block : texture_blocks) {
        std::cout << "  - TextureBlock: " << block.texture_path << "\n"
                  << "      Dimensions: " << block.block.w << "x" << block.block.h << "\n"
                  << "      Subtextures: [\n";
        for (const auto &[key, value] : block.subtextures) {
            std::cout << "        \"" << key << "\": {";
            for (const auto &[sub_key, sub_value] : value) {
                std::cout << " \"" << sub_key << "\": " << sub_value << ",";
            }
            std::cout << " }\n";
        }
        std::cout << "      ]\n";
    }

    // Step 2: Pack the texture blocks into containers
    std::vector<PackedTextureContainer> packed_texture_containers =
        pack_texture_blocks_into_containers(texture_blocks, container_side_length);
    std::cout << "Packed texture blocks into " << packed_texture_containers.size() << " containers:\n";

    for (size_t i = 0; i < packed_texture_containers.size(); ++i) {
        const auto &container = packed_texture_containers[i];
        std::cout << "Container " << i << ":\n";
        std::cout << "  - Number of packed blocks: " << container.packed_texture_blocks.size() << "\n";
        for (const auto &block : container.packed_texture_blocks) {
            std::cout << "    - TextureBlock: " << block.texture_path << "\n"
                      << "        Dimensions: " << block.block.w << "x" << block.block.h << "\n";
            if (block.block.packed_placement) {
                const auto &placement = block.block.packed_placement.value();
                std::cout << "        Placement: (" << placement.top_left_x << ", " << placement.top_left_y << ")\n";
            } else {
                std::cout << "        Placement: Not packed\n";
            }
            std::cout << "        Subtextures: [\n";
            for (const auto &[key, value] : block.subtextures) {
                std::cout << "          \"" << key << "\": {";
                for (const auto &[sub_key, sub_value] : value) {
                    std::cout << " \"" << sub_key << "\": " << sub_value << ",";
                }
                std::cout << " }\n";
            }
            std::cout << "        ]\n";
        }
    }

    // Step 3: Prepare JSON metadata and write packed texture images
    nlohmann::json result;

    for (size_t i = 0; i < packed_texture_containers.size(); ++i) {
        const auto &container = packed_texture_containers[i];
        std::cout << "Processing container " << i << " with " << container.packed_texture_blocks.size()
                  << " texture blocks.\n";

        std::vector<uint8_t> image_data(container_side_length * container_side_length * 4, 0); // Assuming RGBA format

        for (const auto &block : container.packed_texture_blocks) {
            if (!block.block.packed_placement.has_value()) {
                std::cout << "Skipping block without placement.\n";
                continue; // Skip blocks that were not successfully packed
            }

            const auto &placement = block.block.packed_placement.value();
            std::cout << "Processing block: " << block.texture_path << " at position (" << placement.top_left_x << ", "
                      << placement.top_left_y << ").\n";

            // Load the block image (e.g., using stb_image)
            int img_width, img_height, channels;
            std::unique_ptr<uint8_t[], void (*)(void *)> block_image(
                stbi_load(block.texture_path.c_str(), &img_width, &img_height, &channels, 4), stbi_image_free);

            if (!block_image) {
                std::cerr << "Failed to load texture: " << block.texture_path << std::endl;
                continue;
            }

            std::cout << "Loaded image: " << block.texture_path << " with dimensions (" << img_width << "x"
                      << img_height << ").\n";

            // Copy the block image into the container image at the specified position
            for (int row = 0; row < img_height; ++row) {
                for (int col = 0; col < img_width; ++col) {
                    for (int channel = 0; channel < 4; ++channel) {
                        int dest_x = placement.top_left_x + col;
                        int dest_y = placement.top_left_y + row;
                        if (dest_x < 0 || dest_x >= container_side_length || dest_y < 0 ||
                            dest_y >= container_side_length) {
                            std::cerr << "Out-of-bounds access detected for block: " << block.texture_path << "\n";
                            continue;
                        }
                        image_data[(dest_y * container_side_length + dest_x) * 4 + channel] =
                            block_image[(row * img_width + col) * 4 + channel];
                    }
                }
            }

            // Add metadata for this block
            result["sub_textures"][block.texture_path] = {{"container_index", static_cast<int>(i)},
                                                          {"x", placement.top_left_x},
                                                          {"y", placement.top_left_y},
                                                          {"width", block.block.w},
                                                          {"height", block.block.h},
                                                          {"sub_textures", block.subtextures}};
        }

        // Write the packed texture image to a file
        std::string filename = "packed_texture_" + std::to_string(i) + ".png";
        stbi_write_png((output_dir / filename).string().c_str(), container_side_length, container_side_length, 4,
                       image_data.data(), container_side_length * 4);

        std::cout << "Packed texture saved to " << output_dir / filename << std::endl;
    }

    // Write metadata to JSON file
    std::ofstream json_output(output_dir / "packed_textures.json");
    json_output << result.dump(4);
    std::cout << "Metadata saved to " << output_dir / "packed_textures.json" << std::endl;

    std::cout << "Texture packing completed successfully.\n";
}

std::vector<PackedTextureContainer>
TexturePacker::pack_texture_blocks_into_containers(std::vector<TextureBlock> &texture_blocks, int container_size) {
    std::cout << "Starting texture packing into containers. Container size: " << container_size << "x" << container_size
              << "\n";

    // Sort by minimum side length in descending order
    std::sort(texture_blocks.begin(), texture_blocks.end(), [](const TextureBlock &a, const TextureBlock &b) {
        return std::min(a.block.w, a.block.h) > std::min(b.block.w, b.block.h);
    });
    std::cout << "Sorted texture blocks by minimum side length (descending order):\n";
    for (const auto &tb : texture_blocks) {
        std::cout << "  - TextureBlock: " << tb.texture_path << "\n"
                  << "      Dimensions: " << tb.block.w << "x" << tb.block.h << "\n";
    }

    std::vector<PackedTextureContainer> currently_created_packed_texture_containers;

    for (auto &tb : texture_blocks) {
        std::cout << "Processing TextureBlock: " << tb.texture_path << " with dimensions " << tb.block.w << "x"
                  << tb.block.h << "\n";

        if (tb.block.w > container_size || tb.block.h > container_size) {
            std::cerr << "Error: The image " << tb.texture_path << " has dimensions " << tb.block.w << "x" << tb.block.h
                      << ", but the container is " << container_size << "x" << container_size
                      << ". Make the container size bigger.\n";
            continue;
        }

        bool found_container_to_fit_texture_in = false;

        // Try to fit the texture block into an existing container
        for (auto &pt_container : currently_created_packed_texture_containers) {
            std::cout << "  Attempting to fit into an existing container...\n";

            pt_container.packer->fit(tb.block);

            // if the block has been fit in
            if (tb.block.packed_placement) {
                std::cout << "    Successfully packed into existing container.\n";
                for (auto &[_, subtexture_data] : tb.subtextures) {
                    subtexture_data["x"] += tb.block.packed_placement->top_left_x;
                    subtexture_data["y"] += tb.block.packed_placement->top_left_y;
                }

                pt_container.packed_texture_blocks.push_back(tb);
                found_container_to_fit_texture_in = true;
                break;
            } else {
                std::cout << "    Failed to fit into this container.\n";
            }
        }

        if (!found_container_to_fit_texture_in) {
            std::cout << "  Creating a new container for the texture.\n";

            auto new_packer = std::make_shared<SplitPacker>(container_size, container_size);
            new_packer->fit(tb.block);

            PackedTextureContainer pt(new_packer);

            if (tb.block.packed_placement) {
                std::cout << "    Successfully packed into the new container.\n";
                pt.packed_texture_blocks.push_back(tb);

                // No need to update subtexture data, as it's in the top-left corner
            } else {
                std::cerr << "    Error: Created a new container, but the texture still couldn't fit.\n";
            }

            currently_created_packed_texture_containers.push_back(pt);
        }
    }

    // Summary of results
    std::cout << "Packing completed. Created " << currently_created_packed_texture_containers.size()
              << " containers.\n";
    for (size_t i = 0; i < currently_created_packed_texture_containers.size(); ++i) {
        const auto &container = currently_created_packed_texture_containers[i];
        std::cout << "Container " << i << ":\n";
        std::cout << "  - Number of packed blocks: " << container.packed_texture_blocks.size() << "\n";
        for (const auto &block : container.packed_texture_blocks) {
            std::cout << "    - TextureBlock: " << block.texture_path << "\n"
                      << "        Dimensions: " << block.block.w << "x" << block.block.h << "\n";
            if (block.block.packed_placement) {
                const auto &placement = block.block.packed_placement.value();
                std::cout << "        Placement: (" << placement.top_left_x << ", " << placement.top_left_y << ")\n";
            } else {
                std::cout << "        Placement: Not packed\n";
            }
        }
    }

    return currently_created_packed_texture_containers;
}

std::vector<TextureBlock>
TexturePacker::construct_texture_blocks_from_texture_paths(const std::vector<std::string> &texture_paths) {
    std::vector<TextureBlock> texture_blocks;
    for (const auto &file_path : texture_paths) {
        // Load image using stb_image
        int width, height, channels;

        // we're throwing away the image data here, because we only need width and height really
        unsigned char *img_data = stbi_load(file_path.c_str(), &width, &height, &channels, 0);
        if (img_data) {
            if (true) {
                std::map<std::string, std::map<std::string, float>> subtextures;

                // Check for associated JSON file
                std::string json_path = file_path.substr(0, file_path.find_last_of('.')) + ".json";
                std::ifstream json_file(json_path);

                if (json_file.is_open()) {
                    nlohmann::json json;
                    json_file >> json;
                    if (json.contains("sub_textures")) {
                        subtextures = json["sub_textures"];
                        /*for (auto &[key, value] : json["sub_textures"].items()) {*/
                        /*    // Assuming value is a JSON array of integers*/
                        /*    if (value.is_array()) {*/
                        /*        subtextures[key] = value.get<std::vector<int>>();*/
                        /*    }*/
                        /*}*/
                    }
                }

                // Append data including the full file path
                std::cout << "Found texture " << file_path << " with dimensions " << width << "x" << height
                          << std::endl;

                // here the data gets saved into new memory now.
                std::vector<unsigned char> image_data(img_data, img_data + width * height * channels);

                // something about subtextures needs to be thought about.
                /*TextureBlock tb(width, height, file_path, image_data, subtextures);*/
                TextureBlock tb(width, height, file_path);
                tb.subtextures = subtextures;
                texture_blocks.push_back(tb);

                stbi_image_free(img_data);
            } else {
                // in the future we should stop caring about this.
                std::cout << "The texture " << file_path << " did not have power-of-two dimensions" << std::endl;
                stbi_image_free(img_data);
            }
        } else {
            std::cout << "Failed to load texture: " << file_path << std::endl;
        }
    }

    return texture_blocks;
}

void TexturePacker::regenerate(const std::vector<std::string> &new_texture_paths) {

    currently_held_texture_paths.insert(currently_held_texture_paths.end(), new_texture_paths.begin(),
                                        new_texture_paths.end());

    pack_textures(currently_held_texture_paths, this->output_dir, this->container_side_length);

    // clear out anything data which was previously stored
    file_path_to_packed_texture_info.clear();
    texture_index_to_bounding_box.clear();

    std::filesystem::path packed_texture_json_path =
        std::filesystem::path("assets") / "packed_textures" / "packed_textures.json";
    std::filesystem::path texture_directory = std::filesystem::path("assets") / "packed_textures";

    std::vector<std::filesystem::path> packed_texture_paths =
        fs_utils::list_files_matching_regex(texture_directory, "packed_texture_\\d+\\.png");
    std::sort(packed_texture_paths.begin(), packed_texture_paths.end());

    // I think this uniform doesn't have to be bound because its texture unit is 0 and it works straight away?
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &packed_texture_array_gl_id);
    glBindTexture(GL_TEXTURE_2D_ARRAY, packed_texture_array_gl_id);

    int width, height, nrChannels;
    unsigned char *data;
    int num_layers = static_cast<int>(packed_texture_paths.size());

    // Assuming all textures are the same size; load the first texture to get dimensions
    std::cerr << "about to load texture: " << packed_texture_paths[0].string() << std::endl;
    std::string first_matching_texture_path = packed_texture_paths[0].string();
    data = stbi_load(first_matching_texture_path.c_str(), &width, &height, &nrChannels, STBI_rgb_alpha);
    if (!data) {
        std::cerr << "Failed to load texture: " << packed_texture_paths[0].string() << std::endl;
        return;
    }
    stbi_image_free(data);

    set_file_path_to_packed_texture_map(packed_texture_json_path, width, height);
    populate_texture_index_to_bounding_box();
    std::map<std::string, PackedTextureSubTexture> file_path_to_packed_texture_info;

    // initialize the 2d texture array
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, width, height, num_layers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Load each texture layer
    for (int i = 0; i < num_layers; i++) {
        std::string current_packed_texture_path = packed_texture_paths[i].string();
        data = stbi_load(current_packed_texture_path.c_str(), &width, &height, &nrChannels, STBI_rgb_alpha);
        if (data) {
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        } else {
            std::cerr << "Failed to load texture: " << current_packed_texture_path << std::endl;
        }
    }
    // done loading up packed textures, starting to load up bounding boxes.
    glActiveTexture(GL_TEXTURE1);
    glGenTextures(1, &packed_texture_bounding_boxes_gl_id);
    glBindTexture(GL_TEXTURE_1D, packed_texture_bounding_boxes_gl_id);

    int MAX_NUM_TEXTURES = 1024;
    // Check if the texture data exceeds the maximum texture size
    if (texture_index_to_bounding_box.size() > MAX_NUM_TEXTURES) {
        std::cerr << "Error: Too many textures, exceeds MAX_NUM_TEXTURES." << std::endl;
        // Resize to fit the maximum size
        texture_index_to_bounding_box.resize(MAX_NUM_TEXTURES);
    } else if (texture_index_to_bounding_box.size() < MAX_NUM_TEXTURES) {
        // Resize to fit the maximum size and fill the new space with zeros
        texture_index_to_bounding_box.resize(MAX_NUM_TEXTURES, glm::vec4(0.0f));
    }

    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F, MAX_NUM_TEXTURES, 0, GL_RGBA, GL_FLOAT,
                 texture_index_to_bounding_box.data());

    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
}

std::vector<glm::vec2> compute_texture_coordinates(float x, float y, float width, float height, int atlas_width,
                                                   int atlas_height) {
    // Calculate texture coordinates
    float u_min = x / atlas_width;
    float v_min = y / atlas_height;
    float u_max = (x + width) / atlas_width;
    float v_max = (y + height) / atlas_height;

    return {
        // note that top and bottom are inverted compared to the vertex positions due to opengl
        {u_max, v_min}, // Top-right
        {u_max, v_max}, // Bottom-right
        {u_min, v_max}, // Bottom-left
        {u_min, v_min}  // Top-left
    };
}

PackedTextureSubTexture TexturePacker::parse_sub_texture(const nlohmann::json &sub_texture_json, int atlas_width,
                                                         int atlas_height, int texture_index) {
    PackedTextureSubTexture sub_texture;
    int top_left_x = sub_texture_json.at("x").get<int>();
    int top_left_y = sub_texture_json.at("y").get<int>();
    int width = sub_texture_json.at("width").get<int>();
    int height = sub_texture_json.at("height").get<int>();
    int packed_texture_index = sub_texture_json.at("container_index").get<unsigned int>();

    // regular images packed in
    sub_texture.texture_coordinates =
        compute_texture_coordinates(top_left_x, top_left_y, width, height, atlas_width, atlas_height);
    sub_texture.packed_texture_bounding_box_index = texture_index;
    sub_texture.packed_texture_index = packed_texture_index;
    sub_texture.top_left_x = top_left_x;
    sub_texture.top_left_y = top_left_y;
    sub_texture.width = width;
    sub_texture.height = height;

    // sub texture is a texture atlas
    // things that were texture atlases, that also got packed in (one level of recursion)
    if (sub_texture_json.contains("sub_textures")) {
        for (auto &[subtexture_name, sub_atlas_json] : sub_texture_json.at("sub_textures").items()) {
            float sub_top_left_x = sub_atlas_json.at("x").get<int>();
            float sub_top_left_y = sub_atlas_json.at("y").get<int>();
            float sub_width = sub_atlas_json.at("width").get<int>();
            float sub_height = sub_atlas_json.at("height").get<int>();

            PackedTextureSubTexture sub_atlas_sub_texture;
            sub_atlas_sub_texture.top_left_x = sub_top_left_x;
            sub_atlas_sub_texture.top_left_y = sub_top_left_y;
            sub_atlas_sub_texture.packed_texture_index = packed_texture_index;
            sub_atlas_sub_texture.texture_coordinates = compute_texture_coordinates(
                sub_top_left_x, sub_top_left_y, sub_width, sub_height, atlas_width, atlas_height);
            sub_atlas_sub_texture.width = sub_width;
            sub_atlas_sub_texture.height = sub_height;
            sub_texture.sub_atlas[subtexture_name] = sub_atlas_sub_texture;
        }
    }

    return sub_texture;
}

void TexturePacker::set_file_path_to_packed_texture_map(const std::filesystem::path &file_path,
                                                        unsigned int atlas_width, unsigned int atlas_height) {
    std::ifstream file(file_path);
    nlohmann::json j;
    file >> j;

    int texture_index = 0;
    for (const auto &[path, texture_info] : j["sub_textures"].items()) {
        file_path_to_packed_texture_info[path] =
            parse_sub_texture(texture_info, atlas_width, atlas_height, texture_index);
        texture_index++;
    }
}

void TexturePacker::populate_texture_index_to_bounding_box() {
    // find the maximum index from the packed textures
    int max_index = 0;
    for (const auto &[file_path, sub_texture] : file_path_to_packed_texture_info) {
        max_index = std::max(max_index, sub_texture.packed_texture_bounding_box_index);
    }

    // resize the vector to accommodate the maximum index
    texture_index_to_bounding_box.resize(max_index + 1);
    std::cout << "there are " << file_path_to_packed_texture_info.size() << " many packed textures" << std::endl;
    /*texture_index_to_bounding_box.resize(file_path_to_packed_texture_info.size());*/
    for (const auto &[file_path, sub_texture] : file_path_to_packed_texture_info) {
        int packed_texture_bounding_box_index = sub_texture.packed_texture_bounding_box_index;

        // construct bounding box (tlx, tly, width, height) and convert everyhting into 0, 1 space.
        glm::vec4 bounding_box(static_cast<float>(sub_texture.top_left_x) / container_side_length,
                               static_cast<float>(sub_texture.top_left_y) / container_side_length,
                               static_cast<float>(sub_texture.width) / container_side_length,
                               static_cast<float>(sub_texture.height) / container_side_length);

        std::cout << "accessing at " << packed_texture_bounding_box_index << std::endl;
        texture_index_to_bounding_box[packed_texture_bounding_box_index] = bounding_box;
    }
}

int TexturePacker::get_packed_texture_index_of_texture(const std::string &file_path) {
    PackedTextureSubTexture packed_texture = get_packed_texture_sub_texture(file_path);
    return packed_texture.packed_texture_index;
}

/**
 * @brief Finds the texture index of a given texture path from a map of packed texture information.
 *
 * @param file_path_to_packed_texture_info A map linking texture paths to their corresponding PackedTextureSubTexture.
 * @param texture_path The path of the texture to look for.
 * @return The texture index of the corresponding PackedTextureSubTexture.
 * @throws std::runtime_error If the texture path is not found in the map.
 */
int TexturePacker::get_packed_texture_bounding_box_index_of_texture(const std::string &texture_path) {
    auto it = file_path_to_packed_texture_info.find(texture_path);
    if (it != file_path_to_packed_texture_info.end()) {
        return it->second.packed_texture_bounding_box_index;
    } else {
        throw std::runtime_error("Texture path not found: " + texture_path);
    }
}

std::vector<glm::vec2>
TexturePacker::get_packed_texture_coordinates(const std::string &file_path,
                                              const std::vector<glm::vec2> &texture_coordinates) {
    std::vector<glm::vec2> packed_coordinates;

    for (const auto &uv : texture_coordinates) {
        glm::vec2 packed_coord = get_packed_texture_coordinate(file_path, uv);
        packed_coordinates.push_back(packed_coord);
    }

    return packed_coordinates;
}

glm::vec2 TexturePacker::get_packed_texture_coordinate(const std::string &file_path,
                                                       const glm::vec2 &texture_coordinate) {
    PackedTextureSubTexture packed_texture = get_packed_texture_sub_texture(file_path);

    if (packed_texture.texture_coordinates.size() < 2) {
        throw std::runtime_error("Packed texture must have at least two opposing corners defined.");
    }

    // NOTE: this function will invert the passed in texture coordinates vertically this is so that
    // it will work with the coordinate system in opengl, note that ifyou run this twice things will end up
    // upside down, such as taking from the texture_atlas first, keep in mind

    // opposing corners
    const glm::vec2 &corner_1 = packed_texture.texture_coordinates[2];
    const glm::vec2 &corner_2 = packed_texture.texture_coordinates[0];

    glm::vec2 direction_vector = corner_2 - corner_1;

    // (pairwise multiplication) to move to the correct pos
    glm::vec2 scaled_vector = direction_vector * texture_coordinate;

    // don't forget to move back
    glm::vec2 transformed_coord = corner_1 + scaled_vector;

    return transformed_coord;
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
    throw std::runtime_error("File path not found in packed texture: " + file_path);
}

size_t TexturePacker::get_atlas_size_of_sub_texture(const std::string &file_path) {

    if (file_path_to_packed_texture_info.contains(file_path)) {

        auto &texture = file_path_to_packed_texture_info.at(file_path);

        return texture.sub_atlas.size();
    }

    throw std::runtime_error("File path not found in packed texture: " + file_path);
}

void TexturePacker::bind_texture_array() { glBindTexture(GL_TEXTURE_2D_ARRAY, packed_texture_array_gl_id); }
