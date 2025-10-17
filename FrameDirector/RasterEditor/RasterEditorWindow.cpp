#include "RasterEditorWindow.h"

#include "RasterCanvasWidget.h"
#include "RasterDocument.h"
#include "RasterORAImporter.h"
#include "ORAExporter.h"
#include "RasterTools.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <iterator>

namespace
{
struct BlendModeOption
{
    const char* label;
    QPainter::CompositionMode mode;
};

constexpr BlendModeOption kBlendModes[] = {
    { "Normal", QPainter::CompositionMode_SourceOver },
    { "Multiply", QPainter::CompositionMode_Multiply },
    { "Screen", QPainter::CompositionMode_Screen },
    { "Overlay", QPainter::CompositionMode_Overlay },
    { "Darken", QPainter::CompositionMode_Darken },
    { "Lighten", QPainter::CompositionMode_Lighten },
    { "Color Dodge", QPainter::CompositionMode_ColorDodge },
    { "Color Burn", QPainter::CompositionMode_ColorBurn },
    { "Hard Light", QPainter::CompositionMode_HardLight },
    { "Soft Light", QPainter::CompositionMode_SoftLight },
    { "Difference", QPainter::CompositionMode_Difference },
    { "Exclusion", QPainter::CompositionMode_Exclusion }
};

constexpr int kDefaultBrushSize = 12;
}

RasterEditorWindow::RasterEditorWindow(QWidget* parent)
    : QDockWidget(QObject::tr("Raster Editor"), parent)
    , m_document(new RasterDocument(this))
    , m_canvasWidget(nullptr)
    , m_brushTool(new RasterBrushTool(this))
    , m_eraserTool(new RasterEraserTool(this))
    , m_fillTool(new RasterFillTool(this))
    , m_activeTool(nullptr)
    , m_frameLabel(nullptr)
    , m_layerList(nullptr)
    , m_layerInfoLabel(nullptr)
    , m_toolSelector(nullptr)
    , m_brushSizeSlider(nullptr)
    , m_brushSizeValue(nullptr)
    , m_colorButton(nullptr)
    , m_onionSkinCheck(nullptr)
    , m_onionBeforeSpin(nullptr)
    , m_onionAfterSpin(nullptr)
    , m_addLayerButton(nullptr)
    , m_removeLayerButton(nullptr)
    , m_opacitySpin(nullptr)
    , m_blendModeCombo(nullptr)
    , m_primaryColor(Qt::black)
{
    setObjectName(QStringLiteral("RasterEditorWindow"));
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_brushTool->setSize(kDefaultBrushSize);
    m_brushTool->setColor(m_primaryColor);
    m_eraserTool->setSize(kDefaultBrushSize);
    m_fillTool->setColor(m_primaryColor);

    initializeUi();
    connectDocumentSignals();

    refreshLayerList();
    updateLayerPropertiesUi();
    updateLayerInfo();
    updateOnionSkinControls();
    updateToolControls();
    updateColorButton();

    onActiveLayerChanged(m_document->activeLayer());
    onActiveFrameChanged(m_document->activeFrame());
}

