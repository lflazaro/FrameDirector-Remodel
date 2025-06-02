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
    setupAnimationGroup();

    m_mainLayout->addStretch();
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
        "    font-size: 11px;"
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
    m_xSpinBox->setRange(-10000, 10000);
    m_xSpinBox->setDecimals(1);
    m_xSpinBox->setStyleSheet(spinBoxStyle);

    m_ySpinBox = new QDoubleSpinBox;
    m_ySpinBox->setRange(-10000, 10000);
    m_ySpinBox->setDecimals(1);
    m_ySpinBox->setStyleSheet(spinBoxStyle);

    QHBoxLayout* positionLayout = new QHBoxLayout;
    positionLayout->addWidget(new QLabel("X:"));
    positionLayout->addWidget(m_xSpinBox);
    positionLayout->addWidget(new QLabel("Y:"));
    positionLayout->addWidget(m_ySpinBox);
    transformLayout->addRow("Position:", positionLayout);

    // Size
    m_widthSpinBox = new QDoubleSpinBox;
    m_widthSpinBox->setRange(0.1, 10000);
    m_widthSpinBox->setDecimals(1);
    m_widthSpinBox->setStyleSheet(spinBoxStyle);

    m_heightSpinBox = new QDoubleSpinBox;
    m_heightSpinBox->setRange(0.1, 10000);
    m_heightSpinBox->setDecimals(1);
    m_heightSpinBox->setStyleSheet(spinBoxStyle);

    QHBoxLayout* sizeLayout = new QHBoxLayout;
    sizeLayout->addWidget(new QLabel("W:"));
    sizeLayout->addWidget(m_widthSpinBox);
    sizeLayout->addWidget(new QLabel("H:"));
    sizeLayout->addWidget(m_heightSpinBox);
    transformLayout->addRow("Size:", sizeLayout);

    // Rotation
    m_rotationSpinBox = new QDoubleSpinBox;
    m_rotationSpinBox->setRange(-360, 360);
    m_rotationSpinBox->setDecimals(1);
    m_rotationSpinBox->setSuffix("°");
    m_rotationSpinBox->setStyleSheet(spinBoxStyle);
    transformLayout->addRow("Rotation:", m_rotationSpinBox);

    // Scale
    m_scaleXSpinBox = new QDoubleSpinBox;
    m_scaleXSpinBox->setRange(0.01, 100);
    m_scaleXSpinBox->setDecimals(2);
    m_scaleXSpinBox->setValue(1.0);
    m_scaleXSpinBox->setStyleSheet(spinBoxStyle);

    m_scaleYSpinBox = new QDoubleSpinBox;
    m_scaleYSpinBox->setRange(0.01, 100);
    m_scaleYSpinBox->setDecimals(2);
    m_scaleYSpinBox->setValue(1.0);
    m_scaleYSpinBox->setStyleSheet(spinBoxStyle);

    QHBoxLayout* scaleLayout = new QHBoxLayout;
    scaleLayout->addWidget(new QLabel("X:"));
    scaleLayout->addWidget(m_scaleXSpinBox);
    scaleLayout->addWidget(new QLabel("Y:"));
    scaleLayout->addWidget(m_scaleYSpinBox);
    transformLayout->addRow("Scale:", scaleLayout);

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
        "    min-height: 20px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #4A4A4F;"
        "    border: 1px solid #007ACC;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #007ACC;"
        "}";

    // Stroke color
    m_strokeColorButton = new QPushButton("Stroke Color");
    m_strokeColorButton->setStyleSheet(buttonStyle);
    styleLayout->addRow("Stroke:", m_strokeColorButton);

    // Fill color
    m_fillColorButton = new QPushButton("Fill Color");
    m_fillColorButton->setStyleSheet(buttonStyle);
    styleLayout->addRow("Fill:", m_fillColorButton);

    // Stroke width
    m_strokeWidthSpinBox = new QDoubleSpinBox;
    m_strokeWidthSpinBox->setRange(0.1, 100);
    m_strokeWidthSpinBox->setDecimals(1);
    m_strokeWidthSpinBox->setValue(2.0);
    m_strokeWidthSpinBox->setStyleSheet(
        "QDoubleSpinBox {"
        "    background-color: #2D2D30;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 2px;"
        "    padding: 2px;"
        "}"
    );
    styleLayout->addRow("Stroke Width:", m_strokeWidthSpinBox);

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
    );

    m_opacityLabel = new QLabel("100%");
    m_opacityLabel->setStyleSheet("color: white; min-width: 30px;");
    m_opacityLabel->setAlignment(Qt::AlignRight);

    opacityLayout->addWidget(m_opacitySlider);
    opacityLayout->addWidget(m_opacityLabel);
    styleLayout->addRow("Opacity:", opacityLayout);

    // Stroke style
    m_strokeStyleCombo = new QComboBox;
    m_strokeStyleCombo->addItems({ "Solid", "Dashed", "Dotted", "Dash-Dot" });
    m_strokeStyleCombo->setStyleSheet(
        "QComboBox {"
        "    background-color: #2D2D30;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 2px;"
        "    padding: 2px;"
        "}"
        "QComboBox::drop-down {"
        "    border: none;"
        "    width: 15px;"
        "}"
        "QComboBox QAbstractItemView {"
        "    background-color: #2D2D30;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    selection-background-color: #007ACC;"
        "}"
    );
    styleLayout->addRow("Stroke Style:", m_strokeStyleCombo);

    m_mainLayout->addWidget(m_styleGroup);

    // Connect style signals
    connect(m_strokeColorButton, &QPushButton::clicked, this, &PropertiesPanel::onStrokeColorClicked);
    connect(m_fillColorButton, &QPushButton::clicked, this, &PropertiesPanel::onFillColorClicked);
    connect(m_strokeWidthSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &PropertiesPanel::onStyleChanged);
    connect(m_opacitySlider, &QSlider::valueChanged, this, &PropertiesPanel::onStyleChanged);
    connect(m_strokeStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &PropertiesPanel::onStyleChanged);

    // Connect opacity slider to label update
    connect(m_opacitySlider, &QSlider::valueChanged, [this](int value) {
        m_opacityLabel->setText(QString("%1%").arg(value));
        });
}

