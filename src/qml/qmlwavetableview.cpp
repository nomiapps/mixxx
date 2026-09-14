#include "qml/qmlwavetableview.h"

#include <QPen>
#include <QPolygonF>
#include <QQuickWindow>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "mixer/synth.h"
#include "moc_qmlwavetableview.cpp"
#include "qml/qmlsynthproxy.h"

namespace {

// The surface samples every 32nd point of a 2048-sample frame, plus the guard
// sample so each frame closes on its own start: 64 quads across.
constexpr int kSurfaceStride = 32;
constexpr int kSurfacePoints = kWavetableFrameSize / kSurfaceStride + 1;
// The bright frame is finer, since it is the one the eye follows.
constexpr int kCurrentStride = 8;
constexpr int kCurrentPoints = kWavetableFrameSize / kCurrentStride + 1;
constexpr int kDefaultFrames = 32;
// Half-extents of the table in model units. Phase is wide and depth shallow, so the
// table fills a wide, short item; a full-scale waveform is tall enough to read and
// flat enough not to hide the frames behind it.
constexpr qreal kPhaseSpan = 1.8;
constexpr qreal kDepthSpan = 0.8;
constexpr qreal kHeight = 0.40;
// The eye's distance from the table's centre; smaller is more perspective.
constexpr qreal kCameraDistance = 7.0;
// Fraction of the item the fitted table fills.
constexpr qreal kFill = 0.94;
// Light from above, slightly in front and to the left.
constexpr qreal kLightX = -0.35;
constexpr qreal kLightY = 0.85;
constexpr qreal kLightZ = -0.4;
constexpr qreal kFillAlpha = 0.55;
constexpr qreal kFrontLineAlpha = 0.9;
constexpr qreal kBackLineAlpha = 0.3;

qreal radians(qreal degrees) {
    return degrees * std::numbers::pi / 180.0;
}

struct Vertex {
    QPointF screen;
    qreal eyeDepth = 0.0;
    // Rotated model coordinates, for the shading normal.
    qreal x = 0.0;
    qreal y = 0.0;
    qreal z = 0.0;
};

} // namespace

