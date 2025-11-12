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

/**
 * @class TexturePacker
 * @brief Handles automatic texture atlas generation and management for efficient GPU texture storage.
 *
 * The `TexturePacker` class automates the process of collecting textures from a directory,
 * packing them efficiently into texture atlases (also called containers), and providing
 * fast lookup for texture coordinates and atlas indices. It supports regeneration when
 * new textures are added, as well as OpenGL binding for rendering.
 *
 * This is useful in rendering engines or voxel systems that need to batch draw calls by
 * minimizing texture switches.
 *
 * @TODO as of right now the texture packer has a huge problem which is that its run everytime the program starts, in
 * order to correct this we need to improve our fs_utils so that we can save the state of a whole directory to file,
 * only saving the metadata such as when a file was last created or modified, and the ability to read this and then see
 * if new files have been created or modified, whenever this is true we run the texture packer, when this is not true,
 * then all the texture are still packed so we don't have to do anything else.
 */
class TexturePacker {
  public:
    /**
     * @brief Construct a new TexturePacker.
     *
     * @param textures_directory The directory containing source textures to be packed.
     * @param output_dir The directory where packed texture atlases and metadata will be written.
     * @param container_side_length The side length (in pixels) of each texture container (atlas).
     */
    TexturePacker(const std::filesystem::path &textures_directory, const std::filesystem::path &output_dir,
                  int container_side_length);

    /**
     * @brief Rebuilds the texture atlas, optionally with new texture inputs.
     *
     * If no texture paths are provided, this regenerates based on all textures
     * found in the `textures_directory`.
     *
     * @param new_texture_paths Optional list of new texture file paths to include.
     */
    void regenerate(const std::vector<std::string> &new_texture_paths = {});

    /**
     * @brief Collects texture metadata from a directory.
     *
     * Scans the given directory for texture files, excluding those already packed.
     *
     * @param directory The directory containing textures.
     * @param output_dir The output directory where metadata or intermediate files may be written.
     * @param currently_packed_texture_paths Set of file paths already included in the current atlas.
     * @return A vector of `Block` objects representing collected texture data.
     */
    std::vector<Block> collect_textures_data_from_dir(const std::string &directory, const std::string &output_dir,
                                                      const std::set<std::string> &currently_packed_texture_paths);

    /**
     * @brief Packs the given textures into one or more atlas containers.
     *
     * @param texture_paths A list of file paths to textures to be packed.
     * @param output_dir Directory where packed atlases will be stored.
     * @param container_side_length The size (in pixels) of each atlas container.
     */
    void pack_textures(const std::vector<std::string> &texture_paths, const std::filesystem::path &output_dir,
                       int container_side_length);

    /**
     * @brief Packs texture blocks into texture containers (atlases).
     *
     * @param texture_blocks The texture blocks to be packed.
     * @param container_size The side length of each texture container.
     * @return A vector of `PackedTextureContainer` objects representing generated atlases.
     */
    std::vector<PackedTextureContainer> pack_texture_blocks_into_containers(std::vector<TextureBlock> &texture_blocks,
                                                                            int container_size);

    /**
     * @brief Retrieves all texture file paths from a directory.
     *
     * @param directory The source texture directory.
     * @param output_dir The output directory (used for filtering or metadata purposes).
     * @return A vector of texture file paths.
     */
    std::vector<std::string> get_texture_paths(const std::filesystem::path &directory,
                                               const std::filesystem::path &output_dir);

    /**
     * @brief Converts file paths into texture block objects.
     *
     * @param texture_paths A list of texture file paths.
     * @return A vector of constructed `TextureBlock` objects.
     */
    std::vector<TextureBlock>
    construct_texture_blocks_from_texture_paths(const std::vector<std::string> &texture_paths);

    /**
     * @brief Retrieves packed sub-texture information for a given texture file.
     *
     * @param file_path Path to the texture file.
     * @return A `PackedTextureSubTexture` containing UVs, atlas index, and dimensions.
     */
    PackedTextureSubTexture get_packed_texture_sub_texture(const std::string &file_path);

    /**
     * @brief Gets the index of the packed texture container containing a given texture.
     *
     * @param file_path Path to the texture file.
     * @return The integer index of the packed texture.
     */
    int get_packed_texture_index_of_texture(const std::string &file_path);

