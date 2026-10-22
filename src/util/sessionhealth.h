#pragma once

#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <atomic>
#include <optional>

class QQuickWindow;

/// Where one frame's time went, from QQuickWindow's signals. beforeSync runs
/// from the end of animations to the start of the sync: polish (bindings,
/// layout, JavaScript) plus any wait for the render thread. sync is the
/// scene graph sync (updatePaintNode: waveform data, texture uploads), render
/// the draw calls, swap the buffer swap (vsync and GPU). -1 where a signal did
/// not come.
struct SessionHealthFramePhases {
    qint64 totalMs = -1;
    qint64 beforeSyncMs = -1;
    qint64 syncMs = -1;
    qint64 renderMs = -1;
    qint64 swapMs = -1;
};

/// One minute of how the UI kept up: main-thread lateness and the gaps
/// between rendered frames. Kept apart from the timers so it can be tested.
class SessionHealthWindow {
  public:
    /// A gap longer than this is the UI having nothing to draw, not a stall.
    static constexpr qint64 kIdleFrameGapMs = 2000;

    void addMainThreadLag(qint64 lagMs);
    void addFrameGap(qint64 gapMs);
    void reset();

    qint64 maxLagMs = 0;
    int lagsOver50Ms = 0;
    int frames = 0;
    qint64 maxFrameGapMs = 0;
    int frameGapsOver50Ms = 0;
    int frameGapsOver150Ms = 0;
    /// The slowest frame of the minute, split into its phases.
    std::optional<SessionHealthFramePhases> slowestFrame;
};

/// What the operating system says about Mixxx and the machine at one moment.
/// Fields are empty where the platform gives no way to read them.
struct SessionHealthSystemSample {
    std::optional<double> workingSetMb;
    std::optional<double> privateMb;
    std::optional<int> handles;
    std::optional<int> gdiObjects;
    std::optional<int> userObjects;
    std::optional<double> processCpuPercentOfOneCore;
    std::optional<double> systemCpuPercent;
    std::optional<int> memoryLoadPercent;
    std::optional<double> availableMb;
    /// The CPU's actual speed in percent of its base clock (over 100 with
    /// turbo, well under when hot or held back by power settings).
    std::optional<double> cpuPerformancePercent;
    std::optional<int> cpuBaseMhz;
    std::optional<int> cpuLimitMhz;
    std::optional<bool> onMains;
    std::optional<int> batteryPercent;
    std::optional<bool> batterySaver;
};

/// Writes one line a minute to the log saying how the UI kept up and what the
/// process and machine looked like: memory, handles, CPU load, CPU clock and
/// its limit (a hot or unplugged laptop clocks down), power. Made for long
/// sessions that slowly get worse (the Surface on 22 Oct 2026 went from
/// ~20 ms frames to 300 ms stalls over seven hours), where a single look
/// cannot tell a leak from a throttled CPU or a machine out of memory.
class SessionHealthMonitor : public QObject {
    Q_OBJECT
  public:
    static constexpr int kReportIntervalMs = 60 * 1000;
    static constexpr int kLagProbeIntervalMs = 100;

    explicit SessionHealthMonitor(QObject* pParent = nullptr);
    ~SessionHealthMonitor() override;

    /// The window whose frames are measured; a QML reload makes a new one.
    void setWindow(QQuickWindow* pWindow);

    static QString formatReport(qint64 uptimeMs,
            const SessionHealthWindow& window,
            const SessionHealthSystemSample& sample);

  private slots:
    void slotLagProbe();
    void slotFrameEnd();
    void slotReport();

  private:
    static qint64 nowNs();
    /// Called on the thread that rendered, once the frame is swapped.
    void recordFramePhases();

    SessionHealthSystemSample sampleSystem();

    QTimer m_lagProbe;
    QTimer m_report;
    QElapsedTimer m_uptime;
    QElapsedTimer m_sinceLastProbe;
    QElapsedTimer m_sinceLastFrame;
    QPointer<QQuickWindow> m_pWindow;
    SessionHealthWindow m_window;

    // Phase timestamps of the frame in flight (steady clock, ns). Written on
    // the GUI thread (animations) and the render thread (the rest).
    std::atomic<qint64> m_animatedNs{0};
    std::atomic<qint64> m_syncBeginNs{0};
    std::atomic<qint64> m_syncEndNs{0};
    std::atomic<qint64> m_renderBeginNs{0};
    std::atomic<qint64> m_renderEndNs{0};
    QMutex m_slowestMutex;
    SessionHealthFramePhases m_slowest; // guarded by m_slowestMutex

#ifdef Q_OS_WIN
    // Previous CPU time readings, for the load since the last report.
    qint64 m_lastWallNs = 0;
    quint64 m_lastProcessCpu100ns = 0;
    quint64 m_lastSystemIdle100ns = 0;
    quint64 m_lastSystemBusy100ns = 0;
    bool m_haveCpuBaseline = false;
    // A PDH query for the CPU's actual speed against its base clock.
    void* m_pPerformanceQuery = nullptr;
    void* m_pPerformanceCounter = nullptr;
#endif
};
