// Panels/LayerManager.cpp
#include "LayerManager.h"
#include "../MainWindow.h"
#include "../Canvas.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QDrag>
#include <QMimeData>
#include <QPainter>
#include <QApplication>

// LayerItem implementation
LayerItem::LayerItem(const QString& name, int layerIndex)
    : QListWidgetItem(name)
    , m_visible(true)
    , m_locked(false)
    , m_opacity(100)
    , m_layerIndex(layerIndex)
{
    setFlags(flags() | Qt::ItemIsDragEnabled);
    updateDisplay();
}

void LayerItem::setVisible(bool visible)
{
    m_visible = visible;
    updateDisplay();
}

void LayerItem::setLocked(bool locked)
{
    m_locked = locked;
    updateDisplay();
}

void LayerItem::setOpacity(int opacity)
{
    m_opacity = qBound(0, opacity, 100);
    updateDisplay();
}

bool LayerItem::isVisible() const
{
    return m_visible;
}

bool LayerItem::isLocked() const
{
    return m_locked;
}

int LayerItem::getOpacity() const
{
    return m_opacity;
}

int LayerItem::getLayerIndex() const
{
    return m_layerIndex;
}

void LayerItem::updateDisplay()
{
    QString displayText = text();

    // Add visual indicators
    QString indicators;
    if (!m_visible) indicators += " [Hidden]";
    if (m_locked) indicators += " [Locked]";
    if (m_opacity < 100) indicators += QString(" [%1%]").arg(m_opacity);

    setToolTip(displayText + indicators);

    // Set text color based on state
    if (!m_visible) {
        setForeground(QColor(128, 128, 128)); // Gray for hidden
    }
    else if (m_locked) {
        setForeground(QColor(255, 200, 100)); // Orange for locked
    }
    else {
        setForeground(QColor(255, 255, 255)); // White for normal
    }
}

// LayerManager implementation
LayerManager::LayerManager(MainWindow* parent)
    : QWidget(parent)
    , m_mainWindow(parent)
    , m_currentLayer(0)
{
    setupUI();
}