void RasterEditorWindow::initializeUi()
{
    QWidget* container = new QWidget(this);
    setWidget(container);

    QHBoxLayout* rootLayout = new QHBoxLayout(container);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(12);

    // Tool controls
    QGroupBox* toolGroup = new QGroupBox(tr("Tool Controls"), container);
    QVBoxLayout* toolLayout = new QVBoxLayout(toolGroup);

    QLabel* toolLabel = new QLabel(tr("Active Tool"), toolGroup);
    m_toolSelector = new QComboBox(toolGroup);
    m_toolSelector->addItem(tr("Brush"));
    m_toolSelector->addItem(tr("Eraser"));
    m_toolSelector->addItem(tr("Fill"));
    connect(m_toolSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RasterEditorWindow::onToolChanged);
    toolLayout->addWidget(toolLabel);
    toolLayout->addWidget(m_toolSelector);

    QLabel* sizeLabel = new QLabel(tr("Brush Size"), toolGroup);
    m_brushSizeSlider = new QSlider(Qt::Horizontal, toolGroup);
    m_brushSizeSlider->setRange(1, 256);
    m_brushSizeSlider->setValue(kDefaultBrushSize);
    connect(m_brushSizeSlider, &QSlider::valueChanged, this, &RasterEditorWindow::onBrushSizeChanged);
    m_brushSizeValue = new QLabel(QString::number(kDefaultBrushSize), toolGroup);
    m_brushSizeValue->setAlignment(Qt::AlignRight);

    toolLayout->addWidget(sizeLabel);
    toolLayout->addWidget(m_brushSizeSlider);
    toolLayout->addWidget(m_brushSizeValue);

    m_colorButton = new QPushButton(tr("Primary Color"), toolGroup);
    connect(m_colorButton, &QPushButton::clicked, this, &RasterEditorWindow::onColorButtonClicked);
    toolLayout->addWidget(m_colorButton);

    toolLayout->addSpacing(8);

    m_onionSkinCheck = new QCheckBox(tr("Enable Onion Skin"), toolGroup);
    connect(m_onionSkinCheck, &QCheckBox::toggled, this, &RasterEditorWindow::onOnionSkinToggled);
    toolLayout->addWidget(m_onionSkinCheck);

    QHBoxLayout* beforeLayout = new QHBoxLayout();
    QLabel* beforeLabel = new QLabel(tr("Frames Before"), toolGroup);
    m_onionBeforeSpin = new QSpinBox(toolGroup);
    m_onionBeforeSpin->setRange(0, 12);
    connect(m_onionBeforeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &RasterEditorWindow::onOnionBeforeChanged);
    beforeLayout->addWidget(beforeLabel);
    beforeLayout->addWidget(m_onionBeforeSpin);
    toolLayout->addLayout(beforeLayout);

    QHBoxLayout* afterLayout = new QHBoxLayout();
    QLabel* afterLabel = new QLabel(tr("Frames After"), toolGroup);
    m_onionAfterSpin = new QSpinBox(toolGroup);
    m_onionAfterSpin->setRange(0, 12);
    connect(m_onionAfterSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &RasterEditorWindow::onOnionAfterChanged);
    afterLayout->addWidget(afterLabel);
    afterLayout->addWidget(m_onionAfterSpin);
    toolLayout->addLayout(afterLayout);

    toolLayout->addStretch(1);

    // Canvas
    QGroupBox* canvasGroup = new QGroupBox(tr("Canvas"), container);
    QVBoxLayout* canvasLayout = new QVBoxLayout(canvasGroup);
    m_canvasWidget = new RasterCanvasWidget(canvasGroup);
    m_canvasWidget->setDocument(m_document);
    m_canvasWidget->setActiveTool(m_brushTool);
    m_activeTool = m_brushTool;
    m_toolSelector->setCurrentIndex(0);

    canvasLayout->addWidget(m_canvasWidget, 1);
    m_frameLabel = new QLabel(tr("Frame: 1"), canvasGroup);
    m_frameLabel->setAlignment(Qt::AlignCenter);
    canvasLayout->addWidget(m_frameLabel);

    // Layer controls
    QGroupBox* layerGroup = new QGroupBox(tr("Layers"), container);
    QVBoxLayout* layerLayout = new QVBoxLayout(layerGroup);

    QHBoxLayout* fileButtonsLayout = new QHBoxLayout();
    QPushButton* openOraButton = new QPushButton(tr("Open ORA…"), layerGroup);
    connect(openOraButton, &QPushButton::clicked, this, &RasterEditorWindow::onOpenOra);
    QPushButton* saveOraButton = new QPushButton(tr("Save ORA…"), layerGroup);
    connect(saveOraButton, &QPushButton::clicked, this, &RasterEditorWindow::onSaveOra);
    fileButtonsLayout->addWidget(openOraButton);
    fileButtonsLayout->addWidget(saveOraButton);
    layerLayout->addLayout(fileButtonsLayout);

    m_layerList = new QListWidget(layerGroup);
    m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layerList->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    connect(m_layerList, &QListWidget::currentRowChanged, this, &RasterEditorWindow::onLayerSelectionChanged);
    connect(m_layerList, &QListWidget::itemChanged, this, &RasterEditorWindow::onLayerItemChanged);
    layerLayout->addWidget(m_layerList, 1);

    QHBoxLayout* layerButtonLayout = new QHBoxLayout();
    m_addLayerButton = new QToolButton(layerGroup);
    m_addLayerButton->setText(tr("+").trimmed());
    m_addLayerButton->setToolTip(tr("Add Layer"));
    connect(m_addLayerButton, &QToolButton::clicked, this, &RasterEditorWindow::onAddLayer);
    m_removeLayerButton = new QToolButton(layerGroup);
    m_removeLayerButton->setText(tr("-").trimmed());
    m_removeLayerButton->setToolTip(tr("Remove Layer"));
    connect(m_removeLayerButton, &QToolButton::clicked, this, &RasterEditorWindow::onRemoveLayer);
    layerButtonLayout->addWidget(m_addLayerButton);
    layerButtonLayout->addWidget(m_removeLayerButton);
    layerLayout->addLayout(layerButtonLayout);

    QLabel* opacityLabel = new QLabel(tr("Opacity"), layerGroup);
    m_opacitySpin = new QDoubleSpinBox(layerGroup);
    m_opacitySpin->setRange(0.0, 100.0);
    m_opacitySpin->setDecimals(1);
    m_opacitySpin->setSuffix(tr(" %"));
    connect(m_opacitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RasterEditorWindow::onOpacityChanged);
    layerLayout->addWidget(opacityLabel);
    layerLayout->addWidget(m_opacitySpin);

    QLabel* blendLabel = new QLabel(tr("Blend Mode"), layerGroup);
    m_blendModeCombo = new QComboBox(layerGroup);
    for (const BlendModeOption& option : kBlendModes) {
        m_blendModeCombo->addItem(QObject::tr(option.label), static_cast<int>(option.mode));
    }
    connect(m_blendModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RasterEditorWindow::onBlendModeChanged);
    layerLayout->addWidget(blendLabel);
    layerLayout->addWidget(m_blendModeCombo);

    m_layerInfoLabel = new QLabel(layerGroup);
    m_layerInfoLabel->setAlignment(Qt::AlignLeft);
    layerLayout->addWidget(m_layerInfoLabel);

    rootLayout->addWidget(toolGroup);
    rootLayout->addWidget(canvasGroup, 1);
    rootLayout->addWidget(layerGroup);
    rootLayout->setStretchFactor(canvasGroup, 1);
}