void PropertiesPanel::setupAnimationGroup()
{
    m_animationGroup = new QGroupBox("Animation");
    m_animationGroup->setStyleSheet(m_transformGroup->styleSheet());

    QFormLayout* animationLayout = new QFormLayout(m_animationGroup);

    m_enableAnimationCheckBox = new QCheckBox("Enable Animation");
    m_enableAnimationCheckBox->setStyleSheet(
        "QCheckBox {"
        "    color: white;"
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
    animationLayout->addRow(m_enableAnimationCheckBox);

    m_durationSpinBox = new QSpinBox;
    m_durationSpinBox->setRange(1, 10000);
    m_durationSpinBox->setValue(1000);
    m_durationSpinBox->setSuffix(" ms");
    m_durationSpinBox->setStyleSheet(
        "QSpinBox {"
        "    background-color: #2D2D30;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 2px;"
        "    padding: 2px;"
        "}"
    );
    animationLayout->addRow("Duration:", m_durationSpinBox);

    m_easingCombo = new QComboBox;
    m_easingCombo->addItems({ "Linear", "Ease In", "Ease Out", "Ease In Out" });
    m_easingCombo->setStyleSheet(m_strokeStyleCombo->styleSheet());
    animationLayout->addRow("Easing:", m_easingCombo);

    m_mainLayout->addWidget(m_animationGroup);
}

void PropertiesPanel::onSelectionChanged()
{
    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    if (!canvas) return;

    QList<QGraphicsItem*> selectedItems = canvas->getSelectedItems();
    updateProperties(selectedItems);
}

void PropertiesPanel::updateProperties(const QList<QGraphicsItem*>& selectedItems)
{
    m_selectedItems = selectedItems;

    if (selectedItems.isEmpty()) {
        clearProperties();
        return;
    }

    m_updating = true;

    // Update transform properties (use first selected item as reference)
    QGraphicsItem* firstItem = selectedItems.first();
    updateTransformControls(firstItem);
    updateStyleControls(firstItem);

    // Enable/disable controls based on selection
    bool hasSelection = !selectedItems.isEmpty();
    m_transformGroup->setEnabled(hasSelection);
    m_styleGroup->setEnabled(hasSelection);
    m_animationGroup->setEnabled(hasSelection);

    m_updating = false;
}

void PropertiesPanel::clearProperties()
{
    m_updating = true;

    // Clear transform controls
    m_xSpinBox->setValue(0);
    m_ySpinBox->setValue(0);
    m_widthSpinBox->setValue(0);
    m_heightSpinBox->setValue(0);
    m_rotationSpinBox->setValue(0);
    m_scaleXSpinBox->setValue(1.0);
    m_scaleYSpinBox->setValue(1.0);

    // Clear style controls
    m_strokeWidthSpinBox->setValue(2.0);
    m_opacitySlider->setValue(100);
    m_strokeStyleCombo->setCurrentIndex(0);

    // Disable all groups
    m_transformGroup->setEnabled(false);
    m_styleGroup->setEnabled(false);
    m_animationGroup->setEnabled(false);

    m_updating = false;
}

void PropertiesPanel::updateTransformControls(QGraphicsItem* item)
{
    if (!item) return;

    // Position
    QPointF pos = item->pos();
    m_xSpinBox->setValue(pos.x());
    m_ySpinBox->setValue(pos.y());

    // Size (get from bounding rect)
    QRectF boundingRect = item->boundingRect();
    m_widthSpinBox->setValue(boundingRect.width());
    m_heightSpinBox->setValue(boundingRect.height());

    // Rotation
    m_rotationSpinBox->setValue(item->rotation());

    // Scale
    QTransform transform = item->transform();
    m_scaleXSpinBox->setValue(transform.m11());
    m_scaleYSpinBox->setValue(transform.m22());
}

void PropertiesPanel::updateStyleControls(QGraphicsItem* item)
{
    if (!item) return;

    // Opacity
    int opacity = static_cast<int>(item->opacity() * 100);
    m_opacitySlider->setValue(opacity);

    // Try to get stroke and fill colors from different item types
    QColor strokeColor = Qt::black;
    QColor fillColor = Qt::transparent;
    double strokeWidth = 2.0;

    if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
        strokeColor = rectItem->pen().color();
        fillColor = rectItem->brush().color();
        strokeWidth = rectItem->pen().widthF();
    }
    else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
        strokeColor = ellipseItem->pen().color();
        fillColor = ellipseItem->brush().color();
        strokeWidth = ellipseItem->pen().widthF();
    }
    else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
        strokeColor = lineItem->pen().color();
        strokeWidth = lineItem->pen().widthF();
    }
    else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        strokeColor = pathItem->pen().color();
        fillColor = pathItem->brush().color();
        strokeWidth = pathItem->pen().widthF();
    }

    // Update color button backgrounds
    m_strokeColorButton->setStyleSheet(QString(
        "QPushButton {"
        "    background-color: %1;"
        "    color: %2;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 3px;"
        "    padding: 4px 8px;"
        "    min-height: 20px;"
        "}"
        "QPushButton:hover {"
        "    border: 1px solid #007ACC;"
        "}"
    ).arg(strokeColor.name()).arg(strokeColor.lightness() > 128 ? "black" : "white"));

    m_fillColorButton->setStyleSheet(QString(
        "QPushButton {"
        "    background-color: %1;"
        "    color: %2;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 3px;"
        "    padding: 4px 8px;"
        "    min-height: 20px;"
        "}"
        "QPushButton:hover {"
        "    border: 1px solid #007ACC;"
        "}"
    ).arg(fillColor.name()).arg(fillColor.lightness() > 128 ? "black" : "white"));

    m_strokeWidthSpinBox->setValue(strokeWidth);
}