void LayerManager::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(4);

    // Header
    QLabel* headerLabel = new QLabel("Layers");
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

    // Layer list
    m_layerList = new QListWidget;
    m_layerList->setDragDropMode(QAbstractItemView::InternalMove);
    m_layerList->setDefaultDropAction(Qt::MoveAction);
    m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layerList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_layerList->setStyleSheet(
        "QListWidget {"
        "    background-color: #2D2D30;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    selection-background-color: #007ACC;"
        "    outline: none;"
        "}"
        "QListWidget::item {"
        "    padding: 6px 4px;"
        "    border-bottom: 1px solid #3E3E42;"
        "    min-height: 20px;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #007ACC;"
        "    color: white;"
        "}"
        "QListWidget::item:hover {"
        "    background-color: #4A4A4F;"
        "}"
    );

    m_layerList->setMinimumHeight(150);
    m_mainLayout->addWidget(m_layerList);

    // Layer control buttons
    createLayerControls();

    // Layer properties
    QGroupBox* propertiesGroup = new QGroupBox("Layer Properties");
    propertiesGroup->setStyleSheet(
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

    QVBoxLayout* propertiesLayout = new QVBoxLayout(propertiesGroup);

    // Layer name
    m_layerNameLabel = new QLabel("No layer selected");
    m_layerNameLabel->setStyleSheet("color: #CCCCCC; font-weight: normal;");
    propertiesLayout->addWidget(m_layerNameLabel);

    // Visibility checkbox
    m_visibilityCheckBox = new QCheckBox("Visible");
    m_visibilityCheckBox->setStyleSheet(
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
        "    image: url(:/icons/check.png);"
        "}"
    );
    propertiesLayout->addWidget(m_visibilityCheckBox);

    // Lock checkbox
    m_lockCheckBox = new QCheckBox("Locked");
    m_lockCheckBox->setStyleSheet(m_visibilityCheckBox->styleSheet());
    propertiesLayout->addWidget(m_lockCheckBox);

    // Opacity controls
    QHBoxLayout* opacityLayout = new QHBoxLayout;
    QLabel* opacityLabel = new QLabel("Opacity:");
    opacityLabel->setStyleSheet("color: white; font-weight: normal;");

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

    m_opacitySpinBox = new QSpinBox;
    m_opacitySpinBox->setRange(0, 100);
    m_opacitySpinBox->setValue(100);
    m_opacitySpinBox->setSuffix("%");
    m_opacitySpinBox->setMaximumWidth(60);
    m_opacitySpinBox->setStyleSheet(
        "QSpinBox {"
        "    background-color: #2D2D30;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 2px;"
        "    padding: 2px;"
        "}"
        "QSpinBox::up-button, QSpinBox::down-button {"
        "    background-color: #3E3E42;"
        "    border: 1px solid #5A5A5C;"
        "}"
        "QSpinBox::up-button:hover, QSpinBox::down-button:hover {"
        "    background-color: #4A4A4F;"
        "}"
    );

    opacityLayout->addWidget(opacityLabel);
    opacityLayout->addWidget(m_opacitySlider);
    opacityLayout->addWidget(m_opacitySpinBox);
    propertiesLayout->addLayout(opacityLayout);

    m_mainLayout->addWidget(propertiesGroup);

    // Setup context menu
    m_contextMenu = new QMenu(this);
    m_renameAction = m_contextMenu->addAction("Rename Layer");
    m_duplicateAction = m_contextMenu->addAction("Duplicate Layer");
    m_contextMenu->addSeparator();
    m_deleteAction = m_contextMenu->addAction("Delete Layer");

    // Connect signals
    connect(m_layerList, &QListWidget::currentRowChanged, this, &LayerManager::onLayerSelectionChanged);
    connect(m_layerList, &QListWidget::customContextMenuRequested, this, &LayerManager::onLayerContextMenu);
    connect(m_layerList, &QListWidget::itemChanged, [this](QListWidgetItem* item) {
        LayerItem* layerItem = static_cast<LayerItem*>(item);
        if (layerItem) {
            // Handle layer name change
            emit currentLayerChanged(layerItem->getLayerIndex());
        }
        });

    connect(m_visibilityCheckBox, &QCheckBox::toggled, this, &LayerManager::onVisibilityToggled);
    connect(m_lockCheckBox, &QCheckBox::toggled, this, &LayerManager::onLockToggled);
    connect(m_opacitySlider, &QSlider::valueChanged, this, &LayerManager::onOpacityChanged);
    connect(m_opacitySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &LayerManager::onOpacityChanged);

    // Sync opacity controls
    connect(m_opacitySlider, &QSlider::valueChanged, m_opacitySpinBox, &QSpinBox::setValue);
    connect(m_opacitySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), m_opacitySlider, &QSlider::setValue);

    // Context menu actions
    connect(m_renameAction, &QAction::triggered, [this]() {
        LayerItem* item = static_cast<LayerItem*>(m_layerList->currentItem());
        if (item) {
            bool ok;
            QString newName = QInputDialog::getText(this, "Rename Layer", "Layer name:",
                QLineEdit::Normal, item->text(), &ok);
            if (ok && !newName.isEmpty()) {
                item->setText(newName);
            }
        }
        });

    connect(m_duplicateAction, &QAction::triggered, [this]() {
        if (m_currentLayer >= 0) {
            duplicateLayer(m_currentLayer);
        }
        });

    connect(m_deleteAction, &QAction::triggered, [this]() {
        if (m_layerList->count() > 1 && m_currentLayer >= 0) {
            int ret = QMessageBox::question(this, "Delete Layer",
                "Are you sure you want to delete this layer?",
                QMessageBox::Yes | QMessageBox::No);
            if (ret == QMessageBox::Yes) {
                removeLayer(m_currentLayer);
            }
        }
        else {
            QMessageBox::information(this, "Cannot Delete",
                "Cannot delete the last remaining layer.");
        }
        });

    // Initialize with empty state
    updateLayerControls();
}

