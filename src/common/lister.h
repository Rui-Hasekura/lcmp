// Copyright (C) 2026 Rui-Hasekura <ruihasekura@gmail.com>
// SPDX-License-Identifier: GPL-3.0-only

#ifndef LCMP_COMMON_LISTER_H
#define LCMP_COMMON_LISTER_H

#include <algorithm>
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <nbt_tags.h>

#include "reader.h"

namespace lcmp {

  class ListerError : public std::runtime_error {
  public:
    explicit ListerError(const std::string& message)
      : std::runtime_error(message) {}
  };

  namespace internal {
    inline uint32_t CalculateBitsPerBlock(size_t palette_size) {
      if (palette_size <= 4) return 2;
      return std::bit_width(palette_size - 1);
    }

    template<typename Callback>
    inline void ForEachBlockIndex(
      const std::vector<int64_t>& long_array,
      uint32_t bits_per_block,
      uint64_t total_block_count,
      Callback&& callback) {

      const uint64_t mask = (1ULL << bits_per_block) - 1;

      for (uint64_t i = 0; i < total_block_count; ++i) {
        const uint64_t start_offset = i * bits_per_block;
        const size_t start_arr_idx =
          static_cast<size_t>(start_offset >> 6);
        const size_t end_arr_idx =
          static_cast<size_t>(((i + 1) * bits_per_block - 1) >> 6);
        const uint32_t start_bit_offset =
          static_cast<uint32_t>(start_offset & 0x3F);

        if (start_arr_idx >= long_array.size() ||
          end_arr_idx >= long_array.size()) {
          throw ListerError("BlockStates array too small for declared volume");
        }

        uint64_t value =
          static_cast<uint64_t>(long_array[start_arr_idx]) >>
          start_bit_offset;

        if (start_arr_idx != end_arr_idx) {
          value |=
            static_cast<uint64_t>(long_array[end_arr_idx]) <<
            (64 - start_bit_offset);
        }

        callback(static_cast<uint32_t>(value & mask));
      }
    }

  }  // namespace internal

  struct MaterialEntry {
    std::string block_name;
    uint64_t count = 0;
  };

  using MaterialList = std::vector<MaterialEntry>;

  inline MaterialList ListMaterials(const nbt::tag_compound& root) {
    if (!root.has_key("Regions", nbt::tag_type::Compound)) {
      throw ListerError("Missing 'Regions' compound in root");
    }

    const auto& regions = root.at("Regions").as<nbt::tag_compound>();
    absl::flat_hash_map<std::string, uint64_t> global_counts;

    for (const auto& [region_name, region_value] : regions) {
      if (region_value.get().get_type() != nbt::tag_type::Compound) {
        continue;
      }
      const auto& region = region_value.as<nbt::tag_compound>();

      if (!region.has_key("Size", nbt::tag_type::Compound)) {
        continue;
      }
      const auto& size_compound =
        region.at("Size").as<nbt::tag_compound>();

      int64_t size_x = 0, size_y = 0, size_z = 0;
      if (size_compound.has_key("x", nbt::tag_type::Int)) {
        size_x = static_cast<int64_t>(
          size_compound.at("x").as<nbt::tag_int>());
      }
      if (size_compound.has_key("y", nbt::tag_type::Int)) {
        size_y = static_cast<int64_t>(
          size_compound.at("y").as<nbt::tag_int>());
      }
      if (size_compound.has_key("z", nbt::tag_type::Int)) {
        size_z = static_cast<int64_t>(
          size_compound.at("z").as<nbt::tag_int>());
      }

      const uint64_t volume =
        static_cast<uint64_t>(size_x < 0 ? -size_x : size_x) *
        static_cast<uint64_t>(size_y < 0 ? -size_y : size_y) *
        static_cast<uint64_t>(size_z < 0 ? -size_z : size_z);

      if (!region.has_key("BlockStatePalette", nbt::tag_type::List)) {
        continue;
      }
      const auto& palette =
        region.at("BlockStatePalette").as<nbt::tag_list>();

      std::vector<std::string> palette_names;
      palette_names.reserve(palette.size());

      for (size_t i = 0; i < palette.size(); ++i) {
        const auto& entry = palette.at(i);
        if (entry.get().get_type() == nbt::tag_type::Compound) {
          const auto& compound = entry.as<nbt::tag_compound>();
          if (compound.has_key("Name", nbt::tag_type::String)) {
            palette_names.push_back(
              compound.at("Name").as<nbt::tag_string>().get());
            continue;
          }
        }
        palette_names.emplace_back();
      }

      if (!region.has_key("BlockStates", nbt::tag_type::Long_Array)) {
        continue;
      }
      const auto& long_data =
        region.at("BlockStates").as<nbt::tag_array<int64_t>>();
      const auto& long_array = long_data.get();

      const uint32_t bits_per_block =
        internal::CalculateBitsPerBlock(palette_names.size());

      try {
        internal::ForEachBlockIndex(
          long_array, bits_per_block, volume,
          [&palette_names, &global_counts](uint32_t index) {
            if (index < palette_names.size()) {
              const std::string& name = palette_names[index];
              if (!name.empty() && name != "minecraft:air") {
                ++global_counts[name];
              }
            }
          });
      }
      catch (const ListerError&) {
        continue;
      }
    }

    MaterialList materials;
    materials.reserve(global_counts.size());
    for (const auto& [name, count] : global_counts) {
      materials.push_back({ name, count });
    }

    std::sort(
      materials.begin(), materials.end(),
      [](const MaterialEntry& a, const MaterialEntry& b) {
        return a.block_name < b.block_name;
      });

    return materials;
  }

  inline MaterialList ListMaterialsFromFile(const std::string& path) {
    const ReadResult read_result = ReadLitematicFile(path);
    return ListMaterials(*read_result.root);
  }
}  // namespace lcmp

#endif  // LCMP_COMMON_LISTER_H