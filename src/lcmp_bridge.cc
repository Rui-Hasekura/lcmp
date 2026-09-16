// Copyright (C) 2026 Rui-Hasekura <ruihasekura@gmail.com>
// SPDX-License-Identifier: GPL-3.0-only

#include "lcmp_bridge.h"

#include <QUrl>
#include <QVariantList>

#include "core/lcmp.h"

  LcmpBridge::LcmpBridge(QObject* parent)
      : QObject(parent) {}

  QVariantMap LcmpBridge::compareFiles(
      const QString& baseUrl,
      const QString& modifiedUrl
      ) {
          QVariantMap result;
          QString basePath = QUrl(baseUrl).toLocalFile();
          if (basePath.isEmpty()) basePath = baseUrl;

          QString modPath = QUrl(modifiedUrl).toLocalFile();
          if (modPath.isEmpty()) modPath = modifiedUrl;

          // 调用新版 DiffLitematicFiles，返回 Expected<SchematicDiff>
          auto diffResult = lcmp::DiffLitematicFiles(basePath.toStdString(), modPath.toStdString());

          if (diffResult.has_value()) {
              const auto& diff = diffResult.value();

              result["success"] = true;
              result["totalDelta"] = static_cast<qint64>(diff.summary.total_block_delta);
              result["changedTypes"] = static_cast<qint64>(diff.summary.changed_types);

              QVariantList changedList;
              for (const auto& item : diff.changed) {
                  QVariantMap m;
                  m["name"] = QString::fromStdString(item.block_name);
                  m["delta"] = static_cast<qint64>(item.delta);
                  m["baseCount"] = static_cast<qint64>(item.base_count);
                  m["newCount"] = static_cast<qint64>(item.new_count);
                  changedList.append(m);
              }
              result["changed"] = changedList;
          }
          else {
              // 处理错误返回
              const auto& error = diffResult.error();
              result["success"] = false;
              result["error"] = QString::fromStdString(error.message);
              result["errorCode"] = static_cast<int>(error.code);
          }

          return result;
  }