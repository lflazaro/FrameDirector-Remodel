#include "GradientDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColorDialog>
#include <QInputDialog>
#include <QPainter>
#include <algorithm>

// GradientPreview - Custom widget to display and interact with gradient stops

GradientPreview::GradientPreview(QGradientStops* stops, QWidget* parent)
    : QWidget(parent)
    , m_stops(stops)
{
    setMinimumHeight(40);
}

void GradientPreview::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    QRect rect = this->rect();
    QLinearGradient grad(rect.left(), rect.top(), rect.right(), rect.top());
    grad.setStops(*m_stops);
    painter.fillRect(rect, grad);
    painter.setPen(Qt::black);
    for (const auto& stop : *m_stops) {
        int x = rect.left() + stop.first * rect.width();
        painter.drawRect(x - 2, rect.bottom() - 10, 4, 10);
    }
}

void GradientPreview::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    QRect rect = this->rect();
    for (int i = 0; i < m_stops->size(); ++i) {
        int x = rect.left() + (*m_stops)[i].first * rect.width();
        QRect handle(x - 5, rect.bottom() - 15, 10, 15);
        if (handle.contains(event->pos())) {
            m_dragIndex = i;
            break;
        }
    }
}

void GradientPreview::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragIndex < 0) return;
    QRect rect = this->rect();
    double pos = (event->pos().x() - rect.left()) / double(rect.width());
    pos = std::clamp(pos, 0.0, 1.0);
    QGradientStop stop = (*m_stops)[m_dragIndex];
    stop.first = pos;
    m_stops->removeAt(m_dragIndex);
    int newIndex = 0;
    while (newIndex < m_stops->size() && pos > (*m_stops)[newIndex].first)
        ++newIndex;
    m_stops->insert(newIndex, stop);
    m_dragIndex = newIndex;
    update();
    emit stopsChanged();
}

void GradientPreview::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    m_dragIndex = -1;
}

GradientDialog::GradientDialog(const QGradientStops& stops, QWidget* parent)
    : QDialog(parent)
    , m_stops(stops)
{
    if (m_stops.isEmpty()) {
        m_stops << QGradientStop(0.0, Qt::red) << QGradientStop(1.0, Qt::blue);
    }

    setWindowTitle("Gradient Picker");
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_preview = new GradientPreview(&m_stops, this);
    mainLayout->addWidget(m_preview);

    m_stopList = new QListWidget(this);
    mainLayout->addWidget(m_stopList);

    QHBoxLayout* btnLayout = new QHBoxLayout;
    m_addButton = new QPushButton("Add", this);
    m_removeButton = new QPushButton("Remove", this);
    btnLayout->addWidget(m_addButton);
    btnLayout->addWidget(m_removeButton);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    QHBoxLayout* okLayout = new QHBoxLayout;
    QPushButton* ok = new QPushButton("OK", this);
    QPushButton* cancel = new QPushButton("Cancel", this);
    okLayout->addStretch();
    okLayout->addWidget(ok);
    okLayout->addWidget(cancel);
    mainLayout->addLayout(okLayout);

    connect(m_addButton, &QPushButton::clicked, this, &GradientDialog::addStop);
    connect(m_removeButton, &QPushButton::clicked, this, &GradientDialog::removeStop);
    connect(m_stopList, &QListWidget::itemDoubleClicked, this, &GradientDialog::editStop);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_preview, &GradientPreview::stopsChanged, this, [this]() { refreshStopList(); updatePreview(); });

    refreshStopList();
    updatePreview();
}

QGradientStops GradientDialog::getStops() const
{
    return m_stops;
}

void GradientDialog::addStop()
{
    QColor color = QColorDialog::getColor(Qt::white, this, "Select Color");
    if (!color.isValid()) return;
    bool ok = false;
    double pos = QInputDialog::getDouble(this, "Stop Position", "Position (0-1)", 0.5, 0.0, 1.0, 2, &ok);
    if (!ok) return;
    m_stops << QGradientStop(pos, color);
    std::sort(m_stops.begin(), m_stops.end(), [](const QGradientStop& a, const QGradientStop& b) { return a.first < b.first; });
    refreshStopList();
    updatePreview();
}

void GradientDialog::removeStop()
{
    QListWidgetItem* item = m_stopList->currentItem();
    if (!item) return;
    int index = m_stopList->row(item);
    if (m_stops.size() <= 2) return; // keep at least two stops
    m_stops.removeAt(index);
    refreshStopList();
    updatePreview();
}

void GradientDialog::editStop(QListWidgetItem* item)
{
    int index = m_stopList->row(item);
    if (index < 0 || index >= m_stops.size()) return;
    QColor color = QColorDialog::getColor(m_stops[index].second, this, "Select Color");
    if (!color.isValid()) return;
    m_stops[index].second = color;
    bool ok = false;
    double pos = QInputDialog::getDouble(this, "Stop Position", "Position (0-1)", m_stops[index].first, 0.0, 1.0, 2, &ok);
    if (ok) m_stops[index].first = pos;
    std::sort(m_stops.begin(), m_stops.end(), [](const QGradientStop& a, const QGradientStop& b) { return a.first < b.first; });
    refreshStopList();
    updatePreview();
}

void GradientDialog::refreshStopList()
{
    m_stopList->clear();
    for (const auto& stop : m_stops) {
        QListWidgetItem* item = new QListWidgetItem(QString("Pos %1 Color %2")
            .arg(stop.first, 0, 'f', 2)
            .arg(stop.second.name()));
        item->setBackground(QBrush(stop.second));
        m_stopList->addItem(item);
    }
}

void GradientDialog::updatePreview()
{
    m_preview->update();
}