#include <gtest/gtest.h>

#include <QImage>
#include <QPainter>
#include <cmath>
#include <numbers>

#include "engine/channels/wavetable.h"
#include "qml/qmlwavetableview.h"

// The 3D wavetable view's projection and drawing, without a synth or a window: a
// table is drawn straight into an image and the pixels are counted.
namespace {

using Camera = mixxx::qml::QmlWavetableView::Camera;
using View = mixxx::qml::QmlWavetableView;

std::unique_ptr<Wavetable> table(int frames) {
    auto pTable = Wavetable::create(frames);
    for (int f = 0; f < frames; ++f) {
        // A sine whose amplitude sweeps across the frames, so the surface has shape.
        const float amplitude = frames > 1 ? static_cast<float>(f) / (frames - 1) : 1.0f;
        for (int i = 0; i < kWavetableFrameSize; ++i) {
            pTable->frame(f)[i] = amplitude *
                    static_cast<float>(std::sin(2.0 * std::numbers::pi * i / kWavetableFrameSize));
        }
    }
    pTable->fillGuards();
    return pTable;
}

QImage render(const Wavetable& wavetable, qreal yaw, qreal pitch, bool surface, qreal position = -1) {
    QImage image(400, 200, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    const Camera camera = Camera::fit(QSizeF(400, 200), yaw, pitch);
    View::drawTable(&painter, wavetable, camera, 32, QColor(160, 160, 160), surface);
    if (position >= 0) {
        View::drawCurrentFrame(&painter, wavetable, camera, position, QColor(255, 180, 0));
    }
    return image;
}

int opaquePixels(const QImage& image) {
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            count += qAlpha(image.pixel(x, y)) > 0 ? 1 : 0;
        }
    }
    return count;
}

int pixelsOfColour(const QImage& image, QRgb colour) {
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb p = image.pixel(x, y);
            count += (qRed(p) == qRed(colour) && qGreen(p) == qGreen(colour) &&
                             qBlue(p) == qBlue(colour))
                    ? 1
                    : 0;
        }
    }
    return count;
}

TEST(QmlWavetableViewTest, DefaultAngleRecedesUpAndToTheRight) {
    const Camera camera = Camera::fit(QSizeF(400, 200), View::kDefaultYaw, View::kDefaultPitch);
    qreal frontDepth = 0;
    qreal backDepth = 0;
    const QPointF front = camera.project(0.0, 0.0, 0.0, &frontDepth);
    const QPointF back = camera.project(0.0, 0.0, 1.0, &backDepth);
    EXPECT_GT(back.x(), front.x()) << "the back frame is to the right";
    EXPECT_LT(back.y(), front.y()) << "and higher on screen";
    EXPECT_GT(backDepth, frontDepth) << "and farther from the eye";
}

TEST(QmlWavetableViewTest, TheFittedTableStaysInsideTheItemAtAnyAngle) {
    for (qreal yaw = -180; yaw <= 180; yaw += 15) {
        for (qreal pitch = View::kMinPitch; pitch <= View::kMaxPitch; pitch += 15) {
            const Camera camera = Camera::fit(QSizeF(400, 200), yaw, pitch);
            for (const qreal phase : {0.0, 1.0}) {
                for (const qreal sample : {-1.0, 1.0}) {
                    for (const qreal depth : {0.0, 1.0}) {
                        const QPointF p = camera.project(phase, sample, depth);
                        EXPECT_GE(p.x(), -0.5) << yaw << " " << pitch;
                        EXPECT_LE(p.x(), 400.5) << yaw << " " << pitch;
                        EXPECT_GE(p.y(), -0.5) << yaw << " " << pitch;
                        EXPECT_LE(p.y(), 200.5) << yaw << " " << pitch;
                    }
                }
            }
        }
    }
}

TEST(QmlWavetableViewTest, TheSurfaceFillsWhatTheWireframeOnlyOutlines) {
    const auto pTable = table(16);
    const int wire = opaquePixels(render(*pTable, View::kDefaultYaw, View::kDefaultPitch, false));
    const int solid = opaquePixels(render(*pTable, View::kDefaultYaw, View::kDefaultPitch, true));
    EXPECT_GT(wire, 500);
    EXPECT_GT(solid, wire * 2);
}