void LayerManager::createLayerControls()
{
    QHBoxLayout* controlsLayout = new QHBoxLayout;
    controlsLayout->setSpacing(2);

    QString buttonStyle =
        "QPushButton {"
        "    background-color: #3E3E42;"
        "    color: white;"
        "    border: 1px solid #5A5A5C;"
        "    border-radius: 3px;"
        "    padding: 4px 6px;"
        "    font-size: 11px;"
        "    font-weight: bold;"
        "    min-width: 25px;"
        "    min-height: 25px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #4A4A4F;"
        "    border: 1px solid #007ACC;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #007ACC;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #2D2D30;"
        "    color: #666666;"
        "    border: 1px solid #444444;"
        "}";

    m_addLayerButton = new QPushButton("+");
    m_addLayerButton->setToolTip("Add New Layer");
    m_addLayerButton->setStyleSheet(buttonStyle);

    m_removeLayerButton = new QPushButton("-");
    m_removeLayerButton->setToolTip("Remove Selected Layer");
    m_removeLayerButton->setStyleSheet(buttonStyle);

    m_duplicateLayerButton = new QPushButton("⧉");
    m_duplicateLayerButton->setToolTip("Duplicate Selected Layer");
    m_duplicateLayerButton->setStyleSheet(buttonStyle);

    m_moveUpButton = new QPushButton("↑");
    m_moveUpButton->setToolTip("Move Layer Up");
    m_moveUpButton->setStyleSheet(buttonStyle);

    m_moveDownButton = new QPushButton("↓");
    m_moveDownButton->setToolTip("Move Layer Down");
    m_moveDownButton->setStyleSheet(buttonStyle);

    controlsLayout->addWidget(m_addLayerButton);
    controlsLayout->addWidget(m_removeLayerButton);
    controlsLayout->addWidget(m_duplicateLayerButton);
    controlsLayout->addSpacing(10);
    controlsLayout->addWidget(m_moveUpButton);
    controlsLayout->addWidget(m_moveDownButton);
    controlsLayout->addStretch();

    m_mainLayout->addLayout(controlsLayout);

    // Connect button signals
    connect(m_addLayerButton, &QPushButton::clicked, this, &LayerManager::onAddLayerClicked);
    connect(m_removeLayerButton, &QPushButton::clicked, this, &LayerManager::onRemoveLayerClicked);
    connect(m_duplicateLayerButton, &QPushButton::clicked, this, &LayerManager::onDuplicateLayerClicked);
    connect(m_moveUpButton, &QPushButton::clicked, this, &LayerManager::onMoveUpClicked);
    connect(m_moveDownButton, &QPushButton::clicked, this, &LayerManager::onMoveDownClicked);
}

void LayerManager::updateLayers()
{
    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    if (!canvas) return;

    // FIXED: Store current layer states before clearing
    QHash<int, LayerState> savedStates;
    for (int i = 0; i < m_layerList->count(); ++i) {
        LayerItem* item = static_cast<LayerItem*>(m_layerList->item(i));
        if (item) {
            LayerState state;
            state.visible = item->isVisible();
            state.locked = item->isLocked();
            state.opacity = item->getOpacity();
            savedStates[i] = state;
        }
    }

    // Clear current list
    m_layerList->clear();

    // Populate with layer names, preserving states
    for (int i = 0; i < canvas->getLayerCount(); ++i) {
        QString layerName = (i == 0) ? "Background" : QString("Layer %1").arg(i);
        LayerItem* item = new LayerItem(layerName, i);

        // FIXED: Restore saved states if they exist
        if (savedStates.contains(i)) {
            const LayerState& state = savedStates[i];
            item->setVisible(state.visible);
            item->setLocked(state.locked);
            item->setOpacity(state.opacity);

            // Apply the preserved state to the canvas
            canvas->setLayerVisible(i, state.visible);
            canvas->setLayerLocked(i, state.locked);
            canvas->setLayerOpacity(i, state.opacity / 100.0);
        }
        else {
            // Default values for new layers
            item->setVisible(true);
            item->setLocked(false);
            item->setOpacity(100);
        }

        m_layerList->addItem(item);
    }

    // Set current layer
    if (canvas->getCurrentLayer() >= 0 && canvas->getCurrentLayer() < m_layerList->count()) {
        m_layerList->setCurrentRow(canvas->getCurrentLayer());
        m_currentLayer = canvas->getCurrentLayer();
    }

    updateLayerControls();
    qDebug() << "Updated layers with preserved states";
}