    /**
     * @brief Retrieves the bounding box index for a specific texture path.
     *
     * @param texture_path The texture file path.
     * @return The index of the corresponding bounding box in GPU memory.
     */
    int get_packed_texture_bounding_box_index_of_texture(const std::string &texture_path);

    /**
     * @brief Computes the remapped UV coordinate for a texture within its atlas.
     *
     * @param file_path Path to the original texture.
     * @param texture_coordinate Original UV coordinate (0–1 range).
     * @return The remapped UV coordinate within the packed atlas.
     */
    glm::vec2 get_packed_texture_coordinate(const std::string &file_path, const glm::vec2 &texture_coordinate);

    /**
     * @brief Computes multiple remapped UV coordinates for a texture within its atlas.
     *
     * @param file_path Path to the original texture.
     * @param texture_coordinate Original list of UV coordinates.
     * @return Vector of remapped UV coordinates.
     */
    std::vector<glm::vec2> get_packed_texture_coordinates(const std::string &file_path,
                                                          const std::vector<glm::vec2> &texture_coordinate);

    /**
     * @brief Retrieves a specific sub-texture by name from a packed atlas.
     *
     * Useful for sprite sheets or atlases containing multiple logical textures.
     *
     * @param file_path Path to the texture atlas file.
     * @param sub_texture_name The name of the sub-texture defined in metadata.
     * @return The corresponding `PackedTextureSubTexture` object.
     */
    PackedTextureSubTexture get_packed_texture_sub_texture_atlas(const std::string &file_path,
                                                                 const std::string &sub_texture_name);

    /**
     * @brief Gets the number of sub-textures in a given texture atlas.
     *
     * @param file_path Path to the texture atlas file.
     * @return Number of sub-textures contained within that atlas.
     */
    size_t get_atlas_size_of_sub_texture(const std::string &file_path);

    /**
     * @brief Binds the packed texture array and bounding box buffers to OpenGL.
     *
     * Must be called before rendering any geometry that references packed textures.
     */
    void bind_texture_array();

    /** @brief List of currently held texture file paths. */
    std::vector<std::string> currently_held_texture_paths;

    /** @brief Path to the directory containing input textures. */
    std::filesystem::path textures_directory;

    /** @brief Output directory where packed atlases and metadata are saved. */
    std::filesystem::path output_dir;

    /** @brief Side length (in pixels) of each texture container (atlas). */
    int container_side_length;

    /** @brief Mapping from texture index to bounding box (UV min/max and atlas index). */
    std::vector<glm::vec4> texture_index_to_bounding_box;

  private:
    /**
     * @brief Records mapping between a file path and its packed texture information.
     *
     * @param file_path The texture file path.
     * @param atlas_width Width of the packed texture atlas.
     * @param atlas_height Height of the packed texture atlas.
     */
    void set_file_path_to_packed_texture_map(const std::filesystem::path &file_path, unsigned int atlas_width,
                                             unsigned int atlas_height);

    /**
     * @brief Populates the GPU buffer mapping texture indices to bounding boxes.
     */
    void populate_texture_index_to_bounding_box();

    /**
     * @brief Parses a sub-texture entry from JSON metadata.
     *
     * @param sub_texture_json JSON object describing the sub-texture.
     * @param atlas_width Width of the atlas texture.
     * @param atlas_height Height of the atlas texture.
     * @param texture_index The index of the texture within the atlas.
     * @return A `PackedTextureSubTexture` representing the parsed entry.
     */
    PackedTextureSubTexture parse_sub_texture(const nlohmann::json &sub_texture_json, int atlas_width, int atlas_height,
                                              int texture_index);

    /** @brief OpenGL texture array object ID storing packed texture layers. */
    GLuint packed_texture_array_gl_id;

    /** @brief OpenGL buffer object ID for packed texture bounding boxes. */
    GLuint packed_texture_bounding_boxes_gl_id;

    /** @brief Map from file path to its packed texture metadata. */
    // a texture index is simply a unique identifier given to each texture path
    // note that it has nothing ot do with a packed index or anything like that
    std::map<std::string, PackedTextureSubTexture> file_path_to_packed_texture_info;
};

#endif // TEXTURE_PACKER_HPP
