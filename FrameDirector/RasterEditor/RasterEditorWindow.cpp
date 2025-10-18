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

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDirIterator>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QUndoStack>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>
#include <QUuid>
#include <QStyle>
#include <QRgb>
#include <QRegularExpression>
#include <QtGlobal>
#include <QPalette>
#include <iterator>
#include <algorithm>
#include <cmath>

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
    constexpr float kDefaultBrushOpacity = 1.0f;
    constexpr float kDefaultBrushHardness = 1.0f;
    constexpr float kDefaultBrushSpacing = 0.25f;

    QString formatBrushName(const QString& baseName)
    {
        QString cleaned = baseName;
        cleaned.replace(QRegularExpression(QStringLiteral("[_-]+")), QStringLiteral(" "));
        QStringList parts = cleaned.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        for (QString& part : parts) {
            if (!part.isEmpty()) {
                part[0] = part[0].toUpper();
                for (int i = 1; i < part.size(); ++i) {
                    part[i] = part[i].toLower();
                }
            }
        }
        if (parts.isEmpty()) {
            return baseName;
        }
        return parts.join(QLatin1Char(' '));
    }

    double readBrushSetting(const QJsonObject& settings, const QString& key, double fallback)
    {
        const QJsonValue value = settings.value(key);
        if (!value.isObject()) {
            return fallback;
        }
        const QJsonValue baseValue = value.toObject().value(QStringLiteral("base_value"));
        if (!baseValue.isDouble()) {
            return fallback;
        }
        return baseValue.toDouble();
    }
}

