#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QUrl>
#include <memory>

#include "control/controlobject.h"
#include "control/controlpushbutton.h"
#include "qml/qmlconfigproxy.h"
#include "test/mixxxtest.h"

class LinkButtonQmlTest : public MixxxTest {};

TEST_F(LinkButtonQmlTest, ToggleAndPeerCountFollowSharedControls) {
    ControlPushButton enabled(ConfigKey(QStringLiteral("[AbletonLink]"), QStringLiteral("sync_enabled")));
    enabled.setButtonMode(mixxx::control::ButtonMode::Toggle);
    enabled.setStates(2);
    ControlObject peers(ConfigKey(QStringLiteral("[AbletonLink]"), QStringLiteral("num_peers")));
    peers.setReadOnly();
    mixxx::qml::QmlConfigProxy::registerUserSettings(config());
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(RESOURCE_FOLDER "/qml"));
    QQmlComponent component(&engine, QUrl::fromLocalFile(
            QStringLiteral(RESOURCE_FOLDER "/qml/LinkButton.qml")));
    std::unique_ptr<QObject> button(component.create());
    ASSERT_TRUE(button) << qPrintable(component.errorString());
    EXPECT_TRUE(button->property("enabled").toBool());
    EXPECT_FALSE(button->property("checked").toBool());
    ASSERT_TRUE(QMetaObject::invokeMethod(button.get(), "clicked"));
    EXPECT_DOUBLE_EQ(enabled.get(), 1.0);
    peers.forceSet(2.0);
    QCoreApplication::processEvents();
    EXPECT_TRUE(button->property("checked").toBool());
    EXPECT_TRUE(button->property("text").toString().contains(QStringLiteral("2")));
    enabled.set(0.0);
    QCoreApplication::processEvents();
    EXPECT_FALSE(button->property("checked").toBool());
    ASSERT_TRUE(QMetaObject::invokeMethod(button.get(), "clicked"));
    EXPECT_DOUBLE_EQ(enabled.get(), 1.0);
}
