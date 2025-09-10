// Panels/PropertiesPanel.cpp
#include "PropertiesPanel.h"
#include "../MainWindow.h"
#include "../Canvas.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QColorDialog>
#include <QPushButton>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsPathItem>
#include <QGraphicsBlurEffect>
#include <QPen>
#include <QBrush>
#include <QTransform>
#include <QtMath>

PropertiesPanel::PropertiesPanel(MainWindow* parent)
    : QWidget(parent)
    , m_mainWindow(parent)
    , m_updating(false)
{
    setupUI();

    // Connect to canvas selection changes
    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    if (canvas) {
        connect(canvas, &Canvas::selectionChanged, this, &PropertiesPanel::onSelectionChanged);
    }
}

void PropertiesPanel::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(4);

    // Header
    QLabel* headerLabel = new QLabel("Properties");
    headerLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 12px;"
        "    padding: 4px;"
        "    background-color: #3E3E42;"
        "    border-radius: 2px;"
        "}"
    );
    headerLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(headerLabel);

    setupTransformGroup();
    setupStyleGroup();
    //setupAnimationGroup();

    m_mainLayout->addStretch();

    // Initially disable all controls
    clearProperties();
}

void PropertiesPanel::setupTransformGroup()
{
    m_transformGroup = new QGroupBox("Transform");
    m_transformGroup->setStyleSheet(
        "QGroupBox {"
        "    color: white;"
        "    font-weight: bold;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 4px;"
        "    margin: 4px 0px;"
        "    padding-top: 8px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 8px;"
        "    padding: 0 4px 0 4px;"
        "}"
    );

    QFormLayout* transformLayout = new QFormLayout(m_transformGroup);

    QString spinBoxStyle =
        "QDoubleSpinBox {"
        "    background-color: #2D2D30;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 2px;"
        "    padding: 2px;"
        "    font-weight: normal;"
        "}"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
        "    background-color: #3E3E42;"
        "    border: 1px solid #5A5A5C;"
        "}"
        "QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover {"
        "    background-color: #4A4A4F;"
        "}";

    // Position
    m_xSpinBox = new QDoubleSpinBox;
    m_xSpinBox->setRange(-9999, 9999);
    m_xSpinBox->setDecimals(1);
    m_xSpinBox->setStyleSheet(spinBoxStyle);

    m_ySpinBox = new QDoubleSpinBox;
    m_ySpinBox->setRange(-9999, 9999);
    m_ySpinBox->setDecimals(1);
    m_ySpinBox->setStyleSheet(spinBoxStyle);

    QHBoxLayout* posLayout = new QHBoxLayout;
    posLayout->addWidget(m_xSpinBox);
    posLayout->addWidget(m_ySpinBox);

    QLabel* posLabel = new QLabel("Position:");
    posLabel->setStyleSheet("color: white; font-weight: normal;");
    transformLayout->addRow(posLabel, posLayout);

    // Size
    m_widthSpinBox = new QDoubleSpinBox;
    m_widthSpinBox->setRange(1, 9999);
    m_widthSpinBox->setDecimals(1);
    m_widthSpinBox->setStyleSheet(spinBoxStyle);

    m_heightSpinBox = new QDoubleSpinBox;
    m_heightSpinBox->setRange(1, 9999);
    m_heightSpinBox->setDecimals(1);
    m_heightSpinBox->setStyleSheet(spinBoxStyle);

    QHBoxLayout* sizeLayout = new QHBoxLayout;
    sizeLayout->addWidget(m_widthSpinBox);
    sizeLayout->addWidget(m_heightSpinBox);

    QLabel* sizeLabel = new QLabel("Size:");
    sizeLabel->setStyleSheet("color: white; font-weight: normal;");
    transformLayout->addRow(sizeLabel, sizeLayout);

    // Rotation
    m_rotationSpinBox = new QDoubleSpinBox;
    m_rotationSpinBox->setRange(-360, 360);
    m_rotationSpinBox->setDecimals(1);
    m_rotationSpinBox->setSuffix("°");
    m_rotationSpinBox->setStyleSheet(spinBoxStyle);

    QLabel* rotLabel = new QLabel("Rotation:");
    rotLabel->setStyleSheet("color: white; font-weight: normal;");
    transformLayout->addRow(rotLabel, m_rotationSpinBox);

    // Scale
    m_scaleXSpinBox = new QDoubleSpinBox;
    m_scaleXSpinBox->setRange(0.1, 10.0);
    m_scaleXSpinBox->setValue(1.0);
    m_scaleXSpinBox->setDecimals(2);
    m_scaleXSpinBox->setSingleStep(0.1);
    m_scaleXSpinBox->setStyleSheet(spinBoxStyle);

    m_scaleYSpinBox = new QDoubleSpinBox;
    m_scaleYSpinBox->setRange(0.1, 10.0);
    m_scaleYSpinBox->setValue(1.0);
    m_scaleYSpinBox->setDecimals(2);
    m_scaleYSpinBox->setSingleStep(0.1);
    m_scaleYSpinBox->setStyleSheet(spinBoxStyle);

    QHBoxLayout* scaleLayout = new QHBoxLayout;
    scaleLayout->addWidget(m_scaleXSpinBox);
    scaleLayout->addWidget(m_scaleYSpinBox);

    QLabel* scaleLabel = new QLabel("Scale:");
    scaleLabel->setStyleSheet("color: white; font-weight: normal;");
    transformLayout->addRow(scaleLabel, scaleLayout);

    m_mainLayout->addWidget(m_transformGroup);

    // Connect transform signals
    connect(m_xSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &PropertiesPanel::onTransformChanged);
    connect(m_ySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &PropertiesPanel::onTransformChanged);
    connect(m_widthSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &PropertiesPanel::onTransformChanged);
    connect(m_heightSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &PropertiesPanel::onTransformChanged);
    connect(m_rotationSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &PropertiesPanel::onTransformChanged);
    connect(m_scaleXSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &PropertiesPanel::onTransformChanged);
    connect(m_scaleYSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &PropertiesPanel::onTransformChanged);
}

