#include "waveform/renderers/scenegraph/waveformrendererfilteredcached.h"

#include <QMatrix4x4>
#include <algorithm>
#include <cmath>

#include "rendergraph/material/rgbmaterial.h"
#include "rendergraph/vertexupdaters/rgbvertexupdater.h"
#include "track/track.h"
#include "util/logger.h"
#include "util/math.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveform.h"

using namespace rendergraph;

namespace {

constexpr int kVerticesPerRectangle = 6;
// Tiles this far outside the visible range (in tiles) are dropped. Anything
// closer is kept, detached from the scene, for the next scroll or nudge back.
constexpr int kKeepMargin = 4;

const mixxx::Logger kLogger("WaveformRendererFilteredCached");

} // namespace

namespace allshader {

bool WaveformRendererFilteredCached::CacheKey::operator==(const CacheKey& other) const {
    if (pWaveform != other.pWaveform || completion != other.completion ||
            rgbStacked != other.rgbStacked ||
            devicePixelRatio != other.devicePixelRatio) {
        return false;
    }
    // The frames-per-column ratio is recomputed from doubles every frame;
    // treat anything within rounding as the same zoom level.
    if (std::fabs(framesPerColumn - other.framesPerColumn) >
            std::max(framesPerColumn, other.framesPerColumn) * 1e-9) {
        return false;
    }
    for (int band = 0; band < kBands; ++band) {
        for (int c = 0; c < 3; ++c) {
            if (color[band][c] != other.color[band][c]) {
                return false;
            }
        }
    }
    return true;
}

WaveformRendererFilteredCached::WaveformRendererFilteredCached(
        WaveformWidgetRenderer* waveformWidget,
        bool rgbStacked,
        ::WaveformRendererSignalBase::Options options)
        : WaveformRendererSignalBase(waveformWidget, options),
          m_bRgbStacked(rgbStacked) {
    // Draw order matches WaveformRendererFiltered: axis, then low, mid, high.
    auto pAxis = std::make_unique<GeometryNode>();
    pAxis->initForRectangles<RGBMaterial>(1);
    m_pAxisNode = pAxis.release();
    appendChildNode(m_pAxisNode);
    for (int band = 0; band < kBands; ++band) {
        m_bandRoot[band] = new QSGTransformNode();
        appendChildNode(m_bandRoot[band]);
    }
}

WaveformRendererFilteredCached::~WaveformRendererFilteredCached() {
    // Attached tiles would be deleted by their band root; detached ones are
    // ours. Detach everything first so there is exactly one owner.
    clearTiles();
}

void WaveformRendererFilteredCached::onSetup(const QDomNode&) {
}

void WaveformRendererFilteredCached::clearTiles() {
    for (auto& [index, tile] : m_tiles) {
        detach(&tile);
        for (int band = 0; band < kBands; ++band) {
            delete tile.transform[band];
        }
    }
    m_tiles.clear();
}

void WaveformRendererFilteredCached::attach(Tile* pTile) {
    if (pTile->attached) {
        return;
    }
    for (int band = 0; band < kBands; ++band) {
        m_bandRoot[band]->appendChildNode(pTile->transform[band]);
    }
    pTile->attached = true;
}

void WaveformRendererFilteredCached::detach(Tile* pTile) {
    if (!pTile->attached) {
        return;
    }
    for (int band = 0; band < kBands; ++band) {
        m_bandRoot[band]->removeChildNode(pTile->transform[band]);
    }
    pTile->attached = false;
}

void WaveformRendererFilteredCached::hideAll() {
    for (auto& [index, tile] : m_tiles) {
        detach(&tile);
    }
    if (m_pAxisNode->geometry().vertexCount() != 0) {
        m_pAxisNode->geometry().allocate(0);
        m_pAxisNode->markDirtyGeometry();
        m_axisLength = -1.f;
    }
}

void WaveformRendererFilteredCached::updateAxis(float length, float halfBreadth) {
    const float r = static_cast<float>(m_axesColor_r);
    const float g = static_cast<float>(m_axesColor_g);
    const float b = static_cast<float>(m_axesColor_b);
    if (length == m_axisLength && halfBreadth == m_axisHalfBreadth &&
            r == m_axisColor[0] && g == m_axisColor[1] && b == m_axisColor[2]) {
        return;
    }
    m_axisLength = length;
    m_axisHalfBreadth = halfBreadth;
    m_axisColor[0] = r;
    m_axisColor[1] = g;
    m_axisColor[2] = b;
    Geometry& geometry = m_pAxisNode->geometry();
    geometry.allocate(kVerticesPerRectangle);
    RGBVertexUpdater updater{geometry.vertexDataAs<Geometry::RGBColoredPoint2D>()};
    updater.addRectangle({0.f, halfBreadth - 0.5f}, {length, halfBreadth + 0.5f}, {r, g, b});
    m_pAxisNode->markDirtyGeometry();
    m_pAxisNode->markDirtyMaterial();
}

void WaveformRendererFilteredCached::buildTile(
        Tile* pTile, int tileIndex, const WaveformData* pData, int dataSize) {
    mixxx::waveform::FilteredColumnMax columns[kTileColumns];
    mixxx::waveform::reduceFilteredColumns(pData,
            dataSize,
            tileIndex * kTileColumns,
            kTileColumns,
            m_key.framesPerColumn,
            columns);

    // Horizontal unit: logical pixels, tile-local, so a column is 1/dpr wide
    // and starts at 0 for the first column of the tile. Vertical unit: raw
    // waveform values, channel 0 upwards (negative), channel 1 downwards;
    // the band root's matrix maps that onto the widget.
    const float columnWidth = 1.f / m_key.devicePixelRatio;
    const float halfColumn = 0.5f * columnWidth;
    for (int band = 0; band < kBands; ++band) {
        Geometry& geometry = pTile->geometry[band]->geometry();
        RGBVertexUpdater updater{geometry.vertexDataAs<Geometry::RGBColoredPoint2D>()};
        const QVector3D rgb(m_key.color[band][0], m_key.color[band][1], m_key.color[band][2]);
        for (int column = 0; column < kTileColumns; ++column) {
            const float x = static_cast<float>(column) * columnWidth;
            updater.addRectangle(
                    {x - halfColumn, -static_cast<float>(columns[column].value[band][0])},
                    {x + halfColumn, static_cast<float>(columns[column].value[band][1])},
                    rgb);
        }
        DEBUG_ASSERT(updater.index() == kTileColumns * kVerticesPerRectangle);
        pTile->geometry[band]->markDirtyGeometry();
        pTile->geometry[band]->markDirtyMaterial();
    }
    ++m_tilesBuilt;
}

WaveformRendererFilteredCached::Tile* WaveformRendererFilteredCached::tileAt(
        int tileIndex, const WaveformData* pData, int dataSize) {
    auto it = m_tiles.find(tileIndex);
    if (it != m_tiles.end()) {
        return &it->second;
    }
    Tile& tile = m_tiles[tileIndex];
    for (int band = 0; band < kBands; ++band) {
        tile.transform[band] = new QSGTransformNode();
        auto pGeometry = std::make_unique<GeometryNode>();
        pGeometry->initForRectangles<RGBMaterial>(kTileColumns);
        tile.geometry[band] = pGeometry.release();
        // The transform node owns the geometry node (OwnedByParent), and the
        // band root owns the transform node only while it is attached.
        tile.transform[band]->appendChildNode(tile.geometry[band]);
    }
    buildTile(&tile, tileIndex, pData, dataSize);
    return &tile;
}

void WaveformRendererFilteredCached::update() {
    TrackPointer pTrack = m_waveformRenderer->getTrackInfo();
    ConstWaveformPointer waveform = pTrack ? pTrack->getWaveform() : ConstWaveformPointer();
    const int dataSize = waveform ? waveform->getDataSize() : 0;
    const WaveformData* pData = dataSize > 1 ? waveform->data() : nullptr;
#ifdef __STEM__
    if (pData && pTrack) {
        // A stem track is drawn by the stem renderer instead.
        auto stemInfo = pTrack->getStemInfo();
        if (!stemInfo.isEmpty() && waveform->hasStem() && !m_ignoreStem) {
            pData = nullptr;
        }
    }
#endif
    if (!pData) {
        hideAll();
        return;
    }

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();
    const int length = m_waveformRenderer->getLength();
    const int pixelLength = static_cast<int>(length * devicePixelRatio);
    if (pixelLength <= 0) {
        hideAll();
        return;
    }

    // Same grid as WaveformRendererFiltered: frames per device pixel derived
    // from the displayed range, columns snapped to multiples of it.
    const int visualFramesSize = dataSize / 2;
    const double firstVisualFrame =
            m_waveformRenderer->getFirstDisplayedPosition() * visualFramesSize;
    const double lastVisualFrame =
            m_waveformRenderer->getLastDisplayedPosition() * visualFramesSize;
    const double framesPerColumn =
            (lastVisualFrame - firstVisualFrame) / static_cast<double>(pixelLength);
    if (!(framesPerColumn > 0.0)) {
        hideAll();
        return;
    }

    CacheKey key;
    key.pWaveform = waveform.data();
    // While the analyzer is still filling the waveform, the data behind the
    // tiles changes under them: rebuild every frame until it is complete,
    // which is exactly what the uncached renderer always does.
    key.completion = std::min(waveform->getCompletion(), dataSize);
    key.framesPerColumn = framesPerColumn;
    key.devicePixelRatio = devicePixelRatio;
    key.rgbStacked = m_bRgbStacked;
    if (m_bRgbStacked) {
        key.color[0][0] = static_cast<float>(m_rgbLowColor_r);
        key.color[0][1] = static_cast<float>(m_rgbLowColor_g);
        key.color[0][2] = static_cast<float>(m_rgbLowColor_b);
        key.color[1][0] = static_cast<float>(m_rgbMidColor_r);
        key.color[1][1] = static_cast<float>(m_rgbMidColor_g);
        key.color[1][2] = static_cast<float>(m_rgbMidColor_b);
        key.color[2][0] = static_cast<float>(m_rgbHighColor_r);
        key.color[2][1] = static_cast<float>(m_rgbHighColor_g);
        key.color[2][2] = static_cast<float>(m_rgbHighColor_b);
    } else {
        key.color[0][0] = static_cast<float>(m_lowColor_r);
        key.color[0][1] = static_cast<float>(m_lowColor_g);
        key.color[0][2] = static_cast<float>(m_lowColor_b);
        key.color[1][0] = static_cast<float>(m_midColor_r);
        key.color[1][1] = static_cast<float>(m_midColor_g);
        key.color[1][2] = static_cast<float>(m_midColor_b);
        key.color[2][0] = static_cast<float>(m_highColor_r);
        key.color[2][1] = static_cast<float>(m_highColor_g);
        key.color[2][2] = static_cast<float>(m_highColor_b);
    }
    if (!(key == m_key)) {
        clearTiles();
        m_key = key;
        if (key.completion >= dataSize) {
            // Once per track/zoom/colour change; silent while the analyzer
            // is still filling the waveform (that would be every frame).
            kLogger.debug() << "tile cache reset:" << m_waveformRenderer->getGroup()
                            << "framesPerColumn" << framesPerColumn
                            << "dpr" << devicePixelRatio;
        }
    }

    // Per-frame values that live in matrices rather than vertices.
    float allGain = 1.f;
    float bandGain[kBands] = {1.f, 1.f, 1.f};
    getGains(&allGain, &bandGain[0], &bandGain[1], &bandGain[2]);
    const float breadth = static_cast<float>(m_waveformRenderer->getBreadth());
    const float halfBreadth = breadth / 2.f;
    const float heightFactor = allGain * halfBreadth / m_maxValue;

    updateAxis(static_cast<float>(length), halfBreadth);

    for (int band = 0; band < kBands; ++band) {
        QMatrix4x4 matrix;
        matrix.translate(0.f, halfBreadth);
        matrix.scale(1.f, heightFactor * bandGain[band]);
        m_bandRoot[band]->setMatrix(matrix);
    }

    // The first visible column on the grid, as WaveformRendererFiltered
    // rounds it, and the tiles that cover the visible columns.
    const int firstColumn = static_cast<int>(std::lround(firstVisualFrame / framesPerColumn));
    const int lastColumn = firstColumn + pixelLength - 1;
    const int firstTile = static_cast<int>(std::floor(
            static_cast<double>(firstColumn) / kTileColumns));
    const int lastTile = static_cast<int>(std::floor(
            static_cast<double>(lastColumn) / kTileColumns));

    for (int tileIndex = firstTile; tileIndex <= lastTile; ++tileIndex) {
        Tile* pTile = tileAt(tileIndex, pData, dataSize);
        const float offset = static_cast<float>(tileIndex * kTileColumns - firstColumn) /
                devicePixelRatio;
        QMatrix4x4 matrix;
        matrix.translate(offset, 0.f);
        for (int band = 0; band < kBands; ++band) {
            pTile->transform[band]->setMatrix(matrix);
        }
        attach(pTile);
    }

    // Park tiles that scrolled out, drop the ones that are far gone.
    for (auto it = m_tiles.begin(); it != m_tiles.end();) {
        const int tileIndex = it->first;
        if (tileIndex >= firstTile && tileIndex <= lastTile) {
            ++it;
            continue;
        }
        detach(&it->second);
        if (tileIndex < firstTile - kKeepMargin || tileIndex > lastTile + kKeepMargin) {
            for (int band = 0; band < kBands; ++band) {
                delete it->second.transform[band];
            }
            it = m_tiles.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace allshader
