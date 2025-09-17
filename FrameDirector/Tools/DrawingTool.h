// Tools/DrawingTool.h - Enhanced with stabilizer and settings
#ifndef DRAWINGTOOL_H
#define DRAWINGTOOL_H

#include "Tool.h"
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QPointF>
#include <QColor>
#include <QTimer>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>

class QMouseEvent;
class MainWindow;

class DrawingTool : public Tool
{
    Q_OBJECT

public:
    explicit DrawingTool(MainWindow* mainWindow, QObject* parent = nullptr);

    void mousePressEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos) override;
    void mouseReleaseEvent(QMouseEvent* event, const QPointF& scenePos) override;
    QCursor getCursor() const override;

    // Settings and properties
    void setStrokeWidth(double width);
    void setStrokeColor(const QColor& color);
    void setStabilizerAmount(int amount);
    void setSmoothingEnabled(bool enabled);
    void setPressureSensitivity(bool enabled);

    double getStrokeWidth() const;
    QColor getStrokeColor() const;
    int getStabilizerAmount() const;
    bool isSmoothingEnabled() const;
    bool isPressureSensitive() const;

    // Settings dialog
    void showSettingsDialog();

private slots:
    void onStabilizerTimeout();

private:
    void addPointToPath(const QPointF& point);
    void applySmoothingToPath();
    void updateStabilizerDelay();
    // Drawing state
    bool m_drawing;
    QGraphicsPathItem* m_currentPath;
    QPainterPath m_path;
    QPointF m_lastPoint;

    // Drawing settings
    double m_strokeWidth;
    QColor m_strokeColor;
    int m_stabilizerAmount;
    bool m_smoothingEnabled;
    bool m_pressureSensitive;

    // Stabilizer system
    QTimer* m_stabilizerTimer;
    QList<QPointF> m_stabilizerPoints;
    QPointF m_smoothedPoint;
    bool m_hasSmoothedPoint;
};

#endif // DRAWINGTOOL_H