void PropertiesPanel::setupStyleGroup()
{
    m_styleGroup = new QGroupBox("Style");
    m_styleGroup->setStyleSheet(m_transformGroup->styleSheet());

    QFormLayout* styleLayout = new QFormLayout(m_styleGroup);

    QString buttonStyle =
        "QPushButton {"
        "    background-color: #3E3E42;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 3px;"
        "    padding: 4px 8px;"
        "    font-weight: normal;"
        "    min-height: 20px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #4A4A4F;"
        "    border: 1px solid #007ACC;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #007ACC;"
        "}";

    // Color buttons
    m_strokeColorButton = new QPushButton("Black");
    m_strokeColorButton->setStyleSheet(buttonStyle);

    m_fillColorButton = new QPushButton("None");
    m_fillColorButton->setStyleSheet(buttonStyle);

    QLabel* strokeLabel = new QLabel("Stroke:");
    strokeLabel->setStyleSheet("color: white; font-weight: normal;");
    styleLayout->addRow(strokeLabel, m_strokeColorButton);

    QLabel* fillLabel = new QLabel("Fill:");
    fillLabel->setStyleSheet("color: white; font-weight: normal;");
    styleLayout->addRow(fillLabel, m_fillColorButton);

    // Stroke width
    m_strokeWidthSpinBox = new QDoubleSpinBox;
    m_strokeWidthSpinBox->setRange(0.1, 50.0);
    m_strokeWidthSpinBox->setDecimals(1);
    m_strokeWidthSpinBox->setSuffix(" px");
    m_strokeWidthSpinBox->setStyleSheet(
        "QDoubleSpinBox {"
        "    background-color: #2D2D30;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 2px;"
        "    padding: 2px;"
        "    font-weight: normal;"
        "}"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
        "    background-color: #3E3E42;"
        "    border: 1px solid #5A5A5C;"
        "}"
        "QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover {"
        "    background-color: #4A4A4F;"
        "}"
    );

    QLabel* widthLabel = new QLabel("Width:");
    widthLabel->setStyleSheet("color: white; font-weight: normal;");
    styleLayout->addRow(widthLabel, m_strokeWidthSpinBox);

    // Opacity
    QHBoxLayout* opacityLayout = new QHBoxLayout;

    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(100);
    m_opacitySlider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "    border: 1px solid #5A5A5C;"
        "    height: 4px;"
        "    background: #2D2D30;"
        "    border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "    background: #007ACC;"
        "    border: 1px solid #005A9B;"
        "    width: 12px;"
        "    margin: -4px 0;"
        "    border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "    background: #4A9EDF;"
        "}"
    );

    m_opacityLabel = new QLabel("100%");
    m_opacityLabel->setStyleSheet("color: white; font-weight: normal; min-width: 35px;");
    m_opacityLabel->setAlignment(Qt::AlignRight);

    opacityLayout->addWidget(m_opacitySlider);
    opacityLayout->addWidget(m_opacityLabel);

    QLabel* opLabel = new QLabel("Opacity:");
    opLabel->setStyleSheet("color: white; font-weight: normal;");
    styleLayout->addRow(opLabel, opacityLayout);

    // Blur
    QHBoxLayout* blurLayout = new QHBoxLayout;

    m_blurSlider = new QSlider(Qt::Horizontal);
    m_blurSlider->setRange(0, 20);
    m_blurSlider->setValue(0);
    m_blurSlider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "    border: 1px solid #5A5A5C;"
        "    height: 4px;"
        "    background: #2D2D30;"
        "    border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "    background: #007ACC;"
        "    border: 1px solid #005A9B;"
        "    width: 12px;"
        "    margin: -4px 0;"
        "    border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "    background: #4A9EDF;"
        "}"
    );

    m_blurLabel = new QLabel("0px");
    m_blurLabel->setStyleSheet("color: white; font-weight: normal; min-width: 35px;");
    m_blurLabel->setAlignment(Qt::AlignRight);

    blurLayout->addWidget(m_blurSlider);
    blurLayout->addWidget(m_blurLabel);

    QLabel* blurLabel = new QLabel("Blur:");
    blurLabel->setStyleSheet("color: white; font-weight: normal;");
    styleLayout->addRow(blurLabel, blurLayout);

    // Stroke style
    m_strokeStyleCombo = new QComboBox;
    m_strokeStyleCombo->addItems({ "Solid", "Dashed", "Dotted", "Dash Dot" });
    m_strokeStyleCombo->setStyleSheet(
        "QComboBox {"
        "    background-color: #2D2D30;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 2px;"
        "    padding: 2px 6px;"
        "    font-weight: normal;"
        "}"
        "QComboBox::drop-down {"
        "    border: none;"
        "    width: 15px;"
        "}"
        "QComboBox::down-arrow {"
        "    image: none;"
        "    border-left: 4px solid transparent;"
        "    border-right: 4px solid transparent;"
        "    border-top: 4px solid #CCCCCC;"
        "}"
        "QComboBox QAbstractItemView {"
        "    background-color: #2D2D30;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    selection-background-color: #007ACC;"
        "}"
    );

    QLabel* styleLabel = new QLabel("Style:");
    styleLabel->setStyleSheet("color: white; font-weight: normal;");
    styleLayout->addRow(styleLabel, m_strokeStyleCombo);

    m_mainLayout->addWidget(m_styleGroup);

    // Connect style signals
    connect(m_strokeColorButton, &QPushButton::clicked, this, &PropertiesPanel::onStrokeColorClicked);
    connect(m_fillColorButton, &QPushButton::clicked, this, &PropertiesPanel::onFillColorClicked);
    connect(m_strokeWidthSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &PropertiesPanel::onStyleChanged);
    connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int value) {
        m_opacityLabel->setText(QString("%1%").arg(value));
        onStyleChanged();
        });
    connect(m_blurSlider, &QSlider::valueChanged, this, [this](int value) {
        m_blurLabel->setText(QString("%1px").arg(value));
        onStyleChanged();
        });
    connect(m_strokeStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &PropertiesPanel::onStyleChanged);
}

