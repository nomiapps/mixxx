#pragma once

#include <QPainter>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickPaintedItem>

#include "qmltrackproxy.h"
#include "waveform/waveform.h"

namespace mixxx {
namespace qml {

class QmlWaveformOverview : public QQuickPaintedItem {
    Q_OBJECT
    Q_FLAGS(Channels)
    Q_PROPERTY(mixxx::qml::QmlTrackProxy* track READ getTrack WRITE setTrack
                    NOTIFY trackChanged REQUIRED)
    Q_PROPERTY(mixxx::qml::QmlWaveformOverview::Channels channels READ
                    getChannels WRITE setChannels NOTIFY channelsChanged)
    Q_PROPERTY(mixxx::qml::QmlWaveformOverview::Renderer renderer MEMBER
                    m_renderer NOTIFY rendererChanged)
    Q_PROPERTY(QColor colorHigh MEMBER m_colorHigh NOTIFY colorHighChanged)
    Q_PROPERTY(QColor colorMid MEMBER m_colorMid NOTIFY colorMidChanged)
    Q_PROPERTY(QColor colorLow MEMBER m_colorLow NOTIFY colorLowChanged)
    /// The slice of the track to draw, as fractions of its length. Defaults to
    /// the whole track. A sampler holding a loop lifted off a deck keeps the
    /// WHOLE source track, so drawing all of it puts a six-second sample into a
    /// handful of pixels of a four-minute waveform; setting the range to the
    /// loop makes the overview show the part that actually plays.
    Q_PROPERTY(qreal rangeStart READ getRangeStart WRITE setRangeStart NOTIFY rangeStartChanged)
    Q_PROPERTY(qreal rangeEnd READ getRangeEnd WRITE setRangeEnd NOTIFY rangeEndChanged)
    QML_NAMED_ELEMENT(WaveformOverview)

  public:
    enum class ChannelFlag : int {
        LeftChannel = 1,
        RightChannel = 2,
        BothChannels = LeftChannel | RightChannel,
    };
    Q_DECLARE_FLAGS(Channels, ChannelFlag)

    enum class Renderer {
        RGB = 1,
        Filtered = 2,
    };
    Q_ENUM(Renderer)

    QmlWaveformOverview(QQuickItem* parent = nullptr);
    ~QmlWaveformOverview() override = default;

    void paint(QPainter* painter) override;

    void setTrack(QmlTrackProxy* track);
    QmlTrackProxy* getTrack() const;

    void setChannels(Channels channels);
    Channels getChannels() const;

    void setRangeStart(qreal start);
    qreal getRangeStart() const;
    void setRangeEnd(qreal end);
    qreal getRangeEnd() const;
  private slots:
    void slotWaveformUpdated();

  signals:
    void trackChanged();
    void channelsChanged(mixxx::qml::QmlWaveformOverview::Channels channels);
    void rendererChanged(mixxx::qml::QmlWaveformOverview::Renderer renderer);
    void colorHighChanged(const QColor& color);
    void colorMidChanged(const QColor& color);
    void colorLowChanged(const QColor& color);
    void rangeStartChanged(qreal rangeStart);
    void rangeEndChanged(qreal rangeEnd);

  private:
    void drawFiltered(QPainter* pPainter,
            Channels channels,
            ConstWaveformPointer pWaveform,
            int completion) const;
    void drawRgb(QPainter* pPainter,
            Channels channels,
            ConstWaveformPointer pWaveform,
            int completion) const;
    QColor getRgbPenColor(ConstWaveformPointer pWaveform, int completion) const;
    QmlTrackProxy* m_pTrack;
    Channels m_channels;
    Renderer m_renderer;
    QColor m_colorHigh;
    QColor m_colorMid;
    QColor m_colorLow;
    qreal m_rangeStart;
    qreal m_rangeEnd;
};

} // namespace qml
} // namespace mixxx
