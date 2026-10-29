#include "core/resumable-job.h"

#include <QTest>

using namespace whilefresh::core;

using State = ResumableJob::State;
using BatchState = ResumableJob::BatchState;

class ResumableJobTest : public QObject
{
    Q_OBJECT

private slots:
    void runsBatchesInOrderToDone()
    {
        ResumableJob job;
        QCOMPARE(job.state(), State::Pending);
        QVERIFY(!job.plan(3)); // only while running
        QVERIFY(job.start());
        QVERIFY(job.plan(3));
        QCOMPARE(job.nextBatch(), 0);
        QVERIFY(!job.completeBatch(1, "b")); // out of order
        QVERIFY(job.completeBatch(0, "a"));
        QVERIFY(job.completeBatch(1, "b"));
        QCOMPARE(job.state(), State::Running);
        QVERIFY(job.completeBatch(2, "c"));
        QCOMPARE(job.state(), State::Done);
        QCOMPARE(job.results(), (QList<QByteArray>{ "a", "b", "c" }));
        QVERIFY(!job.start());
    }

    void failureResumesAtTheFailedBatch()
    {
        ResumableJob job;
        QVERIFY(job.start());
        QVERIFY(job.plan(3));
        QVERIFY(job.completeBatch(0, "a"));
        QVERIFY(job.fail());
        QCOMPARE(job.state(), State::Failed);
        QCOMPARE(job.batches().at(1).state, BatchState::Failed);
        QCOMPARE(job.nextBatch(), 1);
        QVERIFY(job.start());
        QVERIFY(job.plan(3));  // same plan again is fine
        QVERIFY(!job.plan(4)); // a different one is not
        QVERIFY(job.completeBatch(1, "b"));
        QVERIFY(job.completeBatch(2, "c"));
        QCOMPARE(job.state(), State::Done);
    }

    void restoresPersistedRows()
    {
        const QList<ResumableJob::Batch> rows{ { BatchState::Done, "a" }, { BatchState::Pending, {} }, { BatchState::Pending, {} } };
        // A job that was running when the process died lost its request: it comes back failed.
        const auto job = ResumableJob::restore(State::Running, rows);
        QVERIFY(job);
        QCOMPARE(job->state(), State::Failed);
        QCOMPARE(job->nextBatch(), 1);
        QCOMPARE(job->batches().at(1).state, BatchState::Failed);
        QCOMPARE(job->results(), QList<QByteArray>{ "a" });

        const auto done = ResumableJob::restore(State::Done, { { BatchState::Done, "a" } });
        QVERIFY(done);
        QCOMPARE(done->state(), State::Done);

        const auto pending = ResumableJob::restore(State::Pending, {});
        QVERIFY(pending);
        QCOMPARE(pending->nextBatch(), 0);
    }

    void rejectsInconsistentRows()
    {
        QVERIFY(!ResumableJob::restore(State::Done, { { BatchState::Done, "a" }, { BatchState::Pending, {} } }));
        QVERIFY(!ResumableJob::restore(State::Done, {}));
        QVERIFY(!ResumableJob::restore(State::Failed, { { BatchState::Failed, {} }, { BatchState::Done, "b" } }));
    }

    void resetForgetsBatches()
    {
        ResumableJob job;
        QVERIFY(job.start());
        QVERIFY(job.plan(2));
        QVERIFY(job.completeBatch(0, "a"));
        job.reset();
        QCOMPARE(job.state(), State::Pending);
        QCOMPARE(job.batchCount(), 0);
        QVERIFY(job.start());
        QVERIFY(job.plan(4));
    }

    void keysRoundTrip()
    {
        for (State state : { State::Pending, State::Running, State::Done, State::Failed })
            QCOMPARE(ResumableJob::stateFromKey(ResumableJob::stateKey(state)), std::optional<State>(state));
        for (BatchState state : { BatchState::Pending, BatchState::Done, BatchState::Failed })
            QCOMPARE(ResumableJob::batchStateFromKey(ResumableJob::batchStateKey(state)), std::optional<BatchState>(state));
        QVERIFY(!ResumableJob::stateFromKey(QStringLiteral("paused")));
    }
};

QTEST_GUILESS_MAIN(ResumableJobTest)
#include "resumable-job-test.moc"