RasterEditorWindow::RasterEditorWindow(QWidget* parent)
    : QMainWindow(parent, Qt::Window)
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
    setWindowTitle(tr("Raster Editor"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose, false);

    m_brushTool->setSize(kDefaultBrushSize);
    m_brushTool->setColor(m_primaryColor);
    m_eraserTool->setSize(kDefaultBrushSize);
    m_brushTool->setOpacity(1.0f);
    m_brushTool->setHardness(1.0f);
    m_brushTool->setSpacing(0.25f);
    m_eraserTool->setOpacity(1.0f);
    m_eraserTool->setHardness(1.0f);
    m_eraserTool->setSpacing(0.25f);
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
    setCentralWidget(container);
    container->setStyleSheet(
        "QWidget { background-color: #2D2D30; color: #FFFFFF; }"
        "QLabel { color: #FFFFFF; }"
        "QCheckBox, QSpinBox, QDoubleSpinBox { color: #FFFFFF; }"
        "QComboBox { background-color: #3E3E42; color: #FFFFFF; border: 1px solid #555; }"
        "QListWidget { background-color: #252526; border: 1px solid #3E3E42; }"
        "QPushButton { background-color: #0E639C; color: #FFFFFF; border: none; padding: 6px 12px; border-radius: 3px; }"
        "QPushButton:hover { background-color: #1177BB; }"
        "QSlider::groove:horizontal { background: #3E3E42; border: 1px solid #555; height: 4px; }"
        "QSlider::handle:horizontal { background: #007ACC; width: 14px; margin: -5px 0; border-radius: 7px; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // === HEADER: Canvas + Tools ===
    QFrame* headerFrame = new QFrame(container);
    headerFrame->setObjectName(QStringLiteral("rasterEditorHeader"));
    headerFrame->setStyleSheet(
        "QFrame#rasterEditorHeader { background-color: #3E3E42; border-bottom: 1px solid #555; }"
    );
    QVBoxLayout* headerLayout = new QVBoxLayout(headerFrame);
    headerLayout->setContentsMargins(12, 12, 12, 12);
    headerLayout->setSpacing(12);

    // Tool buttons with better styling
    m_toolButtonGroup = new QButtonGroup(this);
    QHBoxLayout* toolButtonLayout = new QHBoxLayout();
    toolButtonLayout->setContentsMargins(0, 0, 0, 0);
    toolButtonLayout->setSpacing(4);

    auto createToolButton = [&](QToolButton*& button, const QString& text, QStyle::StandardPixmap icon, int id) {
        button = new QToolButton(headerFrame);
        button->setText(text);
        button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIcon(style()->standardIcon(icon));
        button->setIconSize(QSize(20, 20));
        button->setMinimumHeight(36);
        button->setStyleSheet(
            "QToolButton { background-color: #3E3E42; border: 2px solid transparent; border-radius: 4px; padding: 6px 12px; }"
            "QToolButton:checked { background-color: #007ACC; border: 2px solid #0E639C; }"
            "QToolButton:hover { background-color: #4A4A4F; }"
        );
        m_toolButtonGroup->addButton(button, id);
        toolButtonLayout->addWidget(button);
        };

    createToolButton(m_brushButton, tr("Brush"), QStyle::SP_DialogApplyButton, 0);
    createToolButton(m_eraserButton, tr("Eraser"), QStyle::SP_DialogResetButton, 1);
    createToolButton(m_fillButton, tr("Fill"), QStyle::SP_FileDialogNewFolder, 2);

    // Brush size control with live preview
    QFrame* brushSizeFrame = new QFrame(headerFrame);
    QVBoxLayout* brushSizeLayout = new QVBoxLayout(brushSizeFrame);
    brushSizeLayout->setContentsMargins(0, 0, 0, 0);
    brushSizeLayout->setSpacing(4);
    QHBoxLayout* sizeLabelLayout = new QHBoxLayout();
    sizeLabelLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* sizeLabel = new QLabel(tr("Size"), brushSizeFrame);
    sizeLabel->setStyleSheet("font-weight: 600; font-size: 11px;");
    m_brushSizeValue = new QLabel(QString::number(kDefaultBrushSize), brushSizeFrame);
    m_brushSizeValue->setAlignment(Qt::AlignCenter);
    m_brushSizeValue->setMinimumWidth(30);
    m_brushSizeValue->setStyleSheet("background-color: #252526; border-radius: 3px; padding: 2px 6px; font-weight: 600;");
    sizeLabelLayout->addWidget(sizeLabel);
    sizeLabelLayout->addStretch(1);
    sizeLabelLayout->addWidget(m_brushSizeValue);
    m_brushSizeSlider = new QSlider(Qt::Horizontal, brushSizeFrame);
    m_brushSizeSlider->setRange(1, 256);
    m_brushSizeSlider->setValue(kDefaultBrushSize);
    connect(m_brushSizeSlider, &QSlider::valueChanged, this, &RasterEditorWindow::onBrushSizeChanged);
    brushSizeLayout->addLayout(sizeLabelLayout);
    brushSizeLayout->addWidget(m_brushSizeSlider);
    brushSizeFrame->setMinimumWidth(140);

    // Color button with live preview
    m_colorButton = new QPushButton(tr("Color"), headerFrame);
    m_colorButton->setMinimumHeight(36);
    m_colorButton->setMinimumWidth(100);
    connect(m_colorButton, &QPushButton::clicked, this, &RasterEditorWindow::onColorButtonClicked);

    toolButtonLayout->addSpacing(8);
    toolButtonLayout->addWidget(brushSizeFrame, 1);
    toolButtonLayout->addWidget(m_colorButton);
    headerLayout->addLayout(toolButtonLayout);

    mainLayout->addWidget(headerFrame);

    // === MAIN CONTENT: Canvas + Side Panels ===
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, container);
    mainSplitter->setChildrenCollapsible(false);
    mainSplitter->setStyleSheet("QSplitter::handle { background-color: #3E3E42; }");

    // Left panel: Brush parameters
    QFrame* leftPanel = new QFrame(mainSplitter);
    leftPanel->setStyleSheet("QFrame { background-color: #2D2D30; border-right: 1px solid #3E3E42; }");
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    leftLayout->setSpacing(12);

    QLabel* brushParamsTitle = new QLabel(tr("Brush Parameters"), leftPanel);
    brushParamsTitle->setStyleSheet("font-weight: 700; font-size: 12px; color: #00D4FF;");
    leftLayout->addWidget(brushParamsTitle);

    // Brush parameters using a form layout
    QFormLayout* brushParamsForm = new QFormLayout();
    brushParamsForm->setContentsMargins(0, 0, 0, 0);
    brushParamsForm->setSpacing(8);
    brushParamsForm->setLabelAlignment(Qt::AlignRight);

    m_opacitySlider = new QSlider(Qt::Horizontal, leftPanel);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(100);
    m_opacityValue = new QLabel("100%", leftPanel);
    m_opacityValue->setMinimumWidth(35);
    QHBoxLayout* opacityLayout = new QHBoxLayout();
    opacityLayout->addWidget(m_opacitySlider, 1);
    opacityLayout->addWidget(m_opacityValue);
    brushParamsForm->addRow(tr("Opacity:"), opacityLayout);
    connect(m_opacitySlider, &QSlider::valueChanged, this, &RasterEditorWindow::onBrushOpacityChanged);

    m_hardnessSlider = new QSlider(Qt::Horizontal, leftPanel);
    m_hardnessSlider->setRange(0, 100);
    m_hardnessSlider->setValue(100);
    m_hardnessValue = new QLabel("100%", leftPanel);
    m_hardnessValue->setMinimumWidth(35);
    QHBoxLayout* hardnessLayout = new QHBoxLayout();
    hardnessLayout->addWidget(m_hardnessSlider, 1);
    hardnessLayout->addWidget(m_hardnessValue);
    brushParamsForm->addRow(tr("Hardness:"), hardnessLayout);
    connect(m_hardnessSlider, &QSlider::valueChanged, this, &RasterEditorWindow::onBrushHardnessChanged);

    m_spacingSlider = new QSlider(Qt::Horizontal, leftPanel);
    m_spacingSlider->setRange(1, 200);
    m_spacingSlider->setValue(25);
    m_spacingValue = new QLabel("25%", leftPanel);
    m_spacingValue->setMinimumWidth(35);
    QHBoxLayout* spacingLayout = new QHBoxLayout();
    spacingLayout->addWidget(m_spacingSlider, 1);
    spacingLayout->addWidget(m_spacingValue);
    brushParamsForm->addRow(tr("Spacing:"), spacingLayout);
    connect(m_spacingSlider, &QSlider::valueChanged, this, &RasterEditorWindow::onBrushSpacingChanged);

    leftLayout->addLayout(brushParamsForm);

    QLabel* onionSkinTitle = new QLabel(tr("Onion Skin"), leftPanel);
    onionSkinTitle->setStyleSheet("font-weight: 700; font-size: 12px; color: #00D4FF; margin-top: 8px;");
    leftLayout->addWidget(onionSkinTitle);

    m_onionSkinCheck = new QCheckBox(tr("Enable Onion Skin"), leftPanel);
    m_onionSkinCheck->setStyleSheet("QCheckBox { padding: 4px; }");
    connect(m_onionSkinCheck, &QCheckBox::toggled, this, &RasterEditorWindow::onOnionSkinToggled);
    leftLayout->addWidget(m_onionSkinCheck);

    m_projectOnionCheck = new QCheckBox(tr("Project Layers"), leftPanel);
    m_projectOnionCheck->setStyleSheet("QCheckBox { padding: 4px; }");
    m_projectOnionCheck->setToolTip(tr("Overlay project frames when onion skinning."));
    connect(m_projectOnionCheck, &QCheckBox::toggled, this, &RasterEditorWindow::onProjectOnionToggled);
    leftLayout->addWidget(m_projectOnionCheck);

    QFrame* brushPresetFrame = new QFrame(leftPanel);
    QVBoxLayout* brushPresetLayout = new QVBoxLayout(brushPresetFrame);
    brushPresetLayout->setContentsMargins(0, 0, 0, 0);
    brushPresetLayout->setSpacing(4);
    QLabel* brushLabel = new QLabel(tr("Brush Preset"), brushPresetFrame);
    brushLabel->setStyleSheet("font-weight: 600; font-size: 11px;");
    m_brushSelector = new QComboBox(brushPresetFrame);
    m_brushSelector->setMinimumHeight(28);
    m_brushSelector->setStyleSheet(
        "QComboBox { padding: 4px 8px; border-radius: 3px; }"
        "QComboBox::drop-down { border: none; }"
    );
    connect(m_brushSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &RasterEditorWindow::onBrushSelected);
    m_statusLabel = new QLabel(brushPresetFrame);
    m_statusLabel->setStyleSheet("color: #C8C8C8; font-size: 11px;");
    m_statusLabel->setText(tr("Brush: %1").arg(tr("Standard")));
    brushPresetLayout->addWidget(brushLabel);
    brushPresetLayout->addWidget(m_brushSelector);
    brushPresetLayout->addWidget(m_statusLabel);
    leftLayout->addWidget(brushPresetFrame);

    QFormLayout* onionForm = new QFormLayout();
    onionForm->setContentsMargins(0, 0, 0, 0);
    onionForm->setSpacing(6);
    m_onionBeforeSpin = new QSpinBox(leftPanel);
    m_onionBeforeSpin->setRange(0, 12);
    m_onionBeforeSpin->setStyleSheet("QSpinBox { padding: 4px; }");
    connect(m_onionBeforeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &RasterEditorWindow::onOnionBeforeChanged);
    onionForm->addRow(tr("Before:"), m_onionBeforeSpin);

    m_onionAfterSpin = new QSpinBox(leftPanel);
    m_onionAfterSpin->setRange(0, 12);
    m_onionAfterSpin->setStyleSheet("QSpinBox { padding: 4px; }");
    connect(m_onionAfterSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &RasterEditorWindow::onOnionAfterChanged);
    onionForm->addRow(tr("After:"), m_onionAfterSpin);
    leftLayout->addLayout(onionForm);

    leftLayout->addStretch(1);
    leftPanel->setMinimumWidth(200);

    // Center: Canvas
    QWidget* canvasPanel = new QWidget(mainSplitter);
    QVBoxLayout* canvasLayout = new QVBoxLayout(canvasPanel);
    canvasLayout->setContentsMargins(0, 0, 0, 0);
    canvasLayout->setSpacing(0);
    m_canvasWidget = new RasterCanvasWidget(canvasPanel);
    m_canvasWidget->setDocument(m_document);
    m_canvasWidget->setActiveTool(m_brushTool);
    m_activeTool = m_brushTool;
    canvasLayout->addWidget(m_canvasWidget, 1);

    m_frameLabel = new QLabel(tr("Frame: 1"), canvasPanel);
    m_frameLabel->setAlignment(Qt::AlignCenter);
    m_frameLabel->setStyleSheet("background-color: #3E3E42; padding: 8px; font-weight: 600; border-top: 1px solid #555;");
    canvasLayout->addWidget(m_frameLabel);

    // Right panel: Layers and file operations
    QFrame* rightPanel = new QFrame(mainSplitter);
    rightPanel->setStyleSheet("QFrame { background-color: #2D2D30; border-left: 1px solid #3E3E42; }");
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(12, 12, 12, 12);
    rightLayout->setSpacing(12);

    QLabel* fileOpsTitle = new QLabel(tr("File Operations"), rightPanel);
    fileOpsTitle->setStyleSheet("font-weight: 700; font-size: 12px; color: #00D4FF;");
    rightLayout->addWidget(fileOpsTitle);

    QHBoxLayout* fileButtonsLayout = new QHBoxLayout();
    fileButtonsLayout->setContentsMargins(0, 0, 0, 0);
    fileButtonsLayout->setSpacing(6);
    QPushButton* openOraButton = new QPushButton(tr("Open ORA"), rightPanel);
    openOraButton->setMinimumHeight(28);
    openOraButton->setStyleSheet("QPushButton { font-size: 11px; }");
    connect(openOraButton, &QPushButton::clicked, this, &RasterEditorWindow::onOpenOra);
    QPushButton* saveOraButton = new QPushButton(tr("Save ORA"), rightPanel);
    saveOraButton->setMinimumHeight(28);
    saveOraButton->setStyleSheet("QPushButton { font-size: 11px; }");
    connect(saveOraButton, &QPushButton::clicked, this, &RasterEditorWindow::onSaveOra);
    QPushButton* exportButton = new QPushButton(tr("Export"), rightPanel);
    exportButton->setMinimumHeight(28);
    exportButton->setStyleSheet("QPushButton { font-size: 11px; }");
    exportButton->setToolTip(tr("Export the current frame to the active timeline layer."));
    connect(exportButton, &QPushButton::clicked, this, &RasterEditorWindow::onExportToTimeline);
    fileButtonsLayout->addWidget(openOraButton);
    fileButtonsLayout->addWidget(saveOraButton);
    fileButtonsLayout->addWidget(exportButton);
    rightLayout->addLayout(fileButtonsLayout);

    QLabel* layersTitle = new QLabel(tr("Layers"), rightPanel);
    layersTitle->setStyleSheet("font-weight: 700; font-size: 12px; color: #00D4FF;");
    rightLayout->addWidget(layersTitle);

    m_layerList = new QListWidget(rightPanel);
    m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layerList->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_layerList->setMinimumHeight(150);
    m_layerList->setStyleSheet(
        "QListWidget { background-color: #252526; border: 1px solid #3E3E42; border-radius: 3px; }"
        "QListWidget::item { padding: 4px; }"
        "QListWidget::item:selected { background-color: #007ACC; }"
    );
    connect(m_layerList, &QListWidget::currentRowChanged, this, &RasterEditorWindow::onLayerSelectionChanged);
    connect(m_layerList, &QListWidget::itemChanged, this, &RasterEditorWindow::onLayerItemChanged);
    rightLayout->addWidget(m_layerList, 1);

    QHBoxLayout* layerButtonLayout = new QHBoxLayout();
    layerButtonLayout->setContentsMargins(0, 0, 0, 0);
    layerButtonLayout->setSpacing(6);
    m_addLayerButton = new QToolButton(rightPanel);
    m_addLayerButton->setText(tr("Add"));
    m_addLayerButton->setMinimumHeight(28);
    m_addLayerButton->setToolTip(tr("Add a new raster layer"));
    connect(m_addLayerButton, &QToolButton::clicked, this, &RasterEditorWindow::onAddLayer);
    m_removeLayerButton = new QToolButton(rightPanel);
    m_removeLayerButton->setText(tr("Remove"));
    m_removeLayerButton->setMinimumHeight(28);
    m_removeLayerButton->setToolTip(tr("Remove the selected raster layer"));
    connect(m_removeLayerButton, &QToolButton::clicked, this, &RasterEditorWindow::onRemoveLayer);
    layerButtonLayout->addWidget(m_addLayerButton);
    layerButtonLayout->addWidget(m_removeLayerButton);
    rightLayout->addLayout(layerButtonLayout);

    QFormLayout* layerPropsForm = new QFormLayout();
    layerPropsForm->setContentsMargins(0, 0, 0, 0);
    layerPropsForm->setSpacing(8);

    QLabel* opacityLabel = new QLabel(tr("Opacity:"), rightPanel);
    m_opacitySpin = new QDoubleSpinBox(rightPanel);
    m_opacitySpin->setRange(0.0, 100.0);
    m_opacitySpin->setDecimals(1);
    m_opacitySpin->setSuffix(tr(" %"));
    m_opacitySpin->setStyleSheet("QDoubleSpinBox { padding: 4px; }");
    connect(m_opacitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RasterEditorWindow::onOpacityChanged);
    layerPropsForm->addRow(opacityLabel, m_opacitySpin);

    QLabel* blendLabel = new QLabel(tr("Blend:"), rightPanel);
    m_blendModeCombo = new QComboBox(rightPanel);
    m_blendModeCombo->setStyleSheet("QComboBox { padding: 4px; }");
    for (const BlendModeOption& option : kBlendModes) {
        m_blendModeCombo->addItem(QObject::tr(option.label), static_cast<int>(option.mode));
    }
    connect(m_blendModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RasterEditorWindow::onBlendModeChanged);
    layerPropsForm->addRow(blendLabel, m_blendModeCombo);

    rightLayout->addLayout(layerPropsForm);

    m_layerInfoLabel = new QLabel(rightPanel);
    m_layerInfoLabel->setWordWrap(true);
    m_layerInfoLabel->setStyleSheet("color: #999; font-size: 10px; padding: 4px; background-color: #252526; border-radius: 3px;");
    rightLayout->addWidget(m_layerInfoLabel);

    // Add panels to splitter
    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(canvasPanel);
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setStretchFactor(2, 0);
    mainSplitter->setSizes({ 200, 400, 250 });

    mainLayout->addWidget(mainSplitter, 1);

    // Connect tool selection
    connect(m_toolButtonGroup, QOverload<int>::of(&QButtonGroup::idClicked),
        this, &RasterEditorWindow::onToolChanged);
    if (QAbstractButton* brushButton = m_toolButtonGroup->button(0)) {
        brushButton->setChecked(true);
    }

    // Load brushes
    loadAvailableBrushes();
}

