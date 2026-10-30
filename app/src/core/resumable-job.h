#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

namespace whilefresh::core {

// A job done in batches whose progress survives a process restart (R-ADR-9): import recognition
// now, cooking sessions later. The owner persists state() and batches() after every transition
// and rebuilds the job with restore(); the job itself does no I/O.
//
//   Pending --start--> Running --completeBatch (last)--> Done
//                         |  ^
//                    fail |  | start (resumes at nextBatch())
//                         v  |
//                        Failed
//
// The batch count is learnt once the work is planned (e.g. after slicing the images); batches
// complete strictly in order, each carrying an opaque result the owner needs to finish the job.
class ResumableJob
{
public:
    enum class State { Pending, Running, Done, Failed };
    enum class BatchState { Pending, Done, Failed };

    struct Batch {
        BatchState state = BatchState::Pending;
        QByteArray result; // set once Done
    };

    static QString stateKey(State state); // "pending", "running", "done", "failed"
    static std::optional<State> stateFromKey(const QString &key);
    static QString batchStateKey(BatchState state);
    static std::optional<BatchState> batchStateFromKey(const QString &key);

    // From persisted rows. A job that was Running when the process died comes back Failed: the
    // request in flight is lost and has to be sent again. Nullopt when the rows are inconsistent
    // (a Done job with unfinished batches, a done batch after an unfinished one).
    static std::optional<ResumableJob> restore(State state, const QList<Batch> &batches);

    State state() const { return m_state; }
    const QList<Batch> &batches() const { return m_batches; }
    int batchCount() const { return int(m_batches.size()); }
    // First batch that is not done; batchCount() when all are (or none is planned yet).
    int nextBatch() const;
    QList<QByteArray> results() const; // of the done batches, in order

    // Pending or Failed -> Running.
    bool start();
    // Sets the number of batches while Running. Accepted when nothing is planned yet or the count
    // is unchanged; a different count means earlier results no longer line up, so false.
    bool plan(int count);
    // Marks `index` done; it must be nextBatch(). The last one moves the job to Done.
    bool completeBatch(int index, const QByteArray &result);
    // Running -> Failed; the batch that was being worked on is marked Failed.
    bool fail();
    // Forgets every batch and goes back to Pending, e.g. when the plan changed.
    void reset();

private:
    State m_state = State::Pending;
    QList<Batch> m_batches;
};

}
