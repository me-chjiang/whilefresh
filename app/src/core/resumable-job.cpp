#include "core/resumable-job.h"

namespace whilefresh::core {

QString ResumableJob::stateKey(State state)
{
    switch (state) {
    case State::Pending:
        return QStringLiteral("pending");
    case State::Running:
        return QStringLiteral("running");
    case State::Done:
        return QStringLiteral("done");
    case State::Failed:
        return QStringLiteral("failed");
    }
    return QStringLiteral("pending");
}

std::optional<ResumableJob::State> ResumableJob::stateFromKey(const QString &key)
{
    for (State state : { State::Pending, State::Running, State::Done, State::Failed }) {
        if (stateKey(state) == key)
            return state;
    }
    return std::nullopt;
}

QString ResumableJob::batchStateKey(BatchState state)
{
    switch (state) {
    case BatchState::Pending:
        return QStringLiteral("pending");
    case BatchState::Done:
        return QStringLiteral("done");
    case BatchState::Failed:
        return QStringLiteral("failed");
    }
    return QStringLiteral("pending");
}

std::optional<ResumableJob::BatchState> ResumableJob::batchStateFromKey(const QString &key)
{
    for (BatchState state : { BatchState::Pending, BatchState::Done, BatchState::Failed }) {
        if (batchStateKey(state) == key)
            return state;
    }
    return std::nullopt;
}

std::optional<ResumableJob> ResumableJob::restore(State state, const QList<Batch> &batches)
{
    ResumableJob job;
    job.m_batches = batches;
    bool unfinishedSeen = false;
    for (const Batch &batch : batches) {
        if (batch.state == BatchState::Done && unfinishedSeen)
            return std::nullopt;
        unfinishedSeen = unfinishedSeen || batch.state != BatchState::Done;
    }
    if (state == State::Done && (batches.isEmpty() || unfinishedSeen))
        return std::nullopt;
    job.m_state = state;
    if (state == State::Running) {
        job.m_state = State::Failed;
        if (job.nextBatch() < job.batchCount())
            job.m_batches[job.nextBatch()].state = BatchState::Failed;
    }
    return job;
}

int ResumableJob::nextBatch() const
{
    for (int i = 0; i < m_batches.size(); ++i) {
        if (m_batches.at(i).state != BatchState::Done)
            return i;
    }
    return int(m_batches.size());
}

QList<QByteArray> ResumableJob::results() const
{
    QList<QByteArray> list;
    for (const Batch &batch : m_batches) {
        if (batch.state == BatchState::Done)
            list.append(batch.result);
    }
    return list;
}

bool ResumableJob::start()
{
    if (m_state != State::Pending && m_state != State::Failed)
        return false;
    m_state = State::Running;
    return true;
}

bool ResumableJob::plan(int count)
{
    if (m_state != State::Running || count <= 0)
        return false;
    if (m_batches.isEmpty()) {
        m_batches.resize(count);
        return true;
    }
    return m_batches.size() == count;
}

bool ResumableJob::completeBatch(int index, const QByteArray &result)
{
    if (m_state != State::Running || index != nextBatch() || index >= m_batches.size())
        return false;
    m_batches[index] = { BatchState::Done, result };
    if (nextBatch() == m_batches.size())
        m_state = State::Done;
    return true;
}

bool ResumableJob::fail()
{
    if (m_state != State::Running)
        return false;
    m_state = State::Failed;
    if (nextBatch() < m_batches.size())
        m_batches[nextBatch()].state = BatchState::Failed;
    return true;
}

void ResumableJob::reset()
{
    m_state = State::Pending;
    m_batches.clear();
}

}