void LayerManager::setCurrentLayer(int index)
{
    if (index >= 0 && index < m_layerList->count()) {
        m_currentLayer = index;
        m_layerList->setCurrentRow(index);
        updateLayerControls();
    }
}

int LayerManager::getCurrentLayer() const
{
    return m_currentLayer;
}

void LayerManager::addLayer(const QString& name)
{
    int layerIndex = m_layerList->count();
    LayerItem* item = createLayerItem(name.isEmpty() ? QString("Layer %1").arg(layerIndex + 1) : name, layerIndex);
    m_layerList->addItem(item);

    // Select the new layer
    m_layerList->setCurrentItem(item);
    m_currentLayer = layerIndex;

    updateLayerControls();
    emit layerAdded();
}

void LayerManager::removeLayer(int index)
{
    if (index >= 0 && index < m_layerList->count() && m_layerList->count() > 1) {
        delete m_layerList->takeItem(index);

        // Update layer indices for remaining items
        for (int i = index; i < m_layerList->count(); ++i) {
            LayerItem* item = static_cast<LayerItem*>(m_layerList->item(i));
            if (item) {
                item->m_layerIndex = i;
            }
        }

        // Adjust current layer
        if (m_currentLayer >= m_layerList->count()) {
            m_currentLayer = m_layerList->count() - 1;
        }
        if (m_currentLayer >= 0) {
            m_layerList->setCurrentRow(m_currentLayer);
        }

        updateLayerControls();
        emit layerRemoved(index);
    }
}

void LayerManager::duplicateLayer(int index)
{
    if (index >= 0 && index < m_layerList->count()) {
        LayerItem* originalItem = static_cast<LayerItem*>(m_layerList->item(index));
        if (originalItem) {
            QString newName = originalItem->text() + " Copy";
            LayerItem* newItem = createLayerItem(newName, m_layerList->count());

            // Copy properties
            newItem->setVisible(originalItem->isVisible());
            newItem->setLocked(originalItem->isLocked());
            newItem->setOpacity(originalItem->getOpacity());

            m_layerList->addItem(newItem);
            m_layerList->setCurrentItem(newItem);
            m_currentLayer = m_layerList->count() - 1;

            updateLayerControls();
            emit layerDuplicated(index);
        }
    }
}

void LayerManager::moveLayerUp(int index)
{
    if (index > 0 && index < m_layerList->count()) {
        QListWidgetItem* item = m_layerList->takeItem(index);
        m_layerList->insertItem(index - 1, item);
        m_layerList->setCurrentRow(index - 1);
        m_currentLayer = index - 1;

        // Update layer indices
        static_cast<LayerItem*>(m_layerList->item(index - 1))->m_layerIndex = index - 1;
        static_cast<LayerItem*>(m_layerList->item(index))->m_layerIndex = index;


        Canvas* canvas = m_mainWindow->findChild<Canvas*>();
        if (canvas) {
            canvas->moveLayer(index, index - 1);
        }

        updateLayerControls();
        emit layerMoved(index, index - 1);
    }
}

