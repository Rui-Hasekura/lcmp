#ifndef LCMP_CORE_LCMP_H
#define LCMP_CORE_LCMP_H

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include <pdqsort.h>

#include "common/reader.h"
#include "common/lister.h"

namespace lcmp {

  struct BlockDelta {
    std::string block_name;
    int64_t delta = 0;
    uint64_t abs_delta = 0;
    uint64_t base_count = 0;
    uint64_t new_count = 0;
  };

  struct SchematicDiff {
    std::vector<BlockDelta> changed;
    std::vector<BlockDelta> unchanged;
    SchematicInfo base_info;
    SchematicInfo modified_info;

    struct Summary {
      int64_t total_block_delta = 0;
      size_t changed_types = 0;
      size_t unchanged_types = 0;
    } summary;

    [[nodiscard]] bool IsIdentical() const {
      return changed.empty() && summary.total_block_delta == 0;
    }
  };

  inline SchematicDiff DiffMaterialLists(
    MaterialList base,
    MaterialList modified) {

    SchematicDiff diff;

    size_t i = 0, j = 0;

    while (i < base.size() && j < modified.size()) {
      auto& b = base[i];
      auto& m = modified[j];

      if (b.block_name < m.block_name) {
        BlockDelta d;
        d.base_count = b.count;
        d.new_count = 0;
        d.delta = -static_cast<int64_t>(b.count);
        d.abs_delta = b.count;
        d.block_name = std::move(b.block_name);

        diff.summary.total_block_delta += d.delta;
        diff.changed.push_back(std::move(d));
        ++diff.summary.changed_types;
        ++i;
      }
      else if (m.block_name < b.block_name) {
        BlockDelta d;
        d.base_count = 0;
        d.new_count = m.count;
        d.delta = static_cast<int64_t>(m.count);
        d.abs_delta = m.count;
        d.block_name = std::move(m.block_name);

        diff.summary.total_block_delta += d.delta;
        diff.changed.push_back(std::move(d));
        ++diff.summary.changed_types;
        ++j;
      }
      else {
        const int64_t delta =
          static_cast<int64_t>(m.count) -
          static_cast<int64_t>(b.count);

        BlockDelta d;
        d.base_count = b.count;
        d.new_count = m.count;
        d.delta = delta;
        d.abs_delta = static_cast<uint64_t>(std::abs(delta));
        d.block_name = std::move(b.block_name);

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
      auto& b = base[i];
      BlockDelta d;
      d.base_count = b.count;
      d.new_count = 0;
      d.delta = -static_cast<int64_t>(b.count);
      d.abs_delta = b.count;
      d.block_name = std::move(b.block_name);

      diff.summary.total_block_delta += d.delta;
      diff.changed.push_back(std::move(d));
      ++diff.summary.changed_types;
      ++i;
    }

    while (j < modified.size()) {
      auto& m = modified[j];
      BlockDelta d;
      d.base_count = 0;
      d.new_count = m.count;
      d.delta = static_cast<int64_t>(m.count);
      d.abs_delta = m.count;
      d.block_name = std::move(m.block_name);

      diff.summary.total_block_delta += d.delta;
      diff.changed.push_back(std::move(d));
      ++diff.summary.changed_types;
      ++j;
    }

    pdqsort(
      diff.changed.begin(),
      diff.changed.end(),
      [](const BlockDelta& a, const BlockDelta& b) {
        if (a.abs_delta != b.abs_delta) return a.abs_delta > b.abs_delta;
        return a.block_name < b.block_name;
      });

    pdqsort(
      diff.unchanged.begin(),
      diff.unchanged.end(),
      [](const BlockDelta& a, const BlockDelta& b) {
        return a.block_name < b.block_name;
      });

    return diff;
  }

  inline SchematicDiff DiffLitematicFiles(
    const std::filesystem::path& base_path,
    const std::filesystem::path& modified_path) {

    const ReadResult base_read = ReadLitematicFile(base_path);
    MaterialList base = ListMaterials(*base_read.root);

    const ReadResult mod_read = ReadLitematicFile(modified_path);
    MaterialList modified = ListMaterials(*mod_read.root);

    SchematicDiff diff = DiffMaterialLists(
      std::move(base), std::move(modified));

    diff.base_info.name = std::move(base_read.info.name);
    diff.base_info.author = std::move(base_read.info.author);
    diff.base_info.description = std::move(base_read.info.description);
    diff.base_info.total_blocks = base_read.info.total_blocks;
    diff.base_info.total_volume = base_read.info.total_volume;

    diff.modified_info.name = std::move(mod_read.info.name);
    diff.modified_info.author = std::move(mod_read.info.author);
    diff.modified_info.description = std::move(mod_read.info.description);
    diff.modified_info.total_blocks = mod_read.info.total_blocks;
    diff.modified_info.total_volume = mod_read.info.total_volume;

    return diff;
  }

}  // namespace lcmp

#endif  // LCMP_CORE_LCMP_H