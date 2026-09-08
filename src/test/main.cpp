#ifdef USE_BENCH
#include <benchmark/benchmark.h>
#endif

#ifdef MIXXX_USE_QML
#include <QQuickStyle>
#endif

#include "errordialoghandler.h"
#include "mixxxtest.h"
#include "util/logging.h"

int main(int argc, char **argv) {
    // By default, render analyzer waveform tests to an offscreen buffer
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    }

    // We never want to popup error dialogs when running tests.
    ErrorDialogHandler::setEnabled(false);

#ifdef MIXXX_USE_QML
    // Pick the style once, for the whole process, BEFORE any test loads QML that
    // imports Qt Quick Controls. QQuickStyle::setStyle() is a no-op after the style
    // has been resolved, and QmlApplication only calls it when IT starts up -- so in a
    // full test run an earlier QML test resolves the style first, QmlApplication's call
    // is too late, and the skin loads under the platform default instead of Basic. On
    // Windows that default pulls in QtQuick.Effects, which is not deployed, and
    // QmlStartupSmokeTest fails with "Type Menu unavailable" -- but only when run
    // alongside other tests, never alone.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
#endif

#ifdef USE_BENCH
    bool run_benchmarks = false;
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--benchmark") == 0) {
            run_benchmarks = true;
            break;
        } else if (strcmp(argv[i], "--trace") == 0) {
            mixxx::Logging::setLogLevel(mixxx::LogLevel::Trace);
        }
    }

    if (run_benchmarks) {
        benchmark::Initialize(&argc, argv);
        MixxxTest::ApplicationScope applicationScope(argc, argv);
        benchmark::RunSpecifiedBenchmarks();
        return 0;
    }

    // Otherwise, run the test suite:
#endif
    testing::InitGoogleTest(&argc, argv);
    MixxxTest::ApplicationScope applicationScope(argc, argv);
    return RUN_ALL_TESTS();
}