void LayerManager::moveLayerDown(int index)
{
    if (index >= 0 && index < m_layerList->count() - 1) {
        QListWidgetItem* item = m_layerList->takeItem(index);
        m_layerList->insertItem(index + 1, item);
        m_layerList->setCurrentRow(index + 1);
        m_currentLayer = index + 1;

        // Update layer indices
        static_cast<LayerItem*>(m_layerList->item(index))->m_layerIndex = index;
        static_cast<LayerItem*>(m_layerList->item(index + 1))->m_layerIndex = index + 1;


        Canvas* canvas = m_mainWindow->findChild<Canvas*>();
        if (canvas) {
            canvas->moveLayer(index, index + 1);
        }

        updateLayerControls();
        emit layerMoved(index, index + 1);
    }
}


void LayerManager::onAddLayerClicked()
{
    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    if (canvas) {
        QString layerName = QString("Layer %1").arg(canvas->getLayerCount() + 1);

        // FIXED: Add layer without clearing existing states
        int newIndex = canvas->addLayer(layerName);

        // Add the new layer item to the list without calling updateLayers()
        LayerItem* newItem = new LayerItem(layerName, newIndex);
        newItem->setVisible(true);      // Default visible
        newItem->setLocked(false);      // Default unlocked  
        newItem->setOpacity(100);       // Default full opacity
        m_layerList->addItem(newItem);

        // Select the new layer
        m_currentLayer = newIndex;
        m_layerList->setCurrentRow(m_currentLayer);
        canvas->setCurrentLayer(m_currentLayer);

        updateLayerControls();
        emit layerAdded();

        qDebug() << "Added layer without resetting existing layer properties";
    }
}


void LayerManager::onRemoveLayerClicked()
{
    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    if (canvas && m_currentLayer >= 0 && canvas->getLayerCount() > 1) {

        // Remove the layer from canvas
        canvas->removeLayer(m_currentLayer);

        // FIXED: Remove just the specific layer item from the list
        delete m_layerList->takeItem(m_currentLayer);

        // Update indices for remaining items
        for (int i = m_currentLayer; i < m_layerList->count(); ++i) {
            LayerItem* item = static_cast<LayerItem*>(m_layerList->item(i));
            if (item) {
                item->m_layerIndex = i;
            }
        }

        // Adjust current layer selection
        if (m_currentLayer >= m_layerList->count()) {
            m_currentLayer = m_layerList->count() - 1;
        }
        if (m_currentLayer >= 0) {
            m_layerList->setCurrentRow(m_currentLayer);
            canvas->setCurrentLayer(m_currentLayer);
        }

        updateLayerControls();
        emit layerRemoved(m_currentLayer);

        qDebug() << "Removed layer without affecting other layer properties";
    }
    else {
        QMessageBox::information(this, "Cannot Delete",
            "Cannot delete the last remaining layer.");
    }
}



void LayerManager::onDuplicateLayerClicked()
{
    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    if (canvas && m_currentLayer >= 0) {
        LayerItem* currentItem = static_cast<LayerItem*>(m_layerList->item(m_currentLayer));
        if (!currentItem) return;

        QString currentLayerName = currentItem->text();
        QString newName = currentLayerName + " Copy";

        // Add new layer to canvas
        int newIndex = canvas->addLayer(newName);

        // FIXED: Create duplicate with same properties as original
        LayerItem* newItem = new LayerItem(newName, newIndex);
        newItem->setVisible(currentItem->isVisible());
        newItem->setLocked(currentItem->isLocked());
        newItem->setOpacity(currentItem->getOpacity());

        // Apply properties to canvas layer
        canvas->setLayerVisible(newIndex, currentItem->isVisible());
        canvas->setLayerLocked(newIndex, currentItem->isLocked());
        canvas->setLayerOpacity(newIndex, currentItem->getOpacity() / 100.0);

        m_layerList->addItem(newItem);

        // Select the new layer
        m_currentLayer = newIndex;
        m_layerList->setCurrentRow(m_currentLayer);
        canvas->setCurrentLayer(m_currentLayer);

        updateLayerControls();
        emit layerDuplicated(m_currentLayer);

        qDebug() << "Duplicated layer with preserved properties";
    }
}


void LayerManager::onMoveUpClicked()
{
    moveLayerUp(m_currentLayer);
}

