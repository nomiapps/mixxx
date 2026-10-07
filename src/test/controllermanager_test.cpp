#include "controllers/controllermanager.h"

#include <gtest/gtest.h>

#include <QElapsedTimer>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>

#include "controllers/controller.h"
#include "test/mixxxtest.h"

namespace {

constexpr int kScanTimeoutMillis = 30000;
constexpr int kDeletionTimeoutMillis = 5000;
constexpr int kRequests = 5;

/// These scan whatever MIDI and HID devices the machine running the tests
/// has. Nothing is enabled in the test configuration, so none is opened.
class ControllerManagerTest : public MixxxTest {
  protected:
    bool scan(ControllerManager* pManager) {
        QSignalSpy finished(pManager, &ControllerManager::scanFinished);
        pManager->setUpDevices();
        return finished.wait(kScanTimeoutMillis);
    }
};

TEST_F(ControllerManagerTest, RescanReplacesControllersOneForOne) {
    ControllerManager manager(config());
    ASSERT_TRUE(scan(&manager));
    const QList<Controller*> firstScan = manager.getControllers();
    QList<QPointer<Controller>> replaced;
    for (Controller* pController : firstScan) {
        replaced.append(pController);
    }

    // The settings pages rebuild in a handler like this one, on the main
    // thread, and still hold the controllers of the scan before.
    QObject mainThreadListener;
    bool aliveInHandler = true;
    QObject::connect(&manager,
            &ControllerManager::devicesChanged,
            &mainThreadListener,
            [&replaced, &aliveInHandler]() {
                for (const QPointer<Controller>& pController : std::as_const(replaced)) {
                    aliveInHandler = aliveInHandler && !pController.isNull();
                }
            });

    ASSERT_TRUE(scan(&manager));

    // One controller per device, not one more with every scan.
    EXPECT_EQ(firstScan.size(), manager.getControllers().size());
    EXPECT_TRUE(aliveInHandler);

    // Once that handler has run they are deleted.
    QElapsedTimer timer;
    timer.start();
    const auto anyAlive = [&replaced]() {
        for (const QPointer<Controller>& pController : std::as_const(replaced)) {
            if (!pController.isNull()) {
                return true;
            }
        }
        return false;
    };
    while (anyAlive() && timer.elapsed() < kDeletionTimeoutMillis) {
        QTest::qWait(10);
    }
    EXPECT_FALSE(anyAlive());
}

TEST_F(ControllerManagerTest, RequestsMadeWhileOneWaitsShareItsScan) {
    ControllerManager manager(config());
    QSignalSpy started(&manager, &ControllerManager::scanStarted);
    QSignalSpy finished(&manager, &ControllerManager::scanFinished);
    // The manager's thread is still starting up, so these wait together.
    for (int i = 0; i < kRequests; ++i) {
        manager.setUpDevices();
    }
    ASSERT_TRUE(finished.wait(kScanTimeoutMillis));
    QTest::qWait(200);
    EXPECT_LT(started.count(), kRequests);
    EXPECT_EQ(started.count(), finished.count());
}

} // anonymous namespace
