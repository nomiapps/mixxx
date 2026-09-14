#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>

#include "engine/channels/wavetable.h"
#include "preferences/usersettings.h"

/// The tables themselves: generated built-ins and Serum-format .wav decoding.
/// Free functions with no threads or Qt objects behind them, so they can be
/// tested on their own.
namespace wavetable {

/// Every built-in table has this many frames: enough for the stack display to
/// read as a wavetable, and adjacent-frame crossfading makes more pointless.
constexpr int kBuiltinFrames = 16;

/// Sine, morphing through triangle and saw to a square.
std::unique_ptr<Wavetable> generateBasic();
/// A pulse whose width narrows from a square to a sliver.
std::unique_ptr<Wavetable> generatePulse();
/// Harmonics added one at a time at 1/n: a sine growing into a saw.
std::unique_ptr<Wavetable> generateHarmonics();

/// The built-in grid: kGridColumns by kGridRows.
constexpr int kGridColumns = 8;
constexpr int kGridRows = 4;
/// A grid table: along each row the Basic morph, sine through triangle and
/// saw to a square; down the rows the shape is folded back on itself harder,
/// so the top row is the plain shapes and the bottom one bright and hollow.
std::unique_ptr<Wavetable> generateFoldGrid();

/// Reads the grid a file name gives its table: "Name.8x4" is an 8-column,
/// 4-row grid called "Name". Returns false, and leaves the outputs alone,
/// for a name without one or with a grid of no frames or more than a table
/// can hold.
bool parseGridName(const QString& baseName, QString* pName, int* pColumns, int* pRows);

/// What every table gets before anyone plays or draws it: each frame's DC
/// removed (a pulse's mean would thump through the filter on every note),
/// the whole table peak-normalised to 1.0 (the PolyBLEP waves peak at 1, so
/// a table voice is as loud as a saw voice), its mips built, and the guard
/// samples filled.
void finalise(Wavetable* pTable);

/// Grows a one-mip table to levels mips: mip k is every frame with the
/// partials above kWavetableMipPartials(k) removed, through an FFT. Mip 0
/// is left as it is. A table that already has mips is left alone.
void buildMips(Wavetable* pTable, int levels = kWavetableMipLevels);

/// Decodes a Serum-layout wavetable: a .wav whose length is a whole number
/// of 2048-sample-frame cycles, channels averaged to mono. Returns nullptr
/// and a reason (unreadable, not a multiple of 2048, too many frames, short
/// read) when the file is not one. With columns and rows, the file has to
/// hold exactly that grid of cycles, row after row, and the table is laid
/// out as one. Runs on whatever thread calls it; never the engine thread.
std::unique_ptr<Wavetable> decodeSerumWav(
        const QString& path, QString* pReason, int columns = 0, int rows = 0);

} // namespace wavetable

/// The list of tables a synth can choose from and the loading of them: the
/// built-ins first, then every .wav in <settings>/wavetables/, by name. A
/// file named "Name.8x4.wav" is listed as "Name" and loads as an 8-column,
/// 4-row grid. The folder is created on the first scan so there is
/// somewhere to drop files.
/// Nothing is shipped in it, and a file is only decoded when it is chosen.
///
/// load() of a built-in completes before it returns; a file decodes on the
/// global thread pool and reports later. A newer load() supersedes an older
/// one, whose result is dropped when it arrives. Main thread only.
class WavetableLibrary : public QObject {
    Q_OBJECT
  public:
    struct Entry {
        QString name;
        QString path; // empty for a built-in
        // The grid the file name gives, 0 by 0 for a plain list.
        int columns = 0;
        int rows = 0;
    };
    static constexpr int kBuiltinCount = 4;

    explicit WavetableLibrary(UserSettingsPointer pConfig, QObject* parent = nullptr);
    ~WavetableLibrary() override = default;

    /// Rebuilds the list. Emits namesChanged when it differs from before.
    void scan();
    int count() const {
        return m_entries.size();
    }
    QStringList names() const;
    const Entry& entry(int index) const {
        return m_entries[index];
    }
    QString directory() const;

    /// Starts loading entry index; loaded or loadFailed follows, possibly
    /// before this returns. Out-of-range indices fail at once.
    void load(int index);

  signals:
    void namesChanged();
    void loaded(int index, std::shared_ptr<const Wavetable> pTable);
    void loadFailed(int index, const QString& reason);

  private:
    struct DecodeResult {
        std::shared_ptr<const Wavetable> pTable;
        QString reason;
    };

    UserSettingsPointer m_pConfig;
    QVector<Entry> m_entries;
    int m_sequence;
};
