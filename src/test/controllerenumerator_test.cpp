#include "controllers/controllerenumerator.h"

#include <gtest/gtest.h>

#include <QPointer>

#include "test/controller_mapping_validation_test.h"
#include "test/mixxxtest.h"

namespace {

/// Offers one new controller on every scan, as the real enumerators do.
class RescanningEnumerator : public ControllerEnumerator {
  public:
    ~RescanningEnumerator() override {
        qDeleteAll(m_devices);
    }

    QList<Controller*> queryDevices() override {
        retireDevices(&m_devices);
        m_devices.append(new FakeController());
        return m_devices;
    }

  private:
    QList<Controller*> m_devices;
};

class ControllerEnumeratorTest : public MixxxTest {};

/// A rescan replaces every Controller while the settings pages on the main
/// thread still point at the old ones, so the old ones must outlive the scan.
TEST_F(ControllerEnumeratorTest, RescanKeepsReplacedControllersAlive) {
    RescanningEnumerator enumerator;
    const QList<Controller*> firstScan = enumerator.queryDevices();
    ASSERT_EQ(1, firstScan.size());
    EXPECT_TRUE(enumerator.takeRetiredDevices().isEmpty());
    const QPointer<Controller> pFirst(firstScan.first());

    const QList<Controller*> secondScan = enumerator.queryDevices();

    // One controller per device, not one more with every scan.
    ASSERT_EQ(1, secondScan.size());
    EXPECT_NE(pFirst.data(), secondScan.first());
    ASSERT_TRUE(pFirst);
    EXPECT_FALSE(pFirst->isOpen());

    const QList<Controller*> retired = enumerator.takeRetiredDevices();
    ASSERT_EQ(1, retired.size());
    EXPECT_EQ(pFirst.data(), retired.first());
    // Handed over once: the caller owns them now.
    EXPECT_TRUE(enumerator.takeRetiredDevices().isEmpty());

    qDeleteAll(retired);
    EXPECT_FALSE(pFirst);
}

TEST_F(ControllerEnumeratorTest, DeletesReplacedControllersNobodyTook) {
    QPointer<Controller> pFirst;
    {
        RescanningEnumerator enumerator;
        pFirst = enumerator.queryDevices().first();
        enumerator.queryDevices();
        EXPECT_TRUE(pFirst);
    }
    EXPECT_FALSE(pFirst);
}

} // anonymous namespace