void RasterEditorWindow::connectDocumentSignals()
{
    if (!m_document) {
        return;
    }

    connect(m_document, &RasterDocument::layerListChanged, this, &RasterEditorWindow::onDocumentLayerListChanged);
    connect(m_document, &RasterDocument::activeLayerChanged, this, &RasterEditorWindow::onActiveLayerChanged);
    connect(m_document, &RasterDocument::activeFrameChanged, this, &RasterEditorWindow::onActiveFrameChanged);
    connect(m_document, &RasterDocument::layerPropertyChanged, this, &RasterEditorWindow::onLayerPropertiesUpdated);
}

void RasterEditorWindow::setCurrentFrame(int frame)
{
    if (!m_document) {
        return;
    }

    m_document->setActiveFrame(frame);
    onActiveFrameChanged(m_document->activeFrame());
}

void RasterEditorWindow::setCurrentLayer(int layer)
{
    if (!m_document) {
        return;
    }

    m_document->setActiveLayer(layer);
    onActiveLayerChanged(m_document->activeLayer());
}

void RasterEditorWindow::onToolChanged(int index)
{
    RasterTool* tool = m_brushTool;
    switch (index) {
    case 0:
        tool = m_brushTool;
        break;
    case 1:
        tool = m_eraserTool;
        break;
    case 2:
        tool = m_fillTool;
        break;
    default:
        break;
    }

    m_activeTool = tool;
    if (m_canvasWidget) {
        m_canvasWidget->setActiveTool(tool);
    }

    updateToolControls();
}

void RasterEditorWindow::onBrushSizeChanged(int value)
{
    m_brushTool->setSize(value);
    m_eraserTool->setSize(value);
    if (m_brushSizeValue) {
        m_brushSizeValue->setText(QString::number(value));
    }
}

void RasterEditorWindow::onColorButtonClicked()
{
    const QColor color = QColorDialog::getColor(m_primaryColor, this, tr("Select Color"));
    if (!color.isValid()) {
        return;
    }

    m_primaryColor = color;
    m_brushTool->setColor(color);
    m_fillTool->setColor(color);
    updateColorButton();
}

