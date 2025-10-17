#pragma once

#include <QColor>
#include <QDockWidget>
#include <QPointer>
#include <QPainter>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSlider;
class QSpinBox;
class QToolButton;

class RasterBrushTool;
class RasterCanvasWidget;
class RasterDocument;
class RasterEraserTool;
class RasterFillTool;
class RasterTool;

class RasterEditorWindow : public QDockWidget
{
    Q_OBJECT

public:
    explicit RasterEditorWindow(QWidget* parent = nullptr);

public slots:
    void setCurrentFrame(int frame);
    void setCurrentLayer(int layer);

private slots:
    void onToolChanged(int index);
    void onBrushSizeChanged(int value);
    void onColorButtonClicked();
    void onOnionSkinToggled(bool enabled);
    void onOnionBeforeChanged(int value);
    void onOnionAfterChanged(int value);
    void onLayerSelectionChanged(int index);
    void onLayerItemChanged(QListWidgetItem* item);
    void onAddLayer();
    void onRemoveLayer();
    void onOpacityChanged(double value);
    void onBlendModeChanged(int index);
    void onDocumentLayerListChanged();
    void onActiveLayerChanged(int index);
    void onActiveFrameChanged(int frame);
    void onLayerPropertiesUpdated(int index);
    void onOpenOra();
    void onSaveOra();

private:
    void initializeUi();
    void connectDocumentSignals();
    void refreshLayerList();
    void updateLayerInfo();
    void updateToolControls();
    void updateColorButton();
    void updateOnionSkinControls();
    void updateLayerPropertiesUi();
    int indexForBlendMode(QPainter::CompositionMode mode) const;

    QPointer<RasterDocument> m_document;
    RasterCanvasWidget* m_canvasWidget;
    RasterBrushTool* m_brushTool;
    RasterEraserTool* m_eraserTool;
    RasterFillTool* m_fillTool;
    RasterTool* m_activeTool;

    QLabel* m_frameLabel;
    QListWidget* m_layerList;
    QLabel* m_layerInfoLabel;

    QComboBox* m_toolSelector;
    QSlider* m_brushSizeSlider;
    QLabel* m_brushSizeValue;
    QPushButton* m_colorButton;

    QCheckBox* m_onionSkinCheck;
    QSpinBox* m_onionBeforeSpin;
    QSpinBox* m_onionAfterSpin;

    QToolButton* m_addLayerButton;
    QToolButton* m_removeLayerButton;
    QDoubleSpinBox* m_opacitySpin;
    QComboBox* m_blendModeCombo;

    QColor m_primaryColor;
};

