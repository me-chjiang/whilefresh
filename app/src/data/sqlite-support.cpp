#include "data/sqlite-support.h"

#include <QSqlDatabase>

namespace whilefresh::data {

bool isSqliteDriverAvailable()
{
    return QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"));
}

}