void RasterEditorWindow::loadAvailableBrushes()
{
    if (!m_brushSelector) {
        return;
    }

    m_brushPresets.clear();
    m_brushSelector->clear();

    QVector<BrushPreset> loadedPresets;
    QDirIterator it(QStringLiteral(":/brushes"), QStringList() << QStringLiteral("*.myb"), QDir::Files, QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        const QString resourcePath = it.next();
        QFile file(resourcePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Failed to open brush definition" << resourcePath;
            continue;
        }

        const QByteArray data = file.readAll();
        file.close();

        BrushPreset preset;
        preset.brushResource = resourcePath;
        preset.settings.clear();

        const QFileInfo info(resourcePath);
        preset.name = formatBrushName(info.baseName());
        if (preset.name.isEmpty()) {
            preset.name = info.fileName();
        }

        preset.size = kDefaultBrushSize;
        preset.opacity = kDefaultBrushOpacity;
        preset.hardness = kDefaultBrushHardness;
        preset.spacing = kDefaultBrushSpacing;

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            const QJsonObject settings = document.object().value(QStringLiteral("settings")).toObject();

            const double radiusLog = readBrushSetting(settings, QStringLiteral("radius_logarithmic"), std::log(static_cast<double>(kDefaultBrushSize)));
            const double resolvedRadius = std::exp(radiusLog);
            if (resolvedRadius > 0.0) {
                preset.size = static_cast<qreal>(qBound(1.0, resolvedRadius, 200.0));
            }

            const double opacityValue = readBrushSetting(settings, QStringLiteral("opaque"), kDefaultBrushOpacity);
            preset.opacity = qBound(0.0f, static_cast<float>(opacityValue), 1.0f);

            const double hardnessValue = readBrushSetting(settings, QStringLiteral("hardness"), kDefaultBrushHardness);
            preset.hardness = qBound(0.0f, static_cast<float>(hardnessValue), 1.0f);

            const double defaultDabs = 1.0 / std::max(static_cast<double>(kDefaultBrushSpacing), 0.01);
            const double dabsValue = readBrushSetting(settings, QStringLiteral("dabs_per_actual_radius"), defaultDabs);
            if (dabsValue > 0.0) {
                const float spacing = static_cast<float>(1.0 / dabsValue);
                preset.spacing = qBound(0.01f, spacing, 2.0f);
            }
        } else {
            qWarning() << "Failed to parse brush" << resourcePath << parseError.errorString();
        }

        loadedPresets.push_back(preset);
    }

    std::sort(loadedPresets.begin(), loadedPresets.end(), [](const BrushPreset& a, const BrushPreset& b) {
        return a.name.toLower() < b.name.toLower();
    });

    if (loadedPresets.isEmpty()) {
        BrushPreset fallback;
        fallback.name = tr("Standard Round");
        fallback.size = kDefaultBrushSize;
        fallback.opacity = kDefaultBrushOpacity;
        fallback.hardness = kDefaultBrushHardness;
        fallback.spacing = kDefaultBrushSpacing;
        fallback.brushResource.clear();
        fallback.settings.clear();
        loadedPresets.push_back(fallback);
    }

    m_brushPresets = loadedPresets;

    for (const BrushPreset& preset : m_brushPresets) {
        m_brushSelector->addItem(preset.name);
    }

    if (!m_brushPresets.isEmpty()) {
        applyBrushPreset(0);
    }
}