void PropertiesPanel::setupAnimationGroup()
{
    m_animationGroup = new QGroupBox("Animation");
    m_animationGroup->setStyleSheet(m_transformGroup->styleSheet());

    QFormLayout* animLayout = new QFormLayout(m_animationGroup);

    m_enableAnimationCheckBox = new QCheckBox("Enable Animation");
    m_enableAnimationCheckBox->setStyleSheet(
        "QCheckBox {"
        "    color: white;"
        "    font-weight: normal;"
        "}"
        "QCheckBox::indicator {"
        "    width: 16px;"
        "    height: 16px;"
        "}"
        "QCheckBox::indicator:unchecked {"
        "    background-color: #2D2D30;"
        "    border: 1px solid #5A5A5C;"
        "}"
        "QCheckBox::indicator:checked {"
        "    background-color: #007ACC;"
        "    border: 1px solid #005A9B;"
        "}"
    );

    m_durationSpinBox = new QSpinBox;
    m_durationSpinBox->setRange(1, 300);
    m_durationSpinBox->setValue(24);
    m_durationSpinBox->setSuffix(" frames");
    m_durationSpinBox->setEnabled(false);
    m_durationSpinBox->setStyleSheet(
        "QSpinBox {"
        "    background-color: #2D2D30;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 2px;"
        "    padding: 2px;"
        "    font-weight: normal;"
        "}"
        "QSpinBox:disabled {"
        "    color: #666666;"
        "    background-color: #1A1A1A;"
        "}"
    );

    m_easingCombo = new QComboBox;
    m_easingCombo->addItems({ "Linear", "Ease In", "Ease Out", "Ease In Out" });
    m_easingCombo->setEnabled(false);
    m_easingCombo->setStyleSheet(
        "QComboBox {"
        "    background-color: #2D2D30;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 2px;"
        "    padding: 2px 6px;"
        "    font-weight: normal;"
        "}"
        "QComboBox:disabled {"
        "    color: #666666;"
        "    background-color: #1A1A1A;"
        "}"
    );

    animLayout->addRow(m_enableAnimationCheckBox);

    QLabel* durLabel = new QLabel("Duration:");
    durLabel->setStyleSheet("color: white; font-weight: normal;");
    animLayout->addRow(durLabel, m_durationSpinBox);

    QLabel* easingLabel = new QLabel("Easing:");
    easingLabel->setStyleSheet("color: white; font-weight: normal;");
    animLayout->addRow(easingLabel, m_easingCombo);

    m_mainLayout->addWidget(m_animationGroup);

    // Connect animation signals
    connect(m_enableAnimationCheckBox, &QCheckBox::toggled, [this](bool enabled) {
        m_durationSpinBox->setEnabled(enabled);
        m_easingCombo->setEnabled(enabled);
        });
}

