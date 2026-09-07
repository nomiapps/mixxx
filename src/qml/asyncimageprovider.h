#pragma once

#include <QImage>
#include <QQuickAsyncImageProvider>
#include <QRunnable>
#include <QSize>
#include <QString>
#include <QThreadPool>
#include <memory>

#include "library/coverart.h"
#include "library/trackcollectionmanager.h"

namespace mixxx {
namespace qml {

/// Fit an image inside the size Qt Quick requested for it. That size is the
/// Image item's sourceSize, with 0 for a side the QML left unset. Keep the
/// aspect ratio, never enlarge, and resample smoothly. QImage::scaled's
/// defaults are Qt::IgnoreAspectRatio and Qt::FastTransformation, so passing
/// the requested size straight through squashed the cover to the requested
/// proportions and resampled it nearest-neighbour.
inline QImage fitImageToRequestedSize(const QImage& image, const QSize& requestedSize) {
    const bool hasWidth = requestedSize.width() > 0;
    const bool hasHeight = requestedSize.height() > 0;
    if (image.isNull() || (!hasWidth && !hasHeight)) {
        return image;
    }
    if (hasWidth && hasHeight) {
        if (image.width() <= requestedSize.width() &&
                image.height() <= requestedSize.height()) {
            return image;
        }
        return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (hasWidth) {
        return image.width() <= requestedSize.width()
                ? image
                : image.scaledToWidth(requestedSize.width(), Qt::SmoothTransformation);
    }
    return image.height() <= requestedSize.height()
            ? image
            : image.scaledToHeight(requestedSize.height(), Qt::SmoothTransformation);
}

class AsyncImageResponse : public QQuickImageResponse, public QRunnable {
    Q_OBJECT
  public:
    AsyncImageResponse(
            QString id,
            QSize requestedSize,
            std::shared_ptr<TrackCollectionManager> pTrackCollectionManager);

    QQuickTextureFactory* textureFactory() const override;

    void run() override;

  private:
    QString m_id;
    QSize m_requestedSize;
    std::shared_ptr<TrackCollectionManager> m_pTrackCollectionManager;

    QImage m_image;
};

class AsyncImageProvider : public QQuickAsyncImageProvider {
  public:
    AsyncImageProvider(std::shared_ptr<TrackCollectionManager> pTrackCollectionManager);

    QQuickImageResponse* requestImageResponse(
            const QString& id, const QSize& requestedSize) override;

    static const QString kProviderName;
    static QUrl trackLocationToCoverArtUrl(const QString& trackLocation);
    static QString coverArtUrlIdToTrackLocation(const QString& coverArtUrlId);

  private:
    QThreadPool pool;
    std::shared_ptr<TrackCollectionManager> m_pTrackCollectionManager;
};

} // namespace qml
} // namespace mixxx
