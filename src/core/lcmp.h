// Copyright (C) 2026 Rui-Hasekura <ruihasekura@gmail.com>
// SPDX-License-Identifier: GPL-3.0-only

#ifndef LCMP_LCMP_H_
#define LCMP_LCMP_H_

#include <immintrin.h>

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <pdqsort.h>

#include <io/izlibstream.h>
#include <io/stream_reader.h>
#include <nbt_tags.h>

namespace lcmp {

  enum class ErrorCode {
    FileOpenFailed,
    NbtParseError,
    UnexpectedNbtStructure,
    UnexpectedError,
    MissingRegions,
    BlockStatesTooSmall,
  };

  struct LcmpError {
    ErrorCode code;
    std::string message;
  };

  template <typename T>
  using Expected = std::expected<T, LcmpError>;

  // Reader

  [[nodiscard]] inline Expected<std::unique_ptr<nbt::tag_compound>>
    ReadLitematicFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
      return std::unexpected(LcmpError{
          ErrorCode::FileOpenFailed,
          "Cannot open file: " + path.string() });
    }

    try {
      zlib::izlibstream zlib_stream(file);
      auto [root_name, root] = nbt::io::read_compound(zlib_stream);
      (void)root_name;
      if (!root) {
        return std::unexpected(LcmpError{
            ErrorCode::NbtParseError,
            "Failed to parse NBT compound from: " + path.string() });
      }
      return std::move(root);
    }
    catch (const nbt::io::input_error& e) {
      return std::unexpected(LcmpError{
          ErrorCode::NbtParseError,
          std::string("NBT parse error: ") + e.what() });
    }
    catch (const std::bad_cast& e) {
      return std::unexpected(LcmpError{
          ErrorCode::UnexpectedNbtStructure,
          std::string("Unexpected NBT structure: ") + e.what() });
    }
    catch (const std::exception& e) {
      return std::unexpected(LcmpError{
          ErrorCode::UnexpectedError,
          std::string("Unexpected error: ") + e.what() });
    }
  }

  // Lister

  namespace internal {

    [[nodiscard]] inline constexpr uint32_t CalculateBitsPerBlock(
      std::size_t palette_size) noexcept {
      if (palette_size <= 4) return 2;
      return static_cast<uint32_t>(std::bit_width(palette_size - 1));
    }

    // Scalar block-index extraction (tail + bpb > 15 fallback)

    template <std::invocable<uint32_t> Callback>
    [[nodiscard]] Expected<void> ForEachBlockIndexScalar(
      std::span<const int64_t> long_array,
      uint32_t bits_per_block,
      uint64_t total_block_count,
      Callback&& callback,
      uint64_t start_i = 0) {
      const uint64_t mask = (1ULL << bits_per_block) - 1;
      const std::size_t long_size = long_array.size();

      for (uint64_t i = start_i; i < total_block_count; ++i) {
        const uint64_t start_offset = i * bits_per_block;
        const std::size_t start_arr_idx =
          static_cast<std::size_t>(start_offset >> 6);
        const std::size_t end_arr_idx =
          static_cast<std::size_t>(((i + 1) * bits_per_block - 1) >> 6);
        const uint32_t start_bit_offset =
          static_cast<uint32_t>(start_offset & 0x3F);

        if (start_arr_idx >= long_size || end_arr_idx >= long_size)
          [[unlikely]] {
          return std::unexpected(LcmpError{
              ErrorCode::BlockStatesTooSmall,
              "BlockStates array too small for declared volume" });
        }

        uint64_t value =
          static_cast<uint64_t>(long_array[start_arr_idx]) >> start_bit_offset;
        if (start_arr_idx != end_arr_idx) {
          value |= static_cast<uint64_t>(long_array[end_arr_idx])
            << (64 - start_bit_offset);
        }

        callback(static_cast<uint32_t>(value & mask));
      }
      return {};
    }

    // =====================================================================
    // AVX2 fast path
    //
    // bpb ∈ [1, 8]  → 8 blocks/iter, two VPSRLVQ rounds
    // bpb ∈ [9, 15] → 4 blocks/iter, one  VPSRLVQ round
    //
    // 8 consecutive blocks (bpb ≤ 8) span ≤ 64 bit → fit in one uint64
    // window after (l0 >> o) | (l1 << (64-o)) funnel shift.
    // Broadcast to 4 lanes, VPSRLVQ with per-lane shift [0,bpb,2bpb,3bpb]
    // (and [4bpb..7bpb] for the second round).
    //
    // No 32B-alignment dependency: only VPBROADCASTQ (from scalar load)
    // + VMOVDQA store to alignas(32) stack buffer.
    // =====================================================================

    template <std::invocable<uint32_t> Callback>
    [[nodiscard]] Expected<void> ForEachBlockIndexAVX2(
      std::span<const int64_t> long_array,
      uint32_t bits_per_block,
      uint64_t total_block_count,
      Callback&& callback) {
      const uint64_t mask = (1ULL << bits_per_block) - 1;
      const std::size_t long_size = long_array.size();
      const __m256i vmask = _mm256_set1_epi64x(static_cast<int64_t>(mask));

      const __m256i shifts_lo = _mm256_set_epi64x(
        3 * bits_per_block, 2 * bits_per_block,
        1 * bits_per_block, 0);

      alignas(32) uint64_t buf64[4];
      uint64_t i = 0;

      // ---- 8 blocks/iter: bpb ∈ [1, 8] ----
      if (bits_per_block <= 8 && total_block_count >= 8) {
        const __m256i shifts_hi = _mm256_set_epi64x(
          7 * bits_per_block, 6 * bits_per_block,
          5 * bits_per_block, 4 * bits_per_block);

        for (; i + 8 <= total_block_count; i += 8) {
          const uint64_t start_offset = i * bits_per_block;
          const std::size_t start_arr_idx =
            static_cast<std::size_t>(start_offset >> 6);
          const std::size_t end_arr_idx = static_cast<std::size_t>(
            ((i + 8) * bits_per_block - 1) >> 6);

          if (start_arr_idx >= long_size || end_arr_idx >= long_size)
            [[unlikely]] {
            return std::unexpected(LcmpError{
              ErrorCode::BlockStatesTooSmall,
              "BlockStates array too small for declared volume" });
          }

          const uint32_t bit_offset =
            static_cast<uint32_t>(start_offset & 0x3F);
          const uint64_t l0 =
            static_cast<uint64_t>(long_array[start_arr_idx]);

          uint64_t combined;
          if (bit_offset == 0) {
            combined = l0;
          }
          else {
            const uint64_t l1 =
              static_cast<uint64_t>(long_array[end_arr_idx]);
            combined = (l0 >> bit_offset) | (l1 << (64 - bit_offset));
          }

          const __m256i v =
            _mm256_set1_epi64x(static_cast<int64_t>(combined));

          // blocks 0..3
          _mm256_store_si256(
            reinterpret_cast<__m256i*>(buf64),
            _mm256_and_si256(_mm256_srlv_epi64(v, shifts_lo), vmask));
          callback(static_cast<uint32_t>(buf64[0]));
          callback(static_cast<uint32_t>(buf64[1]));
          callback(static_cast<uint32_t>(buf64[2]));
          callback(static_cast<uint32_t>(buf64[3]));

          // blocks 4..7
          _mm256_store_si256(
            reinterpret_cast<__m256i*>(buf64),
            _mm256_and_si256(_mm256_srlv_epi64(v, shifts_hi), vmask));
          callback(static_cast<uint32_t>(buf64[0]));
          callback(static_cast<uint32_t>(buf64[1]));
          callback(static_cast<uint32_t>(buf64[2]));
          callback(static_cast<uint32_t>(buf64[3]));
        }
      }
      // ---- 4 blocks/iter: bpb ∈ [9, 15] ----
      else if (bits_per_block <= 15 && total_block_count >= 4) {
        for (; i + 4 <= total_block_count; i += 4) {
          const uint64_t start_offset = i * bits_per_block;
          const std::size_t start_arr_idx =
            static_cast<std::size_t>(start_offset >> 6);
          const std::size_t end_arr_idx = static_cast<std::size_t>(
            ((i + 4) * bits_per_block - 1) >> 6);

          if (start_arr_idx >= long_size || end_arr_idx >= long_size)
            [[unlikely]] {
            return std::unexpected(LcmpError{
              ErrorCode::BlockStatesTooSmall,
              "BlockStates array too small for declared volume" });
          }

          const uint32_t bit_offset =
            static_cast<uint32_t>(start_offset & 0x3F);
          const uint64_t l0 =
            static_cast<uint64_t>(long_array[start_arr_idx]);

          uint64_t combined;
          if (bit_offset == 0) {
            combined = l0;
          }
          else {
            const uint64_t l1 =
              static_cast<uint64_t>(long_array[end_arr_idx]);
            combined = (l0 >> bit_offset) | (l1 << (64 - bit_offset));
          }

          const __m256i v =
            _mm256_set1_epi64x(static_cast<int64_t>(combined));

          _mm256_store_si256(
            reinterpret_cast<__m256i*>(buf64),
            _mm256_and_si256(_mm256_srlv_epi64(v, shifts_lo), vmask));
          callback(static_cast<uint32_t>(buf64[0]));
          callback(static_cast<uint32_t>(buf64[1]));
          callback(static_cast<uint32_t>(buf64[2]));
          callback(static_cast<uint32_t>(buf64[3]));
        }
      }

      // Scalar tail
      if (i < total_block_count) {
        return ForEachBlockIndexScalar(
          long_array, bits_per_block, total_block_count,
          std::forward<Callback>(callback), i);
      }
      return {};
    }

    template <std::invocable<uint32_t> Callback>
    [[nodiscard]] Expected<void> ForEachBlockIndex(
      std::span<const int64_t> long_array,
      uint32_t bits_per_block,
      uint64_t total_block_count,
      Callback&& callback) {
      if (bits_per_block >= 1 && bits_per_block <= 15) {
        return ForEachBlockIndexAVX2(
          long_array, bits_per_block, total_block_count,
          std::forward<Callback>(callback));
      }
      return ForEachBlockIndexScalar(
        long_array, bits_per_block, total_block_count,
        std::forward<Callback>(callback));
    }

    [[nodiscard]] inline int64_t ReadAxis(
      const nbt::tag_compound& size_compound,
      const std::string& key) {
      if (size_compound.has_key(key, nbt::tag_type::Int)) {
        return static_cast<int64_t>(
          size_compound.at(key).as<nbt::tag_int>());
      }
      return 0;
    }

    [[nodiscard]] inline uint64_t AbsoluteDimension(int64_t v) noexcept {
      return v < 0 ? static_cast<uint64_t>(-(v + 1)) + 1
        : static_cast<uint64_t>(v);
    }

    [[nodiscard]] inline std::vector<std::string>
      ExtractPaletteNames(const nbt::tag_list& palette) {
      std::vector<std::string> names;
      names.reserve(palette.size());
      for (std::size_t i = 0; i < palette.size(); ++i) {
        const auto& entry = palette.at(i);
        if (entry.get().get_type() == nbt::tag_type::Compound) {
          const auto& compound = entry.as<nbt::tag_compound>();
          if (compound.has_key("Name", nbt::tag_type::String)) {
            names.push_back(
              compound.at("Name").as<nbt::tag_string>().get());
            continue;
          }
        }
        names.emplace_back();
      }
      return names;
    }

  }  // namespace internal

  struct MaterialEntry {
    std::string block_name;
    uint64_t count = 0;
  };

  using MaterialList = std::vector<MaterialEntry>;

  // ListMaterials

  [[nodiscard]] inline Expected<MaterialList>
    ListMaterials(const nbt::tag_compound& root) {
    if (!root.has_key("Regions", nbt::tag_type::Compound)) {
      return std::unexpected(LcmpError{
          ErrorCode::MissingRegions,
          "Missing 'Regions' compound in root" });
    }

    const auto& regions = root.at("Regions").as<nbt::tag_compound>();
    absl::flat_hash_map<std::string, uint64_t> global_counts;

    for (const auto& [region_name, region_value] : regions) {
      (void)region_name;
      if (region_value.get().get_type() != nbt::tag_type::Compound)
        continue;
      const auto& region = region_value.as<nbt::tag_compound>();

      if (!region.has_key("Size", nbt::tag_type::Compound)) continue;
      const auto& size_compound =
        region.at("Size").as<nbt::tag_compound>();

      const int64_t size_x = internal::ReadAxis(size_compound, "x");
      const int64_t size_y = internal::ReadAxis(size_compound, "y");
      const int64_t size_z = internal::ReadAxis(size_compound, "z");

      const uint64_t volume = internal::AbsoluteDimension(size_x) *
        internal::AbsoluteDimension(size_y) *
        internal::AbsoluteDimension(size_z);

      if (!region.has_key("BlockStatePalette", nbt::tag_type::List))
        continue;
      const auto& palette =
        region.at("BlockStatePalette").as<nbt::tag_list>();
      std::vector<std::string> palette_names =
        internal::ExtractPaletteNames(palette);

      if (!region.has_key("BlockStates", nbt::tag_type::Long_Array))
        continue;
      const auto& long_data =
        region.at("BlockStates").as<nbt::tag_array<int64_t>>();
      const auto& long_array = long_data.get();

      const uint32_t bits_per_block =
        internal::CalculateBitsPerBlock(palette_names.size());

      // Local histogram: array-indexed, L1-resident for palette ≤ 256
      // Replaces volume × hash-map lookups with volume × array increments
      std::vector<uint64_t> local_counts(palette_names.size(), 0);
      const std::size_t pal_size = palette_names.size();

      auto result = internal::ForEachBlockIndex(
        long_array, bits_per_block, volume,
        [&local_counts, pal_size](uint32_t index) {
          if (index < pal_size) [[likely]] {
            ++local_counts[index];
          }
        });

      if (!result.has_value()) continue;

      // Fold local histogram into global hash map
      // String hashing done pal_size times per region, not volume times
      for (std::size_t i = 0; i < pal_size; ++i) {
        if (local_counts[i] == 0) continue;
        const std::string& name = palette_names[i];
        if (!name.empty() && name != "minecraft:air") {
          global_counts[name] += local_counts[i];
        }
      }
    }

    MaterialList materials;
    materials.reserve(global_counts.size());
    for (auto& [name, count] : global_counts) {
      materials.push_back({ std::move(name), count });
    }

    std::ranges::sort(materials, {}, &MaterialEntry::block_name);

    return materials;
  }

  [[nodiscard]] inline Expected<MaterialList>
    ListMaterialsFromFile(const std::filesystem::path& path) {
    auto root_result = ReadLitematicFile(path);
    if (!root_result.has_value()) {
      return std::unexpected(root_result.error());
    }
    return ListMaterials(*root_result.value());
  }

  // Diff

  struct BlockDelta {
    std::string block_name;
    int64_t delta = 0;
    uint64_t abs_delta = 0;
    uint64_t base_count = 0;
    uint64_t target_count = 0;
  };

  struct SchematicDiff {
    std::vector<BlockDelta> changed;
    std::vector<BlockDelta> unchanged;

    struct Summary {
      int64_t total_block_delta = 0;
      std::size_t changed_types = 0;
      std::size_t unchanged_types = 0;
    } summary;

    [[nodiscard]] bool IsIdentical() const noexcept {
      return changed.empty() && summary.total_block_delta == 0;
    }
  };

  namespace internal {

    inline void EmitRemoval(SchematicDiff& diff, MaterialEntry& entry) {
      BlockDelta d{
          .block_name = std::move(entry.block_name),
          .delta = -static_cast<int64_t>(entry.count),
          .abs_delta = entry.count,
          .base_count = entry.count,
          .target_count = 0,
      };
      diff.summary.total_block_delta += d.delta;
      diff.changed.push_back(std::move(d));
      ++diff.summary.changed_types;
    }

    inline void EmitAddition(SchematicDiff& diff, MaterialEntry& entry) {
      BlockDelta d{
          .block_name = std::move(entry.block_name),
          .delta = static_cast<int64_t>(entry.count),
          .abs_delta = entry.count,
          .base_count = 0,
          .target_count = entry.count,
      };
      diff.summary.total_block_delta += d.delta;
      diff.changed.push_back(std::move(d));
      ++diff.summary.changed_types;
    }

  }  // namespace internal

  [[nodiscard]] inline SchematicDiff
    DiffMaterialLists(MaterialList base, MaterialList target) {
    SchematicDiff diff;

    std::size_t i = 0;
    std::size_t j = 0;

    while (i < base.size() && j < target.size()) {
      auto& b = base[i];
      auto& t = target[j];

      if (b.block_name < t.block_name) {
        internal::EmitRemoval(diff, b);
        ++i;
      }
      else if (t.block_name < b.block_name) {
        internal::EmitAddition(diff, t);
        ++j;
      }
      else {
        const int64_t delta =
          static_cast<int64_t>(t.count) - static_cast<int64_t>(b.count);

        BlockDelta d{
            .block_name = std::move(b.block_name),
            .delta = delta,
            .abs_delta = static_cast<uint64_t>(std::abs(delta)),
            .base_count = b.count,
            .target_count = t.count,
        };

        if (delta != 0) {
          diff.summary.total_block_delta += delta;
          diff.changed.push_back(std::move(d));
          ++diff.summary.changed_types;
        }
        else {
          diff.unchanged.push_back(std::move(d));
          ++diff.summary.unchanged_types;
        }
        ++i;
        ++j;
      }
    }

    while (i < base.size()) {
      internal::EmitRemoval(diff, base[i]);
      ++i;
    }
    while (j < target.size()) {
      internal::EmitAddition(diff, target[j]);
      ++j;
    }

    pdqsort(diff.changed.begin(), diff.changed.end(),
      [](const BlockDelta& a, const BlockDelta& b) {
        if (a.abs_delta != b.abs_delta) return a.abs_delta > b.abs_delta;
        return a.block_name < b.block_name;
      });

    std::ranges::sort(diff.unchanged, {}, &BlockDelta::block_name);

    return diff;
  }

  [[nodiscard]] inline Expected<SchematicDiff>
    DiffLitematicFiles(const std::filesystem::path& base_path,
      const std::filesystem::path& target_path) {
    auto base_root_result = ReadLitematicFile(base_path);
    if (!base_root_result.has_value()) {
      return std::unexpected(base_root_result.error());
    }

    auto base_list_result = ListMaterials(*base_root_result.value());
    if (!base_list_result.has_value()) {
      return std::unexpected(base_list_result.error());
    }

    auto target_root_result = ReadLitematicFile(target_path);
    if (!target_root_result.has_value()) {
      return std::unexpected(target_root_result.error());
    }

    auto target_list_result = ListMaterials(*target_root_result.value());
    if (!target_list_result.has_value()) {
      return std::unexpected(target_list_result.error());
    }

    return DiffMaterialLists(std::move(*base_list_result),
      std::move(*target_list_result));
  }

}  // namespace lcmp

#endif  // LCMP_LCMP_H_