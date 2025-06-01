#include "PropertiesPanel.h"
#include "../MainWindow.h"
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>

PropertiesPanel::PropertiesPanel(MainWindow* parent)
    : QWidget(parent)
    , m_mainWindow(parent)
    , m_updating(false)
{
    setupUI();
}

void PropertiesPanel::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);

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

    m_xSpinBox = new QDoubleSpinBox;
    m_xSpinBox->setRange(-10000, 10000);
    m_xSpinBox->setDecimals(1);

    m_ySpinBox = new QDoubleSpinBox;
    m_ySpinBox->setRange(-10000, 10000);
    m_ySpinBox->setDecimals(1);

    m_widthSpinBox = new QDoubleSpinBox;
    m_widthSpinBox->setRange(0, 10000);
    m_widthSpinBox->setDecimals(1);

    m_heightSpinBox = new QDoubleSpinBox;
    m_heightSpinBox->setRange(0, 10000);
    m_heightSpinBox->setDecimals(1);

    m_rotationSpinBox = new QDoubleSpinBox;
    m_rotationSpinBox->setRange(-360, 360);
    m_rotationSpinBox->setSuffix("°");

    transformLayout->addRow("X:", m_xSpinBox);
    transformLayout->addRow("Y:", m_ySpinBox);
    transformLayout->addRow("Width:", m_widthSpinBox);
    transformLayout->addRow("Height:", m_heightSpinBox);
    transformLayout->addRow("Rotation:", m_rotationSpinBox);

    m_mainLayout->addWidget(m_transformGroup);

    // Connect signals
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
}

void PropertiesPanel::setupStyleGroup()
{
    m_styleGroup = new QGroupBox("Style");
    m_styleGroup->setStyleSheet(m_transformGroup->styleSheet());

    QFormLayout* styleLayout = new QFormLayout(m_styleGroup);

    m_strokeColorButton = new QPushButton;
    m_strokeColorButton->setMaximumSize(50, 25);
    m_strokeColorButton->setStyleSheet("background-color: black; border: 1px solid gray;");

    m_fillColorButton = new QPushButton;
    m_fillColorButton->setMaximumSize(50, 25);
    m_fillColorButton->setStyleSheet("background-color: white; border: 1px solid gray;");

    m_strokeWidthSpinBox = new QDoubleSpinBox;
    m_strokeWidthSpinBox->setRange(0, 100);
    m_strokeWidthSpinBox->setDecimals(1);
    m_strokeWidthSpinBox->setValue(2.0);

    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(100);

    m_opacityLabel = new QLabel("100%");
    m_opacityLabel->setMinimumWidth(40);

    QHBoxLayout* opacityLayout = new QHBoxLayout;
    opacityLayout->addWidget(m_opacitySlider);
    opacityLayout->addWidget(m_opacityLabel);

    styleLayout->addRow("Stroke:", m_strokeColorButton);
    styleLayout->addRow("Fill:", m_fillColorButton);
    styleLayout->addRow("Width:", m_strokeWidthSpinBox);
    styleLayout->addRow("Opacity:", opacityLayout);

    m_mainLayout->addWidget(m_styleGroup);

    // Connect signals
    connect(m_strokeColorButton, &QPushButton::clicked,
        this, &PropertiesPanel::onStrokeColorClicked);
    connect(m_fillColorButton, &QPushButton::clicked,
        this, &PropertiesPanel::onFillColorClicked);
    connect(m_strokeWidthSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &PropertiesPanel::onStyleChanged);
    connect(m_opacitySlider, &QSlider::valueChanged, [this](int value) {
        m_opacityLabel->setText(QString("%1%").arg(value));
        onStyleChanged();
        });
}

void PropertiesPanel::setupAnimationGroup()
{
    m_animationGroup = new QGroupBox("Animation");
    m_animationGroup->setStyleSheet(m_transformGroup->styleSheet());

    QFormLayout* animLayout = new QFormLayout(m_animationGroup);

    m_enableAnimationCheckBox = new QCheckBox("Enable Animation");
    m_durationSpinBox = new QSpinBox;
    m_durationSpinBox->setRange(1, 10000);
    m_durationSpinBox->setValue(1000);
    m_durationSpinBox->setSuffix(" ms");

    m_easingCombo = new QComboBox;
    m_easingCombo->addItems({ "Linear", "Ease In", "Ease Out", "Ease In Out" });

    animLayout->addRow(m_enableAnimationCheckBox);
    animLayout->addRow("Duration:", m_durationSpinBox);
    animLayout->addRow("Easing:", m_easingCombo);

    m_mainLayout->addWidget(m_animationGroup);
}

void PropertiesPanel::updateProperties(const QList<QGraphicsItem*>& selectedItems)
{
    m_updating = true;
    m_selectedItems = selectedItems;

    if (selectedItems.isEmpty()) {
        clearProperties();
    }
    else if (selectedItems.size() == 1) {
        updateTransformControls(selectedItems.first());
        updateStyleControls(selectedItems.first());
    }
    else {
        // Multiple items selected - show common properties
        // Implementation for multiple selection...
    }

    m_updating = false;
}

void PropertiesPanel::clearProperties()
{
    m_xSpinBox->setValue(0);
    m_ySpinBox->setValue(0);
    m_widthSpinBox->setValue(0);
    m_heightSpinBox->setValue(0);
    m_rotationSpinBox->setValue(0);
    m_strokeWidthSpinBox->setValue(2);
    m_opacitySlider->setValue(100);
}

void PropertiesPanel::updateTransformControls(QGraphicsItem* item)
{
    if (!item) return;

    QRectF rect = item->boundingRect();
    QPointF pos = item->pos();

    m_xSpinBox->setValue(pos.x());
    m_ySpinBox->setValue(pos.y());
    m_widthSpinBox->setValue(rect.width());
    m_heightSpinBox->setValue(rect.height());
    m_rotationSpinBox->setValue(item->rotation());
}

void PropertiesPanel::updateStyleControls(QGraphicsItem* item)
{
    // Implementation for updating style controls based on item type
    // This would check item type and update appropriate controls
}

void PropertiesPanel::onTransformChanged()
{
    if (m_updating || m_selectedItems.isEmpty()) return;

    for (QGraphicsItem* item : m_selectedItems) {
        if (item) {
            item->setPos(m_xSpinBox->value(), m_ySpinBox->value());
            item->setRotation(m_rotationSpinBox->value());
            // Handle width/height changes based on item type
        }
    }

    emit propertyChanged();
}

void PropertiesPanel::onStyleChanged()
{
    if (m_updating || m_selectedItems.isEmpty()) return;

    // Apply style changes to selected items
    // Implementation depends on item types

    emit propertyChanged();
}

void PropertiesPanel::onStrokeColorClicked()
{
    QColor color = QColorDialog::getColor(Qt::black, this, "Select Stroke Color");
    if (color.isValid()) {
        m_strokeColorButton->setStyleSheet(
            QString("background-color: %1; border: 1px solid gray;").arg(color.name()));
        onStyleChanged();
    }
}

void PropertiesPanel::onFillColorClicked()
{
    QColor color = QColorDialog::getColor(Qt::white, this, "Select Fill Color");
    if (color.isValid()) {
        m_fillColorButton->setStyleSheet(
            QString("background-color: %1; border: 1px solid gray;").arg(color.name()));
        onStyleChanged();
    }
}

// Additional panel implementations would continue here...
// ColorPanel, LayerManager, AlignmentPanel implementations follow similar patterns
