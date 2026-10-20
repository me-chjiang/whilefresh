#pragma once

#include <QDate>
#include <QDateTime>

#include <utility>

namespace whilefresh::core {

// Source of "now"; injected wherever freshness or timestamps are computed so tests can move time.
class Clock
{
public:
    virtual ~Clock() = default;
    virtual QDateTime now() const = 0;
    QDate today() const { return now().toLocalTime().date(); }
};

class SystemClock : public Clock
{
public:
    QDateTime now() const override { return QDateTime::currentDateTime(); }
};

class FixedClock : public Clock
{
public:
    explicit FixedClock(QDateTime now) : m_now(std::move(now)) {}
    QDateTime now() const override { return m_now; }
    void set(const QDateTime &now) { m_now = now; }
    void advanceDays(int days) { m_now = m_now.addDays(days); }

private:
    QDateTime m_now;
};

}
