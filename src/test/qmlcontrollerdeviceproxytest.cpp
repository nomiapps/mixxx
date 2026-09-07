#include <gtest/gtest.h>

#include <QSet>
#include <memory>
#include <optional>

#include "qml/qmlpreferencesproxy.h"
#include "test/controller_mapping_validation_test.h"
#include "test/mixxxtest.h"

using namespace mixxx::qml;

namespace {

class QmlControllerDeviceProxyTest : public MixxxTest {};

/// Rescanning for controllers destroys every Controller and builds replacements.
/// The settings card for a device outlives its controller by at least the hop back
/// to the main thread, so it has to notice the device leaving rather than
/// dereference freed memory.
TEST_F(QmlControllerDeviceProxyTest, SurvivesItsControllerBeingDestroyed) {
    auto pController = std::make_unique<FakeController>();
    QmlControllerDeviceProxy device(
            pController.get(), std::nullopt, QSet<QmlControllerMappingProxy*>(), nullptr);

    ASSERT_EQ(device.internal(), pController.get());
    const QString name = device.getName();
    const QmlControllerDeviceProxy::Type type = device.getType();
    EXPECT_FALSE(name.isEmpty());

    pController.reset();

    EXPECT_EQ(device.internal(), nullptr);
    // Name and type are cached at construction, so the card can still say which
    // device it belonged to after that device is gone.
    EXPECT_EQ(device.getName(), name);
    EXPECT_EQ(device.getType(), type);
    // Everything that did need the device degrades instead of crashing.
    EXPECT_FALSE(device.getEnabled());
    EXPECT_EQ(device.vendor(), QStringLiteral("N/A"));
    EXPECT_EQ(device.product(), QStringLiteral("N/A"));
    EXPECT_EQ(device.serialNumber(), QStringLiteral("N/A"));
}

/// Saving such a card must not write settings keyed to a controller that is no
/// longer there, nor emit a mapping assignment naming a destroyed device.
TEST_F(QmlControllerDeviceProxyTest, DoesNotSaveWithoutItsController) {
    auto pController = std::make_unique<FakeController>();
    QmlControllerDeviceProxy device(
            pController.get(), std::nullopt, QSet<QmlControllerMappingProxy*>(), nullptr);

    // An untouched card is a no-op save either way, and never reaches the config.
    EXPECT_TRUE(device.save(nullptr));

    device.setEdited();
    pController.reset();
    EXPECT_FALSE(device.save(nullptr));
}

} // namespace
