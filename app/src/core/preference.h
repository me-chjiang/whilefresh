#pragma once

#include <QString>

namespace whilefresh::core {

// One remembered taste or habit ("不吃香菜"). `source` is "user" for what the user typed and "ai"
// for what a service proposed and the user kept.
struct Preference {
    qint64 id = 0;
    QString text;
    QString source = QStringLiteral("user");
    QString createdAt;
};

inline constexpr int kPreferenceMaxChars = 100;
inline constexpr int kPreferenceMaxCount = 50;

}