void PropertiesPanel::onTransformChanged()
{
    if (m_updating || m_selectedItems.isEmpty()) return;

    for (QGraphicsItem* item : m_selectedItems) {
        if (!item) continue;

        // Update position
        QPointF newPos(m_xSpinBox->value(), m_ySpinBox->value());
        item->setPos(newPos);

        // Update rotation
        item->setRotation(m_rotationSpinBox->value());

        // Update scale
        QTransform transform;
        transform.scale(m_scaleXSpinBox->value(), m_scaleYSpinBox->value());
        item->setTransform(transform);

        // Update size (for rect and ellipse items)
        QRectF newRect(0, 0, m_widthSpinBox->value(), m_heightSpinBox->value());

        if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            rectItem->setRect(newRect);
        }
        else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
            ellipseItem->setRect(newRect);
        }
    }

    // Update canvas frame state
    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    if (canvas) {
        canvas->storeCurrentFrameState();
    }

    emit propertyChanged();
}

void PropertiesPanel::onStyleChanged()
{
    if (m_updating || m_selectedItems.isEmpty()) return;

    for (QGraphicsItem* item : m_selectedItems) {
        if (!item) continue;

        // Update opacity
        double opacity = m_opacitySlider->value() / 100.0;
        item->setOpacity(opacity);

        // Update stroke width for applicable items
        double strokeWidth = m_strokeWidthSpinBox->value();

        if (auto rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            QPen pen = rectItem->pen();
            pen.setWidthF(strokeWidth);
            rectItem->setPen(pen);
        }
        else if (auto ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
            QPen pen = ellipseItem->pen();
            pen.setWidthF(strokeWidth);
            ellipseItem->setPen(pen);
        }
        else if (auto lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
            QPen pen = lineItem->pen();
            pen.setWidthF(strokeWidth);
            lineItem->setPen(pen);
        }
        else if (auto pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
            QPen pen = pathItem->pen();
            pen.setWidthF(strokeWidth);
            pathItem->setPen(pen);
        }
    }

    // Update canvas frame state
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
        }

        // Update button color
        updateStyleControls(m_selectedItems.first());

        // Update canvas frame state
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

    QColor currentColor = Qt::transparent;
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

        // Update button color
        updateStyleControls(m_selectedItems.first());

        // Update canvas frame state
        Canvas* canvas = m_mainWindow->findChild<Canvas*>();
        if (canvas) {
            canvas->storeCurrentFrameState();
        }

        emit propertyChanged();
    }
}