#include "RasterEditorWindow.h"

#include <QAbstractItemView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace
{
constexpr int kMinimumPlaceholderHeight = 200;
constexpr int kMinimumPlaceholderWidth = 160;
}

RasterEditorWindow::RasterEditorWindow(QWidget* parent)
    : QDockWidget(QObject::tr("Raster Editor"), parent)
    , m_frameLabel(nullptr)
    , m_canvasPlaceholder(nullptr)
    , m_toolPlaceholder(nullptr)
    , m_layerInfoLabel(nullptr)
    , m_layerList(nullptr)
{
    setObjectName(QStringLiteral("RasterEditorWindow"));
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    QWidget* container = new QWidget(this);
    setWidget(container);

    QHBoxLayout* rootLayout = new QHBoxLayout(container);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(12);

    // Tool controls placeholder
    QGroupBox* toolGroup = new QGroupBox(tr("Tool Controls"), container);
    QVBoxLayout* toolLayout = new QVBoxLayout(toolGroup);
    m_toolPlaceholder = new QLabel(tr("Tool controls placeholder"), toolGroup);
    m_toolPlaceholder->setAlignment(Qt::AlignCenter);
    m_toolPlaceholder->setMinimumSize(kMinimumPlaceholderWidth, kMinimumPlaceholderHeight);
    m_toolPlaceholder->setStyleSheet(QStringLiteral("QLabel { color: palette(mid); border: 1px dashed palette(midlight); }"));
    toolLayout->addWidget(m_toolPlaceholder, 1);
    toolLayout->addStretch();

    // Canvas placeholder
    QGroupBox* canvasGroup = new QGroupBox(tr("Canvas"), container);
    QVBoxLayout* canvasLayout = new QVBoxLayout(canvasGroup);
    m_canvasPlaceholder = new QLabel(tr("Canvas placeholder"), canvasGroup);
    m_canvasPlaceholder->setAlignment(Qt::AlignCenter);
    m_canvasPlaceholder->setMinimumSize(kMinimumPlaceholderWidth * 2, kMinimumPlaceholderHeight);
    m_canvasPlaceholder->setStyleSheet(QStringLiteral("QLabel { color: palette(mid); border: 1px dashed palette(midlight); }"));
    canvasLayout->addWidget(m_canvasPlaceholder, 1);

    m_frameLabel = new QLabel(tr("Current frame: 0"), canvasGroup);
    m_frameLabel->setAlignment(Qt::AlignCenter);
    canvasLayout->addWidget(m_frameLabel);

    // Layer list placeholder
    QGroupBox* layerGroup = new QGroupBox(tr("Layers"), container);
    QVBoxLayout* layerLayout = new QVBoxLayout(layerGroup);
    m_layerList = new QListWidget(layerGroup);
    m_layerList->setFocusPolicy(Qt::NoFocus);
    m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layerList->addItem(tr("Layer 1"));
    layerLayout->addWidget(m_layerList, 1);

    m_layerInfoLabel = new QLabel(tr("Selected layer: 1"), layerGroup);
    m_layerInfoLabel->setAlignment(Qt::AlignLeft);
    layerLayout->addWidget(m_layerInfoLabel);

    rootLayout->addWidget(toolGroup);
    rootLayout->addWidget(canvasGroup, 1);
    rootLayout->addWidget(layerGroup);
    rootLayout->setStretchFactor(canvasGroup, 1);
}

void RasterEditorWindow::setCurrentFrame(int frame)
{
    if (!m_frameLabel) {
        return;
    }

    if (frame < 0) {
        frame = 0;
    }

    m_frameLabel->setText(tr("Current frame: %1").arg(frame));
}

void RasterEditorWindow::setCurrentLayer(int layer)
{
    if (!m_layerList || !m_layerInfoLabel) {
        return;
    }

    if (layer < 0) {
        m_layerList->clearSelection();
        m_layerInfoLabel->setText(tr("Selected layer: none"));
        return;
    }

    ensureLayerPlaceholderCount(layer + 1);

    {
        QSignalBlocker blocker(m_layerList);
        m_layerList->setCurrentRow(layer);
    }

    m_layerInfoLabel->setText(tr("Selected layer: %1").arg(layer + 1));
}

void RasterEditorWindow::ensureLayerPlaceholderCount(int count)
{
    if (!m_layerList) {
        return;
    }

    while (m_layerList->count() < count) {
        const int index = m_layerList->count();
        m_layerList->addItem(tr("Layer %1").arg(index + 1));
    }
}