void RasterEditorWindow::applyBrushPreset(int index)
{
    if (!m_brushTool || index < 0 || index >= m_brushPresets.size()) {
        return;
    }

    const BrushPreset& preset = m_brushPresets.at(index);
    m_activePresetIndex = index;

    if (m_brushSelector && m_brushSelector->currentIndex() != index) {
        QSignalBlocker blocker(m_brushSelector);
        m_brushSelector->setCurrentIndex(index);
    }

    if (m_brushSizeSlider) {
        QSignalBlocker blocker(m_brushSizeSlider);
        m_brushSizeSlider->setValue(qRound(preset.size));
    }
    if (m_brushSizeValue) {
        m_brushSizeValue->setText(QString::number(qRound(preset.size)));
    }

    m_brushTool->setSize(preset.size);
    if (m_eraserTool) {
        m_eraserTool->setSize(preset.size);
    }

    m_brushTool->setOpacity(preset.opacity);
    if (m_eraserTool) {
        m_eraserTool->setOpacity(preset.opacity);
    }

    m_brushTool->setHardness(preset.hardness);
    if (m_eraserTool) {
        m_eraserTool->setHardness(preset.hardness);
    }

    m_brushTool->setSpacing(preset.spacing);
    if (m_eraserTool) {
        m_eraserTool->setSpacing(preset.spacing);
    }

    m_brushTool->applyPreset(preset.settings, preset.brushResource);

    if (m_opacitySlider) {
        QSignalBlocker blocker(m_opacitySlider);
        m_opacitySlider->setValue(qRound(preset.opacity * 100.0f));
    }
    if (m_hardnessSlider) {
        QSignalBlocker blocker(m_hardnessSlider);
        m_hardnessSlider->setValue(qRound(preset.hardness * 100.0f));
    }
    if (m_spacingSlider) {
        QSignalBlocker blocker(m_spacingSlider);
        m_spacingSlider->setValue(qRound(preset.spacing * 100.0f));
    }

    if (m_statusLabel) {
        m_statusLabel->setText(tr("Brush: %1").arg(preset.name));
    }

    updateToolControls();
}