void PropertiesPanel::updateProperties(const QList<QGraphicsItem*>& selectedItems)
{
    if (selectedItems.isEmpty()) {
        clearProperties();
        return;
    }

    m_updating = true;
    m_selectedItems = selectedItems;

    // For multiple selection, show properties of the first item
    QGraphicsItem* item = selectedItems.first();

    updateTransformControls(item);
    updateStyleControls(item);

    // Enable all controls
    m_transformGroup->setEnabled(true);
    m_styleGroup->setEnabled(true);
    //m_animationGroup->setEnabled(true);

    m_updating = false;
}

void PropertiesPanel::clearProperties()
{
    m_updating = true;
    m_selectedItems.clear();

    // Disable all controls
    m_transformGroup->setEnabled(false);
    m_styleGroup->setEnabled(false);
    //m_animationGroup->setEnabled(false);

    // Reset values
    m_xSpinBox->setValue(0);
    m_ySpinBox->setValue(0);
    m_widthSpinBox->setValue(0);
    m_heightSpinBox->setValue(0);
    m_rotationSpinBox->setValue(0);
    m_scaleXSpinBox->setValue(1.0);
    m_scaleYSpinBox->setValue(1.0);

    m_strokeColorButton->setText("Black");
    m_fillColorButton->setText("None");
    m_strokeWidthSpinBox->setValue(1.0);
    m_opacitySlider->setValue(100);
    m_opacityLabel->setText("100%");
    m_blurSlider->setValue(0);
    m_blurLabel->setText("0px");
    m_strokeStyleCombo->setCurrentIndex(0);

    m_updating = false;
}

void PropertiesPanel::updateTransformControls(QGraphicsItem* item)
{
    if (!item) return;

    QPointF pos = item->pos();
    m_xSpinBox->setValue(pos.x());
    m_ySpinBox->setValue(pos.y());

    QRectF rect = item->boundingRect();
    m_widthSpinBox->setValue(rect.width());
    m_heightSpinBox->setValue(rect.height());

    m_rotationSpinBox->setValue(item->rotation());

    QTransform transform = item->transform();
    m_scaleXSpinBox->setValue(transform.m11());
    m_scaleYSpinBox->setValue(transform.m22());
}

