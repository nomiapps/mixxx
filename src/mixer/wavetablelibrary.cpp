#include "mixer/wavetablelibrary.h"

#include <dsp/transforms/FFT.h>

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QRegularExpression>
#include <QtConcurrentRun>
#include <QtDebug>
#include <algorithm>
#include <cmath>
#include <vector>

#include "moc_wavetablelibrary.cpp"
#include "sources/soundsourceproxy.h"
#include "track/track.h"
#include "util/assert.h"
#include "util/samplebuffer.h"
#include "util/types.h"

namespace {

constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
const QString kDirectoryName = QStringLiteral("wavetables");

// Each generator fills frame f, sample i, from a shape function of the
// normalised phase x = i / 2048.
template<typename Shape>
std::unique_ptr<Wavetable> generate(
        Shape shape, int frames = wavetable::kBuiltinFrames, int columns = 0) {
    auto pTable = Wavetable::create(frames);
    pTable->columns = columns;
    for (int f = 0; f < frames; ++f) {
        float* pFrame = pTable->frame(f);
        for (int i = 0; i < kWavetableFrameSize; ++i) {
            const double x = static_cast<double>(i) / kWavetableFrameSize;
            pFrame[i] = static_cast<float>(shape(f, x));
        }
    }
    wavetable::finalise(pTable.get());
    return pTable;
}

double sineShape(double x) {
    return std::sin(kTwoPi * x);
}
double triangleShape(double x) {
    return 4.0 * std::fabs(x - 0.5) - 1.0;
}
double sawShape(double x) {
    return 2.0 * x - 1.0;
}
double squareShape(double x) {
    return x < 0.5 ? 1.0 : -1.0;
}

// Sine to triangle to saw to square as t runs 0..1, in three equal morphs.
double basicMorph(double t, double x) {
    const double scaled = 3.0 * std::clamp(t, 0.0, 1.0);
    const int segment = std::min(2, static_cast<int>(scaled));
    const double u = scaled - segment;
    double from = 0.0;
    double to = 0.0;
    switch (segment) {
    case 0:
        from = sineShape(x);
        to = triangleShape(x);
        break;
    case 1:
        from = triangleShape(x);
        to = sawShape(x);
        break;
    default:
        from = sawShape(x);
        to = squareShape(x);
        break;
    }
    return from + (to - from) * u;
}

} // namespace