namespace mixxx {
namespace qml {

// static
QmlWavetableView::Camera QmlWavetableView::Camera::fit(
        const QSizeF& size, qreal yawDegrees, qreal pitchDegrees) {
    Camera camera;
    camera.cosYaw = std::cos(radians(yawDegrees));
    camera.sinYaw = std::sin(radians(yawDegrees));
    camera.cosPitch = std::cos(radians(pitchDegrees));
    camera.sinPitch = std::sin(radians(pitchDegrees));
    // Project the table's bounding box at unit scale, then scale and centre it.
    qreal left = 1e9;
    qreal right = -1e9;
    qreal top = 1e9;
    qreal bottom = -1e9;
    for (const qreal phase : {0.0, 1.0}) {
        for (const qreal sample : {-1.0, 1.0}) {
            for (const qreal depth : {0.0, 1.0}) {
                const QPointF p = camera.project(phase, sample, depth);
                left = std::min(left, p.x());
                right = std::max(right, p.x());
                top = std::min(top, p.y());
                bottom = std::max(bottom, p.y());
            }
        }
    }
    const qreal width = std::max(right - left, 1e-6);
    const qreal height = std::max(bottom - top, 1e-6);
    camera.scale = std::min(size.width() * kFill / width, size.height() * kFill / height);
    camera.centerX = size.width() / 2.0 - camera.scale * (left + right) / 2.0;
    camera.centerY = size.height() / 2.0 - camera.scale * (top + bottom) / 2.0;
    return camera;
}

QPointF QmlWavetableView::Camera::project(
        qreal phase, qreal sample, qreal depth, qreal* pEyeDepth) const {
    const qreal x = (2.0 * phase - 1.0) * kPhaseSpan;
    const qreal y = sample * kHeight;
    const qreal z = (2.0 * depth - 1.0) * kDepthSpan;
    // Yaw about the vertical axis, then pitch about the horizontal one.
    const qreal x1 = x * cosYaw - z * sinYaw;
    const qreal z1 = x * sinYaw + z * cosYaw;
    const qreal y2 = y * cosPitch + z1 * sinPitch;
    const qreal z2 = -y * sinPitch + z1 * cosPitch;
    const qreal eye = kCameraDistance + z2;
    if (pEyeDepth) {
        *pEyeDepth = eye;
    }
    const qreal perspective = kCameraDistance / std::max(eye, 0.1);
    return QPointF(centerX + x1 * perspective * scale, centerY - y2 * perspective * scale);
}

// static
void QmlWavetableView::drawTable(QPainter* pPainter,
        const Wavetable& table,
        const Camera& camera,
        int maxFrames,
        const QColor& frameColor,
        bool surface) {
    const int frameCount = table.frameCount;
    if (frameCount <= 0) {
        return;
    }
    const int drawn = std::clamp(maxFrames, 1, frameCount);
    // Frame index and depth of each drawn row, evenly thinned.
    std::vector<int> frames(drawn);
    std::vector<qreal> depths(drawn);
    for (int k = 0; k < drawn; ++k) {
        frames[k] = drawn > 1
                ? static_cast<int>(std::lround(static_cast<qreal>(k) * (frameCount - 1) / (drawn - 1)))
                : 0;
        depths[k] = frameCount > 1 ? static_cast<qreal>(frames[k]) / (frameCount - 1) : 0.0;
    }

    const qreal cy = camera.cosYaw;
    const qreal sy = camera.sinYaw;
    const qreal cp = camera.cosPitch;
    const qreal sp = camera.sinPitch;
    std::vector<Vertex> grid(static_cast<std::size_t>(drawn) * kSurfacePoints);
    for (int k = 0; k < drawn; ++k) {
        const float* pFrame = table.frame(frames[k]);
        for (int j = 0; j < kSurfacePoints; ++j) {
            const qreal phase = static_cast<qreal>(j) / (kSurfacePoints - 1);
            const qreal sample = pFrame[j * kSurfaceStride];
            Vertex& v = grid[static_cast<std::size_t>(k) * kSurfacePoints + j];
            v.screen = camera.project(phase, sample, depths[k], &v.eyeDepth);
            const qreal x = (2.0 * phase - 1.0) * kPhaseSpan;
            const qreal y = sample * kHeight;
            const qreal z = (2.0 * depths[k] - 1.0) * kDepthSpan;
            v.x = x * cy - z * sy;
            const qreal z1 = x * sy + z * cy;
            v.y = y * cp + z1 * sp;
            v.z = -y * sp + z1 * cp;
        }
    }
    const auto at = [&](int k, int j) -> const Vertex& {
        return grid[static_cast<std::size_t>(k) * kSurfacePoints + j];
    };

    // Everything is drawn far to near: each quad of surface between two frames, and
    // each segment of a frame line, as its own item, so nearer surface hides farther
    // lines without a depth buffer.
    struct Item {
        qreal depth;
        int k;
        int j;
        bool quad;
    };
    std::vector<Item> items;
    items.reserve(static_cast<std::size_t>(drawn) * kSurfacePoints * 2);
    for (int k = 0; k < drawn; ++k) {
        for (int j = 0; j + 1 < kSurfacePoints; ++j) {
            items.push_back({(at(k, j).eyeDepth + at(k, j + 1).eyeDepth) / 2.0, k, j, false});
            if (surface && k + 1 < drawn) {
                const qreal depth = (at(k, j).eyeDepth + at(k, j + 1).eyeDepth +
                                            at(k + 1, j).eyeDepth + at(k + 1, j + 1).eyeDepth) /
                        4.0;
                // A hair farther than the lines along its edges, so they draw over it.
                items.push_back({depth + 1e-6, k, j, true});
            }
        }
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        return a.depth > b.depth;
    });

