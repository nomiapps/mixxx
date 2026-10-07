#pragma once

#include <QMutex>
#include <QObject>
#include <QQmlAbstractUrlInterceptor>
#include <QSet>
#include <QString>

#include "util/autofilereloader.h"

namespace mixxx {
namespace qml {

class QmlAutoReload : public QObject, public QQmlAbstractUrlInterceptor {
    Q_OBJECT
  public:
    explicit QmlAutoReload();

    QUrl intercept(const QUrl& url, QQmlAbstractUrlInterceptor::DataType type) override;

    void clear();

  signals:
    void triggered();

  private:
    AutoFileReloader m_autoReloader;
    /// Every local path already looked at. The engine asks about the same URL
    /// again whenever a binding re-resolves it, from the GUI thread and from
    /// its type loader thread.
    QMutex m_mutex;
    QSet<QString> m_seenPaths;
};

} // namespace qml
} // namespace mixxx
