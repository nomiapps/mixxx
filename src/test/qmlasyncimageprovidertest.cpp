#include <gtest/gtest.h>

#include <QColor>
#include <QImage>
#include <QSize>

#include "qml/asyncimageprovider.h"

namespace {

using mixxx::qml::fitImageToRequestedSize;

QImage solidImage(int width, int height) {
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::darkCyan);
    return image;
}

// Alternating single black and white pixels, so every 2x2 source block a 2:1
// downsample folds into one output pixel holds two of each: a smooth filter
// averages them to grey, a nearest-neighbour one picks one and stays pure.
// (2x2 blocks would NOT discriminate -- at exactly 2:1 each output pixel then
// covers one uniform block and both filters return pure black or white.)
QImage checkerboard(int size) {
    QImage image(size, size, QImage::Format_ARGB32);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool dark = (x + y) % 2 == 0;
            image.setPixelColor(x, y, dark ? Qt::black : Qt::white);
        }
    }
    return image;
}

} // namespace

TEST(QmlAsyncImageProviderTest, KeepsAspectRatioInsideRequestedSize) {
    const QImage fitted = fitImageToRequestedSize(solidImage(400, 200), QSize(100, 100));
    EXPECT_EQ(QSize(100, 50), fitted.size());
}

TEST(QmlAsyncImageProviderTest, ScalesByWidthAloneWhenHeightIsUnset) {
    const QImage fitted = fitImageToRequestedSize(solidImage(400, 200), QSize(100, 0));
    EXPECT_EQ(QSize(100, 50), fitted.size());
}

TEST(QmlAsyncImageProviderTest, ScalesByHeightAloneWhenWidthIsUnset) {
    const QImage fitted = fitImageToRequestedSize(solidImage(400, 200), QSize(0, 100));
    EXPECT_EQ(QSize(200, 100), fitted.size());
}

TEST(QmlAsyncImageProviderTest, NeverEnlarges) {
    const QImage source = solidImage(50, 50);
    EXPECT_EQ(source.size(), fitImageToRequestedSize(source, QSize(200, 200)).size());
    EXPECT_EQ(source.size(), fitImageToRequestedSize(source, QSize(200, 0)).size());
    EXPECT_EQ(source.size(), fitImageToRequestedSize(source, QSize(0, 200)).size());
}

TEST(QmlAsyncImageProviderTest, PassesThroughWithoutARequestedSize) {
    const QImage source = solidImage(300, 100);
    EXPECT_EQ(source, fitImageToRequestedSize(source, QSize()));
    EXPECT_EQ(source, fitImageToRequestedSize(source, QSize(0, 0)));
    EXPECT_EQ(source, fitImageToRequestedSize(source, QSize(-1, -1)));
    EXPECT_TRUE(fitImageToRequestedSize(QImage(), QSize(10, 10)).isNull());
}

TEST(QmlAsyncImageProviderTest, ResamplesSmoothly) {
    const QImage fitted = fitImageToRequestedSize(checkerboard(8), QSize(4, 4));
    ASSERT_EQ(QSize(4, 4), fitted.size());
    // Each output pixel folds two black and two white source pixels: smooth
    // gives mid-grey, the nearest-neighbour default gives only 0 or 255.
    for (int y = 0; y < fitted.height(); ++y) {
        for (int x = 0; x < fitted.width(); ++x) {
            const int value = fitted.pixelColor(x, y).value();
            EXPECT_GT(value, 0) << "pure black at " << x << "," << y;
            EXPECT_LT(value, 255) << "pure white at " << x << "," << y;
        }
    }
}
