#ifndef ALIGNMENTPANEL_H
#define ALIGNMENTPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QGroupBox>
#include <QLabel>
#include "../MainWindow.h"

class AlignmentPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AlignmentPanel(MainWindow* parent = nullptr);

signals:
    void alignmentRequested(MainWindow::AlignmentType alignment);

private slots:
    void onAlignmentClicked();

private:
    void setupUI();
    void createAlignmentGroup();
    void createDistributeGroup();
    void createArrangeGroup();

    MainWindow* m_mainWindow;
    QVBoxLayout* m_mainLayout;

    // Alignment buttons
    QGroupBox* m_alignmentGroup;
    QPushButton* m_alignLeftButton;
    QPushButton* m_alignCenterButton;
    QPushButton* m_alignRightButton;
    QPushButton* m_alignTopButton;
    QPushButton* m_alignMiddleButton;
    QPushButton* m_alignBottomButton;

    // Distribution buttons
    QGroupBox* m_distributeGroup;
    QPushButton* m_distributeHorizontalButton;
    QPushButton* m_distributeVerticalButton;

    // Arrange buttons
    QGroupBox* m_arrangeGroup;
    QPushButton* m_bringToFrontButton;
    QPushButton* m_bringForwardButton;
    QPushButton* m_sendBackwardButton;
    QPushButton* m_sendToBackButton;
};
#endif // ALIGNMENTPANEL_H