#pragma once

#include <QObject>
#include <QString>

namespace whilefresh::ui {

class AppInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString platform READ platform CONSTANT)
    Q_PROPERTY(bool recognitionConfigured READ recognitionConfigured CONSTANT)

public:
    explicit AppInfo(QObject *parent = nullptr);

    QString version() const;
    QString platform() const;
    bool recognitionConfigured() const;
};

}