void RasterEditorWindow::onBrushSelected(int index)
{
    if (!m_brushSelector) {
        return;
    }

    applyBrushPreset(index);
}

void RasterEditorWindow::onBrushOpacityChanged(int value)
{
    const int clamped = qBound(0, value, 100);
    if (m_opacityValue) {
        m_opacityValue->setText(QString::number(clamped) + "%");
    }
    const float normalized = clamped / 100.0f;
    if (m_brushTool) {
        m_brushTool->setOpacity(normalized);
    }
    if (m_eraserTool) {
        m_eraserTool->setOpacity(normalized);
    }
}

void RasterEditorWindow::onBrushHardnessChanged(int value)
{
    const int clamped = qBound(0, value, 100);
    if (m_hardnessValue) {
        m_hardnessValue->setText(QString::number(clamped) + "%");
    }
    const float normalized = clamped / 100.0f;
    if (m_brushTool) {
        m_brushTool->setHardness(normalized);
    }
    if (m_eraserTool) {
        m_eraserTool->setHardness(normalized);
    }
}

void RasterEditorWindow::onBrushSpacingChanged(int value)
{
    const int clamped = qMax(1, value);
    if (m_spacingSlider && clamped != value) {
        QSignalBlocker blocker(m_spacingSlider);
        m_spacingSlider->setValue(clamped);
    }
    if (m_spacingValue) {
        m_spacingValue->setText(QString::number(clamped) + "%");
    }
    const float spacingRatio = clamped / 100.0f;
    if (m_brushTool) {
        m_brushTool->setSpacing(spacingRatio);
    }
    if (m_eraserTool) {
        m_eraserTool->setSpacing(spacingRatio);
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

void RasterEditorWindow::onLayerSelectionChanged(int row)
{
    if (!m_document || row < 0 || row >= m_layerList->count()) {
        return;
    }

    QListWidgetItem* item = m_layerList->item(row);
    if (!item) return;

    int layerIndex = item->data(Qt::UserRole).toInt();
    m_document->setActiveLayer(layerIndex);
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

    if (layerIndex < 0 || projectFrame < 0) {
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

    if (m_canvas && m_canvas->scene()) {
        m_canvas->scene()->clearSelection();
        pixmapItem->setSelected(true);
    }
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

    // Display layers top-to-bottom (reverse order from internal storage)
    for (int i = m_document->layerCount() - 1; i >= 0; --i) {
        const RasterLayer& layer = m_document->layerAt(i);
        QListWidgetItem* item = new QListWidgetItem(layer.name(), m_layerList);
        item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setCheckState(layer.isVisible() ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, i);  // Store actual layer index
    }

    const int active = m_document->activeLayer();
    if (active >= 0) {
        // Find the visual row for this layer
        for (int row = 0; row < m_layerList->count(); ++row) {
            QListWidgetItem* item = m_layerList->item(row);
            if (item && item->data(Qt::UserRole).toInt() == active) {
                m_layerList->setCurrentRow(row);
                break;
            }
        }
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
    }
    else {
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

    const bool brushSettingsEnabled = isBrushTool || isEraserTool;
    RasterBrushTool* activeBrushTool = nullptr;
    if (isBrushTool) {
        activeBrushTool = m_brushTool;
    }
    else if (isEraserTool) {
        activeBrushTool = m_eraserTool;
    }

    if (m_opacitySlider) {
        m_opacitySlider->setEnabled(brushSettingsEnabled);
        if (activeBrushTool) {
            QSignalBlocker blocker(m_opacitySlider);
            m_opacitySlider->setValue(qRound(activeBrushTool->opacity() * 100.0f));
        }
    }
    if (m_opacityValue) {
        m_opacityValue->setEnabled(brushSettingsEnabled);
        if (activeBrushTool) {
            m_opacityValue->setText(QString::number(qRound(activeBrushTool->opacity() * 100.0f)) + QStringLiteral("%"));
        }
    }
    if (m_hardnessSlider) {
        m_hardnessSlider->setEnabled(brushSettingsEnabled);
        if (activeBrushTool) {
            QSignalBlocker blocker(m_hardnessSlider);
            m_hardnessSlider->setValue(qRound(activeBrushTool->hardness() * 100.0f));
        }
    }
    if (m_hardnessValue) {
        m_hardnessValue->setEnabled(brushSettingsEnabled);
        if (activeBrushTool) {
            m_hardnessValue->setText(QString::number(qRound(activeBrushTool->hardness() * 100.0f)) + QStringLiteral("%"));
        }
    }
    if (m_spacingSlider) {
        m_spacingSlider->setEnabled(brushSettingsEnabled);
        if (activeBrushTool) {
            QSignalBlocker blocker(m_spacingSlider);
            m_spacingSlider->setValue(qRound(activeBrushTool->spacing() * 100.0f));
        }
    }
    if (m_spacingValue) {
        m_spacingValue->setEnabled(brushSettingsEnabled);
        if (activeBrushTool) {
            m_spacingValue->setText(QString::number(qRound(activeBrushTool->spacing() * 100.0f)) + QStringLiteral("%"));
        }
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

void RasterEditorWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    emit visibilityChanged(true);
}

void RasterEditorWindow::hideEvent(QHideEvent* event)
{
    QMainWindow::hideEvent(event);
    emit visibilityChanged(false);
}

void RasterEditorWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();
    hide();
}
