#include "qml/qmlwavetableview.h"

#include <QPen>
#include <QQuickWindow>
#include <algorithm>
#include <cmath>

#include "mixer/synth.h"
#include "moc_qmlwavetableview.cpp"
#include "qml/qmlsynthproxy.h"

namespace {

// Every eighth sample of a 2048-sample frame, plus the guard sample so the
// line closes on the frame's own start.
constexpr int kSamplesPerPoint = 8;
constexpr int kPointsPerFrame = kWavetableFrameSize / kSamplesPerPoint + 1;
constexpr int kDefaultStackFrames = 32;
// The stack fades from the front frame to the back one.
constexpr qreal kFrontAlpha = 0.85;
constexpr qreal kBackAlpha = 0.22;

} // namespace

namespace mixxx {
namespace qml {

QmlWavetableView::QmlWavetableView(QQuickItem* parent)
        : QQuickPaintedItem(parent),
          m_position(0.0),
          m_frameColor(Qt::gray),
          m_currentColor(Qt::white),
          m_maxStackFrames(kDefaultStackFrames),
          m_stackDirty(true) {
    setAntialiasing(true);
}

void QmlWavetableView::setGroup(const QString& group) {
    if (m_group == group) {
        return;
    }
    if (m_pSynth) {
        disconnect(m_pSynth, nullptr, this, nullptr);
    }
    m_group = group;
    m_pSynth = QmlSynthProxy::synthForGroup(group);
    if (m_pSynth) {
        connect(m_pSynth, &Synth::wavetableChanged, this, &QmlWavetableView::slotTableChanged);
    }
    emit groupChanged();
    slotTableChanged();
}

void QmlWavetableView::slotTableChanged() {
    // The table is immutable and this is the GUI thread: swapping the
    // pointer here is the only write paint() ever races with, and paint()
    // runs while this thread is blocked.
    m_pTable = m_pSynth ? m_pSynth->currentTable() : nullptr;
    m_stackDirty = true;
    emit tableChanged();
    update();
}

void QmlWavetableView::setPosition(qreal position) {
    const qreal clamped = std::clamp(position, 0.0, 1.0);
    if (m_position == clamped) {
        return;
    }
    m_position = clamped;
    emit positionChanged();
    update();
}

void QmlWavetableView::setFrameColor(const QColor& color) {
    if (m_frameColor == color) {
        return;
    }
    m_frameColor = color;
    m_stackDirty = true;
    emit frameColorChanged();
    update();
}

void QmlWavetableView::setCurrentColor(const QColor& color) {
    if (m_currentColor == color) {
        return;
    }
    m_currentColor = color;
    emit currentColorChanged();
    update();
}

void QmlWavetableView::setMaxStackFrames(int frames) {
    frames = std::max(1, frames);
    if (m_maxStackFrames == frames) {
        return;
    }
    m_maxStackFrames = frames;
    m_stackDirty = true;
    emit maxStackFramesChanged();
    update();
}

QmlWavetableView::Perspective QmlWavetableView::perspective() const {
    // The front frame sits low and left; the back frame is shifted up and
    // right and drawn smaller, and every frame between is on the line
    // joining them. The whole stack fills the item.
    const qreal w = width();
    const qreal h = height();
    Perspective p;
    p.shiftX = w * 0.26;
    p.shiftY = h * 0.42;
    p.left = w * 0.03;
    p.span = w - p.left - p.shiftX - w * 0.03;
    p.amplitude = (h - p.shiftY) * 0.46;
    p.baseline = h - (h - p.shiftY) * 0.5;
    p.shrink = 0.35;
    return p;
}

void QmlWavetableView::frameToPolyline(
        const float* pFrame, qreal depth, QVector<QPointF>* pPoints) const {
    const Perspective p = perspective();
    const qreal x0 = p.left + depth * p.shiftX;
    const qreal y0 = p.baseline - depth * p.shiftY;
    const qreal amplitude = p.amplitude * (1.0 - p.shrink * depth);
    pPoints->resize(kPointsPerFrame);
    for (int j = 0; j < kPointsPerFrame; ++j) {
        const qreal x = x0 + p.span * j / (kPointsPerFrame - 1);
        const qreal y = y0 - pFrame[j * kSamplesPerPoint] * amplitude;
        (*pPoints)[j] = QPointF(x, y);
    }
}

void QmlWavetableView::blendedFrameToPolyline(qreal depth, QVector<QPointF>* pPoints) const {
    // The same two frames and blend the engine resolves wt_position to, so
    // what is drawn bright is what sounds.
    const int last = m_pTable->frameCount - 1;
    const qreal position = depth * last;
    const int frameA = static_cast<int>(position);
    const int frameB = std::min(frameA + 1, last);
    const float blend = static_cast<float>(position - frameA);
    const float* pA = m_pTable->frame(frameA);
    const float* pB = m_pTable->frame(frameB);
    const Perspective p = perspective();
    const qreal x0 = p.left + depth * p.shiftX;
    const qreal y0 = p.baseline - depth * p.shiftY;
    const qreal amplitude = p.amplitude * (1.0 - p.shrink * depth);
    pPoints->resize(kPointsPerFrame);
    for (int j = 0; j < kPointsPerFrame; ++j) {
        const int i = j * kSamplesPerPoint;
        const float sample = pA[i] + (pB[i] - pA[i]) * blend;
        const qreal x = x0 + p.span * j / (kPointsPerFrame - 1);
        (*pPoints)[j] = QPointF(x, y0 - sample * amplitude);
    }
}

void QmlWavetableView::rebuildStack() {
    const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
    const QSize pixels(static_cast<int>(std::ceil(width() * dpr)),
            static_cast<int>(std::ceil(height() * dpr)));
    m_stack = QImage(pixels, QImage::Format_ARGB32_Premultiplied);
    m_stack.setDevicePixelRatio(dpr);
    m_stack.fill(Qt::transparent);
    m_stackDirty = false;
    if (!m_pTable || m_pTable->frameCount <= 0) {
        return;
    }

    QPainter painter(&m_stack);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const int frameCount = m_pTable->frameCount;
    const int drawn = std::min(frameCount, m_maxStackFrames);
    QVector<QPointF> points;
    // Back to front, so the nearer frames are drawn over the farther ones.
    for (int k = drawn - 1; k >= 0; --k) {
        const int frame = drawn > 1 ? static_cast<int>(std::lround(
                                              static_cast<qreal>(k) * (frameCount - 1) / (drawn - 1)))
                                    : 0;
        const qreal depth = frameCount > 1 ? static_cast<qreal>(frame) / (frameCount - 1) : 0.0;
        QColor color = m_frameColor;
        color.setAlphaF(kFrontAlpha + (kBackAlpha - kFrontAlpha) * depth);
        painter.setPen(QPen(color, 1.0));
        frameToPolyline(m_pTable->frame(frame), depth, &points);
        painter.drawPolyline(points.constData(), points.size());
    }
}

void QmlWavetableView::paint(QPainter* pPainter) {
    if (width() <= 0 || height() <= 0) {
        return;
    }
    const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
    const QSize wanted(static_cast<int>(std::ceil(width() * dpr)),
            static_cast<int>(std::ceil(height() * dpr)));
    if (m_stackDirty || m_stack.size() != wanted) {
        rebuildStack();
    }
    pPainter->drawImage(QPointF(0.0, 0.0), m_stack);
    if (!m_pTable || m_pTable->frameCount <= 0) {
        return;
    }
    QVector<QPointF> points;
    blendedFrameToPolyline(m_position, &points);
    pPainter->setRenderHint(QPainter::Antialiasing, true);
    pPainter->setPen(QPen(m_currentColor, 2.0));
    pPainter->drawPolyline(points.constData(), points.size());
}

} // namespace qml
} // namespace mixxx