void PropertiesPanel::updateStyleControls(QGraphicsItem* item)
{
    if (!item) return;

    QPen pen;
    QBrush brush;

    // Get pen and brush based on item type
    if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
        pen = rectItem->pen();
        brush = rectItem->brush();
    }
    else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
        pen = ellipseItem->pen();
        brush = ellipseItem->brush();
    }
    else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
        pen = lineItem->pen();
        brush = QBrush(Qt::transparent);
    }
    else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        pen = pathItem->pen();
        brush = pathItem->brush();
    }
    else if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
        pen = QPen(textItem->defaultTextColor());
        brush = QBrush(Qt::transparent);
    }

    // Update stroke color button
    QColor strokeColor = pen.color();
    m_strokeColorButton->setText(strokeColor.name());
    m_strokeColorButton->setStyleSheet(
        QString("QPushButton { background-color: %1; color: %2; }")
        .arg(strokeColor.name())
        .arg(strokeColor.lightness() > 128 ? "black" : "white"));

    // Update fill color button
    QColor fillColor = brush.color();
    if (brush.style() == Qt::NoBrush) {
        m_fillColorButton->setText("None");
        m_fillColorButton->setStyleSheet("QPushButton { background-color: #3E3E42; color: white; }");
    }
    else {
        m_fillColorButton->setText(fillColor.name());
        m_fillColorButton->setStyleSheet(
            QString("QPushButton { background-color: %1; color: %2; }")
            .arg(fillColor.name())
            .arg(fillColor.lightness() > 128 ? "black" : "white"));
    }

    // Update stroke width
    m_strokeWidthSpinBox->setValue(pen.widthF());

    // Update opacity
    int opacity = static_cast<int>(item->opacity() * 100);
    m_opacitySlider->setValue(opacity);
    m_opacityLabel->setText(QString("%1%").arg(opacity));

    // Update blur
    int blur = 0;
    if (auto blurEffect = dynamic_cast<QGraphicsBlurEffect*>(item->graphicsEffect())) {
        blur = static_cast<int>(blurEffect->blurRadius());
    }
    m_blurSlider->setValue(blur);
    m_blurLabel->setText(QString("%1px").arg(blur));

    // Update stroke style
    switch (pen.style()) {
    case Qt::SolidLine: m_strokeStyleCombo->setCurrentIndex(0); break;
    case Qt::DashLine: m_strokeStyleCombo->setCurrentIndex(1); break;
    case Qt::DotLine: m_strokeStyleCombo->setCurrentIndex(2); break;
    case Qt::DashDotLine: m_strokeStyleCombo->setCurrentIndex(3); break;
    default: m_strokeStyleCombo->setCurrentIndex(0); break;
    }
}

void PropertiesPanel::onTransformChanged()
{
    if (m_updating || m_selectedItems.isEmpty()) return;

    for (QGraphicsItem* item : m_selectedItems) {
        // Position
        item->setPos(m_xSpinBox->value(), m_ySpinBox->value());

        // Rotation
        item->setRotation(m_rotationSpinBox->value());

        // Scale
        QTransform transform;
        transform.scale(m_scaleXSpinBox->value(), m_scaleYSpinBox->value());
        item->setTransform(transform);

        // Size (for shapes that support it)
        if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            QRectF rect = rectItem->rect();
            rect.setSize(QSizeF(m_widthSpinBox->value(), m_heightSpinBox->value()));
            rectItem->setRect(rect);
        }
        else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
            QRectF rect = ellipseItem->rect();
            rect.setSize(QSizeF(m_widthSpinBox->value(), m_heightSpinBox->value()));
            ellipseItem->setRect(rect);
        }
    }

    // Store frame state after transformation
    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    if (canvas) {
        canvas->storeCurrentFrameState();
    }

    emit propertyChanged();
}

