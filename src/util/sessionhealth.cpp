#include "util/sessionhealth.h"

#include <QQuickWindow>
#include <QStringList>
#include <QtDebug>
#include <algorithm>
#include <vector>

#include "moc_sessionhealth.cpp"

#ifdef Q_OS_WIN
#include <windows.h>
// After windows.h
#include <pdh.h>
#include <pdhmsg.h>
#include <powerbase.h>
#include <psapi.h>
#endif

namespace {

#ifdef Q_OS_WIN
// Documented for CallNtPowerInformation(ProcessorInformation) but declared in
// no SDK header.
struct ProcessorPowerInformation {
    ULONG Number;
    ULONG MaxMhz;
    ULONG CurrentMhz;
    ULONG MhzLimit;
    ULONG MaxIdleState;
    ULONG CurrentIdleState;
};

quint64 fileTimeTo100ns(const FILETIME& fileTime) {
    return (static_cast<quint64>(fileTime.dwHighDateTime) << 32) | fileTime.dwLowDateTime;
}
#endif

QString megabytes(double mb) {
    return QString::number(mb, 'f', 0) + QStringLiteral(" MB");
}

} // namespace

void SessionHealthWindow::addMainThreadLag(qint64 lagMs) {
    if (lagMs > maxLagMs) {
        maxLagMs = lagMs;
    }
    if (lagMs > 50) {
        ++lagsOver50Ms;
    }
}

void SessionHealthWindow::addFrameGap(qint64 gapMs) {
    ++frames;
    if (gapMs > kIdleFrameGapMs) {
        return;
    }
    if (gapMs > maxFrameGapMs) {
        maxFrameGapMs = gapMs;
    }
    if (gapMs > 50) {
        ++frameGapsOver50Ms;
    }
    if (gapMs > 150) {
        ++frameGapsOver150Ms;
    }
}

void SessionHealthWindow::reset() {
    *this = SessionHealthWindow();
}

SessionHealthMonitor::SessionHealthMonitor(QObject* pParent)
        : QObject(pParent) {
    m_uptime.start();
    m_sinceLastProbe.start();
    m_lagProbe.setTimerType(Qt::PreciseTimer);
    m_lagProbe.setInterval(kLagProbeIntervalMs);
    connect(&m_lagProbe, &QTimer::timeout, this, &SessionHealthMonitor::slotLagProbe);
    m_lagProbe.start();
    m_report.setInterval(kReportIntervalMs);
    connect(&m_report, &QTimer::timeout, this, &SessionHealthMonitor::slotReport);
    m_report.start();
#ifdef Q_OS_WIN
    // CallNtPowerInformation's CurrentMhz mostly sits at the base clock on
    // Windows 10 and 11 whatever the CPU really does, so the speed comes from
    // the performance counter Task Manager shows.
    PDH_HQUERY query = nullptr;
    if (PdhOpenQueryW(nullptr, 0, &query) == ERROR_SUCCESS) {
        PDH_HCOUNTER counter = nullptr;
        if (PdhAddEnglishCounterW(query,
                    L"\\Processor Information(_Total)\\% Processor Performance",
                    0,
                    &counter) == ERROR_SUCCESS) {
            m_pPerformanceQuery = query;
            m_pPerformanceCounter = counter;
        } else {
            PdhCloseQuery(query);
        }
    }
#endif
    // The first CPU reading only sets the baselines.
    sampleSystem();
}

SessionHealthMonitor::~SessionHealthMonitor() {
#ifdef Q_OS_WIN
    if (m_pPerformanceQuery) {
        PdhCloseQuery(static_cast<PDH_HQUERY>(m_pPerformanceQuery));
    }
#endif
}

void SessionHealthMonitor::setWindow(QQuickWindow* pWindow) {
    if (m_pWindow == pWindow) {
        return;
    }
    if (m_pWindow) {
        disconnect(m_pWindow, nullptr, this, nullptr);
    }
    m_pWindow = pWindow;
    m_sinceLastFrame.invalidate();
    if (pWindow) {
        // Emitted on the render thread with the threaded render loop; queued
        // to this thread then, so a frame counts when the main thread could
        // take it, which is what the user sees.
        connect(pWindow,
                &QQuickWindow::afterFrameEnd,
                this,
                &SessionHealthMonitor::slotFrameEnd);
    }
}

