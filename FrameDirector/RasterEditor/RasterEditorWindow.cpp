#include "RasterEditorWindow.h"

#include "RasterCanvasWidget.h"
#include "RasterDocument.h"
#include "RasterOnionSkinProvider.h"
#include "RasterORAImporter.h"
#include "ORAExporter.h"
#include "RasterTools.h"

#include "../MainWindow.h"
#include "../Canvas.h"
#include "../Timeline.h"
#include "../Panels/LayerManager.h"
#include "../Commands/UndoCommands.h"
#include "../Common/GraphicsItemRoles.h"

#include <QAbstractItemView>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QFrame>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QSplitter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QVariant>
#include <QGraphicsPixmapItem>
#include <QGraphicsItem>
#include <QUndoStack>
#include <QJsonDocument>
#include <QUuid>
#include <QStyle>
#include <QRgb>
#include <QtGlobal>
#include <QPalette>
#include <iterator>
#include <algorithm>

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
    , m_toolButtonGroup(nullptr)
    , m_brushButton(nullptr)
    , m_eraserButton(nullptr)
    , m_fillButton(nullptr)
    , m_brushSizeSlider(nullptr)
    , m_brushSizeValue(nullptr)
    , m_colorButton(nullptr)
    , m_onionSkinCheck(nullptr)
    , m_projectOnionCheck(nullptr)
    , m_onionBeforeSpin(nullptr)
    , m_onionAfterSpin(nullptr)
    , m_addLayerButton(nullptr)
    , m_removeLayerButton(nullptr)
    , m_opacitySpin(nullptr)
    , m_blendModeCombo(nullptr)
    , m_primaryColor(Qt::black)
    , m_mainWindow(nullptr)
    , m_canvas(nullptr)
    , m_timeline(nullptr)
    , m_layerManager(nullptr)
    , m_onionProvider(nullptr)
    , m_layerMismatchWarned(false)
    , m_projectContextInitialized(false)
    , m_sessionId(QUuid::createUuid().toString(QUuid::WithoutBraces))
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

    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    QFrame* headerFrame = new QFrame(container);
    headerFrame->setObjectName(QStringLiteral("rasterEditorHeader"));
    headerFrame->setFrameShape(QFrame::StyledPanel);
    headerFrame->setFrameShadow(QFrame::Raised);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(12, 8, 12, 8);
    headerLayout->setSpacing(12);

    m_toolButtonGroup = new QButtonGroup(this);
    auto createToolButton = [&](QToolButton*& button, const QString& text, QStyle::StandardPixmap icon, int id) {
        button = new QToolButton(headerFrame);
        button->setText(text);
        button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIcon(style()->standardIcon(icon));
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        m_toolButtonGroup->addButton(button, id);
        headerLayout->addWidget(button);
    };

    createToolButton(m_brushButton, tr("Brush"), QStyle::SP_DialogApplyButton, 0);
    createToolButton(m_eraserButton, tr("Eraser"), QStyle::SP_DialogResetButton, 1);
    createToolButton(m_fillButton, tr("Fill"), QStyle::SP_FileDialogNewFolder, 2);

    headerLayout->addSpacing(8);

    QWidget* sizeContainer = new QWidget(headerFrame);
    QVBoxLayout* sizeLayout = new QVBoxLayout(sizeContainer);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->setSpacing(4);
    QHBoxLayout* sizeLabelLayout = new QHBoxLayout();
    sizeLabelLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* sizeLabel = new QLabel(tr("Brush Size"), sizeContainer);
    m_brushSizeValue = new QLabel(QString::number(kDefaultBrushSize), sizeContainer);
    m_brushSizeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    sizeLabelLayout->addWidget(sizeLabel);
    sizeLabelLayout->addStretch(1);
    sizeLabelLayout->addWidget(m_brushSizeValue);
    m_brushSizeSlider = new QSlider(Qt::Horizontal, sizeContainer);
    m_brushSizeSlider->setRange(1, 256);
    m_brushSizeSlider->setValue(kDefaultBrushSize);
    connect(m_brushSizeSlider, &QSlider::valueChanged, this, &RasterEditorWindow::onBrushSizeChanged);
    sizeLayout->addLayout(sizeLabelLayout);
    sizeLayout->addWidget(m_brushSizeSlider);
    sizeContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    headerLayout->addWidget(sizeContainer, 1);

    m_colorButton = new QPushButton(tr("Primary Color"), headerFrame);
    m_colorButton->setMinimumWidth(140);
    connect(m_colorButton, &QPushButton::clicked, this, &RasterEditorWindow::onColorButtonClicked);
    headerLayout->addWidget(m_colorButton);

    headerLayout->addStretch(1);

    mainLayout->addWidget(headerFrame);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, container);
    splitter->setChildrenCollapsible(false);

    QFrame* toolPanel = new QFrame(splitter);
    toolPanel->setFrameShape(QFrame::StyledPanel);
    QVBoxLayout* toolLayout = new QVBoxLayout(toolPanel);
    toolLayout->setContentsMargins(12, 12, 12, 12);
    toolLayout->setSpacing(8);

    QLabel* onionTitle = new QLabel(tr("Onion Skin"), toolPanel);
    onionTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
    toolLayout->addWidget(onionTitle);

    m_onionSkinCheck = new QCheckBox(tr("Enable Onion Skin"), toolPanel);
    connect(m_onionSkinCheck, &QCheckBox::toggled, this, &RasterEditorWindow::onOnionSkinToggled);
    toolLayout->addWidget(m_onionSkinCheck);

    m_projectOnionCheck = new QCheckBox(tr("Use Project Layers"), toolPanel);
    m_projectOnionCheck->setToolTip(tr("Overlay project frames when onion skinning."));
    connect(m_projectOnionCheck, &QCheckBox::toggled, this, &RasterEditorWindow::onProjectOnionToggled);
    toolLayout->addWidget(m_projectOnionCheck);

    QGridLayout* onionGrid = new QGridLayout();
    onionGrid->setContentsMargins(0, 0, 0, 0);
    onionGrid->setHorizontalSpacing(8);
    onionGrid->setVerticalSpacing(4);
    QLabel* beforeLabel = new QLabel(tr("Frames Before"), toolPanel);
    m_onionBeforeSpin = new QSpinBox(toolPanel);
    m_onionBeforeSpin->setRange(0, 12);
    connect(m_onionBeforeSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RasterEditorWindow::onOnionBeforeChanged);
    onionGrid->addWidget(beforeLabel, 0, 0);
    onionGrid->addWidget(m_onionBeforeSpin, 0, 1);
    QLabel* afterLabel = new QLabel(tr("Frames After"), toolPanel);
    m_onionAfterSpin = new QSpinBox(toolPanel);
    m_onionAfterSpin->setRange(0, 12);
    connect(m_onionAfterSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RasterEditorWindow::onOnionAfterChanged);
    onionGrid->addWidget(afterLabel, 1, 0);
    onionGrid->addWidget(m_onionAfterSpin, 1, 1);
    toolLayout->addLayout(onionGrid);
    toolLayout->addStretch(1);

    QWidget* canvasPanel = new QWidget(splitter);
    QVBoxLayout* canvasLayout = new QVBoxLayout(canvasPanel);
    canvasLayout->setContentsMargins(0, 0, 0, 0);
    canvasLayout->setSpacing(8);
    m_canvasWidget = new RasterCanvasWidget(canvasPanel);
    m_canvasWidget->setDocument(m_document);
    m_canvasWidget->setActiveTool(m_brushTool);
    m_activeTool = m_brushTool;
    canvasLayout->addWidget(m_canvasWidget, 1);
    m_frameLabel = new QLabel(tr("Frame: 1"), canvasPanel);
    m_frameLabel->setAlignment(Qt::AlignCenter);
    canvasLayout->addWidget(m_frameLabel);

    QFrame* layerPanel = new QFrame(splitter);
    layerPanel->setFrameShape(QFrame::StyledPanel);
    QVBoxLayout* layerLayout = new QVBoxLayout(layerPanel);
    layerLayout->setContentsMargins(12, 12, 12, 12);
    layerLayout->setSpacing(8);

    QHBoxLayout* fileButtonsLayout = new QHBoxLayout();
    fileButtonsLayout->setContentsMargins(0, 0, 0, 0);
    fileButtonsLayout->setSpacing(8);
    QPushButton* openOraButton = new QPushButton(tr("Open ORA…"), layerPanel);
    connect(openOraButton, &QPushButton::clicked, this, &RasterEditorWindow::onOpenOra);
    QPushButton* saveOraButton = new QPushButton(tr("Save ORA…"), layerPanel);
    connect(saveOraButton, &QPushButton::clicked, this, &RasterEditorWindow::onSaveOra);
    QPushButton* exportButton = new QPushButton(tr("Export"), layerPanel);
    exportButton->setToolTip(tr("Export the current frame to the active timeline layer."));
    connect(exportButton, &QPushButton::clicked, this, &RasterEditorWindow::onExportToTimeline);
    fileButtonsLayout->addWidget(openOraButton);
    fileButtonsLayout->addWidget(saveOraButton);
    fileButtonsLayout->addWidget(exportButton);
    layerLayout->addLayout(fileButtonsLayout);

    m_layerList = new QListWidget(layerPanel);
    m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layerList->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    connect(m_layerList, &QListWidget::currentRowChanged, this, &RasterEditorWindow::onLayerSelectionChanged);
    connect(m_layerList, &QListWidget::itemChanged, this, &RasterEditorWindow::onLayerItemChanged);
    layerLayout->addWidget(m_layerList, 1);

    QHBoxLayout* layerButtonLayout = new QHBoxLayout();
    layerButtonLayout->setContentsMargins(0, 0, 0, 0);
    layerButtonLayout->setSpacing(8);
    m_addLayerButton = new QToolButton(layerPanel);
    m_addLayerButton->setText(tr("Add"));
    m_addLayerButton->setToolTip(tr("Add a new raster layer"));
    connect(m_addLayerButton, &QToolButton::clicked, this, &RasterEditorWindow::onAddLayer);
    m_removeLayerButton = new QToolButton(layerPanel);
    m_removeLayerButton->setText(tr("Remove"));
    m_removeLayerButton->setToolTip(tr("Remove the selected raster layer"));
    connect(m_removeLayerButton, &QToolButton::clicked, this, &RasterEditorWindow::onRemoveLayer);
    layerButtonLayout->addWidget(m_addLayerButton);
    layerButtonLayout->addWidget(m_removeLayerButton);
    layerLayout->addLayout(layerButtonLayout);

    QLabel* opacityLabel = new QLabel(tr("Layer Opacity"), layerPanel);
    m_opacitySpin = new QDoubleSpinBox(layerPanel);
    m_opacitySpin->setRange(0.0, 100.0);
    m_opacitySpin->setDecimals(1);
    m_opacitySpin->setSuffix(tr(" %"));
    connect(m_opacitySpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &RasterEditorWindow::onOpacityChanged);
    layerLayout->addWidget(opacityLabel);
    layerLayout->addWidget(m_opacitySpin);

    QLabel* blendLabel = new QLabel(tr("Blend Mode"), layerPanel);
    m_blendModeCombo = new QComboBox(layerPanel);
    for (const BlendModeOption& option : kBlendModes) {
        m_blendModeCombo->addItem(QObject::tr(option.label), static_cast<int>(option.mode));
    }
    connect(m_blendModeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &RasterEditorWindow::onBlendModeChanged);
    layerLayout->addWidget(blendLabel);
    layerLayout->addWidget(m_blendModeCombo);

    m_layerInfoLabel = new QLabel(layerPanel);
    m_layerInfoLabel->setWordWrap(true);
    layerLayout->addWidget(m_layerInfoLabel);

    splitter->addWidget(toolPanel);
    splitter->addWidget(canvasPanel);
    splitter->addWidget(layerPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);

    toolPanel->setMinimumWidth(220);
    layerPanel->setMinimumWidth(260);

    mainLayout->addWidget(splitter, 1);

    connect(m_toolButtonGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, &RasterEditorWindow::onToolChanged);
    if (QAbstractButton* brushButton = m_toolButtonGroup->button(0)) {
        brushButton->setChecked(true);
    }
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
    connect(m_document, &RasterDocument::onionSkinSettingsChanged, this, &RasterEditorWindow::updateOnionSkinControls);
}

