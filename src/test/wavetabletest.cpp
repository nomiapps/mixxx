#include <gtest/gtest.h>

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <algorithm>
#include <cmath>
#include <vector>

#include "engine/channels/wavetable.h"
#include "mixer/wavetablelibrary.h"
#include "test/mixxxtest.h"
#include "test/soundsourceproviderregistration.h"

namespace {

constexpr double kTwoPi = 2.0 * 3.14159265358979323846;

class WavetableTest : public MixxxTest, SoundSourceProviderRegistration {
  protected:
    static double sine(int i) {
        return std::sin(kTwoPi * i / kWavetableFrameSize);
    }

    // A 16-bit PCM .wav of interleaved samples (-1..1) in the test data dir.
    QString writeWav(const QString& name, int channels, const std::vector<double>& interleaved) {
        const QString path = getTestDataDir().filePath(name);
        QFile file(path);
        EXPECT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QDataStream out(&file);
        out.setByteOrder(QDataStream::LittleEndian);
        const quint32 dataBytes = static_cast<quint32>(interleaved.size() * 2);
        constexpr quint32 kRate = 44100;
        out.writeRawData("RIFF", 4);
        out << static_cast<quint32>(36 + dataBytes);
        out.writeRawData("WAVE", 4);
        out.writeRawData("fmt ", 4);
        out << static_cast<quint32>(16) << static_cast<quint16>(1)
            << static_cast<quint16>(channels) << kRate
            << static_cast<quint32>(kRate * channels * 2)
            << static_cast<quint16>(channels * 2) << static_cast<quint16>(16);
        out.writeRawData("data", 4);
        out << dataBytes;
        for (const double sample : interleaved) {
            out << static_cast<qint16>(std::lround(std::clamp(sample, -1.0, 1.0) * 32767.0));
        }
        return path;
    }

    // Even frames a sine, odd frames its inverse, channels identical.
    QString writeTwoFrameWav(const QString& name, int channels, int frames = 2) {
        std::vector<double> samples;
        samples.reserve(frames * kWavetableFrameSize * channels);
        for (int f = 0; f < frames; ++f) {
            for (int i = 0; i < kWavetableFrameSize; ++i) {
                for (int c = 0; c < channels; ++c) {
                    samples.push_back(f % 2 == 0 ? sine(i) : -sine(i));
                }
            }
        }
        return writeWav(name, channels, samples);
    }

    // Magnitude of one bin of a frame's spectrum, by the definition, so the
    // test does not depend on the FFT the library uses.
    static double binMagnitude(const float* pFrame, int bin) {
        double re = 0.0;
        double im = 0.0;
        for (int i = 0; i < kWavetableFrameSize; ++i) {
            const double angle = kTwoPi * bin * i / kWavetableFrameSize;
            re += pFrame[i] * std::cos(angle);
            im -= pFrame[i] * std::sin(angle);
        }
        return std::sqrt(re * re + im * im);
    }

