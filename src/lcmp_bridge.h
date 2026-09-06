#ifndef LCMP_LCMP_BRIDGE_H_
#define LCMP_LCMP_BRIDGE_H_

#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <qqmlintegration.h>

#include "core/lcmp.h"

class LcmpBridge : public QObject {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit LcmpBridge(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QVariantMap compareFiles(const QString &baseUrl, const QString &modifiedUrl) {
        QVariantMap result;
        QString basePath = QUrl(baseUrl).toLocalFile();
        if (basePath.isEmpty()) basePath = baseUrl;

        QString modPath = QUrl(modifiedUrl).toLocalFile();
        if (modPath.isEmpty()) modPath = modifiedUrl;

        try {
            auto diff = lcmp::DiffLitematicFiles(basePath.toStdString(), modPath.toStdString());

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
        } catch (const std::exception& e) {
            result["success"] = false;
            result["error"] = QString::fromUtf8(e.what());
        }
        return result;
    }
};

#endif // LCMP_LCMP_BRIDGE_H_