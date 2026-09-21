#include "mmcc/mmccpathtable.h"

#include <QByteArray>
#include <cmath>

namespace mmcc {

namespace {

constexpr bool R = true; // readable
constexpr bool W = true; // writable
constexpr bool C = true; // continuous
constexpr bool X = true; // conflict when empty
constexpr bool A = true; // applied by the engine thread
constexpr bool no = false;

using O = Owner;
using S = Source;
using T = ValueType;
using E = WhenEmpty;

// One row per row of the "Paths" table in docs/mixxx-adapter-paths.md, in the
// same order. Columns: path, owner, group, key, source, type, decimals,
// readable, writable, continuous, value on an empty deck, conflict on an
// empty deck, applied asynchronously by the engine thread.
// clang-format off
const std::vector<PathRow> kPathRows = {
    {"surface.connection.status",      O::Adapter,    "",                                  "",               S::Constant,         T::Status,   0, R,  no, no, E::Keep, no, no},
    {"mixxx.crossfader",               O::Global,     "[Master]",                          "crossfader",     S::ControlValue,     T::Bipolar,  4, R,  W,  no, E::Keep, no, no},
    {"mixxx.main.gain",                O::Global,     "[Master]",                          "gain",           S::ControlParameter, T::Unit,     3, R,  W,  no, E::Keep, no, no},
    {"channel[%1].name",               O::Deck,       "",                                  "",               S::TrackTitle,       T::Text,     0, R,  no, no, E::Null, no, no},
    {"channel[%1].volume",             O::Deck,       "[Channel%1]",                       "volume",         S::ControlParameter, T::Unit,     3, R,  W,  no, E::Keep, no, no},
    {"channel[%1].mute",               O::Deck,       "[Channel%1]",                       "mute",           S::ControlValue,     T::Bool,     0, R,  W,  no, E::Keep, no, no},
    {"mixxx.deck[%1].loaded",          O::Deck,       "[Channel%1]",                       "track_loaded",   S::ControlValue,     T::Bool,     0, R,  no, no, E::Zero, no, no},
    {"mixxx.deck[%1].play",            O::Deck,       "[Channel%1]",                       "play",           S::ControlValue,     T::Bool,     0, R,  W,  no, E::Zero, X, no},
    {"mixxx.deck[%1].cue",             O::Deck,       "[Channel%1]",                       "cue_default",    S::ControlValue,     T::Bool,     0, no, W,  no, E::Keep, X, no},
    {"mixxx.deck[%1].cue_indicator",   O::Deck,       "[Channel%1]",                       "cue_indicator",  S::ControlValue,     T::Bool,     0, R,  no, no, E::Zero, no, no},
    {"mixxx.deck[%1].sync",            O::Deck,       "[Channel%1]",                       "sync_enabled",   S::ControlValue,     T::Bool,     0, R,  W,  no, E::Keep, no, A},
    {"mixxx.deck[%1].pfl",             O::Deck,       "[Channel%1]",                       "pfl",            S::ControlValue,     T::Bool,     0, R,  W,  no, E::Keep, no, no},
    {"mixxx.deck[%1].bpm",             O::Deck,       "[Channel%1]",                       "bpm",            S::ControlValue,     T::Positive, 2, R,  no, no, E::Null, no, no},
    {"mixxx.deck[%1].rate",            O::Deck,       "[Channel%1]",                       "rate",           S::ControlValue,     T::Bipolar,  4, R,  W,  no, E::Keep, no, no},
    {"mixxx.deck[%1].artist",          O::Deck,       "",                                  "",               S::TrackArtist,      T::Text,     0, R,  no, no, E::Null, no, no},
    {"mixxx.deck[%1].stem_count",      O::Deck,       "[Channel%1]",                       "stem_count",     S::ControlValue,     T::Count,    0, R,  no, no, E::Zero, no, no},
    {"mixxx.deck[%1].quick_effect",    O::Deck,       "[QuickEffectRack1_[Channel%1]]",    "super1",         S::ControlValue,     T::Unit,     3, R,  W,  no, E::Keep, no, no},
    {"channel[%1].meter.left",         O::Deck,       "[Channel%1]",                       "vu_meter_left",  S::ControlValue,     T::Unit,     3, R,  no, C,  E::Keep, no, no},
    {"channel[%1].meter.right",        O::Deck,       "[Channel%1]",                       "vu_meter_right", S::ControlValue,     T::Unit,     3, R,  no, C,  E::Keep, no, no},
    {"mixxx.deck[%1].position",        O::Deck,       "[Channel%1]",                       "playposition",   S::ControlValue,     T::Number,   4, R,  no, C,  E::Null, no, no},
    {"mixxx.deck[%1].stem[%2].volume", O::Stem,       "[Channel%1_Stem%2]",                "volume",         S::ControlValue,     T::Unit,     3, R,  W,  no, E::Null, no, no},
    {"mixxx.deck[%1].stem[%2].mute",   O::Stem,       "[Channel%1_Stem%2]",                "mute",           S::ControlValue,     T::Bool,     0, R,  W,  no, E::Null, no, no},
    {"mixxx.effect_unit[%1].mix",      O::EffectUnit, "[EffectRack1_EffectUnit%1]",        "mix",            S::ControlValue,     T::Unit,     3, R,  W,  no, E::Keep, no, no},
    {"mixxx.effect_unit[%1].enabled",  O::EffectUnit, "[EffectRack1_EffectUnit%1]",        "enabled",        S::ControlValue,     T::Bool,     0, R,  W,  no, E::Keep, no, no},
    {"mixxx.effect_unit[%1].super",    O::EffectUnit, "[EffectRack1_EffectUnit%1]",        "super1",         S::ControlValue,     T::Unit,     3, R,  W,  no, E::Keep, no, no},
    {"mixxx.effect_unit[%1].loaded",   O::EffectUnit, "[EffectRack1_EffectUnit%1_Effect%2]", "loaded",       S::AnyControlValue,  T::Bool,     0, R,  no, no, E::Keep, no, no},
    {"mixxx.sampler[%1].loaded",       O::Sampler,    "[Sampler%1]",                       "track_loaded",   S::ControlValue,     T::Bool,     0, R,  no, no, E::Keep, no, no},
    {"mixxx.sampler[%1].play",         O::Sampler,    "[Sampler%1]",                       "play",           S::ControlValue,     T::Bool,     0, R,  W,  no, E::Zero, X, no},
    {"mixxx.sampler[%1].volume",       O::Sampler,    "[Sampler%1]",                       "volume",         S::ControlParameter, T::Unit,     3, R,  W,  no, E::Keep, no, no},
};
// clang-format on

int ownerCount(Owner owner) {
    switch (owner) {
    case Owner::Deck:
    case Owner::Stem:
        return kNumDecks;
    case Owner::EffectUnit:
        return kNumEffectUnits;
    case Owner::Sampler:
        return kNumSamplers;
    case Owner::Adapter:
    case Owner::Global:
        break;
    }
    return 1;
}

PathEntry makeEntry(const PathRow& row, int index, int subIndex) {
    PathEntry entry;
    entry.pRow = &row;
    const bool indexed = row.owner != Owner::Adapter && row.owner != Owner::Global;
    entry.index = indexed ? index : -1;
    entry.subIndex = subIndex;
    QString path = QString::fromLatin1(row.path);
    if (indexed) {
        path = path.arg(index);
    }
    if (subIndex >= 0) {
        path = path.arg(subIndex);
    }
    entry.path = path;
    const QString group = QString::fromLatin1(row.group);
    const QString key = QString::fromLatin1(row.key);
    switch (row.source) {
    case Source::ControlValue:
    case Source::ControlParameter:
        if (subIndex >= 0) {
            entry.keys.append(ConfigKey(group.arg(index + 1).arg(subIndex + 1), key));
        } else if (indexed) {
            entry.keys.append(ConfigKey(group.arg(index + 1), key));
        } else {
            entry.keys.append(ConfigKey(group, key));
        }
        break;
    case Source::AnyControlValue:
        entry.keys.reserve(kEffectSlotsPerUnit);
        for (int slot = 0; slot < kEffectSlotsPerUnit; ++slot) {
            entry.keys.append(ConfigKey(group.arg(index + 1).arg(slot + 1), key));
        }
        break;
    case Source::Constant:
    case Source::TrackTitle:
    case Source::TrackArtist:
        break;
    }
    return entry;
}

} // namespace

const std::vector<PathRow>& pathRows() {
    return kPathRows;
}

QList<PathEntry> buildPathTable() {
    QList<PathEntry> entries;
    entries.reserve(kExpectedReadablePaths + kNumDecks);
    // Consecutive rows of one owner and one delivery class form a group that
    // is expanded owner-major: all of deck 0, then all of deck 1.
    std::size_t first = 0;
    while (first < kPathRows.size()) {
        const PathRow& head = kPathRows[first];
        std::size_t last = first;
        while (last + 1 < kPathRows.size() &&
                kPathRows[last + 1].owner == head.owner &&
                kPathRows[last + 1].continuous == head.continuous) {
            ++last;
        }
        for (int index = 0; index < ownerCount(head.owner); ++index) {
            const int subCount = head.owner == Owner::Stem ? kStemsPerDeck : 1;
            for (int sub = 0; sub < subCount; ++sub) {
                for (std::size_t row = first; row <= last; ++row) {
                    entries.append(makeEntry(kPathRows[row],
                            index,
                            head.owner == Owner::Stem ? sub : -1));
                }
            }
        }
        first = last + 1;
    }
    return entries;
}

double quantise(double value, int decimals) {
    constexpr double kScales[] = {1.0, 10.0, 100.0, 1000.0, 10000.0};
    constexpr int kMaxDecimals = 4;
    const int places = decimals < 0 ? 0 : (decimals > kMaxDecimals ? kMaxDecimals : decimals);
    // The number of 10^-places steps, halves away from zero.
    const double steps = std::floor(std::fabs(value) * kScales[places] + 0.5);
    if (!(steps >= 1.0)) {
        // Zero, and never -0.0. Also what a NaN becomes.
        return 0.0;
    }
    // The double nearest to steps / 10^places, which is the value a peer
    // reads back from "0.124". It is taken from the decimal text rather than
    // from a division: Mixxx is built with -ffast-math on GCC and Clang,
    // where the division becomes a multiplication by an inexact reciprocal
    // and 124 / 1000 comes out as 0.12399999999999992.
    QByteArray text = QByteArray::number(steps, 'f', 0);
    text.append("e-");
    text.append(QByteArray::number(places));
    const double magnitude = text.toDouble();
    return value < 0.0 ? -magnitude : magnitude;
}

} // namespace mmcc
