#ifndef GRADIENTDIALOG_H
#define GRADIENTDIALOG_H

#include <QDialog>
#include <QGradientStops>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>
#include <QMouseEvent>

class GradientPreview : public QWidget
{
    Q_OBJECT
public:
    explicit GradientPreview(QGradientStops* stops, QWidget* parent = nullptr);
signals:
    void stopsChanged();
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
private:
    QGradientStops* m_stops;
    int m_dragIndex = -1;
};

class GradientDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GradientDialog(const QGradientStops& stops, QWidget* parent = nullptr);
    QGradientStops getStops() const;

private slots:
    void addStop();
    void removeStop();
    void editStop(QListWidgetItem* item);
    void updatePreview();

private:
    void refreshStopList();

    QGradientStops m_stops;
    GradientPreview* m_preview;
    QListWidget* m_stopList;
    QPushButton* m_addButton;
    QPushButton* m_removeButton;
};

#endif // GRADIENTDIALOG_H
