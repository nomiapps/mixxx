#pragma once

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickPaintedItem>
#include <QSizeF>
#include <QString>
#include <QVector>
#include <memory>

#include "engine/channels/wavetable.h"

class Synth;

namespace mixxx {
namespace qml {

/// A synth's wavetable as a 3D surface: phase runs across, the frames recede in
/// depth, and each frame's waveform is the height, shaded so the shape of the whole
/// table reads at a glance. Drag to rotate (yaw, pitch); the frame wt_position is
/// playing is drawn bright at its depth, so the cursor moves through the landscape
/// the ear hears. With `surface` off it is the wireframe stack of frame lines.
///
/// It draws from the Synth's own copy of the table (Synth::currentTable), which is
/// immutable and only ever replaced on the GUI thread, so no lock is needed even
/// though paint() runs during scene-graph sync. The surface is rendered once per
/// table, size, angle or colour into an image; a position change only redraws the
/// one bright frame over it.
class QmlWavetableView : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString group READ group WRITE setGroup NOTIFY groupChanged REQUIRED)
    /// 0..1 through the table, the same reading the engine's wt_position gets: the
    /// bright frame is the crossfade the ear hears.
    Q_PROPERTY(qreal position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor NOTIFY frameColorChanged)
    Q_PROPERTY(QColor currentColor READ currentColor WRITE setCurrentColor NOTIFY currentColorChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY tableChanged)
    /// Frames drawn; a bigger table is thinned evenly to this.
    Q_PROPERTY(int maxStackFrames READ maxStackFrames WRITE setMaxStackFrames NOTIFY
                    maxStackFramesChanged)
    /// Rotation about the vertical axis, degrees. Negative turns the back frames right.
    Q_PROPERTY(qreal yaw READ yaw WRITE setYaw NOTIFY viewChanged)
    /// Tilt, degrees, clamped to kMinPitch..kMaxPitch. Positive looks down onto the table.
    Q_PROPERTY(qreal pitch READ pitch WRITE setPitch NOTIFY viewChanged)
    /// Shaded surface between the frames (true) or the frame lines alone (false).
    Q_PROPERTY(bool surface READ surface WRITE setSurface NOTIFY viewChanged)
    QML_NAMED_ELEMENT(WavetableView)

  public:
    static constexpr qreal kDefaultYaw = -14.0;
    static constexpr qreal kDefaultPitch = 40.0;
    static constexpr qreal kMinPitch = -10.0;
    static constexpr qreal kMaxPitch = 80.0;

    /// Where a point of the table lands on screen for one view angle and item size.
    /// Model space: phase across, sample height up, depth from the front frame to the
    /// back one, each centred on 0; a perspective camera sits in front of the table.
    struct Camera {
        qreal cosYaw = 1.0;
        qreal sinYaw = 0.0;
        qreal cosPitch = 1.0;
        qreal sinPitch = 0.0;
        qreal scale = 1.0;
        qreal centerX = 0.0;
        qreal centerY = 0.0;

        /// The camera that fits the whole table, at this angle, inside `size`.
        static Camera fit(const QSizeF& size, qreal yawDegrees, qreal pitchDegrees);
        /// phase and depth 0..1, sample -1..1. Returns the screen point; `pEyeDepth`
        /// (optional) gets the distance from the eye, larger is farther.
        QPointF project(qreal phase, qreal sample, qreal depth, qreal* pEyeDepth = nullptr) const;
    };

    /// Draws the table at `camera` onto `pPainter`: the thinned frames and, when
    /// `surface`, the shaded quads between them, far to near.
    static void drawTable(QPainter* pPainter,
            const Wavetable& table,
            const Camera& camera,
            int maxFrames,
            const QColor& frameColor,
            bool surface);
    /// Draws the frame the engine plays at `position` (0..1), blended the same way.
    static void drawCurrentFrame(QPainter* pPainter,
            const Wavetable& table,
            const Camera& camera,
            qreal position,
            const QColor& color);

    explicit QmlWavetableView(QQuickItem* parent = nullptr);
    ~QmlWavetableView() override = default;

    void paint(QPainter* pPainter) override;

    QString group() const {
        return m_group;
    }
    void setGroup(const QString& group);
    qreal position() const {
        return m_position;
    }
    void setPosition(qreal position);
    QColor frameColor() const {
        return m_frameColor;
    }
    void setFrameColor(const QColor& color);
    QColor currentColor() const {
        return m_currentColor;
    }
    void setCurrentColor(const QColor& color);
    int frameCount() const {
        return m_pTable ? m_pTable->frameCount : 0;
    }
    int maxStackFrames() const {
        return m_maxStackFrames;
    }
    void setMaxStackFrames(int frames);
    qreal yaw() const {
        return m_yaw;
    }
    void setYaw(qreal degrees);
    qreal pitch() const {
        return m_pitch;
    }
    void setPitch(qreal degrees);
    bool surface() const {
        return m_surface;
    }
    void setSurface(bool surface);

    /// Back to the default angle.
    Q_INVOKABLE void resetView();

  signals:
    void groupChanged();
    void positionChanged();
    void frameColorChanged();
    void currentColorChanged();
    void tableChanged();
    void maxStackFramesChanged();
    void viewChanged();

  private slots:
    void slotTableChanged();

  private:
    void rebuildImage();

    QString m_group;
    qreal m_position;
    QColor m_frameColor;
    QColor m_currentColor;
    int m_maxStackFrames;
    qreal m_yaw;
    qreal m_pitch;
    bool m_surface;
    QPointer<Synth> m_pSynth;
    std::shared_ptr<const Wavetable> m_pTable;
    QImage m_image;
    bool m_imageDirty;
};

} // namespace qml
} // namespace mixxx
