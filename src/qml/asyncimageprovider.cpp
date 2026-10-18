#include "qml/asyncimageprovider.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>

#include "library/coverartcache.h"
#include "moc_asyncimageprovider.cpp"
#include "track/track.h"

namespace {
const QString kCoverArtPrefix = QStringLiteral("coverart/");
const QString kCoverInfoPrefix = QStringLiteral("coverinfo/");
const QChar kQueryDelimiter = QLatin1Char('?');
constexpr double kMaxCornerFraction = 0.5;
} // namespace

namespace mixxx {

namespace qml {

QImage roundImageCorners(const QImage& image, double cornerFraction) {
    if (image.isNull() || cornerFraction <= 0.0) {
        return image;
    }
    const int side = qMin(image.width(), image.height());
    const QRect square((image.width() - side) / 2, (image.height() - side) / 2, side, side);
    QImage rounded(side, side, QImage::Format_ARGB32_Premultiplied);
    rounded.fill(Qt::transparent);
    const double radius = qMin(cornerFraction, kMaxCornerFraction) * side;
    QPainterPath shape;
    shape.addRoundedRect(QRectF(0, 0, side, side), radius, radius);
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setClipPath(shape);
    painter.drawImage(QPoint(0, 0), image, square);
    return rounded;
}

CoverArtRequest parseCoverArtRequestId(const QString& requestId) {
    const qsizetype queryStart = requestId.indexOf(kQueryDelimiter);
    if (queryStart < 0) {
        return {requestId, 0.0};
    }
    double cornerFraction = 0.0;
    const QString query = requestId.mid(queryStart);
    if (query.startsWith(AsyncImageProvider::kCornerQuery)) {
        bool ok = false;
        const double value = QStringView(query).mid(AsyncImageProvider::kCornerQuery.size()).toDouble(&ok);
        if (ok && value > 0.0) {
            cornerFraction = qMin(value, kMaxCornerFraction);
        }
    }
    return {requestId.left(queryStart), cornerFraction};
}

AsyncImageResponse::AsyncImageResponse(
        const QString& id,
        QSize requestedSize,
        std::shared_ptr<TrackCollectionManager> pTrackCollectionManager)
        : m_id(parseCoverArtRequestId(id).id),
          m_cornerFraction(parseCoverArtRequestId(id).cornerFraction),
          m_requestedSize(requestedSize),
          m_pTrackCollectionManager(std::move(pTrackCollectionManager)) {
    setAutoDelete(false);
}

QQuickTextureFactory* AsyncImageResponse::textureFactory() const {
    return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void AsyncImageResponse::run() {
    if (m_id.startsWith(kCoverInfoPrefix)) {
        // Everything is in the URL: read the image here, on the pool thread,
        // without touching the track or the GUI thread.
        const CoverInfo coverInfo = AsyncImageProvider::coverArtUrlIdToCoverInfo(m_id);
        const CoverInfo::LoadedImage loadedImage = coverInfo.loadImage();
        if (loadedImage.result == CoverInfo::LoadedImage::Result::Ok) {
            m_image = roundImageCorners(
                    fitImageToRequestedSize(loadedImage.image, m_requestedSize),
                    m_cornerFraction);
        } else if (loadedImage.result != CoverInfo::LoadedImage::Result::NoImage) {
            qWarning() << "ImageProvider: Failed to load cover art" << loadedImage.location;
        }
        emit finished();
        return;
    }
    if (!m_id.startsWith(kCoverArtPrefix)) {
        qWarning() << "ImageProvider: Unsupported ID" << m_id;
        emit finished();
        return;
    }
    const QString trackLocation = AsyncImageProvider::coverArtUrlIdToTrackLocation(m_id);
    const auto trackRef = TrackRef::fromFilePath(trackLocation);

    // TODO: Only load CoverInfo from TrackCollectionManager
    // instead of the entire, stateful track object.
    TrackPointer pTrack;
    if (QThread::currentThread() != m_pTrackCollectionManager->thread()) {
        QMetaObject::invokeMethod(
                m_pTrackCollectionManager.get(),
                [this, trackRef] {
                    return m_pTrackCollectionManager->getTrackByRef(trackRef);
                },
                // This invocation will block the current thread!
                Qt::BlockingQueuedConnection,
                &pTrack);
    } else {
        pTrack = m_pTrackCollectionManager->getTrackByRef(trackRef);
    }
    if (!pTrack) {
        qWarning() << "ImageProvider: Failed to load track" << trackRef;
        emit finished();
        return;
    }
    const auto coverInfo = CoverInfo(pTrack->getCoverInfo(), trackLocation);
    // Release the track reference asap, i.e. before loading the image
    pTrack.reset();

    const CoverInfo::LoadedImage loadedImage = coverInfo.loadImage();
    switch (loadedImage.result) {
    case CoverInfo::LoadedImage::Result::NoImage:
        break;
    case CoverInfo::LoadedImage::Result::Ok:
        DEBUG_ASSERT(!loadedImage.image.isNull());
        m_image = roundImageCorners(
                fitImageToRequestedSize(loadedImage.image, m_requestedSize),
                m_cornerFraction);
        break;
    default:
        qWarning() << "ImageProvider: Failed to load cover art" << trackRef;
        break;
    }

    emit finished();
}

AsyncImageProvider::AsyncImageProvider(
        std::shared_ptr<TrackCollectionManager> pTrackCollectionManager)
        : QQuickAsyncImageProvider(),
          m_pTrackCollectionManager(pTrackCollectionManager) {
}

QQuickImageResponse* AsyncImageProvider::requestImageResponse(
        const QString& id, const QSize& requestedSize) {
    AsyncImageResponse* response = new AsyncImageResponse(
            id, requestedSize, m_pTrackCollectionManager);
    pool.start(response);
    return response;
}

// static
const QString AsyncImageProvider::kProviderName = QStringLiteral("mixxx");

// static
const QString AsyncImageProvider::kCornerQuery = QStringLiteral("?corner=");

// static
QUrl AsyncImageProvider::trackLocationToCoverArtUrl(const QString& trackLocation) {
    QUrl url("image://" + kProviderName + "/" + kCoverArtPrefix);
    return url.resolved(
            QString::fromLatin1(trackLocation.toUtf8().toBase64(
                    QByteArray::Base64UrlEncoding)));
}

//static
QString AsyncImageProvider::coverArtUrlIdToTrackLocation(const QString& coverArtUrlId) {
    return QString::fromUtf8(QByteArray::fromBase64(
            coverArtUrlId.mid(kCoverArtPrefix.size()).toLatin1(), QByteArray::Base64UrlEncoding));
}

// static
QUrl AsyncImageProvider::coverInfoToCoverArtUrl(const CoverInfo& coverInfo) {
    const QJsonObject json{
            {QStringLiteral("type"), static_cast<int>(coverInfo.type)},
            {QStringLiteral("source"), static_cast<int>(coverInfo.source)},
            {QStringLiteral("cover"), coverInfo.coverLocation},
            {QStringLiteral("track"), coverInfo.trackLocation},
            {QStringLiteral("digest"), QString::fromLatin1(coverInfo.imageDigest().toHex())},
            {QStringLiteral("hash"), static_cast<int>(coverInfo.legacyHash())},
    };
    QUrl url("image://" + kProviderName + "/" + kCoverInfoPrefix);
    const QByteArray encoded = QJsonDocument(json).toJson(QJsonDocument::Compact);
    return url.resolved(QString::fromLatin1(
            encoded.toBase64(QByteArray::Base64UrlEncoding)));
}

// static
CoverInfo AsyncImageProvider::coverArtUrlIdToCoverInfo(const QString& coverArtUrlId) {
    const QByteArray encoded = coverArtUrlId.mid(kCoverInfoPrefix.size()).toLatin1();
    const QByteArray decoded = QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding);
    const QJsonObject json = QJsonDocument::fromJson(decoded).object();
    CoverInfo coverInfo;
    coverInfo.type = static_cast<CoverInfo::Type>(json.value(QStringLiteral("type")).toInt());
    coverInfo.source = static_cast<CoverInfo::Source>(
            json.value(QStringLiteral("source")).toInt());
    coverInfo.coverLocation = json.value(QStringLiteral("cover")).toString();
    coverInfo.trackLocation = json.value(QStringLiteral("track")).toString();
    coverInfo.setImageDigest(
            QByteArray::fromHex(json.value(QStringLiteral("digest")).toString().toLatin1()),
            static_cast<quint16>(json.value(QStringLiteral("hash")).toInt()));
    return coverInfo;
}

} // namespace qml
} // namespace mixxx