    const qreal lightLength = std::sqrt(kLightX * kLightX + kLightY * kLightY + kLightZ * kLightZ);
    const qreal lx = kLightX / lightLength;
    const qreal ly = kLightY / lightLength;
    const qreal lz = kLightZ / lightLength;
    const QColor baseHsl = frameColor.toHsl();
    pPainter->setRenderHint(QPainter::Antialiasing, true);
    QPolygonF quad(4);
    for (const Item& item : items) {
        const Vertex& a = at(item.k, item.j);
        const Vertex& b = at(item.k, item.j + 1);
        if (!item.quad) {
            QColor line = frameColor;
            const qreal depth = drawn > 1 ? static_cast<qreal>(item.k) / (drawn - 1) : 0.0;
            line.setAlphaF(static_cast<float>(
                    kFrontLineAlpha + (kBackLineAlpha - kFrontLineAlpha) * depth));
            pPainter->setPen(QPen(line, 1.0));
            pPainter->drawLine(a.screen, b.screen);
            continue;
        }
        const Vertex& c = at(item.k + 1, item.j + 1);
        const Vertex& d = at(item.k + 1, item.j);
        // Lambert shading from the quad's normal; either side faces the light.
        const qreal ux = b.x - a.x;
        const qreal uy = b.y - a.y;
        const qreal uz = b.z - a.z;
        const qreal vx = d.x - a.x;
        const qreal vy = d.y - a.y;
        const qreal vz = d.z - a.z;
        const qreal nx = uy * vz - uz * vy;
        const qreal ny = uz * vx - ux * vz;
        const qreal nz = ux * vy - uy * vx;
        const qreal length = std::sqrt(nx * nx + ny * ny + nz * nz);
        const qreal lambert = length > 1e-12 ? std::abs((nx * lx + ny * ly + nz * lz) / length) : 0.0;
        QColor fill;
        fill.setHslF(baseHsl.hslHueF(),
                baseHsl.hslSaturationF(),
                static_cast<float>(std::clamp(0.18 + 0.62 * lambert, 0.0, 1.0)),
                static_cast<float>(kFillAlpha));
        pPainter->setPen(Qt::NoPen);
        pPainter->setBrush(fill);
        quad[0] = a.screen;
        quad[1] = b.screen;
        quad[2] = c.screen;
        quad[3] = d.screen;
        pPainter->drawPolygon(quad);
    }
    pPainter->setBrush(Qt::NoBrush);
}

// static
void QmlWavetableView::drawCurrentFrame(QPainter* pPainter,
        const Wavetable& table,
        const Camera& camera,
        qreal position,
        const QColor& color) {
    if (table.frameCount <= 0) {
        return;
    }
    // The same two frames and blend the engine resolves wt_position to, so what is
    // drawn bright is what sounds.
    position = std::clamp(position, 0.0, 1.0);
    const int last = table.frameCount - 1;
    const qreal index = position * last;
    const int frameA = static_cast<int>(index);
    const int frameB = std::min(frameA + 1, last);
    const float blend = static_cast<float>(index - frameA);
    const float* pA = table.frame(frameA);
    const float* pB = table.frame(frameB);
    QPolygonF line(kCurrentPoints);
    for (int j = 0; j < kCurrentPoints; ++j) {
        const int i = j * kCurrentStride;
        const float sample = pA[i] + (pB[i] - pA[i]) * blend;
        line[j] = camera.project(static_cast<qreal>(j) / (kCurrentPoints - 1), sample, position);
    }
    pPainter->setRenderHint(QPainter::Antialiasing, true);
    pPainter->setPen(QPen(color, 2.0));
    pPainter->drawPolyline(line);
}

QmlWavetableView::QmlWavetableView(QQuickItem* parent)
        : QQuickPaintedItem(parent),
          m_position(0.0),
          m_frameColor(Qt::gray),
          m_currentColor(Qt::white),
          m_maxStackFrames(kDefaultFrames),
          m_yaw(kDefaultYaw),
          m_pitch(kDefaultPitch),
          m_surface(true),
          m_imageDirty(true) {
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
    // The table is immutable and this is the GUI thread: swapping the pointer here
    // is the only write paint() ever races with, and paint() runs while this thread
    // is blocked.
    m_pTable = m_pSynth ? m_pSynth->currentTable() : nullptr;
    m_imageDirty = true;
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
    m_imageDirty = true;
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
    m_imageDirty = true;
    emit maxStackFramesChanged();
    update();
}

void QmlWavetableView::setYaw(qreal degrees) {
    // Kept in -180..180 so a long drag does not grow without bound.
    degrees = std::remainder(degrees, 360.0);
    if (m_yaw == degrees) {
        return;
    }
    m_yaw = degrees;
    m_imageDirty = true;
    emit viewChanged();
    update();
}

void QmlWavetableView::setPitch(qreal degrees) {
    degrees = std::clamp(degrees, kMinPitch, kMaxPitch);
    if (m_pitch == degrees) {
        return;
    }
    m_pitch = degrees;
    m_imageDirty = true;
    emit viewChanged();
    update();
}

void QmlWavetableView::setSurface(bool surface) {
    if (m_surface == surface) {
        return;
    }
    m_surface = surface;
    m_imageDirty = true;
    emit viewChanged();
    update();
}

void QmlWavetableView::resetView() {
    setYaw(kDefaultYaw);
    setPitch(kDefaultPitch);
}

void QmlWavetableView::rebuildImage() {
    const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
    const QSize pixels(static_cast<int>(std::ceil(width() * dpr)),
            static_cast<int>(std::ceil(height() * dpr)));
    m_image = QImage(pixels, QImage::Format_ARGB32_Premultiplied);
    m_image.setDevicePixelRatio(dpr);
    m_image.fill(Qt::transparent);
    m_imageDirty = false;
    if (!m_pTable || m_pTable->frameCount <= 0) {
        return;
    }
    QPainter painter(&m_image);
    drawTable(&painter,
            *m_pTable,
            Camera::fit(QSizeF(width(), height()), m_yaw, m_pitch),
            m_maxStackFrames,
            m_frameColor,
            m_surface);
}

void QmlWavetableView::paint(QPainter* pPainter) {
    if (width() <= 0 || height() <= 0) {
        return;
    }
    const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
    const QSize wanted(static_cast<int>(std::ceil(width() * dpr)),
            static_cast<int>(std::ceil(height() * dpr)));
    if (m_imageDirty || m_image.size() != wanted) {
        rebuildImage();
    }
    pPainter->drawImage(QPointF(0.0, 0.0), m_image);
    if (!m_pTable || m_pTable->frameCount <= 0) {
        return;
    }
    drawCurrentFrame(pPainter,
            *m_pTable,
            Camera::fit(QSizeF(width(), height()), m_yaw, m_pitch),
            m_position,
            m_currentColor);
}

} // namespace qml
} // namespace mixxx
