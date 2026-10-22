#include <gtest/gtest.h>

#include "util/sessionhealth.h"

namespace {

TEST(SessionHealthTest, FrameGapsCountStallsButNotIdle) {
    SessionHealthWindow window;
    window.addFrameGap(16);
    window.addFrameGap(17);
    window.addFrameGap(320); // a stall, as on the Surface on 22 Oct
    window.addFrameGap(60);
    window.addFrameGap(5000); // nothing to draw for five seconds
    EXPECT_EQ(5, window.frames);
    EXPECT_EQ(320, window.maxFrameGapMs);
    EXPECT_EQ(2, window.frameGapsOver50Ms);
    EXPECT_EQ(1, window.frameGapsOver150Ms);
}

TEST(SessionHealthTest, MainThreadLag) {
    SessionHealthWindow window;
    window.addMainThreadLag(0);
    window.addMainThreadLag(51);
    window.addMainThreadLag(300);
    EXPECT_EQ(300, window.maxLagMs);
    EXPECT_EQ(2, window.lagsOver50Ms);
    window.reset();
    EXPECT_EQ(0, window.maxLagMs);
    EXPECT_EQ(0, window.lagsOver50Ms);
    EXPECT_EQ(0, window.frames);
}

TEST(SessionHealthTest, ReportNamesWhatWasMeasured) {
    SessionHealthWindow window;
    window.addMainThreadLag(300);
    window.addFrameGap(16);
    window.addFrameGap(320);
    SessionHealthSystemSample sample;
    sample.workingSetMb = 1712.4;
    sample.privateMb = 2104.0;
    sample.handles = 2310;
    sample.processCpuPercentOfOneCore = 61.2;
    sample.systemCpuPercent = 78.0;
    sample.cpuPerformancePercent = 52.4;
    sample.cpuBaseMhz = 2100;
    sample.cpuLimitMhz = 1500;
    sample.memoryLoadPercent = 86;
    sample.availableMb = 1020.0;
    sample.onMains = false;
    sample.batteryPercent = 84;
    const QString line = SessionHealthMonitor::formatReport(
            (7 * 60 + 5) * 60 * 1000, window, sample);
    EXPECT_TRUE(line.startsWith(QStringLiteral("Session health: watching for 7h05m"))) << line.toStdString();
    EXPECT_TRUE(line.contains(QStringLiteral("late by up to 300 ms, 1 times over 50 ms")))
            << line.toStdString();
    EXPECT_TRUE(line.contains(QStringLiteral("longest gap 320 ms"))) << line.toStdString();
    EXPECT_TRUE(line.contains(QStringLiteral("working set 1712 MB"))) << line.toStdString();
    EXPECT_TRUE(line.contains(QStringLiteral("CPU speed 52% of its 2100 MHz base, power limit 1500 MHz")))
            << line.toStdString();
    EXPECT_TRUE(line.contains(QStringLiteral("on battery 84%"))) << line.toStdString();
}

TEST(SessionHealthTest, ReportSplitsTheSlowestFrame) {
    SessionHealthWindow window;
    SessionHealthFramePhases slow;
    slow.totalMs = 320;
    slow.beforeSyncMs = 290;
    slow.syncMs = 20;
    slow.renderMs = 6;
    slow.swapMs = -1;
    window.slowestFrame = slow;
    const QString line = SessionHealthMonitor::formatReport(
            60000, window, SessionHealthSystemSample());
    EXPECT_TRUE(line.contains(QStringLiteral(
            "slowest frame 320 ms: before sync 290, sync 20, render 6, swap ?")))
            << line.toStdString();
}

TEST(SessionHealthTest, ReportLeavesOutWhatThePlatformCannotTell) {
    const QString line = SessionHealthMonitor::formatReport(
            0, SessionHealthWindow(), SessionHealthSystemSample());
    EXPECT_FALSE(line.contains(QStringLiteral("process:"))) << line.toStdString();
    EXPECT_FALSE(line.contains(QStringLiteral("machine:"))) << line.toStdString();
}

} // namespace
