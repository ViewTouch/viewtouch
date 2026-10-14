#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QThread>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

// A live stress test (vtmodern --stress): a busy dinner played on a
// throwaway store while the screen runs. Pretend staff work real
// terminals - servers (half on handhelds, over the network like the real
// ones), a host seating the waitlist and bussing, a bartender's tabs,
// phone orders and a driver, the kitchen and expo bumping, a manager
// running reports, 86ing items and approving voids - with tickets going to
// slow pretend printers. Meanwhile it measures how the screen keeps up
// (event loop delays), how long each command takes, the database queue,
// memory and CPU, and at the end checks that every check balances and
// that the database holds exactly what the screen does.

namespace vt::app { class PosShared; class PosSession; }
namespace vt::layout { struct Layout; }
namespace vt::storage { class AsyncWriter; }

namespace vt::net {

class LayoutHub;
class PosServer;
class FakePrinters;

struct StressOptions {
    int minutes = 10;          // the dinner, start to last check
    double speed = 12;         // times faster than real life (12: two hours in ten minutes)
    int servers = 6;           // pretend servers (every other one on a handheld)
    int watchers = 3;          // more handhelds that only watch (host iPad, expo, office)
    QStringList tables;        // the dining room (from the floor plans)
    QStringList barSeats;      // the bar's seats
    QString dbPath;            // checked against at the end
    storage::AsyncWriter *writer = nullptr;
};

class StressRunner : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY statsChanged)
    Q_PROPERTY(bool finished READ isFinished NOTIFY statsChanged)
    Q_PROPERTY(bool passed READ passed NOTIFY statsChanged)
    Q_PROPERTY(QString report READ report NOTIFY statsChanged)

public:
    StressRunner(app::PosShared *shared, LayoutHub *hub, StressOptions options, QObject *parent = nullptr);
    ~StressRunner() override;

    // The tables on the floor plans: the dining room, and the bar's seats ("Bar 1").
    static void tablesOn(const layout::Layout &layout, QStringList &tables, QStringList &barSeats);
    // The staff, the pretend printers, the server for the handhelds; then the dinner.
    bool start(QString *error);
    QVariantMap stats() const;
    bool isFinished() const { return finished_; }
    bool passed() const { return passed_; }
    QString report() const { return report_; }
    // The screen drew a frame (for frames a second).
    void frameDrawn() { ++frames_; }

    // For the people (bots): how it went.
    struct Bot;
    void recordCommand(const QString &method, bool remote, qint64 nsecs);
    void recordRefused(const QString &who, const QString &method, const QString &why, bool expected);
    std::int64_t simMinute() const;   // one pretend minute, in real milliseconds
    double intensity() const;         // 0..1: how busy it is right now (the rush in the middle)
    bool winding() const { return winding_; }
    QStringList freeTables(const QVariantMap &floor) const;
    QStringList serverIds() const { return serverIds_; }
    void counted(const char *what, int n = 1)
    {
        const QMutexLocker lock(&mutex_);
        counts_[QString::fromLatin1(what)] += n;
    }

signals:
    void statsChanged();
    void done(bool passed);

private:
    void sample();      // every 20 ms: how late was it?
    void everySecond();
    void windDown();
    void finish();
    void checkEverything(QStringList &problems, QStringList &notes);
    void measureUpdate();   // in the rush: what one screen's update costs, piece by piece

    app::PosShared *shared_;
    LayoutHub *hub_;
    StressOptions o_;
    std::unique_ptr<PosServer> server_;
    std::unique_ptr<FakePrinters> printers_;
    std::vector<std::unique_ptr<Bot>> bots_;
    std::vector<std::unique_ptr<app::PosSession>> watchers_;
    // The handhelds run on a thread of their own, like separate devices: their
    // work (reading the store's updates) isn't this computer's.
    QThread handhelds_;
    QObject *handheldHome_ = nullptr;
    mutable QRecursiveMutex mutex_;   // what the handhelds report
    QStringList serverIds_;
    QStringList drinks_, food_;   // what guests order
    QString driverId_;

    QElapsedTimer clock_;
    QTimer sampler_, second_;
    qint64 lastSample_ = 0;
    std::vector<int> lagWindow_;            // this minute's delays (ms)
    std::vector<int> lagAll_;
    int stalls100_ = 0, stalls250_ = 0, stalls1000_ = 0, worstLag_ = 0;
    int frames_ = 0, fps_ = 0;
    struct Timing { int n = 0; qint64 total = 0, worst = 0; std::vector<int> us; };
    QHash<QString, Timing> local_, remote_;
    QHash<QString, int> counts_;
    QHash<QString, int> refusedWhy_;        // "who: what - why"
    int refused_ = 0, unexpected_ = 0;
    std::vector<int> actionsPerSecond_;
    std::atomic<int> actionsThisSecond_{0};
    int dbPending_ = 0, dbWorst_ = 0;
    double cpu_ = 0, rssMb_ = 0, rssWorstMb_ = 0;
    qint64 cpuTicks_ = 0, cpuAt_ = 0;
    std::atomic<bool> winding_{false}, finished_{false};
    bool passed_ = false;
    qint64 windStartedMs_ = 0;
    QString report_;
    QString updateCost_;
    QStringList timeline_;            // every 10 seconds: how it was going
    qint64 lastUpdateNsecs_ = 0;
    int lastUpdateRounds_ = 0;
    bool measured_ = false;
    QVariantMap last_;
};

} // namespace vt::net