void RasterEditorWindow::setCurrentFrame(int frame)
{
    if (!m_document) {
        return;
    }

    const int targetFrame = clampProjectFrame(frame);
    m_document->setActiveFrame(targetFrame);
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
    if (m_toolButtonGroup) {
        if (QAbstractButton* button = m_toolButtonGroup->button(index)) {
            QSignalBlocker blocker(m_toolButtonGroup);
            button->setChecked(true);
        }
    }

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
    refreshProjectMetadata();
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

void RasterEditorWindow::onExportToTimeline()
{
    if (!m_document || !m_canvas || !m_mainWindow) {
        return;
    }

    const int documentFrame = m_document->activeFrame();
    const int projectFrame = m_canvas->getCurrentFrame();
    const int layerIndex = m_canvas->getCurrentLayer();

    if (layerIndex < 0 || projectFrame < 1) {
        QMessageBox::warning(this, tr("Raster Editor"), tr("Select a valid layer and frame in the timeline before exporting."));
        return;
    }

    QImage flattened = m_document->flattenFrame(documentFrame);
    if (flattened.size().isEmpty()) {
        QMessageBox::information(this, tr("Raster Editor"), tr("There is no raster content to export for the current frame."));
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(flattened);
    if (pixmap.isNull()) {
        QMessageBox::warning(this, tr("Raster Editor"), tr("Failed to convert the raster document into a pixmap."));
        return;
    }

    auto pixmapItem = new QGraphicsPixmapItem(pixmap);
    pixmapItem->setTransformationMode(Qt::SmoothTransformation);
    pixmapItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
    pixmapItem->setFlag(QGraphicsItem::ItemIsMovable, true);
    pixmapItem->setData(0, 1.0);
    pixmapItem->setOpacity(1.0);

    const QByteArray documentState = serializeDocumentState();
    if (!documentState.isEmpty()) {
        pixmapItem->setData(GraphicsItemRoles::RasterDocumentJsonRole, documentState);
    }
    pixmapItem->setData(GraphicsItemRoles::RasterSessionIdRole, m_sessionId);
    pixmapItem->setData(GraphicsItemRoles::RasterFrameIndexRole, projectFrame);

    QList<QGraphicsItem*> existingItems = rasterItemsForFrame(layerIndex, projectFrame);
    if (!existingItems.isEmpty()) {
        if (auto previousPixmap = qgraphicsitem_cast<QGraphicsPixmapItem*>(existingItems.first())) {
            pixmapItem->setPos(previousPixmap->pos());
            pixmapItem->setTransform(previousPixmap->transform());
            pixmapItem->setOffset(previousPixmap->offset());
            pixmapItem->setZValue(previousPixmap->zValue());
            pixmapItem->setOpacity(previousPixmap->opacity());
            QVariant baseOpacity = previousPixmap->data(0);
            pixmapItem->setData(0, baseOpacity.isValid() ? baseOpacity : previousPixmap->opacity());
        }
        else {
            pixmapItem->setPos(existingItems.first()->pos());
            pixmapItem->setZValue(existingItems.first()->zValue());
        }
    }
    else {
        QRectF canvasRect = m_canvas->getCanvasRect();
        QRectF itemRect = pixmapItem->boundingRect();
        pixmapItem->setPos(canvasRect.center() - itemRect.center());
    }

    QUndoStack* undoStack = nullptr;
    if (m_mainWindow) {
        undoStack = m_mainWindow->getUndoStack();
    }
    if (!undoStack) {
        delete pixmapItem;
        return;
    }

    const QString macroText = tr("Export Raster Frame");
    undoStack->beginMacro(macroText);
    if (!existingItems.isEmpty()) {
        undoStack->push(new RemoveItemCommand(m_canvas, existingItems));
    }
    undoStack->push(new AddItemCommand(m_canvas, pixmapItem));
    undoStack->endMacro();
}

void RasterEditorWindow::setProjectContext(MainWindow* mainWindow, Canvas* canvas, Timeline* timeline, LayerManager* layerManager)
{
    m_mainWindow = mainWindow;
    m_canvas = canvas;
    m_timeline = timeline;
    m_layerManager = layerManager;

    if (!m_onionProvider && m_mainWindow) {
        m_onionProvider = new RasterOnionSkinProvider(m_mainWindow, this);
    }

    if (m_canvasWidget) {
        m_canvasWidget->setOnionSkinProvider(m_onionProvider);
    }

    if (m_document && m_onionProvider) {
        connect(m_document, &RasterDocument::documentReset, m_onionProvider, &RasterOnionSkinProvider::invalidate, Qt::UniqueConnection);
    }

    if (!m_projectContextInitialized) {
        if (m_canvas) {
            connect(m_canvas, &Canvas::layerAdded, this, &RasterEditorWindow::onProjectLayersChanged, Qt::UniqueConnection);
            connect(m_canvas, &Canvas::layerRemoved, this, &RasterEditorWindow::onProjectLayersChanged, Qt::UniqueConnection);
            connect(m_canvas, &Canvas::layerNameChanged, this, &RasterEditorWindow::onProjectLayerRenamed, Qt::UniqueConnection);
            connect(m_canvas, &Canvas::layerVisibilityChanged, this, &RasterEditorWindow::onProjectLayerAppearanceChanged, Qt::UniqueConnection);
            connect(m_canvas, &Canvas::layerOpacityChanged, this, &RasterEditorWindow::onProjectLayerAppearanceChanged, Qt::UniqueConnection);
            connect(m_canvas, &Canvas::keyframeCreated, this, &RasterEditorWindow::onProjectFrameStructureChanged, Qt::UniqueConnection);
            connect(m_canvas, &Canvas::frameExtended, this, &RasterEditorWindow::onProjectFrameStructureChanged, Qt::UniqueConnection);
        }

        if (m_timeline) {
            connect(m_timeline, &Timeline::totalFramesChanged, this, &RasterEditorWindow::onTimelineLengthChanged, Qt::UniqueConnection);
            connect(m_timeline, &Timeline::keyframeAdded, this, &RasterEditorWindow::onProjectFrameStructureChanged, Qt::UniqueConnection);
            connect(m_timeline, &Timeline::keyframeRemoved, this, &RasterEditorWindow::onProjectFrameStructureChanged, Qt::UniqueConnection);
            connect(m_timeline, &Timeline::frameExtended, this, &RasterEditorWindow::onProjectFrameStructureChanged, Qt::UniqueConnection);
            connect(m_timeline, &Timeline::frameChanged, this, &RasterEditorWindow::onTimelineFrameChanged, Qt::UniqueConnection);
        }

        m_projectContextInitialized = true;
    }

    if (m_document) {
        if (m_canvas) {
            m_document->setCanvasSize(m_canvas->getCanvasSize());
        }
        if (m_timeline) {
            m_document->setFrameCount(m_timeline->getTotalFrames());
            setCurrentFrame(m_timeline->getCurrentFrame());
        }
    }

    syncProjectLayers();
    updateOnionSkinControls();
    refreshProjectMetadata();
}

void RasterEditorWindow::onProjectOnionToggled(bool enabled)
{
    if (!m_document) {
        return;
    }

    m_document->setUseProjectOnionSkin(enabled);
    updateOnionSkinControls();
}

void RasterEditorWindow::onProjectLayersChanged()
{
    syncProjectLayers();
    if (m_onionProvider) {
        m_onionProvider->invalidate();
    }
    refreshProjectMetadata();
}

void RasterEditorWindow::onProjectLayerRenamed(int index, const QString& name)
{
    Q_UNUSED(name);
    Q_UNUSED(index);
    syncProjectLayers();
    if (m_onionProvider) {
        m_onionProvider->invalidate();
    }
    refreshProjectMetadata();
}

void RasterEditorWindow::onProjectLayerAppearanceChanged()
{
    if (m_onionProvider) {
        m_onionProvider->invalidate();
    }
}

void RasterEditorWindow::onProjectFrameStructureChanged()
{
    if (m_onionProvider) {
        m_onionProvider->invalidate();
    }
    ensureDocumentFrameBounds();
    refreshProjectMetadata();
}

void RasterEditorWindow::onTimelineLengthChanged(int frames)
{
    if (!m_document) {
        return;
    }

    m_document->setFrameCount(frames);
    ensureDocumentFrameBounds();
    if (m_onionProvider) {
        m_onionProvider->invalidate();
    }
    refreshProjectMetadata();
}

void RasterEditorWindow::onTimelineFrameChanged(int frame)
{
    setCurrentFrame(frame);
}

void RasterEditorWindow::syncProjectLayers()
{
    m_projectLayerNames.clear();
    if (m_canvas) {
        for (int i = 0; i < m_canvas->getLayerCount(); ++i) {
            m_projectLayerNames.append(m_canvas->getLayerName(i));
        }
    }

    if (m_onionProvider) {
        QVector<int> layers;
        layers.reserve(m_projectLayerNames.size());
        for (int i = 0; i < m_projectLayerNames.size(); ++i) {
            layers.append(i);
        }
        m_onionProvider->setLayerFilter(layers);
    }
}

QList<QGraphicsItem*> RasterEditorWindow::rasterItemsForFrame(int layerIndex, int frame) const
{
    if (!m_canvas || layerIndex < 0 || frame < 1) {
        return QList<QGraphicsItem*>();
    }

    QList<QGraphicsItem*> items = m_canvas->getLayerFrameItems(layerIndex, frame);
    QList<QGraphicsItem*> matches;
    matches.reserve(items.size());
    for (QGraphicsItem* item : items) {
        if (!item) {
            continue;
        }

        if (item->data(GraphicsItemRoles::RasterSessionIdRole).toString() == m_sessionId
            && item->data(GraphicsItemRoles::RasterFrameIndexRole).toInt() == frame) {
            matches.append(item);
        }
    }

    return matches;
}

QByteArray RasterEditorWindow::serializeDocumentState() const
{
    if (!m_document) {
        return QByteArray();
    }

    QJsonDocument doc(m_document->toJson());
    return doc.toJson(QJsonDocument::Compact);
}

void RasterEditorWindow::refreshProjectMetadata()
{
    updateLayerInfo();

    if (!m_document) {
        return;
    }

    if (!m_projectOnionCheck) {
        return;
    }

    m_projectOnionCheck->setEnabled(m_document->onionSkinEnabled() && m_onionProvider);

    const bool mismatch = m_document->layerCount() != m_projectLayerNames.size();
    if (!mismatch) {
        m_layerMismatchWarned = false;
        return;
    }

    auto frameHasContent = [](const QImage& image) {
        if (image.isNull()) {
            return false;
        }

        QImage converted = image;
        if (converted.format() != QImage::Format_ARGB32_Premultiplied) {
            converted = converted.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }

        for (int y = 0; y < converted.height(); ++y) {
            const QRgb* row = reinterpret_cast<const QRgb*>(converted.constScanLine(y));
            for (int x = 0; x < converted.width(); ++x) {
                if (qAlpha(row[x]) > 0) {
                    return true;
                }
            }
        }
        return false;
    };

    const QVector<RasterLayerDescriptor> descriptors = m_document->layerDescriptors();
    const bool documentHasContent = std::any_of(descriptors.begin(), descriptors.end(), [&](const RasterLayerDescriptor& descriptor) {
        return std::any_of(descriptor.frames.begin(), descriptor.frames.end(), frameHasContent);
    });

    if (!documentHasContent) {
        m_layerMismatchWarned = false;
        return;
    }

    if (!m_layerMismatchWarned) {
        QMessageBox::warning(this, tr("Raster Editor"),
                             tr("Project layers changed since the raster document was prepared. Please review layer assignments."));
        m_layerMismatchWarned = true;
    }
}

void RasterEditorWindow::ensureDocumentFrameBounds()
{
    if (!m_document) {
        return;
    }

    const int frameCount = m_document->frameCount();
    if (frameCount <= 0) {
        return;
    }

    const int active = m_document->activeFrame();
    if (active >= frameCount) {
        m_document->setActiveFrame(frameCount - 1);
    }
}

int RasterEditorWindow::clampProjectFrame(int frame) const
{
    if (!m_document) {
        return 0;
    }

    int clamped = frame - 1;
    if (clamped < 0) {
        clamped = 0;
    }
    const int frameCount = m_document->frameCount();
    if (frameCount > 0 && clamped >= frameCount) {
        clamped = frameCount - 1;
    }
    return clamped;
}

QJsonObject RasterEditorWindow::toJson() const
{
    QJsonObject json;
    json[QStringLiteral("sessionId")] = m_sessionId;
    if (m_document) {
        json[QStringLiteral("document")] = m_document->toJson();
    }
    return json;
}

void RasterEditorWindow::loadFromJson(const QJsonObject& json)
{
    if (json.isEmpty() || !m_document) {
        return;
    }

    const QString sessionId = json.value(QStringLiteral("sessionId")).toString();
    if (!sessionId.isEmpty()) {
        m_sessionId = sessionId;
    }

    const QJsonObject documentObject = json.value(QStringLiteral("document")).toObject();
    if (!documentObject.isEmpty()) {
        m_document->fromJson(documentObject);
    }

    m_layerMismatchWarned = false;

    refreshLayerList();
    updateLayerPropertiesUi();
    updateLayerInfo();
    updateOnionSkinControls();
    updateToolControls();
    updateColorButton();
    ensureDocumentFrameBounds();
    refreshProjectMetadata();
}

void RasterEditorWindow::resetDocument()
{
    if (!m_document) {
        return;
    }

    m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_document->loadFromDescriptors(m_document->canvasSize(), QVector<RasterLayerDescriptor>(), 1);
    m_layerMismatchWarned = false;

    if (m_canvas) {
        m_document->setCanvasSize(m_canvas->getCanvasSize());
    }
    if (m_timeline) {
        m_document->setFrameCount(m_timeline->getTotalFrames());
        setCurrentFrame(m_timeline->getCurrentFrame());
    }

    refreshLayerList();
    updateLayerPropertiesUi();
    updateLayerInfo();
    updateOnionSkinControls();
    updateToolControls();
    updateColorButton();
    ensureDocumentFrameBounds();
    refreshProjectMetadata();
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
        m_layerInfoLabel->setStyleSheet(QString());
        return;
    }

    const RasterLayer& layerData = m_document->layerAt(layer);
    QString text = tr("Selected layer: %1").arg(layerData.name());
    QString projectName;
    if (layer >= 0 && layer < m_projectLayerNames.size()) {
        projectName = m_projectLayerNames.at(layer);
    }
    if (!projectName.isEmpty()) {
        text += tr(" (Project: %1)").arg(projectName);
    }
    bool nameMismatch = false;
    if (!projectName.isEmpty() && projectName != layerData.name()) {
        text += QStringLiteral(" ") + QString::fromUtf8("\xE2\x9A\xA0");
        nameMismatch = true;
    }
    m_layerInfoLabel->setText(text);
    if (nameMismatch) {
        m_layerInfoLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;").arg(palette().color(QPalette::Link).name()));
    } else {
        m_layerInfoLabel->setStyleSheet(QString());
    }
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

    const QColor textColor = (qGray(m_primaryColor.rgb()) < 128) ? Qt::white : Qt::black;
    const QString style = QStringLiteral("QPushButton { background-color: %1; color: %2; border: 1px solid palette(mid); padding: 6px 12px; }")
                              .arg(m_primaryColor.name(QColor::HexArgb))
                              .arg(textColor.name());
    m_colorButton->setStyleSheet(style);
    m_colorButton->setToolTip(tr("Current brush color: %1").arg(m_primaryColor.name(QColor::HexRgb).toUpper()));
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
    if (m_projectOnionCheck) {
        QSignalBlocker blocker(m_projectOnionCheck);
        m_projectOnionCheck->setChecked(m_document->useProjectOnionSkin());
        m_projectOnionCheck->setEnabled(enabled && m_onionProvider);
    }
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

