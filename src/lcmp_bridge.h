// Copyright (C) 2026 Rui-Hasekura <ruihasekura@gmail.com>
// SPDX-License-Identifier: GPL-3.0-only

#ifndef LCMP_LCMP_BRIDGE_H_
#define LCMP_LCMP_BRIDGE_H_

#include <QObject>
#include <QVariantMap>
#include <qqmlintegration.h>

class LcmpBridge : public QObject {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit LcmpBridge(QObject* parent = nullptr);

    Q_INVOKABLE QVariantMap compareFiles(
        const QString& baseUrl,
        const QString& modifiedUrl
        );
};

#endif