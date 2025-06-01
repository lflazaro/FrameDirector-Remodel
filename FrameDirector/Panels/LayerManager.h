#ifndef LAYERMANAGER_H
#define LAYERMANAGER_H

#include <QWidget>
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
#include <QGroupBox>  // Add this include

class MainWindow;

class LayerItem : public QListWidgetItem
{
public:
    LayerItem(const QString& name, int layerIndex);

    void setVisible(bool visible);
    void setLocked(bool locked);
    void setOpacity(int opacity);

    bool isVisible() const;
    bool isLocked() const;
    int getOpacity() const;
    int getLayerIndex() const;

    void updateDisplay(); // Add this method declaration

public: // Changed from private to public so LayerManager can access
    bool m_visible;
    bool m_locked;
    int m_opacity;
    int m_layerIndex;
};

class LayerManager : public QWidget
{
    Q_OBJECT

public:
    explicit LayerManager(MainWindow* parent = nullptr);

    void updateLayers();
    void setCurrentLayer(int index);
    int getCurrentLayer() const;

    void addLayer(const QString& name = QString());
    void removeLayer(int index);
    void duplicateLayer(int index);
    void moveLayerUp(int index);
    void moveLayerDown(int index);

signals:
    void layerAdded();
    void layerRemoved(int index);
    void layerDuplicated(int index);
    void layerMoved(int from, int to);
    void layerVisibilityChanged(int index, bool visible);
    void layerLockChanged(int index, bool locked);
    void layerOpacityChanged(int index, int opacity);
    void currentLayerChanged(int index);

private slots:
    void onAddLayerClicked();
    void onRemoveLayerClicked();
    void onDuplicateLayerClicked();
    void onMoveUpClicked();
    void onMoveDownClicked();
    void onLayerSelectionChanged();
    void onLayerContextMenu(const QPoint& pos);
    void onVisibilityToggled(bool visible);
    void onLockToggled(bool locked);
    void onOpacityChanged(int opacity);

private:
    void setupUI();
    void createLayerControls();
    void updateLayerControls();
    LayerItem* createLayerItem(const QString& name, int index);

    MainWindow* m_mainWindow;
    QVBoxLayout* m_mainLayout;

    // Layer list
    QListWidget* m_layerList;

    // Layer controls
    QPushButton* m_addLayerButton;
    QPushButton* m_removeLayerButton;
    QPushButton* m_duplicateLayerButton;
    QPushButton* m_moveUpButton;
    QPushButton* m_moveDownButton;

    // Layer properties
    QCheckBox* m_visibilityCheckBox;
    QCheckBox* m_lockCheckBox;
    QSlider* m_opacitySlider;
    QSpinBox* m_opacitySpinBox;
    QLabel* m_layerNameLabel;

    // Context menu
    QMenu* m_contextMenu;
    QAction* m_renameAction;
    QAction* m_deleteAction;
    QAction* m_duplicateAction;

    int m_currentLayer;
};

#endif // LAYERMANAGER_H
