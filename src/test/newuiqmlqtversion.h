#pragma once

#include <gtest/gtest.h>

#include <QLibraryInfo>
#include <QVersionNumber>

/// The New UI's QML needs Qt 6.6. Twenty-odd of its Shapes set
/// preferredRendererType to reach the CurveRenderer, which draws
/// resolution-independent antialiased curves on the GPU; the property arrived in
/// Qt 6.6. On an older Qt it does not merely fall back to a softer arc -- the
/// type carrying it fails to load outright, and every type built on it with it:
/// Knob, Slider, Spinny, PlayButton, the synth curves, the settings cards.
///
/// The fork builds against Qt 6.8 on Windows, so nothing there ever sees this.
/// Ubuntu 24.04, which CI builds on, carries Qt 6.4.2, where loading any New UI
/// QML fails at the first Shape.
///
/// Tests that load New UI QML use this, so that on an older Qt they say what is
/// missing instead of failing over a null object for a reason the test is not
/// about. It is a macro because GTEST_SKIP() returns from the function it is
/// written in, so a helper function could not skip its caller.
#define SKIP_IF_NEW_UI_QML_UNSUPPORTED()                               \
    do {                                                               \
        const QVersionNumber mixxxQtVersion = QLibraryInfo::version(); \
        if (mixxxQtVersion < QVersionNumber(6, 6)) {                   \
            GTEST_SKIP() << "the New UI's QML needs Qt 6.6 for "       \
                            "Shape.preferredRendererType; this is Qt " \
                         << qPrintable(mixxxQtVersion.toString());     \
        }                                                              \
    } while (0)
