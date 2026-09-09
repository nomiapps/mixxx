#pragma once

#include <QRectF>
#include <QSGClipNode>
#include <QSGNode>
#include <QSGTransformNode>
#include <QVarLengthArray>
#include <array>
#include <map>
#include <memory>
#include <numeric>
#include <vector>

#include "engine/engine.h"
#include "rendergraph/geometrynode.h"
#include "util/class.h"
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"
#include "waveform/renderers/waveformstemcolumns.h"

class ControlProxy;
struct WaveformData;

namespace allshader {
class WaveformRendererStemCached;
} // namespace allshader

/// A scene-graph-only twin of WaveformRendererStem that scrolls instead of
/// redrawing, the same treatment WaveformRendererFilteredCached gives the
/// filtered waveform.
///
/// WaveformRendererStem rebuilds one vertex buffer per frame holding every
/// strip of every stem, twice over (an outline layer and a body layer), and
/// on a four-deck layout that is the largest per-frame cost left in the
/// waveforms -- while almost all of it reproduces the previous frame's
/// columns shifted by one strip.
///
/// Here the waveform is cut into fixed-width tiles of columns on the same
/// grid the original uses. A tile's geometry is built once, in tile-local
/// logical pixels horizontally and raw waveform values (0..255) vertically,
/// and then only moved. What the original recomputes per frame lives in
/// matrices instead:
///
///  - the widget height, the pre-gain and the deck gain scale a stem's
///    transform;
///  - a stem's lane position translates it;
///  - stem volume, mute and the selected-stem mask scale the body layer
///    only, which is what makes the outline show the ungained signal;
///  - a single-stem view scales the other stems to nothing.
///
/// The one thing a matrix cannot express is the original's
/// `min(height, halfBreadth)`, which stops a loud stem drawing into its
/// neighbour's lane while split. A clip node per stem does it instead, and
/// clipping a rectangle at the lane edge and flattening it there give the
/// same pixels.
///
/// Horizontal orientation only, which is all the QML display renders.
class allshader::WaveformRendererStemCached final
        : public allshader::WaveformRendererSignalBase,
          public QSGNode {
  public:
    static constexpr int kStems = mixxx::kMaxSupportedStems;
    /// Outline first, then the body drawn over it.
    static constexpr int kLayers = 2;
    /// Columns per tile.
    static constexpr int kTileColumns = 256;

    explicit WaveformRendererStemCached(WaveformWidgetRenderer* waveformWidget,
            ::WaveformRendererAbstract::PositionSource type =
                    ::WaveformRendererAbstract::Play,
            ::WaveformRendererSignalBase::Options options =
                    ::WaveformRendererSignalBase::Option::None);
    ~WaveformRendererStemCached() override;

    // Pure virtual from WaveformRendererSignalBase, not used
    void onSetup(const QDomNode& node) override;

    bool init() override;

    bool supportsSlip() const override {
        return true;
    }

    /// Called once per frame by QmlWaveformDisplay from updatePaintNode.
    void update() override;

    /// Diagnostics, also used by the tests.
    int cachedTileCount() const {
        return static_cast<int>(m_tiles.size());
    }
    int tilesBuilt() const {
        return m_tilesBuilt;
    }

  public slots:
    void setSplitStemTracks(bool splitStemTracks) {
        m_splitStemTracks = splitStemTracks;
    }
    // -1 draws every stem (the normal case). A stem index draws only that
    // stem, across the full breadth.
    void setStemIndex(int stemIndex) {
        m_stemIndex = stemIndex;
    }
    void setReorderOnChange(bool value) {
        m_reorderOnChange = value;
        std::iota(m_stackOrder.begin(), m_stackOrder.end(), 0);
        m_stackOrderDirty = true;
    }
    void setOutlineOpacity(float value) {
        m_outlineOpacity = value;
    }
    void setOpacity(float value) {
        m_opacity = value;
    }

  private:
    struct Tile {
        QSGTransformNode* transform[kStems][kLayers]{};
        rendergraph::GeometryNode* geometry[kStems][kLayers]{};
        bool attached{false};
    };

    /// Everything a tile's vertex data depends on. When any of it changes the
    /// tiles are thrown away. Gains, mute, lane layout and the selected-stem
    /// mask are deliberately absent: those are matrices, not vertices.
    struct CacheKey {
        const void* pWaveform{nullptr};
        int completion{-1};
        double framesPerColumn{0.0};
        float devicePixelRatio{1.f};
        float color[kStems][kLayers][4]{};

        bool operator==(const CacheKey& other) const;
    };

    void clearTiles();
    void hideAll();
    Tile* tileAt(int tileIndex, const WaveformData* pData, int dataSize);
    void buildTile(Tile* pTile, int tileIndex, const WaveformData* pData, int dataSize);
    void attach(Tile* pTile);
    void detach(Tile* pTile);
    void applyStackOrder();

    const bool m_isSlipRenderer;
    bool m_splitStemTracks{false};
    int m_stemIndex{-1};
    bool m_reorderOnChange{false};
    float m_outlineOpacity{0.15f};
    float m_opacity{0.75f};

    std::vector<std::unique_ptr<ControlProxy>> m_pStemGain;
    std::vector<std::unique_ptr<ControlProxy>> m_pStemMute;

    QVarLengthArray<int, mixxx::kMaxSupportedStems> m_stackOrder;
    bool m_stackOrderDirty{true};

    QSGClipNode* m_stemClip[kStems]{};
    QRectF m_clipRect[kStems];
    QSGTransformNode* m_stemRoot[kStems]{};
    QSGTransformNode* m_layerRoot[kStems][kLayers]{};

    std::map<int, Tile> m_tiles;
    CacheKey m_key;
    int m_tilesBuilt{0};

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererStemCached);
};
