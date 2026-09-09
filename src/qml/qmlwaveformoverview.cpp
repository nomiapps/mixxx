#include "qml/qmlwaveformoverview.h"

#include <algorithm>
#include <cmath>

// QmlLibraryProxy::get() returns a Library*, and emitting its signal needs the
// complete type, not the forward declaration the proxy header carries.
#include "library/library.h"
#include "moc_qmlwaveformoverview.cpp"
#include "qmllibraryproxy.h"
#include "qmlplayerproxy.h"
#include "qmltrackproxy.h"
#include "track/track.h"

namespace {
constexpr double kDesiredChannelHeight = 255;
} // namespace

namespace mixxx {
namespace qml {

QmlWaveformOverview::QmlWaveformOverview(QQuickItem* parent)
        : QQuickPaintedItem(parent),
          m_pTrack(nullptr),
          m_channels(ChannelFlag::BothChannels),
          m_renderer(Renderer::RGB),
          m_colorHigh(0xFF0000),
          m_colorMid(0x00FF00),
          m_colorLow(0x0000FF),
          m_rangeStart(0.0),
          m_rangeEnd(1.0) {
}

qreal QmlWaveformOverview::getRangeStart() const {
    return m_rangeStart;
}

void QmlWaveformOverview::setRangeStart(qreal start) {
    const qreal clamped = std::clamp(start, 0.0, 1.0);
    if (m_rangeStart == clamped) {
        return;
    }
    m_rangeStart = clamped;
    emit rangeStartChanged(clamped);
    update();
}

qreal QmlWaveformOverview::getRangeEnd() const {
    return m_rangeEnd;
}

void QmlWaveformOverview::setRangeEnd(qreal end) {
    const qreal clamped = std::clamp(end, 0.0, 1.0);
    if (m_rangeEnd == clamped) {
        return;
    }
    m_rangeEnd = clamped;
    emit rangeEndChanged(clamped);
    update();
}

QmlTrackProxy* QmlWaveformOverview::getTrack() const {
    return m_pTrack;
}

void QmlWaveformOverview::setTrack(QmlTrackProxy* pTrack) {
    if (m_pTrack == pTrack) {
        return;
    }

    if (m_pTrack != nullptr && m_pTrack->internal() != nullptr) {
        m_pTrack->internal()->disconnect(this);
    }

    m_pTrack = pTrack;

    if (m_pTrack != nullptr && pTrack->internal() != nullptr) {
        connect(pTrack->internal().get(),
                &Track::waveformSummaryUpdated,
                this,
                &QmlWaveformOverview::slotWaveformUpdated);

        // Ask for the summary when the track has never been analysed. paint()
        // draws nothing without one and nothing else was requesting it, so an
        // unanalysed track showed an empty overview and went on showing it.
        // The waveform display beside it asks for its own data exactly this
        // way, which is why that one filled in and only the overview stayed
        // blank. The analyser reports progress through waveformSummaryUpdated,
        // connected just above, so the overview draws as it is generated.
        const TrackPointer pTrackInternal = pTrack->internal();
        if (!pTrackInternal->getWaveformSummary() && pTrackInternal->getId().isValid()) {
            emit QmlLibraryProxy::get() -> analyzeTracks({pTrackInternal->getId()});
        }
    }
    slotWaveformUpdated();
}

QmlWaveformOverview::Channels QmlWaveformOverview::getChannels() const {
    return m_channels;
}

void QmlWaveformOverview::setChannels(QmlWaveformOverview::Channels channels) {
    if (m_channels == channels) {
        return;
    }

    m_channels = channels;
    emit channelsChanged(channels);
}

void QmlWaveformOverview::slotWaveformUpdated() {
    update();
}

void QmlWaveformOverview::paint(QPainter* pPainter) {
    if (!m_pTrack) {
        return;
    }
    TrackPointer pTrack = m_pTrack->internal();
    if (!pTrack) {
        return;
    }

    ConstWaveformPointer pWaveform = pTrack->getWaveformSummary();
    if (!pWaveform) {
        return;
    }

    const int dataSize = pWaveform->getDataSize();
    if (dataSize == 0) {
        return;
    }

    constexpr int actualCompletion = 0;
    // Always multiple of 2
    const int waveformCompletion = pWaveform->getCompletion();
    // Test if there is some new to draw (at least of pixel width)
    const int completionIncrement = waveformCompletion - actualCompletion;

    const qreal fullWidth = static_cast<qreal>(dataSize) / 2;
    const double visiblePixelIncrement = completionIncrement * fullWidth / dataSize;
    if (waveformCompletion < (dataSize - 2) &&
            (completionIncrement < 2 || visiblePixelIncrement == 0)) {
        return;
    }

    // The columns the range covers. Both are in waveform columns, not pixels:
    // the data holds two entries per column, so a column index is always even
    // when doubled back into the array. A degenerate or inverted range falls
    // back to the whole track rather than drawing nothing.
    qreal rangeStart = m_rangeStart;
    qreal rangeEnd = m_rangeEnd;
    if (!(rangeEnd > rangeStart)) {
        rangeStart = 0.0;
        rangeEnd = 1.0;
    }
    const int firstColumn = static_cast<int>(std::floor(rangeStart * fullWidth));
    const int lastColumn = static_cast<int>(std::ceil(rangeEnd * fullWidth));
    // At least one column, or the scale below divides by zero.
    const qreal desiredWidth = std::max(1, lastColumn - firstColumn);

    const int nextCompletion = std::min(actualCompletion + completionIncrement, lastColumn * 2);
    const int startCompletion = std::max(actualCompletion, firstColumn * 2);

    const Channels channels = m_channels;
    pPainter->save();

    switch (channels) {
    case static_cast<int>(ChannelFlag::LeftChannel):
        // Draw both channels.
        // Set the y axis to half the height of the item
        pPainter->translate(0.0, height());
        // Set the x axis to half the height of the item
        pPainter->scale(width() / desiredWidth, height() / kDesiredChannelHeight);
        break;
    case static_cast<int>(ChannelFlag::RightChannel):
        // Set the x axis to half the height of the item
        pPainter->scale(width() / desiredWidth, height() / kDesiredChannelHeight);
        break;
    default:
        // Draw both channels.
        // Set the y axis to half the height of the item
        pPainter->translate(0.0, height() / 2);
        // Set the x axis to half the height of the item
        pPainter->scale(width() / desiredWidth, height() / (2 * kDesiredChannelHeight));
    }
    // Slide the first drawn column to x=0. Applied after the scale above, so it
    // is expressed in waveform columns like the offsets drawFiltered/drawRgb use.
    pPainter->translate(-static_cast<qreal>(firstColumn), 0.0);

    Renderer renderer = m_renderer;
    for (int currentCompletion = startCompletion;
            currentCompletion < nextCompletion;
            currentCompletion += 2) {
        switch (renderer) {
        case Renderer::Filtered:
            drawFiltered(pPainter, channels, pWaveform, currentCompletion);
            break;
        default:
            drawRgb(pPainter, channels, pWaveform, currentCompletion);
        }
    }
    pPainter->restore();
}

void QmlWaveformOverview::drawRgb(QPainter* pPainter,
        Channels channels,
        ConstWaveformPointer pWaveform,
        int completion) const {
    const double offsetX = completion / 2.0;

    if (channels.testFlag(ChannelFlag::LeftChannel)) {
        // Draw left channel
        const QColor leftColor = getRgbPenColor(pWaveform, completion);
        if (leftColor.isValid()) {
            const uint8_t leftValue = pWaveform->getAll(completion);
            pPainter->setPen(leftColor);
            pPainter->drawLine(QPointF(offsetX, -leftValue), QPointF(offsetX, 0.0));
        }
    }

    if (channels.testFlag(ChannelFlag::RightChannel)) {
        // Draw right channel
        QColor rightColor = getRgbPenColor(pWaveform, completion + 1);
        if (rightColor.isValid()) {
            const uint8_t rightValue = pWaveform->getAll(completion + 1);
            pPainter->setPen(rightColor);
            pPainter->drawLine(QPointF(offsetX, 0.0), QPointF(offsetX, rightValue));
        }
    }
}

void QmlWaveformOverview::drawFiltered(QPainter* pPainter,
        Channels channels,
        ConstWaveformPointer pWaveform,
        int completion) const {
    const double offsetX = completion / 2.0;

    if (channels.testFlag(ChannelFlag::LeftChannel)) {
        const uint8_t leftHigh = pWaveform->getHigh(completion);
        pPainter->setPen(m_colorHigh);
        pPainter->drawLine(QPointF(offsetX, 2 * -leftHigh), QPointF(offsetX, 0.0));

        const uint8_t leftMid = pWaveform->getMid(completion);
        pPainter->setPen(m_colorMid);
        pPainter->drawLine(QPointF(offsetX, 1.5 * -leftMid), QPointF(offsetX, 0.0));

        const uint8_t leftLow = pWaveform->getLow(completion);
        pPainter->setPen(m_colorLow);
        pPainter->drawLine(QPointF(offsetX, -leftLow), QPointF(offsetX, 0.0));
    }

    if (channels.testFlag(ChannelFlag::RightChannel)) {
        const uint8_t rightHigh = pWaveform->getHigh(completion + 1);
        pPainter->setPen(m_colorHigh);
        pPainter->drawLine(QPointF(offsetX, 0), QPointF(offsetX, 2 * rightHigh));

        const uint8_t rightMid = pWaveform->getMid(completion + 1) * 2;
        pPainter->setPen(m_colorMid);
        pPainter->drawLine(QPointF(offsetX, 0), QPointF(offsetX, 1.5 * rightMid));

        const uint8_t rightLow = pWaveform->getLow(completion + 1);
        pPainter->setPen(m_colorLow);
        pPainter->drawLine(QPointF(offsetX, 0), QPointF(offsetX, rightLow));
    }
}

QColor QmlWaveformOverview::getRgbPenColor(ConstWaveformPointer pWaveform, int completion) const {
    // Retrieve "raw" LMH values from waveform
    qreal low = static_cast<qreal>(pWaveform->getLow(completion));
    qreal mid = static_cast<qreal>(pWaveform->getMid(completion));
    qreal high = static_cast<qreal>(pWaveform->getHigh(completion));

    // Do matrix multiplication
    qreal red = low * m_colorLow.redF() + mid * m_colorMid.redF() + high * m_colorHigh.redF();
    qreal green = low * m_colorLow.greenF() + mid * m_colorMid.greenF() +
            high * m_colorHigh.greenF();
    qreal blue = low * m_colorLow.blueF() + mid * m_colorMid.blueF() + high * m_colorHigh.blueF();

    // Normalize and draw
    qreal max = math_max3(red, green, blue);
    if (max > 0.0) {
        QColor color;
        color.setRgbF(
                static_cast<float>(red / max),
                static_cast<float>(green / max),
                static_cast<float>(blue / max));
        return color;
    }
    return QColor();
}

} // namespace qml
} // namespace mixxx
