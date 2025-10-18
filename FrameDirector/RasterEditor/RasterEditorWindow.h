#pragma once

#include <QColor>
#include <QMainWindow>
#include <QPointer>
#include <QPainter>
#include <QPair>
#include <QStringList>
#include <QJsonObject>
#include <QByteArray>
#include <QVector>

#include "RasterDocument.h"

#include <third_party/libmypaint/mypaint-brush-settings.h>

class QCheckBox;
class QButtonGroup;
class QComboBox;
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

class RasterEditorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit RasterEditorWindow(QWidget* parent = nullptr);

    void setProjectContext(MainWindow* mainWindow, Canvas* canvas, Timeline* timeline, LayerManager* layerManager);
    QJsonObject toJson() const;
    void loadFromJson(const QJsonObject& json);
    void resetDocument();

signals:
    void visibilityChanged(bool visible);

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
    void onBrushSelected(int index);
    void onBrushOpacityChanged(int value);
    void onBrushHardnessChanged(int value);
    void onBrushSpacingChanged(int value);

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
    void loadAvailableBrushes();
    void applyBrushPreset(int index);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

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


    QComboBox* m_brushSelector;           // Brush selection dropdown
    QSlider* m_opacitySlider;             // Brush opacity
    QLabel* m_opacityValue;               // Opacity display
    QSlider* m_hardnessSlider;            // Brush hardness
    QLabel* m_hardnessValue;              // Hardness display
    QSlider* m_spacingSlider;             // Brush spacing
    QLabel* m_spacingValue;               // Spacing display
    QLabel* m_statusLabel;                // Status indicator

    struct BrushPreset
    {
        QString name;
        float size;
        float opacity;
        float hardness;
        float spacing;
        QVector<QPair<MyPaintBrushSetting, float>> settings;
    };

    QVector<BrushPreset> m_brushPresets;
    int m_activePresetIndex = -1;
};

