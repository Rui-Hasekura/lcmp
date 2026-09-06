// Copyright (C) 2026 Rui-Hasekura <ruihasekura@gmail.com>
// SPDX-License-Identifier: GPL-3.0-only

#ifndef LCMP_CORE_LCMP_H_
#define LCMP_CORE_LCMP_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "common/lister.h"
#include "common/reader.h"

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

SchematicDiff DiffMaterialLists(
    MaterialList base,
    MaterialList modified);

SchematicDiff DiffLitematicFiles(
    const std::filesystem::path& base_path,
    const std::filesystem::path& modified_path);

}  // namespace lcmp

#endif  // LCMP_CORE_LCMP_H_