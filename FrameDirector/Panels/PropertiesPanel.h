#ifndef PROPERTIESPANEL_H
#define PROPERTIESPANEL_H

#include <QWidget>
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

class MainWindow;

class PropertiesPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertiesPanel(MainWindow* parent = nullptr);

    void updateProperties(const QList<QGraphicsItem*>& selectedItems);
    void clearProperties();

signals:
    void propertyChanged();

private slots:
    void onTransformChanged();
    void onStyleChanged();
    void onStrokeColorClicked();
    void onFillColorClicked();

private:
    void setupUI();
    void setupTransformGroup();
    void setupStyleGroup();
    void setupAnimationGroup();
    void updateTransformControls(QGraphicsItem* item);
    void updateStyleControls(QGraphicsItem* item);

    MainWindow* m_mainWindow;
    QVBoxLayout* m_mainLayout;

    // Transform properties
    QGroupBox* m_transformGroup;
    QDoubleSpinBox* m_xSpinBox;
    QDoubleSpinBox* m_ySpinBox;
    QDoubleSpinBox* m_widthSpinBox;
    QDoubleSpinBox* m_heightSpinBox;
    QDoubleSpinBox* m_rotationSpinBox;
    QDoubleSpinBox* m_scaleXSpinBox;
    QDoubleSpinBox* m_scaleYSpinBox;

    // Style properties
    QGroupBox* m_styleGroup;
    QPushButton* m_strokeColorButton;
    QPushButton* m_fillColorButton;
    QDoubleSpinBox* m_strokeWidthSpinBox;
    QSlider* m_opacitySlider;
    QLabel* m_opacityLabel;
    QComboBox* m_strokeStyleCombo;

    // Animation properties
    QGroupBox* m_animationGroup;
    QCheckBox* m_enableAnimationCheckBox;
    QSpinBox* m_durationSpinBox;
    QComboBox* m_easingCombo;

    QList<QGraphicsItem*> m_selectedItems;
    bool m_updating;
};

#endif // PROPERTIESPANEL_H