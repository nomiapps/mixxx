#include "qml/qmlautoreload.h"

#include <QDebug>
#include <QFileInfo>
#include <QUrl>

#include "moc_qmlautoreload.cpp"
#include "util/autofilereloader.h"

namespace mixxx {

namespace qml {

QmlAutoReload::QmlAutoReload()
        : m_autoReloader(RuntimeLoggingCategory(QStringLiteral("qml_auto_reload"))) {
    // propagate inner signal outwards
    connect(&m_autoReloader, &AutoFileReloader::fileChanged, this, &QmlAutoReload::triggered);
};

QUrl QmlAutoReload::intercept(const QUrl& url, QQmlAbstractUrlInterceptor::DataType) {
    if (!url.isLocalFile()) {
        return url;
    }
    QString filePath = url.toLocalFile();
    // Once per path. Asking the file system and the watcher again on every
    // call made moving the window to a screen with another scale factor
    // freeze the GUI thread for seconds: every ShaderEffect in the scene
    // resolves its shader URLs again when the screen changes, and each of
    // those calls came through here.
    const QMutexLocker locker(&m_mutex);
    if (m_seenPaths.contains(filePath)) {
        return url;
    }
    m_seenPaths.insert(filePath);
    if (!QFileInfo(filePath).isFile()) {
        return url;
    }
    m_autoReloader.addPath(filePath);
    return url;
}

void QmlAutoReload::clear() {
    const QMutexLocker locker(&m_mutex);
    m_seenPaths.clear();
    m_autoReloader.clear();
}

} // namespace qml
} // namespace mixxx