TEST(QmlWavetableViewTest, RotatingChangesThePicture) {
    const auto pTable = table(16);
    const QImage a = render(*pTable, View::kDefaultYaw, View::kDefaultPitch, true);
    const QImage b = render(*pTable, View::kDefaultYaw + 90, View::kDefaultPitch, true);
    const QImage c = render(*pTable, View::kDefaultYaw, View::kMaxPitch, true);
    EXPECT_NE(a, b);
    EXPECT_NE(a, c);
}

TEST(QmlWavetableViewTest, TheCurrentFrameIsDrawnInItsColour) {
    const auto pTable = table(16);
    const QRgb amber = qRgb(255, 180, 0);
    EXPECT_EQ(pixelsOfColour(render(*pTable, View::kDefaultYaw, View::kDefaultPitch, true), amber), 0);
    EXPECT_GT(pixelsOfColour(render(*pTable, View::kDefaultYaw, View::kDefaultPitch, true, 0.5), amber),
            100);
}

TEST(QmlWavetableViewTest, AGridIsDrawnARowAtATime) {
    // 2 columns by 2 rows: the top row at amplitude 0.2, the bottom at 1.
    auto pGrid = Wavetable::create(4);
    pGrid->columns = 2;
    for (int f = 0; f < 4; ++f) {
        const float amplitude = f < 2 ? 0.2f : 1.0f;
        for (int i = 0; i < kWavetableFrameSize; ++i) {
            pGrid->frame(f)[i] = amplitude *
                    static_cast<float>(std::sin(2.0 * std::numbers::pi * i / kWavetableFrameSize));
        }
    }
    pGrid->fillGuards();

    const auto pRow = View::gridRow(*pGrid, 0.5);
    ASSERT_TRUE(pRow);
    EXPECT_EQ(2, pRow->frameCount);
    EXPECT_EQ(1, pRow->gridRows());
    const int peak = kWavetableFrameSize / 4;
    EXPECT_NEAR(0.6f, pRow->frame(0)[peak], 1e-5f);
    EXPECT_NEAR(0.6f, pRow->frame(1)[peak], 1e-5f);
    EXPECT_FLOAT_EQ(pRow->frame(1)[0], pRow->frame(1)[kWavetableFrameSize]);
    EXPECT_NEAR(1.0f, View::gridRow(*pGrid, 1.0)->frame(0)[peak], 1e-5f);

    EXPECT_FALSE(View::gridRow(*table(16), 0.5)) << "a plain list is its own row";
}

TEST(QmlWavetableViewTest, TheGridMapMarksThePosition) {
    QImage image(80, 40, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        View::drawGridMap(&painter,
                QRectF(5, 5, 70, 30),
                8,
                4,
                1.0,
                1.0,
                QColor(160, 160, 160),
                QColor(255, 180, 0));
    }
    // Amber only around the bottom-right corner, where x = y = 1 is.
    int amberNear = 0;
    int amberFar = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb p = image.pixel(x, y);
            if (qAlpha(p) > 100 && qRed(p) > 200 && qBlue(p) < 90) {
                (x > 60 && y > 25 ? amberNear : amberFar) += 1;
            }
        }
    }
    EXPECT_GT(amberNear, 5);
    EXPECT_EQ(0, amberFar);
    EXPECT_GT(opaquePixels(image), 32) << "a dot for each of the 32 frames";
}

TEST(QmlWavetableViewTest, ASingleFrameTableDrawsItsLineAndNoSurface) {
    const auto pTable = table(1);
    const int wire = opaquePixels(render(*pTable, View::kDefaultYaw, View::kDefaultPitch, false));
    const int solid = opaquePixels(render(*pTable, View::kDefaultYaw, View::kDefaultPitch, true));
    EXPECT_GT(wire, 100);
    EXPECT_EQ(solid, wire);
}

} // namespace