void SessionHealthMonitor::slotLagProbe() {
    const qint64 elapsedMs = m_sinceLastProbe.restart();
    m_window.addMainThreadLag(std::max<qint64>(0, elapsedMs - kLagProbeIntervalMs));
}

void SessionHealthMonitor::slotFrameEnd() {
    if (m_sinceLastFrame.isValid()) {
        m_window.addFrameGap(m_sinceLastFrame.restart());
    } else {
        m_sinceLastFrame.start();
    }
}

void SessionHealthMonitor::slotReport() {
    const SessionHealthSystemSample sample = sampleSystem();
    qInfo().noquote() << formatReport(m_uptime.elapsed(), m_window, sample);
    m_window.reset();
}

// static
QString SessionHealthMonitor::formatReport(qint64 uptimeMs,
        const SessionHealthWindow& window,
        const SessionHealthSystemSample& sample) {
    // Rounded: the first report comes a few milliseconds short of a minute.
    const qint64 minutes = (uptimeMs + 30000) / 60000;
    QStringList parts;
    parts << QStringLiteral("Session health: watching for %1h%2m")
                     .arg(minutes / 60)
                     .arg(minutes % 60, 2, 10, QLatin1Char('0'));
    parts << QStringLiteral("main thread late by up to %1 ms, %2 times over 50 ms")
                     .arg(window.maxLagMs)
                     .arg(window.lagsOver50Ms);
    parts << QStringLiteral("%1 frames, longest gap %2 ms, %3 gaps over 50 ms, %4 over 150 ms")
                     .arg(window.frames)
                     .arg(window.maxFrameGapMs)
                     .arg(window.frameGapsOver50Ms)
                     .arg(window.frameGapsOver150Ms);
    QStringList process;
    if (sample.processCpuPercentOfOneCore) {
        process << QStringLiteral("CPU %1% of one core")
                           .arg(*sample.processCpuPercentOfOneCore, 0, 'f', 0);
    }
    if (sample.workingSetMb) {
        process << QStringLiteral("working set ") + megabytes(*sample.workingSetMb);
    }
    if (sample.privateMb) {
        process << QStringLiteral("private ") + megabytes(*sample.privateMb);
    }
    if (sample.handles) {
        process << QStringLiteral("handles %1").arg(*sample.handles);
    }
    if (sample.gdiObjects && sample.userObjects) {
        process << QStringLiteral("GDI %1, USER %2")
                           .arg(*sample.gdiObjects)
                           .arg(*sample.userObjects);
    }
    if (!process.isEmpty()) {
        parts << QStringLiteral("process: ") + process.join(QStringLiteral(", "));
    }
    QStringList machine;
    if (sample.systemCpuPercent) {
        machine << QStringLiteral("CPU %1% busy").arg(*sample.systemCpuPercent, 0, 'f', 0);
    }
    if (sample.cpuPerformancePercent) {
        QString speed = QStringLiteral("CPU speed %1%")
                                .arg(*sample.cpuPerformancePercent, 0, 'f', 0);
        if (sample.cpuBaseMhz) {
            speed += QStringLiteral(" of its %1 MHz base").arg(*sample.cpuBaseMhz);
        }
        machine << speed;
    }
    if (sample.cpuLimitMhz && sample.cpuBaseMhz && *sample.cpuLimitMhz < *sample.cpuBaseMhz) {
        machine << QStringLiteral("power limit %1 MHz").arg(*sample.cpuLimitMhz);
    }
    if (sample.memoryLoadPercent) {
        machine << QStringLiteral("memory %1% used").arg(*sample.memoryLoadPercent);
    }
    if (sample.availableMb) {
        machine << megabytes(*sample.availableMb) + QStringLiteral(" free");
    }
    if (sample.onMains) {
        QString power = *sample.onMains ? QStringLiteral("on mains")
                                        : QStringLiteral("on battery");
        if (sample.batteryPercent) {
            power += QStringLiteral(" %1%").arg(*sample.batteryPercent);
        }
        if (sample.batterySaver && *sample.batterySaver) {
            power += QStringLiteral(", battery saver on");
        }
        machine << power;
    }
    if (!machine.isEmpty()) {
        parts << QStringLiteral("machine: ") + machine.join(QStringLiteral(", "));
    }
    return parts.join(QStringLiteral(" | "));
}