void PropertiesPanel::onStyleChanged()
{
    if (m_updating || m_selectedItems.isEmpty()) return;

    double strokeWidth = m_strokeWidthSpinBox->value();
    double opacity = m_opacitySlider->value() / 100.0;
    int blur = m_blurSlider->value();

    Qt::PenStyle penStyle = Qt::SolidLine;
    switch (m_strokeStyleCombo->currentIndex()) {
    case 1: penStyle = Qt::DashLine; break;
    case 2: penStyle = Qt::DotLine; break;
    case 3: penStyle = Qt::DashDotLine; break;
    default: penStyle = Qt::SolidLine; break;
    }

    for (QGraphicsItem* item : m_selectedItems) {
        Canvas* canvas = m_mainWindow->findChild<Canvas*>();
        double layerOpacity = 1.0;
        if (canvas) {
            int layerIdx = canvas->getItemLayerIndex(item);
            layerOpacity = canvas->getLayerOpacity(layerIdx);
        }
        item->setData(0, opacity);
        item->setOpacity(opacity * layerOpacity);

        // Apply blur effect
        if (blur > 0) {
            QGraphicsBlurEffect* blurEffect = dynamic_cast<QGraphicsBlurEffect*>(item->graphicsEffect());
            if (!blurEffect) {
                blurEffect = new QGraphicsBlurEffect();
                item->setGraphicsEffect(blurEffect);
            }
            blurEffect->setBlurRadius(blur);
        }
        else if (item->graphicsEffect()) {
            item->setGraphicsEffect(nullptr);
        }

        // Update pen and brush based on item type
        if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            QPen pen = rectItem->pen();
            pen.setWidthF(strokeWidth);
            pen.setStyle(penStyle);
            rectItem->setPen(pen);
        }
        else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
            QPen pen = ellipseItem->pen();
            pen.setWidthF(strokeWidth);
            pen.setStyle(penStyle);
            ellipseItem->setPen(pen);
        }
        else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
            QPen pen = lineItem->pen();
            pen.setWidthF(strokeWidth);
            pen.setStyle(penStyle);
            lineItem->setPen(pen);
        }
        else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
            QPen pen = pathItem->pen();
            pen.setWidthF(strokeWidth);
            pen.setStyle(penStyle);
            pathItem->setPen(pen);
        }
    }

    // Store frame state after style change
    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    if (canvas) {
        canvas->storeCurrentFrameState();
    }

    emit propertyChanged();
}

void PropertiesPanel::onStrokeColorClicked()
{
    if (m_selectedItems.isEmpty()) return;

    QColor currentColor = Qt::black;
    if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(m_selectedItems.first())) {
        currentColor = rectItem->pen().color();
    }

    QColor color = QColorDialog::getColor(currentColor, this, "Select Stroke Color");
    if (color.isValid()) {
        for (QGraphicsItem* item : m_selectedItems) {
            if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
                QPen pen = rectItem->pen();
                pen.setColor(color);
                rectItem->setPen(pen);
            }
            else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
                QPen pen = ellipseItem->pen();
                pen.setColor(color);
                ellipseItem->setPen(pen);
            }
            else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
                QPen pen = lineItem->pen();
                pen.setColor(color);
                lineItem->setPen(pen);
            }
            else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
                QPen pen = pathItem->pen();
                pen.setColor(color);
                pathItem->setPen(pen);
            }
            else if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
                textItem->setDefaultTextColor(color);
            }
        }

        // Update button appearance
        m_strokeColorButton->setText(color.name());
        m_strokeColorButton->setStyleSheet(
            QString("QPushButton { background-color: %1; color: %2; }")
            .arg(color.name())
            .arg(color.lightness() > 128 ? "black" : "white"));

        // Store frame state after color change
        Canvas* canvas = m_mainWindow->findChild<Canvas*>();
        if (canvas) {
            canvas->storeCurrentFrameState();
        }

        emit propertyChanged();
    }
}

void PropertiesPanel::onFillColorClicked()
{
    if (m_selectedItems.isEmpty()) return;

    QColor currentColor = Qt::white;
    if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(m_selectedItems.first())) {
        currentColor = rectItem->brush().color();
    }

    QColor color = QColorDialog::getColor(currentColor, this, "Select Fill Color");
    if (color.isValid()) {
        for (QGraphicsItem* item : m_selectedItems) {
            if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
                rectItem->setBrush(QBrush(color));
            }
            else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
                ellipseItem->setBrush(QBrush(color));
            }
            else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
                pathItem->setBrush(QBrush(color));
            }
        }

        // Update button appearance
        m_fillColorButton->setText(color.name());
        m_fillColorButton->setStyleSheet(
            QString("QPushButton { background-color: %1; color: %2; }")
            .arg(color.name())
            .arg(color.lightness() > 128 ? "black" : "white"));

        // Store frame state after color change
        Canvas* canvas = m_mainWindow->findChild<Canvas*>();
        if (canvas) {
            canvas->storeCurrentFrameState();
        }

        emit propertyChanged();
    }
}

void PropertiesPanel::onSelectionChanged()
{
    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    if (canvas) {
        QList<QGraphicsItem*> selectedItems = canvas->getSelectedItems();
        updateProperties(selectedItems);
    }
}