#ifndef COLORPANEL_H
#define COLORPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QColorDialog>
#include <QColor>
#include <QPainter>
#include <QMouseEvent>

class MainWindow;
class ColorWheel;
class ColorPreview;

class ColorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ColorPanel(MainWindow* parent = nullptr);

    void setStrokeColor(const QColor& color);
    void setFillColor(const QColor& color);
    QColor getStrokeColor() const;
    QColor getFillColor() const;

signals:
    void strokeColorChanged(const QColor& color);
    void fillColorChanged(const QColor& color);

private slots:
    void onStrokeColorClicked();
    void onFillColorClicked();
    void onColorChanged(const QColor& color);
    void onSwapColors();
    void onResetColors();

private:
    void setupUI();
    void setupColorSwatches();
    void setupColorControls();
    void updateColorDisplay();

    MainWindow* m_mainWindow;
    QVBoxLayout* m_mainLayout;

    // Color display
    ColorPreview* m_colorPreview;
    QPushButton* m_strokeColorButton;
    QPushButton* m_fillColorButton;
    QPushButton* m_swapButton;
    QPushButton* m_resetButton;

    // Color wheel
    ColorWheel* m_colorWheel;

    // RGB controls
    QSlider* m_redSlider;
    QSlider* m_greenSlider;
    QSlider* m_blueSlider;
    QSlider* m_alphaSlider;
    QSpinBox* m_redSpinBox;
    QSpinBox* m_greenSpinBox;
    QSpinBox* m_blueSpinBox;
    QSpinBox* m_alphaSpinBox;

    // Color swatches
    QGridLayout* m_swatchesLayout;
    QList<QPushButton*> m_colorSwatches;

    QColor m_strokeColor;
    QColor m_fillColor;
    QColor m_currentColor;
    bool m_updating;
};

#endif // COLORPANEL_H