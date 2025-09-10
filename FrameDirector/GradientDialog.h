#ifndef GRADIENTDIALOG_H
#define GRADIENTDIALOG_H

#include <QDialog>
#include <QGradientStops>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>

class GradientPreview : public QWidget
{
    Q_OBJECT
public:
    explicit GradientPreview(QGradientStops* stops, QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    QGradientStops* m_stops;
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