namespace wavetable {

std::unique_ptr<Wavetable> generateBasic() {
    // Three morphs over the frames: sine to triangle, triangle to saw, saw to
    // square. The last frame is exactly the square.
    return generate([](int f, double x) {
        return basicMorph(static_cast<double>(f) / (kBuiltinFrames - 1), x);
    });
}

std::unique_ptr<Wavetable> generatePulse() {
    return generate([](int f, double x) {
        const double width = 0.5 - 0.47 * f / (kBuiltinFrames - 1);
        return x < width ? 1.0 : -1.0;
    });
}

std::unique_ptr<Wavetable> generateHarmonics() {
    return generate([](int f, double x) {
        double value = 0.0;
        for (int n = 1; n <= f + 1; ++n) {
            value += std::sin(kTwoPi * n * x) / n;
        }
        return value;
    });
}

std::unique_ptr<Wavetable> generateFoldGrid() {
    return generate(
            [](int f, double x) {
                const int column = f % kGridColumns;
                const int row = f / kGridColumns;
                const double shape = basicMorph(static_cast<double>(column) / (kGridColumns - 1), x);
                // A sine wavefolder whose drive grows down the rows, crossfaded
                // in so the top row is the shape itself.
                const double fold = static_cast<double>(row) / (kGridRows - 1);
                const double folded = std::sin(kTwoPi / 4.0 * (1.0 + 3.0 * fold) * shape);
                return shape + (folded - shape) * fold;
            },
            kGridColumns * kGridRows,
            kGridColumns);
}

bool parseGridName(const QString& baseName, QString* pName, int* pColumns, int* pRows) {
    static const QRegularExpression kGrid(QStringLiteral("^(.*)\\.(\\d{1,3})x(\\d{1,3})$"),
            QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = kGrid.match(baseName);
    if (!match.hasMatch()) {
        return false;
    }
    const int columns = match.captured(2).toInt();
    const int rows = match.captured(3).toInt();
    if (columns < 1 || rows < 1 || columns * rows > kWavetableMaxFrames ||
            match.captured(1).isEmpty()) {
        return false;
    }
    *pName = match.captured(1);
    *pColumns = columns;
    *pRows = rows;
    return true;
}

void finalise(Wavetable* pTable) {
    float peak = 0.0f;
    for (int f = 0; f < pTable->frameCount; ++f) {
        float* pFrame = pTable->frame(f);
        double sum = 0.0;
        for (int i = 0; i < kWavetableFrameSize; ++i) {
            sum += pFrame[i];
        }
        const float mean = static_cast<float>(sum / kWavetableFrameSize);
        for (int i = 0; i < kWavetableFrameSize; ++i) {
            pFrame[i] -= mean;
            peak = std::max(peak, std::fabs(pFrame[i]));
        }
    }
    if (peak > 0.0f) {
        const float scale = 1.0f / peak;
        for (int f = 0; f < pTable->frameCount; ++f) {
            float* pFrame = pTable->frame(f);
            for (int i = 0; i < kWavetableFrameSize; ++i) {
                pFrame[i] *= scale;
            }
        }
    }
    buildMips(pTable);
    pTable->fillGuards();
}

void buildMips(Wavetable* pTable, int levels) {
    if (pTable->mipCount != 1 || levels <= 1 || pTable->frameCount <= 0) {
        return;
    }
    // Mip 0 keeps its place at the front, so growing the vector in place
    // leaves it where frame(i) already finds it.
    pTable->samples.resize(static_cast<std::size_t>(levels) * pTable->frameCount *
            kWavetableFrameStride);
    pTable->mipCount = levels;

    FFTReal fft(kWavetableFrameSize);
    std::vector<double> in(kWavetableFrameSize);
    std::vector<double> re(kWavetableFrameSize);
    std::vector<double> im(kWavetableFrameSize);
    std::vector<double> reCut(kWavetableFrameSize);
    std::vector<double> imCut(kWavetableFrameSize);
    std::vector<double> out(kWavetableFrameSize);
    for (int f = 0; f < pTable->frameCount; ++f) {
        const float* pFull = pTable->frame(f, 0);
        for (int i = 0; i < kWavetableFrameSize; ++i) {
            in[i] = pFull[i];
        }
        fft.forward(in.data(), re.data(), im.data());
        for (int mip = 1; mip < levels; ++mip) {
            // inverse() reads bins 0 .. N/2; everything from the limit up
            // is silenced.
            const int limit = kWavetableMipPartials(mip);
            for (int b = 0; b <= kWavetableFrameSize / 2; ++b) {
                const bool keep = b <= limit;
                reCut[b] = keep ? re[b] : 0.0;
                imCut[b] = keep ? im[b] : 0.0;
            }
            fft.inverse(reCut.data(), imCut.data(), out.data());
            float* pMip = pTable->frame(f, mip);
            for (int i = 0; i < kWavetableFrameSize; ++i) {
                pMip[i] = static_cast<float>(out[i]);
            }
        }
    }
}

std::unique_ptr<Wavetable> decodeSerumWav(
        const QString& path, QString* pReason, int columns, int rows) {
    const auto fail = [pReason](const QString& reason) {
        if (pReason != nullptr) {
            *pReason = reason;
        }
        return std::unique_ptr<Wavetable>();
    };

    // A temporary track is the way into the SoundSource providers; it is not
    // registered anywhere and goes away with this function.
    TrackPointer pTrack = Track::newTemporary(path);
    mixxx::AudioSourcePointer pSource = SoundSourceProxy(pTrack).openAudioSource();
    if (!pSource) {
        return fail(QStringLiteral("cannot open the file as audio"));
    }
    const mixxx::IndexRange range = pSource->frameIndexRange();
    const SINT frames = range.length();
    const int channels = pSource->getSignalInfo().getChannelCount().value();
    if (frames <= 0 || channels <= 0) {
        pSource->close();
        return fail(QStringLiteral("the file holds no audio"));
    }
    if (frames % kWavetableFrameSize != 0) {
        pSource->close();
        return fail(QStringLiteral("%1 sample frames is not a whole number of %2-sample cycles")
                        .arg(frames)
                        .arg(kWavetableFrameSize));
    }
    const SINT frameCount = frames / kWavetableFrameSize;
    if (frameCount > kWavetableMaxFrames) {
        pSource->close();
        return fail(QStringLiteral("%1 cycles is more than the %2 a table can hold")
                        .arg(frameCount)
                        .arg(kWavetableMaxFrames));
    }
    if (columns > 0 && frameCount != static_cast<SINT>(columns) * rows) {
        pSource->close();
        return fail(QStringLiteral("%1 cycles is not the %2x%3 grid the name gives")
                        .arg(frameCount)
                        .arg(columns)
                        .arg(rows));
    }

    mixxx::SampleBuffer buffer(frames * channels);
    const auto readable = pSource->readSampleFrames(mixxx::WritableSampleFrames(
            range, mixxx::SampleBuffer::WritableSlice(buffer)));
    pSource->close();
    if (readable.frameIndexRange() != range) {
        return fail(QStringLiteral("the file could not be read to the end"));
    }

    auto pTable = Wavetable::create(static_cast<int>(frameCount));
    pTable->columns = columns;
    const CSAMPLE* pIn = buffer.data();
    const float channelScale = 1.0f / channels;
    for (int f = 0; f < pTable->frameCount; ++f) {
        float* pFrame = pTable->frame(f);
        for (int i = 0; i < kWavetableFrameSize; ++i) {
            float sum = 0.0f;
            for (int c = 0; c < channels; ++c) {
                sum += *pIn++;
            }
            pFrame[i] = sum * channelScale;
        }
    }
    finalise(pTable.get());
    return pTable;
}

} // namespace wavetable

WavetableLibrary::WavetableLibrary(UserSettingsPointer pConfig, QObject* parent)
        : QObject(parent),
          m_pConfig(std::move(pConfig)),
          m_sequence(0) {
}

QString WavetableLibrary::directory() const {
    return QDir(m_pConfig->getSettingsPath()).filePath(kDirectoryName);
}

QStringList WavetableLibrary::names() const {
    QStringList names;
    names.reserve(m_entries.size());
    for (const Entry& entry : m_entries) {
        names.append(entry.name);
    }
    return names;
}

void WavetableLibrary::scan() {
    QVector<Entry> entries;
    entries.append({QStringLiteral("Basic"), QString()});
    entries.append({QStringLiteral("Pulse"), QString()});
    entries.append({QStringLiteral("Harmonics"), QString()});
    entries.append({QStringLiteral("Fold Grid"), QString(), wavetable::kGridColumns, wavetable::kGridRows});
    DEBUG_ASSERT(entries.size() == kBuiltinCount);

    QDir dir(directory());
    if (!dir.exists()) {
        if (!dir.mkpath(QStringLiteral("."))) {
            qWarning() << "Cannot create the wavetable folder" << dir.path();
        }
    }
    dir.setFilter(QDir::Files | QDir::Readable);
    dir.setNameFilters({QStringLiteral("*.wav")});
    dir.setSorting(QDir::Name | QDir::IgnoreCase);
    const QFileInfoList files = dir.entryInfoList();
    for (const QFileInfo& file : files) {
        Entry entry{file.completeBaseName(), file.absoluteFilePath()};
        wavetable::parseGridName(file.completeBaseName(), &entry.name, &entry.columns, &entry.rows);
        entries.append(entry);
    }
    qDebug() << "Wavetables:" << (entries.size() - kBuiltinCount) << "file(s) in" << dir.path();

    const QStringList before = names();
    m_entries = entries;
    if (names() != before) {
        emit namesChanged();
    }
}

void WavetableLibrary::load(int index) {
    const int sequence = ++m_sequence;
    if (index < 0 || index >= m_entries.size()) {
        emit loadFailed(index, QStringLiteral("no such wavetable"));
        return;
    }
    const Entry entry = m_entries[index];
    if (entry.path.isEmpty()) {
        std::unique_ptr<Wavetable> pTable;
        switch (index) {
        case 0:
            pTable = wavetable::generateBasic();
            break;
        case 1:
            pTable = wavetable::generatePulse();
            break;
        case 2:
            pTable = wavetable::generateHarmonics();
            break;
        default:
            pTable = wavetable::generateFoldGrid();
            break;
        }
        emit loaded(index, std::shared_ptr<const Wavetable>(std::move(pTable)));
        return;
    }

    // The decode happens on the pool; the result comes back here, on the
    // main thread, through the watcher. Nothing of this object is touched by
    // the worker.
    auto* pWatcher = new QFutureWatcher<DecodeResult>(this);
    connect(pWatcher,
            &QFutureWatcherBase::finished,
            this,
            [this, pWatcher, index, sequence] {
                const DecodeResult result = pWatcher->result();
                pWatcher->deleteLater();
                if (sequence != m_sequence) {
                    return; // a later load() superseded this one
                }
                if (result.pTable) {
                    emit loaded(index, result.pTable);
                } else {
                    emit loadFailed(index, result.reason);
                }
            });
    const QString path = entry.path;
    const int columns = entry.columns;
    const int rows = entry.rows;
    pWatcher->setFuture(QtConcurrent::run([path, columns, rows] {
        DecodeResult result;
        result.pTable = wavetable::decodeSerumWav(path, &result.reason, columns, rows);
        return result;
    }));
}