void RasterEditorWindow::onOnionSkinToggled(bool enabled)
{
    if (!m_document) {
        return;
    }

    m_document->setOnionSkinEnabled(enabled);
    updateOnionSkinControls();
}

void RasterEditorWindow::onOnionBeforeChanged(int value)
{
    if (m_document) {
        m_document->setOnionSkinRange(value, m_document->onionSkinAfter());
    }
}

void RasterEditorWindow::onOnionAfterChanged(int value)
{
    if (m_document) {
        m_document->setOnionSkinRange(m_document->onionSkinBefore(), value);
    }
}

void RasterEditorWindow::onLayerSelectionChanged(int index)
{
    if (!m_document || index < 0) {
        return;
    }

    m_document->setActiveLayer(index);
    updateLayerPropertiesUi();
    updateLayerInfo();
}

void RasterEditorWindow::onLayerItemChanged(QListWidgetItem* item)
{
    if (!item || !m_document) {
        return;
    }

    const int row = m_layerList->row(item);
    if (row < 0 || row >= m_document->layerCount()) {
        return;
    }

    const RasterLayer& layer = m_document->layerAt(row);
    const bool visible = item->checkState() == Qt::Checked;
    if (layer.isVisible() != visible) {
        m_document->setLayerVisible(row, visible);
    }
    if (layer.name() != item->text()) {
        m_document->renameLayer(row, item->text());
    }
}

void RasterEditorWindow::onAddLayer()
{
    if (!m_document) {
        return;
    }

    const int index = m_document->addLayer();
    refreshLayerList();
    m_layerList->setCurrentRow(index);
}

void RasterEditorWindow::onRemoveLayer()
{
    if (!m_document) {
        return;
    }

    const int row = m_layerList->currentRow();
    if (row >= 0) {
        m_document->removeLayer(row);
    }
}

void RasterEditorWindow::onOpacityChanged(double value)
{
    if (!m_document) {
        return;
    }

    const int layer = m_document->activeLayer();
    if (layer >= 0) {
        m_document->setLayerOpacity(layer, value / 100.0);
    }
}

void RasterEditorWindow::onBlendModeChanged(int index)
{
    if (!m_document || index < 0) {
        return;
    }

    const int layer = m_document->activeLayer();
    if (layer < 0) {
        return;
    }

    const QVariant modeValue = m_blendModeCombo->itemData(index);
    if (!modeValue.isValid()) {
        return;
    }

    const auto mode = static_cast<QPainter::CompositionMode>(modeValue.toInt());
    m_document->setLayerBlendMode(layer, mode);
}

void RasterEditorWindow::onDocumentLayerListChanged()
{
    refreshLayerList();
    updateLayerInfo();
    updateLayerPropertiesUi();
}

void RasterEditorWindow::onActiveLayerChanged(int index)
{
    if (!m_document) {
        return;
    }

    if (m_layerList && m_layerList->currentRow() != index) {
        QSignalBlocker blocker(m_layerList);
        m_layerList->setCurrentRow(index);
    }

    updateLayerInfo();
    updateLayerPropertiesUi();
    m_removeLayerButton->setEnabled(m_document->layerCount() > 1);
}

void RasterEditorWindow::onActiveFrameChanged(int frame)
{
    if (!m_frameLabel) {
        return;
    }

    m_frameLabel->setText(tr("Frame: %1").arg(frame + 1));
}

void RasterEditorWindow::onLayerPropertiesUpdated(int index)
{
    Q_UNUSED(index);
    refreshLayerList();
    updateLayerPropertiesUi();
    updateLayerInfo();
}

