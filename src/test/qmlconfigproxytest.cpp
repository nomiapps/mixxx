#include <gtest/gtest.h>

#include <QSignalSpy>
#include <memory>

#include "control/controlobject.h"
#include "library/library_prefs.h"
#include "preferences/keydetectionsettings.h"
#include "qml/qmlconfigproxy.h"
#include "test/mixxxtest.h"
#include "track/keyutils.h"

using namespace mixxx::qml;

namespace {

class QmlConfigProxyTest : public MixxxTest {
  protected:
    void SetUp() override {
        // Library owns this control in the real application.
        m_pKeyNotationControl = std::make_unique<ControlObject>(
                mixxx::library::prefs::kKeyNotationConfigKey);
        m_pProxy = std::make_unique<QmlConfigProxy>(config());
    }

    void TearDown() override {
        // The notation map is process-wide; do not leak it into other suites.
        KeyUtils::setNotation({});
    }

    std::unique_ptr<ControlObject> m_pKeyNotationControl;
    std::unique_ptr<QmlConfigProxy> m_pProxy;
};

TEST_F(QmlConfigProxyTest, KeyNotationDefaultsToTraditional) {
    EXPECT_EQ(m_pProxy->keyNotation(),
            static_cast<int>(KeyUtils::KeyNotation::Traditional));
}

TEST_F(QmlConfigProxyTest, SetKeyNotationUpdatesConfigControlAndMap) {
    QSignalSpy spy(m_pProxy.get(), &QmlConfigProxy::keyNotationChanged);

    m_pProxy->set_keyNotation(static_cast<int>(KeyUtils::KeyNotation::Lancelot));

    EXPECT_EQ(m_pProxy->keyNotation(), static_cast<int>(KeyUtils::KeyNotation::Lancelot));
    // Same setting the legacy DlgPrefKey reads, so the two dialogs agree.
    EXPECT_EQ(KeyDetectionSettings(config()).getKeyNotation(),
            QStringLiteral(KEY_NOTATION_LANCELOT));
    // The library sorts and the JS player proxy formats by this control.
    EXPECT_DOUBLE_EQ(m_pKeyNotationControl->get(),
            static_cast<double>(KeyUtils::KeyNotation::Lancelot));
    // Track::getKeyText() and the library column render through the map.
    EXPECT_EQ(KeyUtils::keyToString(mixxx::track::io::key::C_MAJOR), QStringLiteral("8B"));
    EXPECT_EQ(KeyUtils::keyToString(mixxx::track::io::key::A_MINOR), QStringLiteral("8A"));
    EXPECT_EQ(spy.count(), 1);

    // Re-setting the current value is a no-op.
    m_pProxy->set_keyNotation(static_cast<int>(KeyUtils::KeyNotation::Lancelot));
    EXPECT_EQ(spy.count(), 1);

    m_pProxy->set_keyNotation(static_cast<int>(KeyUtils::KeyNotation::OpenKey));
    EXPECT_EQ(KeyUtils::keyToString(mixxx::track::io::key::C_MAJOR), QStringLiteral("1d"));
    EXPECT_EQ(spy.count(), 2);
}

TEST_F(QmlConfigProxyTest, SetKeyNotationIgnoresUnsupportedValues) {
    m_pProxy->set_keyNotation(static_cast<int>(KeyUtils::KeyNotation::OpenKey));
    QSignalSpy spy(m_pProxy.get(), &QmlConfigProxy::keyNotationChanged);

    // Invalid, Custom (no editor for it in the New UI), out of range.
    m_pProxy->set_keyNotation(static_cast<int>(KeyUtils::KeyNotation::Invalid));
    m_pProxy->set_keyNotation(static_cast<int>(KeyUtils::KeyNotation::Custom));
    m_pProxy->set_keyNotation(99);

    EXPECT_EQ(m_pProxy->keyNotation(), static_cast<int>(KeyUtils::KeyNotation::OpenKey));
    EXPECT_EQ(KeyUtils::keyToString(mixxx::track::io::key::C_MAJOR), QStringLiteral("1d"));
    EXPECT_DOUBLE_EQ(m_pKeyNotationControl->get(),
            static_cast<double>(KeyUtils::KeyNotation::OpenKey));
    EXPECT_EQ(spy.count(), 0);
}

} // namespace
