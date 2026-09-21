#pragma once

#include <QList>
#include <QString>
#include <vector>

#include "preferences/configobject.h"

/// The MMCC path contract of the Mixxx adapter as one declarative table.
///
/// The contract is `docs/mixxx-adapter-paths.md` in the MMCC repository. Every
/// row of its "Paths" table is one row of `kPathRows` in mmccpathtable.cpp, in
/// the same order, so the two can be compared line by line. Nothing about a
/// path lives anywhere else: the adapter and the sessions only interpret rows.
namespace mmcc {

constexpr int kNumDecks = 4;
constexpr int kStemsPerDeck = 4;
constexpr int kNumEffectUnits = 4;
constexpr int kEffectSlotsPerUnit = 4;
constexpr int kNumSamplers = 8;

constexpr int kExpectedReadablePaths = 139;
constexpr int kExpectedWritablePaths = 94;

constexpr int kMaxTagChars = 256;

/// The Mixxx object a path belongs to. It decides when the path is `null`
/// and when a command on it is `stale_object`.
enum class Owner {
    Adapter,    // the adapter itself, never unavailable
    Global,     // [Master], never unavailable
    Deck,       // [ChannelN]; unavailable when n >= [App],num_decks
    Stem,       // [ChannelN_StemK]; unavailable when k >= stem_count
    EffectUnit, // [EffectRack1_EffectUnitU]; never unavailable
    Sampler,    // [SamplerS]; unavailable when s >= [App],num_samplers
};

/// Where the value comes from.
enum class Source {
    Constant,         // the adapter's own connection status
    ControlValue,     // ControlObject value
    ControlParameter, // ControlObject parameter (audio-taper gains)
    TrackTitle,       // Track::getTitle of the deck's loaded track
    TrackArtist,      // Track::getArtist of the deck's loaded track
    AnyControlValue,  // true when any of several controls is > 0
};

enum class ValueType {
    Status,   // the string "connected"
    Bool,     // true exactly when the control's value is > 0
    Unit,     // number 0.0..1.0
    Bipolar,  // number -1.0..1.0 for commands; readings are reported as they are
    Positive, // number > 0.0, `null` when the control reports 0 or less
    Number,   // number reported as it is
    Count,    // integer 0..4
    Text,     // string cut at kMaxTagChars
};

/// What an available deck (or sampler) reports for this path while it holds
/// no track.
enum class WhenEmpty {
    Keep, // the control's value: the path describes the deck, not the track
    Zero, // false / 0: play, cue_indicator, stem_count
    Null, // null: the path describes the track
};

struct PathRow {
    /// Path pattern. `%1` is the 0-based index of the owner, `%2` the 0-based
    /// stem index.
    const char* path;
    Owner owner;
    /// Group pattern. `%1` is the 1-based index of the owner, `%2` the 1-based
    /// stem (or effect slot) index. Empty for sources that are not controls.
    const char* group;
    const char* key;
    Source source;
    ValueType type;
    /// Decimal places of the reported and the effective value; halves are
    /// rounded away from zero.
    int decimals;
    bool readable;
    /// Every writable path takes `set` and nothing else.
    bool writable;
    /// Rate limited and coalesced: meters and positions.
    bool continuous;
    WhenEmpty whenEmpty;
    /// A `set` on a deck or sampler without a track is `conflict`.
    bool conflictWhenEmpty;
    /// The control only files a request; the engine thread applies it in its
    /// next callback (`sync_enabled`). The read-back of such a path waits,
    /// without blocking, until the engine has answered or a deadline passes.
    bool appliedByEngine;
};

/// One concrete path: a row with its indices filled in.
struct PathEntry {
    QString path;
    const PathRow* pRow;
    /// 0-based index of the deck, effect unit or sampler; -1 for globals.
    int index;
    /// 0-based stem index; -1 for everything else.
    int subIndex;
    /// The controls behind the path. One for every control source, several
    /// for `AnyControlValue`, none otherwise.
    QList<ConfigKey> keys;
};

/// The rows, in the order of the contract's table.
const std::vector<PathRow>& pathRows();

/// Every concrete path. The readable entries are in the order of the
/// contract's `readable_paths`: globals, decks, meters and positions, stems,
/// effect units, samplers; deck-major within a group.
QList<PathEntry> buildPathTable();

/// Rounds to `decimals` places, halves away from zero. Never returns -0.0.
double quantise(double value, int decimals);

} // namespace mmcc