SessionHealthSystemSample SessionHealthMonitor::sampleSystem() {
    SessionHealthSystemSample sample;
#ifdef Q_OS_WIN
    const HANDLE process = GetCurrentProcess();

    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    if (GetProcessMemoryInfo(process,
                reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                sizeof(memory))) {
        sample.workingSetMb = memory.WorkingSetSize / (1024.0 * 1024.0);
        sample.privateMb = memory.PrivateUsage / (1024.0 * 1024.0);
    }
    DWORD handles = 0;
    if (GetProcessHandleCount(process, &handles)) {
        sample.handles = static_cast<int>(handles);
    }
    sample.gdiObjects = static_cast<int>(GetGuiResources(process, GR_GDIOBJECTS));
    sample.userObjects = static_cast<int>(GetGuiResources(process, GR_USEROBJECTS));

    // CPU load since the previous sample.
    FILETIME creation, exit, kernel, user;
    FILETIME idle, systemKernel, systemUser;
    if (GetProcessTimes(process, &creation, &exit, &kernel, &user) &&
            GetSystemTimes(&idle, &systemKernel, &systemUser)) {
        const qint64 wallNs = m_uptime.nsecsElapsed();
        const quint64 processCpu = fileTimeTo100ns(kernel) + fileTimeTo100ns(user);
        const quint64 systemIdle = fileTimeTo100ns(idle);
        // System kernel time includes idle time.
        const quint64 systemBusy =
                fileTimeTo100ns(systemKernel) + fileTimeTo100ns(systemUser) - systemIdle;
        if (m_haveCpuBaseline && wallNs > m_lastWallNs) {
            const double wall100ns = (wallNs - m_lastWallNs) / 100.0;
            sample.processCpuPercentOfOneCore =
                    100.0 * (processCpu - m_lastProcessCpu100ns) / wall100ns;
            const quint64 busy = systemBusy - m_lastSystemBusy100ns;
            const quint64 total = busy + (systemIdle - m_lastSystemIdle100ns);
            if (total > 0) {
                sample.systemCpuPercent = 100.0 * busy / total;
            }
        }
        m_lastWallNs = wallNs;
        m_lastProcessCpu100ns = processCpu;
        m_lastSystemIdle100ns = systemIdle;
        m_lastSystemBusy100ns = systemBusy;
        m_haveCpuBaseline = true;
    }

    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        sample.memoryLoadPercent = static_cast<int>(status.dwMemoryLoad);
        sample.availableMb = status.ullAvailPhys / (1024.0 * 1024.0);
    }

    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    const DWORD processors = systemInfo.dwNumberOfProcessors;
    if (processors > 0) {
        std::vector<ProcessorPowerInformation> power(processors);
        const ULONG bytes = static_cast<ULONG>(power.size() * sizeof(ProcessorPowerInformation));
        if (CallNtPowerInformation(ProcessorInformation, nullptr, 0, power.data(), bytes) == 0) {
            ULONG maxMhz = 0;
            ULONG limitMhz = ~0UL;
            for (const auto& cpu : power) {
                maxMhz = std::max(maxMhz, cpu.MaxMhz);
                limitMhz = std::min(limitMhz, cpu.MhzLimit);
            }
            sample.cpuBaseMhz = static_cast<int>(maxMhz);
            sample.cpuLimitMhz = static_cast<int>(limitMhz);
        }
    }

    if (m_pPerformanceQuery &&
            PdhCollectQueryData(static_cast<PDH_HQUERY>(m_pPerformanceQuery)) ==
                    ERROR_SUCCESS) {
        PDH_FMT_COUNTERVALUE value{};
        // Fails on the very first collection: a rate needs two.
        if (PdhGetFormattedCounterValue(static_cast<PDH_HCOUNTER>(m_pPerformanceCounter),
                    PDH_FMT_DOUBLE,
                    nullptr,
                    &value) == ERROR_SUCCESS &&
                value.CStatus == PDH_CSTATUS_VALID_DATA) {
            sample.cpuPerformancePercent = value.doubleValue;
        }
    }

    SYSTEM_POWER_STATUS powerStatus{};
    if (GetSystemPowerStatus(&powerStatus)) {
        if (powerStatus.ACLineStatus != 255) {
            sample.onMains = powerStatus.ACLineStatus == 1;
        }
        if (powerStatus.BatteryLifePercent != 255) {
            sample.batteryPercent = powerStatus.BatteryLifePercent;
        }
        sample.batterySaver = powerStatus.SystemStatusFlag == 1;
    }
#endif
    return sample;
}
