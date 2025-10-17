#pragma once

#include <QDockWidget>

class QLabel;
class QListWidget;

class RasterEditorWindow : public QDockWidget
{
    Q_OBJECT

public:
    explicit RasterEditorWindow(QWidget* parent = nullptr);

public slots:
    void setCurrentFrame(int frame);
    void setCurrentLayer(int layer);

private:
    void ensureLayerPlaceholderCount(int count);

    QLabel* m_frameLabel;
    QLabel* m_canvasPlaceholder;
    QLabel* m_toolPlaceholder;
    QLabel* m_layerInfoLabel;
    QListWidget* m_layerList;
};

