#pragma once

#include <QColor>
#include <QDockWidget>
#include <QPointer>
#include <QPainter>
#include <QStringList>
#include <QJsonObject>
#include <QByteArray>

#include "RasterDocument.h"

class QCheckBox;
class QButtonGroup;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSlider;
class QSpinBox;
class QToolButton;
class QFrame;
class QSplitter;

class RasterBrushTool;
class RasterCanvasWidget;
class RasterEraserTool;
class RasterFillTool;
class RasterTool;
class RasterOnionSkinProvider;
class MainWindow;
class Canvas;
class Timeline;
class LayerManager;
class QGraphicsItem;

class RasterEditorWindow : public QDockWidget
{
    Q_OBJECT

public:
    explicit RasterEditorWindow(QWidget* parent = nullptr);

    void setProjectContext(MainWindow* mainWindow, Canvas* canvas, Timeline* timeline, LayerManager* layerManager);
    QJsonObject toJson() const;
    void loadFromJson(const QJsonObject& json);
    void resetDocument();

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
    void onProjectOnionToggled(bool enabled);
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
    void onExportToTimeline();
    void onProjectLayersChanged();
    void onProjectLayerRenamed(int index, const QString& name);
    void onProjectLayerAppearanceChanged();
    void onProjectFrameStructureChanged();
    void onTimelineLengthChanged(int frames);
    void onTimelineFrameChanged(int frame);

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
    void syncProjectLayers();
    void refreshProjectMetadata();
    void ensureDocumentFrameBounds();
    int clampProjectFrame(int frame) const;
    QList<QGraphicsItem*> rasterItemsForFrame(int layerIndex, int frame) const;
    QByteArray serializeDocumentState() const;

    QPointer<RasterDocument> m_document;
    RasterCanvasWidget* m_canvasWidget;
    RasterBrushTool* m_brushTool;
    RasterEraserTool* m_eraserTool;
    RasterFillTool* m_fillTool;
    RasterTool* m_activeTool;

    QLabel* m_frameLabel;
    QListWidget* m_layerList;
    QLabel* m_layerInfoLabel;

    QButtonGroup* m_toolButtonGroup;
    QToolButton* m_brushButton;
    QToolButton* m_eraserButton;
    QToolButton* m_fillButton;
    QSlider* m_brushSizeSlider;
    QLabel* m_brushSizeValue;
    QPushButton* m_colorButton;

    QCheckBox* m_onionSkinCheck;
    QCheckBox* m_projectOnionCheck;
    QSpinBox* m_onionBeforeSpin;
    QSpinBox* m_onionAfterSpin;

    QToolButton* m_addLayerButton;
    QToolButton* m_removeLayerButton;
    QDoubleSpinBox* m_opacitySpin;
    QComboBox* m_blendModeCombo;

    QColor m_primaryColor;
    MainWindow* m_mainWindow;
    Canvas* m_canvas;
    Timeline* m_timeline;
    LayerManager* m_layerManager;
    RasterOnionSkinProvider* m_onionProvider;
    QStringList m_projectLayerNames;
    bool m_layerMismatchWarned;
    bool m_projectContextInitialized;
    QString m_sessionId;
};

