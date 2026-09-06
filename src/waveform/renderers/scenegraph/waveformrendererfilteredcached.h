#pragma once

#include <QSGNode>
#include <QSGTransformNode>
#include <array>
#include <cstdint>
#include <map>

#include "rendergraph/geometrynode.h"
#include "util/class.h"
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"
#include "waveform/renderers/waveformfilteredcolumns.h"

struct WaveformData;

namespace allshader {
class WaveformRendererFilteredCached;
} // namespace allshader

/// A scene-graph-only twin of WaveformRendererFiltered that scrolls instead
/// of redrawing.
///
/// WaveformRendererFiltered walks every pixel column of the visible range and
/// rebuilds its whole vertex buffer on every frame. That is fine for one deck
/// waveform in a window; on a 2560 px strip showing four of them it is the
/// dominant per-frame cost, and almost all of it recomputes columns that were
/// identical last frame, just one pixel to the left.
///
/// Here the waveform is cut into fixed-width tiles of columns on the same
/// column grid the original renderer uses. A tile's geometry is built once,
/// in "column units" horizontally and raw waveform values (0..255)
/// vertically, and then only moved: a QSGTransformNode per tile carries the
/// scroll offset, and one per band carries the vertical scale that the
/// widget height, the pre-gain and the EQ gains impose. A frame therefore
/// touches a handful of matrices and builds a tile only when one scrolls into
/// view for the first time. Zoom, track, colour, device pixel ratio and
/// analysis progress changes invalidate the cache, so at worst a frame costs
/// what every frame used to.
///
/// Horizontal orientation only, which is all the QML display renders.
class allshader::WaveformRendererFilteredCached final
        : public allshader::WaveformRendererSignalBase,
          public QSGNode {
  public:
    static constexpr int kBands = mixxx::waveform::FilteredColumnMax::kBands;
    /// Device pixels (columns) per tile.
    static constexpr int kTileColumns = 256;

    explicit WaveformRendererFilteredCached(WaveformWidgetRenderer* waveformWidget,
            bool rgbStacked,
            ::WaveformRendererSignalBase::Options options);
    ~WaveformRendererFilteredCached() override;

    // Pure virtual from WaveformRendererSignalBase, not used
    void onSetup(const QDomNode& node) override;

    /// Called once per frame by QmlWaveformDisplay from updatePaintNode.
    void update() override;

    /// Diagnostics, also used by the tests.
    int cachedTileCount() const {
        return static_cast<int>(m_tiles.size());
    }
    int tilesBuilt() const {
        return m_tilesBuilt;
    }

  private:
    struct Tile {
        std::array<QSGTransformNode*, kBands> transform{};
        std::array<rendergraph::GeometryNode*, kBands> geometry{};
        bool attached{false};
    };

    /// Everything a tile's vertex data depends on. When any of it changes
    /// the tiles are thrown away.
    struct CacheKey {
        const void* pWaveform{nullptr};
        int completion{-1};
        double framesPerColumn{0.0};
        float devicePixelRatio{1.f};
        bool rgbStacked{false};
        float color[kBands][3]{};

        bool operator==(const CacheKey& other) const;
    };

    void clearTiles();
    void hideAll();
    Tile* tileAt(int tileIndex, const WaveformData* pData, int dataSize);
    void buildTile(Tile* pTile, int tileIndex, const WaveformData* pData, int dataSize);
    void attach(Tile* pTile);
    void detach(Tile* pTile);
    void updateAxis(float length, float halfBreadth);

    const bool m_bRgbStacked;
    std::array<QSGTransformNode*, kBands> m_bandRoot{};
    rendergraph::GeometryNode* m_pAxisNode{nullptr};
    std::map<int, Tile> m_tiles;
    CacheKey m_key;
    int m_tilesBuilt{0};
    float m_axisLength{-1.f};
    float m_axisHalfBreadth{-1.f};
    float m_axisColor[3]{-1.f, -1.f, -1.f};

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererFilteredCached);
};