    static void expectFinalised(const Wavetable& table) {
        ASSERT_EQ(static_cast<std::size_t>(table.mipCount) * table.frameCount *
                        kWavetableFrameStride,
                table.samples.size());
        EXPECT_EQ(kWavetableMipLevels, table.mipCount);
        float peak = 0.0f;
        for (int f = 0; f < table.frameCount; ++f) {
            const float* pFrame = table.frame(f);
            EXPECT_FLOAT_EQ(pFrame[0], pFrame[kWavetableFrameSize]) << "guard of frame " << f;
            double sum = 0.0;
            for (int i = 0; i < kWavetableFrameSize; ++i) {
                sum += pFrame[i];
                peak = std::max(peak, std::fabs(pFrame[i]));
            }
            EXPECT_LT(std::fabs(sum / kWavetableFrameSize), 1e-3) << "DC in frame " << f;
        }
        EXPECT_GE(peak, 0.99f);
        EXPECT_LE(peak, 1.0f);
    }
};

TEST_F(WavetableTest, BuiltinsAreFinalisedSixteenFrameTables) {
    for (const auto& pTable : {wavetable::generateBasic(),
                 wavetable::generatePulse(),
                 wavetable::generateHarmonics()}) {
        ASSERT_TRUE(pTable);
        EXPECT_EQ(wavetable::kBuiltinFrames, pTable->frameCount);
        expectFinalised(*pTable);
    }
}

TEST_F(WavetableTest, BasicRunsFromSineToSquare) {
    const auto pTable = wavetable::generateBasic();
    const float* pFirst = pTable->frame(0);
    const float* pLast = pTable->frame(wavetable::kBuiltinFrames - 1);
    for (int i = 0; i < kWavetableFrameSize; ++i) {
        // Removing the DC of the morph frames costs the end frames a hair of
        // level through the shared normalisation; the shapes are exact.
        EXPECT_NEAR(sine(i), pFirst[i], 2e-3) << "sample " << i;
        EXPECT_NEAR(i < kWavetableFrameSize / 2 ? 1.0 : -1.0, pLast[i], 2e-3) << "sample " << i;
    }
}

TEST_F(WavetableTest, PulseStartsSquareAndNarrows) {
    const auto pTable = wavetable::generatePulse();
    int highFirst = 0;
    int highLast = 0;
    for (int i = 0; i < kWavetableFrameSize; ++i) {
        highFirst += pTable->frame(0)[i] > 0.0f;
        highLast += pTable->frame(wavetable::kBuiltinFrames - 1)[i] > 0.0f;
    }
    EXPECT_EQ(kWavetableFrameSize / 2, highFirst);
    EXPECT_LT(highLast, kWavetableFrameSize / 10);
    EXPECT_GT(highLast, 0);
}

TEST_F(WavetableTest, HarmonicsStartsAsASine) {
    const auto pTable = wavetable::generateHarmonics();
    // Frame 0 is the fundamental alone; the table is normalised on a later,
    // louder frame, so compare shapes rather than levels.
    const float* pFirst = pTable->frame(0);
    const float scale = pFirst[kWavetableFrameSize / 4]; // the sine's peak
    EXPECT_GT(scale, 0.1f);
    for (int i = 0; i < kWavetableFrameSize; ++i) {
        EXPECT_NEAR(sine(i) * scale, pFirst[i], 1e-4) << "sample " << i;
    }
}

TEST_F(WavetableTest, FoldGridIsAnEightByFourGrid) {
    const auto pTable = wavetable::generateFoldGrid();
    ASSERT_TRUE(pTable);
    EXPECT_EQ(wavetable::kGridColumns * wavetable::kGridRows, pTable->frameCount);
    EXPECT_EQ(wavetable::kGridColumns, pTable->gridColumns());
    EXPECT_EQ(wavetable::kGridRows, pTable->gridRows());
    expectFinalised(*pTable);
    // The top row starts on the plain sine; folding changes the rows below it.
    // A folded frame is the table's loudest, so compare shape, not level.
    const float* pTopLeft = pTable->frame(0);
    const float scale = pTopLeft[kWavetableFrameSize / 4];
    EXPECT_GT(scale, 0.5f);
    const float* pBottomLeft = pTable->frame((wavetable::kGridRows - 1) * wavetable::kGridColumns);
    double difference = 0.0;
    for (int i = 0; i < kWavetableFrameSize; ++i) {
        EXPECT_NEAR(sine(i) * scale, pTopLeft[i], 2e-3) << "sample " << i;
        difference += std::fabs(pTopLeft[i] - pBottomLeft[i]);
    }
    EXPECT_GT(difference / kWavetableFrameSize, 0.2);
}

TEST_F(WavetableTest, ACellIsTheFourFramesAroundAPoint) {
    auto pGrid = Wavetable::create(12);
    pGrid->columns = 4; // three rows
    WavetableCell cell = WavetableCell::locate(*pGrid, 0.5, 0.25);
    EXPECT_EQ(1, cell.frameA0);
    EXPECT_EQ(2, cell.frameB0);
    EXPECT_EQ(5, cell.frameA1);
    EXPECT_EQ(6, cell.frameB1);
    EXPECT_DOUBLE_EQ(0.5, cell.blendX);
    EXPECT_DOUBLE_EQ(0.5, cell.blendY);

    // The far corner, and out of range clamps to it.
    cell = WavetableCell::locate(*pGrid, 3.0, 1.5);
    EXPECT_EQ(11, cell.frameA0);
    EXPECT_EQ(11, cell.frameB1);
    EXPECT_DOUBLE_EQ(0.0, cell.blendX);
    EXPECT_DOUBLE_EQ(0.0, cell.blendY);

    // A plain list has one row, so y does nothing.
    auto pList = Wavetable::create(12);
    cell = WavetableCell::locate(*pList, 0.5, 0.9);
    EXPECT_EQ(5, cell.frameA0);
    EXPECT_EQ(6, cell.frameB0);
    EXPECT_EQ(cell.frameA0, cell.frameA1);
    EXPECT_DOUBLE_EQ(0.5, cell.blendX);
    EXPECT_DOUBLE_EQ(0.0, cell.blendY);
}

TEST_F(WavetableTest, ColumnsThatMakeNoGridAreAPlainList) {
    auto pTable = Wavetable::create(12);
    for (const int columns : {0, 5, 12, 13}) {
        pTable->columns = columns;
        EXPECT_EQ(12, pTable->gridColumns()) << columns;
        EXPECT_EQ(1, pTable->gridRows()) << columns;
    }
    pTable->columns = 6;
    EXPECT_EQ(2, pTable->gridRows());
}

TEST_F(WavetableTest, AFileNameCanGiveAGrid) {
    QString name;
    int columns = 0;
    int rows = 0;
    ASSERT_TRUE(wavetable::parseGridName(QStringLiteral("Fold.8x4"), &name, &columns, &rows));
    EXPECT_EQ(QStringLiteral("Fold"), name);
    EXPECT_EQ(8, columns);
    EXPECT_EQ(4, rows);
    ASSERT_TRUE(wavetable::parseGridName(QStringLiteral("a.b.2X3"), &name, &columns, &rows));
    EXPECT_EQ(QStringLiteral("a.b"), name);
    EXPECT_EQ(2, columns);
    EXPECT_EQ(3, rows);
    for (const char* plain : {"Plain", "x.0x4", "big.20x20", ".8x4", "Fold.8x"}) {
        name = QStringLiteral("unchanged");
        EXPECT_FALSE(wavetable::parseGridName(QLatin1String(plain), &name, &columns, &rows))
                << plain;
        EXPECT_EQ(QStringLiteral("unchanged"), name) << plain;
    }
}

TEST_F(WavetableTest, AGridFileMustHoldItsGrid) {
    const QString path = writeTwoFrameWav("grid.wav", 1, 4);
    QString reason;
    const auto pGrid = wavetable::decodeSerumWav(path, &reason, 2, 2);
    ASSERT_TRUE(pGrid) << reason.toStdString();
    EXPECT_EQ(2, pGrid->gridColumns());
    EXPECT_EQ(2, pGrid->gridRows());
    expectFinalised(*pGrid);

    EXPECT_FALSE(wavetable::decodeSerumWav(path, &reason, 3, 1));
    EXPECT_TRUE(reason.contains(QStringLiteral("3x1"))) << reason.toStdString();
}

TEST_F(WavetableTest, MipsRemovePartialsAboveTheirLimit) {
    // The pulse is the brightest built-in: every partial present.
    const auto pTable = wavetable::generatePulse();
    ASSERT_EQ(kWavetableMipLevels, pTable->mipCount);
    const int frame = wavetable::kBuiltinFrames - 1;
    const float* pFull = pTable->frame(frame, 0);
    const double fundamental = binMagnitude(pFull, 1);
    ASSERT_GT(fundamental, 1.0);
    for (int mip = 1; mip < pTable->mipCount; ++mip) {
        const float* pMip = pTable->frame(frame, mip);
        const int limit = kWavetableMipPartials(mip);
        // Kept partials match the full frame; removed ones are gone.
        EXPECT_NEAR(fundamental, binMagnitude(pMip, 1), fundamental * 1e-3) << "mip " << mip;
        EXPECT_NEAR(binMagnitude(pFull, limit), binMagnitude(pMip, limit), fundamental * 1e-3)
                << "mip " << mip;
        EXPECT_LT(binMagnitude(pMip, limit + 1), fundamental * 1e-3) << "mip " << mip;
        EXPECT_LT(binMagnitude(pMip, std::min(1023, limit * 2)), fundamental * 1e-3)
                << "mip " << mip;
        EXPECT_FLOAT_EQ(pMip[0], pMip[kWavetableFrameSize]) << "guard of mip " << mip;
    }
}

TEST_F(WavetableTest, DecodesATwoFrameSerumWav) {
    QString reason;
    const auto pTable = wavetable::decodeSerumWav(writeTwoFrameWav("two.wav", 1), &reason);
    ASSERT_TRUE(pTable) << reason.toStdString();
    EXPECT_EQ(2, pTable->frameCount);
    expectFinalised(*pTable);
    for (int i = 0; i < kWavetableFrameSize; ++i) {
        EXPECT_NEAR(sine(i), pTable->frame(0)[i], 2e-3) << "sample " << i;
        EXPECT_NEAR(-pTable->frame(0)[i], pTable->frame(1)[i], 2e-3) << "sample " << i;
    }
}

TEST_F(WavetableTest, StereoFramesAreAveragedToMono) {
    QString reason;
    const auto pTable = wavetable::decodeSerumWav(writeTwoFrameWav("stereo.wav", 2), &reason);
    ASSERT_TRUE(pTable) << reason.toStdString();
    EXPECT_EQ(2, pTable->frameCount);
    for (int i = 0; i < kWavetableFrameSize; ++i) {
        EXPECT_NEAR(sine(i), pTable->frame(0)[i], 2e-3) << "sample " << i;
    }
}

TEST_F(WavetableTest, RejectsALengthThatIsNotWholeFrames) {
    QString reason;
    // 1,323,000 frames: 645 cycles and 1240 samples over.
    EXPECT_FALSE(wavetable::decodeSerumWav(getTestDir().filePath("sine-30.wav"), &reason));
    EXPECT_TRUE(reason.contains(QString::number(kWavetableFrameSize))) << reason.toStdString();

    std::vector<double> odd(3000, 0.1);
    reason.clear();
    EXPECT_FALSE(wavetable::decodeSerumWav(writeWav("odd.wav", 1, odd), &reason));
    EXPECT_FALSE(reason.isEmpty());
}

TEST_F(WavetableTest, RejectsAFileThatIsNotAudio) {
    const QString path = getTestDataDir().filePath("text.wav");
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("this is not a wav file\n");
    file.close();
    QString reason;
    EXPECT_FALSE(wavetable::decodeSerumWav(path, &reason));
    EXPECT_FALSE(reason.isEmpty());
}

TEST_F(WavetableTest, ScanListsBuiltinsThenFilesByName) {
    WavetableLibrary library(config());
    library.scan();
    EXPECT_TRUE(QDir(library.directory()).exists());
    ASSERT_EQ(WavetableLibrary::kBuiltinCount, library.count());
    EXPECT_EQ(QStringLiteral("Basic"), library.entry(0).name);
    EXPECT_TRUE(library.entry(0).path.isEmpty());

    std::vector<double> zeros(kWavetableFrameSize, 0.0);
    QFile::copy(writeWav("zed.wav", 1, zeros), QDir(library.directory()).filePath("Zed.wav"));
    QFile::copy(writeWav("alpha.wav", 1, zeros), QDir(library.directory()).filePath("alpha.wav"));
    QFile::copy(writeWav("grid.wav", 1, zeros), QDir(library.directory()).filePath("grid.2x2.wav"));
    library.scan();
    const int first = WavetableLibrary::kBuiltinCount;
    ASSERT_EQ(first + 3, library.count());
    EXPECT_EQ(QStringLiteral("alpha"), library.entry(first).name);
    EXPECT_EQ(0, library.entry(first).columns);
    // Listed by its name without the grid, which the entry carries.
    EXPECT_EQ(QStringLiteral("grid"), library.entry(first + 1).name);
    EXPECT_EQ(2, library.entry(first + 1).columns);
    EXPECT_EQ(2, library.entry(first + 1).rows);
    EXPECT_EQ(QStringLiteral("Zed"), library.entry(first + 2).name);
    EXPECT_FALSE(library.entry(first + 2).path.isEmpty());
}

TEST_F(WavetableTest, LoadingABuiltinCompletesAtOnce) {
    WavetableLibrary library(config());
    library.scan();
    int loadedIndex = -1;
    std::shared_ptr<const Wavetable> pLoaded;
    QObject::connect(&library,
            &WavetableLibrary::loaded,
            [&](int index, std::shared_ptr<const Wavetable> pTable) {
                loadedIndex = index;
                pLoaded = pTable;
            });
    library.load(1);
    EXPECT_EQ(1, loadedIndex);
    ASSERT_TRUE(pLoaded);
    EXPECT_EQ(wavetable::kBuiltinFrames, pLoaded->frameCount);
}

} // namespace
