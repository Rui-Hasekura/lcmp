#ifndef LCMP_COMMON_READER_H
#define LCMP_COMMON_READER_H

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <io/izlibstream.h>
#include <io/stream_reader.h>
#include <nbt_tags.h>

namespace lcmp {

  struct SchematicInfo {
    std::string name;
    std::string author;
    std::string description;
    int32_t total_blocks = 0;
    int64_t total_volume = 0;
  };

  struct ReadResult {
    std::unique_ptr<nbt::tag_compound> root;
    SchematicInfo info;
  };

  class ReadError : public std::runtime_error {
  public:
    explicit ReadError(const std::string& message)
      : std::runtime_error(message) {}
  };

  inline ReadResult ReadLitematicFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
      throw ReadError("Cannot open file: " + path.string());
    }

    try {
      zlib::izlibstream zlib_stream(file);
      auto [root_name, root] = nbt::io::read_compound(zlib_stream);

      if (!root) {
        throw ReadError("Failed to parse NBT compound from: " + path.string());
      }

      ReadResult result;
      result.root = std::move(root);

      if (result.root->has_key("Metadata", nbt::tag_type::Compound)) {
        const auto& metadata =
          result.root->at("Metadata").as<nbt::tag_compound>();

        if (metadata.has_key("Name", nbt::tag_type::String)) {
          result.info.name =
            metadata.at("Name").as<nbt::tag_string>().get();
        }
        if (metadata.has_key("Author", nbt::tag_type::String)) {
          result.info.author =
            metadata.at("Author").as<nbt::tag_string>().get();
        }
        if (metadata.has_key("Description", nbt::tag_type::String)) {
          result.info.description =
            metadata.at("Description").as<nbt::tag_string>().get();
        }
        if (metadata.has_key("TotalBlocks", nbt::tag_type::Int)) {
          result.info.total_blocks = static_cast<int32_t>(
            metadata.at("TotalBlocks").as<nbt::tag_int>());
        }
        if (metadata.has_key("TotalVolume", nbt::tag_type::Int)) {
          result.info.total_volume = static_cast<int64_t>(
            metadata.at("TotalVolume").as<nbt::tag_int>());
        }
        else if (metadata.has_key("TotalVolume", nbt::tag_type::Long)) {
          result.info.total_volume = static_cast<int64_t>(
            metadata.at("TotalVolume").as<nbt::tag_long>());
        }
      }

      return result;

    }
    catch (const nbt::io::input_error& e) {
      throw ReadError("NBT parse error: " + std::string(e.what()));
    }
    catch (const std::bad_cast& e) {
      throw ReadError("Unexpected NBT structure: " + std::string(e.what()));
    }
    catch (const std::exception& e) {
      throw ReadError("Unexpected error: " + std::string(e.what()));
    }
  }

}  // namespace lcmp

#endif  // LCMP_COMMON_READER_H