void RasterEditorWindow::onOpenOra()
{
    if (!m_document) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(this, tr("Open ORA"), QString(), tr("OpenRaster Files (*.ora);;All Files (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!RasterORAImporter::importFile(filePath, m_document, &errorMessage)) {
        QMessageBox::warning(this, tr("Open ORA"), errorMessage.isEmpty() ? tr("Failed to import the selected ORA file.") : errorMessage);
        return;
    }

    updateLayerInfo();
    updateLayerPropertiesUi();
    updateOnionSkinControls();
}

void RasterEditorWindow::onSaveOra()
{
    if (!m_document) {
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(this, tr("Save ORA"), QString(), tr("OpenRaster Files (*.ora);;All Files (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFileInfo info(filePath);
    if (info.suffix().isEmpty()) {
        filePath.append(QStringLiteral(".ora"));
    }

    QString errorMessage;
    if (!ORAExporter::exportDocument(*m_document, filePath, &errorMessage)) {
        QMessageBox::warning(this, tr("Save ORA"), errorMessage.isEmpty() ? tr("Failed to export the ORA file.") : errorMessage);
    }
}

void RasterEditorWindow::refreshLayerList()
{
    if (!m_layerList || !m_document) {
        return;
    }

    QSignalBlocker blocker(m_layerList);
    m_layerList->clear();

    for (int i = 0; i < m_document->layerCount(); ++i) {
        const RasterLayer& layer = m_document->layerAt(i);
        QListWidgetItem* item = new QListWidgetItem(layer.name(), m_layerList);
        item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setCheckState(layer.isVisible() ? Qt::Checked : Qt::Unchecked);
    }

    const int active = m_document->activeLayer();
    if (active >= 0) {
        m_layerList->setCurrentRow(active);
    }
}

void RasterEditorWindow::updateLayerInfo()
{
    if (!m_layerInfoLabel || !m_document) {
        return;
    }

    const int layer = m_document->activeLayer();
    if (layer < 0 || layer >= m_document->layerCount()) {
        m_layerInfoLabel->setText(tr("Selected layer: none"));
        return;
    }

    const RasterLayer& layerData = m_document->layerAt(layer);
    m_layerInfoLabel->setText(tr("Selected layer: %1").arg(layerData.name()));
}

void RasterEditorWindow::updateToolControls()
{
    const bool isBrushTool = (m_activeTool == m_brushTool);
    const bool isEraserTool = (m_activeTool == m_eraserTool);

    const bool sizeEnabled = isBrushTool || isEraserTool;
    if (m_brushSizeSlider) {
        m_brushSizeSlider->setEnabled(sizeEnabled);
    }
    if (m_brushSizeValue) {
        m_brushSizeValue->setEnabled(sizeEnabled);
    }
    if (m_colorButton) {
        m_colorButton->setEnabled(!isEraserTool);
    }
}

void RasterEditorWindow::updateColorButton()
{
    if (!m_colorButton) {
        return;
    }

    const QString style = QStringLiteral("QPushButton { background-color: %1; border: 1px solid palette(mid); }")
                              .arg(m_primaryColor.name(QColor::HexArgb));
    m_colorButton->setStyleSheet(style);
}

void RasterEditorWindow::updateOnionSkinControls()
{
    if (!m_document) {
        return;
    }

    const bool enabled = m_document->onionSkinEnabled();
    {
        QSignalBlocker blocker(m_onionSkinCheck);
        m_onionSkinCheck->setChecked(enabled);
    }
    {
        QSignalBlocker blocker(m_onionBeforeSpin);
        m_onionBeforeSpin->setValue(m_document->onionSkinBefore());
    }
    {
        QSignalBlocker blocker(m_onionAfterSpin);
        m_onionAfterSpin->setValue(m_document->onionSkinAfter());
    }
    m_onionBeforeSpin->setEnabled(enabled);
    m_onionAfterSpin->setEnabled(enabled);
}

void RasterEditorWindow::updateLayerPropertiesUi()
{
    if (!m_document) {
        return;
    }

    const int layer = m_document->activeLayer();
    if (layer < 0 || layer >= m_document->layerCount()) {
        m_opacitySpin->setEnabled(false);
        m_blendModeCombo->setEnabled(false);
        return;
    }

    const RasterLayer& layerData = m_document->layerAt(layer);
    {
        QSignalBlocker blocker(m_opacitySpin);
        m_opacitySpin->setEnabled(true);
        m_opacitySpin->setValue(layerData.opacity() * 100.0);
    }
    {
        QSignalBlocker blocker(m_blendModeCombo);
        m_blendModeCombo->setEnabled(true);
        const int index = indexForBlendMode(layerData.blendMode());
        if (index >= 0) {
            m_blendModeCombo->setCurrentIndex(index);
        }
    }
}

int RasterEditorWindow::indexForBlendMode(QPainter::CompositionMode mode) const
{
    for (int i = 0; i < static_cast<int>(std::size(kBlendModes)); ++i) {
        if (kBlendModes[i].mode == mode) {
            return i;
        }
    }
    return 0;
}

