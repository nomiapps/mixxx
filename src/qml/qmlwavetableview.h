#pragma once

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickPaintedItem>
#include <QString>
#include <QVector>
#include <memory>

#include "engine/channels/wavetable.h"

class Synth;

namespace mixxx {
namespace qml {

/// A synth's wavetable as a stack of frames in perspective, the first frame
/// at the front and the rest receding up and to the right, with the frame
/// wt_position is playing drawn bright at its depth: the display Serum
/// popularised, so the table reads as a landscape you move a cursor through.
///
/// It draws from the Synth's own copy of the table (Synth::currentTable),
/// which is immutable and only ever replaced on the GUI thread, so no lock
/// is needed even though paint() runs during scene-graph sync. The stack is
/// rendered once per table or size into an image; a position change only
/// redraws the one bright frame over it.
class QmlWavetableView : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString group READ group WRITE setGroup NOTIFY groupChanged REQUIRED)
    /// 0..1 through the table, the same reading the engine's wt_position
    /// gets: the bright frame is the crossfade the ear hears.
    Q_PROPERTY(qreal position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor NOTIFY frameColorChanged)
    Q_PROPERTY(QColor currentColor READ currentColor WRITE setCurrentColor NOTIFY currentColorChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY tableChanged)
    /// Frames drawn in the stack; a bigger table is thinned evenly to this.
    Q_PROPERTY(int maxStackFrames READ maxStackFrames WRITE setMaxStackFrames NOTIFY
                    maxStackFramesChanged)
    QML_NAMED_ELEMENT(WavetableView)

  public:
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

  signals:
    void groupChanged();
    void positionChanged();
    void frameColorChanged();
    void currentColorChanged();
    void tableChanged();
    void maxStackFramesChanged();

  private slots:
    void slotTableChanged();

  private:
    /// Where the item puts a frame at depth 0 (front) .. 1 (back).
    struct Perspective {
        qreal left;      // x of the front frame's first point
        qreal span;      // width of one frame
        qreal baseline;  // y of the front frame's zero line
        qreal amplitude; // half-height of the front frame
        qreal shiftX;    // how far the back frame moves right
        qreal shiftY;    // how far the back frame moves up
        qreal shrink;    // how much smaller the back frame is, 0..1
    };
    Perspective perspective() const;
    void frameToPolyline(const float* pFrame, qreal depth, QVector<QPointF>* pPoints) const;
    void blendedFrameToPolyline(qreal depth, QVector<QPointF>* pPoints) const;
    void rebuildStack();

    QString m_group;
    qreal m_position;
    QColor m_frameColor;
    QColor m_currentColor;
    int m_maxStackFrames;
    QPointer<Synth> m_pSynth;
    std::shared_ptr<const Wavetable> m_pTable;
    QImage m_stack;
    bool m_stackDirty;
};

} // namespace qml
} // namespace mixxx