void LayerManager::onMoveDownClicked()
{
    moveLayerDown(m_currentLayer);
}

void LayerManager::onLayerSelectionChanged()
{
    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    m_currentLayer = m_layerList->currentRow();

    if (canvas && m_currentLayer >= 0) {
        canvas->setCurrentLayer(m_currentLayer);
    }

    updateLayerControls();
    emit currentLayerChanged(m_currentLayer);
}

void LayerManager::onLayerContextMenu(const QPoint& pos)
{
    if (m_layerList->itemAt(pos)) {
        m_contextMenu->exec(m_layerList->mapToGlobal(pos));
    }
}


void LayerManager::onVisibilityToggled(bool visible)
{
    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    if (canvas && m_currentLayer >= 0) {
        canvas->setLayerVisible(m_currentLayer, visible);

        LayerItem* item = static_cast<LayerItem*>(m_layerList->currentItem());
        if (item) {
            item->setVisible(visible);
            qDebug() << "Layer" << m_currentLayer << "visibility changed to:" << visible;
        }
        emit layerVisibilityChanged(m_currentLayer, visible);
    }
}

void LayerManager::onLockToggled(bool locked)
{
    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    if (canvas && m_currentLayer >= 0) {
        canvas->setLayerLocked(m_currentLayer, locked);

        LayerItem* item = static_cast<LayerItem*>(m_layerList->currentItem());
        if (item) {
            item->setLocked(locked);
            qDebug() << "Layer" << m_currentLayer << "lock state changed to:" << locked;
        }
        emit layerLockChanged(m_currentLayer, locked);
    }
}

// FIXED: Enhanced opacity change handling
void LayerManager::onOpacityChanged(int opacity)
{
    Canvas* canvas = m_mainWindow->findChild<Canvas*>();
    if (canvas && m_currentLayer >= 0) {
        canvas->setLayerOpacity(m_currentLayer, opacity / 100.0);

        LayerItem* item = static_cast<LayerItem*>(m_layerList->currentItem());
        if (item) {
            item->setOpacity(opacity);
            qDebug() << "Layer" << m_currentLayer << "opacity changed to:" << opacity << "%";
        }
        emit layerOpacityChanged(m_currentLayer, opacity);
    }
}

void LayerManager::updateLayerControls()
{
    bool hasLayers = m_layerList->count() > 0;
    bool hasSelection = m_currentLayer >= 0 && m_currentLayer < m_layerList->count();
    bool canMoveUp = hasSelection && m_currentLayer > 0;
    bool canMoveDown = hasSelection && m_currentLayer < m_layerList->count() - 1;
    bool canDelete = hasLayers && m_layerList->count() > 1;

    m_removeLayerButton->setEnabled(canDelete);
    m_duplicateLayerButton->setEnabled(hasSelection);
    m_moveUpButton->setEnabled(canMoveUp);
    m_moveDownButton->setEnabled(canMoveDown);

    if (hasSelection) {
        LayerItem* item = static_cast<LayerItem*>(m_layerList->item(m_currentLayer));
        if (item) {
            m_layerNameLabel->setText(item->text());
            m_visibilityCheckBox->setChecked(item->isVisible());
            m_lockCheckBox->setChecked(item->isLocked());
            m_opacitySlider->setValue(item->getOpacity());
            m_opacitySpinBox->setValue(item->getOpacity());

            m_visibilityCheckBox->setEnabled(true);
            m_lockCheckBox->setEnabled(true);
            m_opacitySlider->setEnabled(true);
            m_opacitySpinBox->setEnabled(true);
        }
    }
    else {
        m_layerNameLabel->setText("No layer selected");
        m_visibilityCheckBox->setEnabled(false);
        m_lockCheckBox->setEnabled(false);
        m_opacitySlider->setEnabled(false);
        m_opacitySpinBox->setEnabled(false);
    }
}

LayerItem* LayerManager::createLayerItem(const QString& name, int index)
{
    return new LayerItem(name, index);
}