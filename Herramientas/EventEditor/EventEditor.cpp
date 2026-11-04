#include"EventEditor.h"
#include<QAction>
#include<QAbstractItemModel>
#include<QByteArray>
#include<QCloseEvent>
#include<QColorDialog>
#include<QComboBox>
#include<QDialog>
#include<QDialogButtonBox>
#include<QDir>
#include<QDoubleSpinBox>
#include<QFile>
#include<QFileDialog>
#include<QFileInfo>
#include<QFormLayout>
#include<QGroupBox>
#include<QHBoxLayout>
#include<QJsonArray>
#include<QJsonDocument>
#include<QJsonObject>
#include<QKeySequence>
#include<QLabel>
#include<QLineEdit>
#include<QMenu>
#include<QMessageBox>
#include<QMouseEvent>
#include<QPainter>
#include<QPushButton>
#include<QRegularExpression>
#include<QScrollArea>
#include<QSignalBlocker>
#include<QScrollBar>
#include<QSlider>
#include<QSaveFile>
#include<QSpinBox>
#include<QStatusBar>
#include<QStringList>
#include<QToolBar>
#include<QVBoxLayout>
#include<QHeaderView>
#include<QIntValidator>
#include<QStyledItemDelegate>
#include<QTabWidget>
#include<QTableWidget>
#include<QtMath>
#include<algorithm>
#include<cstddef>
#include<cstdint>
#include<exception>
#include<functional>
#include<limits>
#include<sstream>
#include<string>
#include<utility>
#include<unordered_map>
import ohana.ast;
import ohana.bytecode;
import ohana.bytecode_io;
import ohana.compiler;
import ohana.lexer;
import ohana.parser;
namespace
{
    constexpr int kTitleWidth = 220;
    constexpr int kRowHeight = 34;
    constexpr int kRulerHeight = 32;
    constexpr int kMinimumVisibleRows = 6;
    constexpr int kMinimumFrames = 10;
    constexpr int kMaximumFrames = 10000;
    QColor eventColor(int index)
    {
        return QColor::fromHsv((index * 47 + 205) % 360, 155, 225);
    }
    int clampFrame(int frame, int maxFrames)
    {
        return qBound(0, frame, qMax(0, maxFrames - 1));
    }
    QString frameRangeText(int start, int end)
    {
        return QStringLiteral("%1–%2 (%3 frames)").arg(start).arg(end).arg(qMax(1, end - start + 1));
    }
    QString normalizeEventType(const QString &type)
    {
        if (type.isEmpty() || type == QStringLiteral("General") || type == QStringLiteral("Effect"))
        {
            return QStringLiteral("Animation");
        }
        return type;
    }
    QJsonArray vector3ToJson(const std::array<double, 3> &value)
    {
        return QJsonArray{value[0], value[1], value[2]};
    }
    void readVector3(const QJsonValue &value, std::array<double, 3> &output)
    {
        if (!value.isArray())
        {
            return;
        }
        const QJsonArray array = value.toArray();
        for (qsizetype axis = 0; axis < 3 && axis < array.size(); ++axis)
        {
            if (array[axis].isDouble())
            {
                output[static_cast<std::size_t>(axis)] = array[axis].toDouble();
            }
        }
    }
    QJsonObject transformToJson(const CameraTransform &transform)
    {
        QJsonObject object;
        object.insert(QStringLiteral("position"), vector3ToJson(transform.position));
        object.insert(QStringLiteral("rotation"), vector3ToJson(transform.rotation));
        return object;
    }
    void readTransform(const QJsonValue &value, CameraTransform &transform)
    {
        if (!value.isObject())
        {
            return;
        }
        const QJsonObject object = value.toObject();
        readVector3(object.value(QStringLiteral("position")), transform.position);
        readVector3(object.value(QStringLiteral("rotation")), transform.rotation);
    }
    std::string ohanaStringLiteral(const QString &value)
    {
        const QByteArray utf8 = value.toUtf8();
        std::string literal = "\"";
        for (const char byte : utf8)
        {
            switch (byte)
            {
            case '\\': literal += "\\\\"; break;
            case '"': literal += "\\\""; break;
            case '\n': literal += "\\n"; break;
            case '\r': literal += "\\n"; break;
            case '\t': literal += "\\t"; break;
            default: literal += byte; break;
            }
        }
        literal += '"';
        return literal;
    }
    QString ohanaFloatLiteral(double value)
    {
        QString number = QString::number(value, 'f', 17);
        while (number.contains(QLatin1Char('.')) && number.endsWith(QLatin1Char('0')))
        {
            number.chop(1);
        }
        if (number.endsWith(QLatin1Char('.')))
        {
            number += QLatin1Char('0');
        }
        return number;
    }
    class IntegerItemDelegate final : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;
        QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override
        {
            auto *editor = new QLineEdit(parent);
            editor->setValidator(new QIntValidator((std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)(), editor));
            editor->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            return editor;
        }
        void setEditorData(QWidget *editor, const QModelIndex &index) const override
        {
            auto *lineEdit = qobject_cast<QLineEdit *>(editor);
            if (lineEdit)
            {
                lineEdit->setText(index.data(Qt::EditRole).toString());
                lineEdit->selectAll();
            }
        }
        void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
        {
            auto *lineEdit = qobject_cast<QLineEdit *>(editor);
            if (lineEdit)
            {
                bool validInteger = false;
                const int value = lineEdit->text().toInt(&validInteger);
                if (validInteger)
                {
                    model->setData(index, value, Qt::EditRole);
                }
            }
        }
        void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const override
        {
            editor->setGeometry(option.rect);
        }
    };
}
class EventNamesCanvas : public QWidget
{
public:
    EventNamesCanvas(QVector<TrackItem> *tracks, int *selectedTrack, std::function<void(int)> selectCallback, QWidget *parent = nullptr) : QWidget(parent),
          mTracks(tracks),
          mSelectedTrack(selectedTrack),
          mSelectCallback(std::move(selectCallback))
    {
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    QSize sizeHint() const override
    {
        const int rows = qMax(kMinimumVisibleRows, static_cast<int>(mTracks->size()));
        return QSize(kTitleWidth, rows * kRowHeight);
    }
protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        painter.fillRect(event->rect(), QColor(250, 250, 250));
        const int rows = qMax(kMinimumVisibleRows, static_cast<int>(mTracks->size()));
        const int firstRow = qMax(0, event->rect().top() / kRowHeight);
        const int lastRow = qMin(rows - 1, event->rect().bottom() / kRowHeight);
        for (int row = firstRow; row <= lastRow; ++row)
        {
            const QRect rowRect(0, row * kRowHeight, width(), kRowHeight);
            QColor background = (row % 2 == 0) ? QColor(248, 249, 251) : QColor(239, 242, 246);
            if (row == *mSelectedTrack)
            {
                background = QColor(211, 230, 250);
            }
            painter.fillRect(rowRect, background);
            painter.setPen(QColor(212, 216, 222));
            painter.drawLine(rowRect.bottomLeft(), rowRect.bottomRight());
            if (row < mTracks->size())
            {
                QFont trackFont = painter.font();
                trackFont.setBold(row == *mSelectedTrack);
                painter.setFont(trackFont);
                painter.setPen(QColor(40, 45, 52));
                const QString name = mTracks->at(row).name.isEmpty() ? QStringLiteral("Track %1").arg(row + 1) : mTracks->at(row).name;
                painter.drawText(QRect(12, rowRect.top() + 3, kTitleWidth - 22, kRowHeight - 6), Qt::AlignLeft | Qt::AlignVCenter, painter.fontMetrics().elidedText(name, Qt::ElideRight, kTitleWidth - 22));
            }
        }
        painter.setPen(QColor(190, 195, 202));
        painter.drawLine(width() - 1, event->rect().top(), width() - 1, event->rect().bottom());
        if (mTracks->isEmpty())
        {
            painter.setPen(QColor(115, 122, 132));
            painter.drawText(QRect(12, 0, width() - 24, kRowHeight), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Añade una pista para comenzar"));
        }
    }
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton)
        {
            return;
        }
        const int row = event->position().toPoint().y() / kRowHeight;
        if (row >= 0 && row < mTracks->size())
        {
            mSelectCallback(row);
            setFocus();
        }
    }
private:
    QVector<TrackItem> *mTracks;
    int *mSelectedTrack;
    std::function<void(int)> mSelectCallback;
};
class EventTimelineCanvas : public QWidget
{
public:
    EventTimelineCanvas(QVector<EventItem> *events, QVector<TrackItem> *tracks, int *selectedIndex, int *selectedTrack, int *currentFrame, int *maxFrames, int *frameWidth, std::function<void(int)> selectCallback, std::function<void(int)> frameCallback,std::function<void()> changedCallback, std::function<void(int)> editCallback, QWidget *parent = nullptr) : QWidget(parent),
          mEvents(events),
          mTracks(tracks),
          mSelectedIndex(selectedIndex),
          mSelectedTrack(selectedTrack),
          mCurrentFrame(currentFrame),
          mMaxFrames(maxFrames),
          mFrameWidth(frameWidth),
          mSelectCallback(std::move(selectCallback)),
          mFrameCallback(std::move(frameCallback)),
          mChangedCallback(std::move(changedCallback)),
          mEditCallback(std::move(editCallback))
    {
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    QSize sizeHint() const override
    {
        const int rows = qMax(kMinimumVisibleRows, static_cast<int>(mTracks->size()));
        return QSize(*mMaxFrames * *mFrameWidth + 40, rows * kRowHeight);
    }
protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(event->rect(), Qt::white);
        const int visibleStartFrame = qMax(0, event->rect().left() / *mFrameWidth);
        const int visibleEndFrame = qMin(*mMaxFrames, event->rect().right() / *mFrameWidth + 1);
        const int visibleRows = qMax(kMinimumVisibleRows, static_cast<int>(mTracks->size()));
        const int firstRow = qMax(0, event->rect().top() / kRowHeight);
        const int lastRow = qMin(visibleRows - 1, event->rect().bottom() / kRowHeight);
        for (int row = firstRow; row <= lastRow; ++row)
        {
            const QRect rowRect(0, row * kRowHeight, width(), kRowHeight);
            QColor rowColor = row % 2 == 0 ? QColor(255, 255, 255) : QColor(249, 250, 252);
            if (row == *mSelectedTrack)
            {
                rowColor = QColor(239, 246, 253);
            }
            painter.fillRect(rowRect, rowColor);
            painter.setPen(QColor(224, 227, 232));
            painter.drawLine(rowRect.bottomLeft(), rowRect.bottomRight());
        }
        for (int frame = visibleStartFrame; frame <= visibleEndFrame; ++frame)
        {
            const int x = frame * *mFrameWidth;
            painter.setPen(frame % 5 == 0 ? QColor(205, 210, 218) : QColor(235, 237, 241));
            painter.drawLine(x, event->rect().top(), x, event->rect().bottom());
        }
        for (int index = 0; index < mEvents->size(); ++index)
        {
            const EventItem &item = mEvents->at(index);
            const int row = item.trackIndex;
            if (row < firstRow || row > lastRow || row < 0 || row >= mTracks->size())
            {
                continue;
            }
            const int barX = item.startFrame * *mFrameWidth + 2;
            const int barY = row * kRowHeight + 6;
            const int barWidth = qMax(5, (item.endFrame - item.startFrame + 1) * *mFrameWidth - 4);
            const QRect barRect(barX, barY, barWidth, kRowHeight - 12);
            QColor outline = item.color.darker(150);
            painter.setPen(QPen(outline, index == *mSelectedIndex ? 2 : 1));
            painter.setBrush(item.color);
            painter.drawRoundedRect(barRect, 3, 3);
            if (index == *mSelectedIndex)
            {
                painter.setPen(QPen(QColor(35, 93, 155), 2));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(barRect.adjusted(1, 1, -1, -1), 3, 3);
            }
            painter.setPen(item.color.lightness() < 140 ? QColor(255, 255, 255) : QColor(25, 32, 40));
            const QString label = painter.fontMetrics().elidedText(item.name, Qt::ElideRight, qMax(0, barRect.width() - 12));
            painter.drawText(barRect.adjusted(6, 0, -5, 0), Qt::AlignVCenter | Qt::AlignLeft, label);
        }
        const int currentX = *mCurrentFrame * *mFrameWidth;
        painter.setPen(QPen(QColor(220, 55, 55), 2));
        painter.drawLine(currentX, event->rect().top(), currentX, event->rect().bottom());
        if (mEvents->isEmpty())
        {
            painter.setPen(QColor(125, 132, 141));
            painter.drawText(QRect(12, 0, qMax(0, width() - 24), kRowHeight), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("La línea roja indica el frame actual"));
        }
    }
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton)
        {
            return;
        }
        const QPoint pos = event->position().toPoint();
        const int frame = clampFrame(pos.x() / *mFrameWidth, *mMaxFrames);
        const int trackIndex = pos.y() / kRowHeight;
        mFrameCallback(frame);
        mDragMode = DragNone;
        mDragIndex = -1;
        if (trackIndex >= 0 && trackIndex < mTracks->size())
        {
            *mSelectedTrack = trackIndex;
            const int eventIndex = eventAtPosition(pos);
            mSelectCallback(eventIndex);
            if (eventIndex >= 0)
            {
                EventItem &item = (*mEvents)[eventIndex];
                const int left = item.startFrame * *mFrameWidth + 2;
                const int right = (item.endFrame + 1) * *mFrameWidth - 2;
                if (pos.x() <= left + 5)
                {
                    mDragMode = DragStart;
                }
                else if (pos.x() >= right - 5)
                {
                    mDragMode = DragEnd;
                }
                else
                {
                    mDragMode = DragMove;
                }
                mDragIndex = eventIndex;
                mDragOriginFrame = frame;
                mOriginalStart = item.startFrame;
                mOriginalEnd = item.endFrame;
            }
        }
        else
        {
            mSelectCallback(-1);
        }
        setFocus();
        update();
    }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!(event->buttons() & Qt::LeftButton) || mDragIndex < 0 || mDragIndex >= mEvents->size() || mDragMode == DragNone)
        {
            return;
        }
        const int frame = clampFrame(event->position().toPoint().x() / *mFrameWidth, *mMaxFrames);
        EventItem &item = (*mEvents)[mDragIndex];
        if (mDragMode == DragStart)
        {
            item.startFrame = qMin(frame, mOriginalEnd);
            item.endFrame = mOriginalEnd;
        }
        else if (mDragMode == DragEnd)
        {
            item.startFrame = mOriginalStart;
            item.endFrame = qMax(frame, mOriginalStart);
        }
        else
        {
            const int length = mOriginalEnd - mOriginalStart;
            const int newStart = qBound(0, mOriginalStart + frame - mDragOriginFrame, qMax(0, *mMaxFrames - 1 - length));
            item.startFrame = newStart;
            item.endFrame = qMin(*mMaxFrames - 1, newStart + length);
            const int targetTrack = qBound(0, event->position().toPoint().y() / kRowHeight, static_cast<int>(mTracks->size()) - 1);
            item.trackIndex = targetTrack;
            *mSelectedTrack = targetTrack;
        }
        mChangedCallback();
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            mDragMode = DragNone;
            mDragIndex = -1;
            update();
        }
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton)
        {
            return;
        }
        const int eventIndex = eventAtPosition(event->position().toPoint());
        if (eventIndex >= 0)
        {
            *mSelectedTrack = mEvents->at(eventIndex).trackIndex;
            mSelectCallback(eventIndex);
            mEditCallback(eventIndex);
        }
    }
private:
    enum DragMode
    {
        DragNone,
        DragStart,
        DragEnd,
        DragMove
    };
    int eventAtPosition(const QPoint &pos) const
    {
        const int trackIndex = pos.y() / kRowHeight;
        if (trackIndex < 0 || trackIndex >= mTracks->size())
        {
            return -1;
        }
        for (int index = mEvents->size() - 1; index >= 0; --index)
        {
            const EventItem &item = mEvents->at(index);
            if (item.trackIndex != trackIndex)
            {
                continue;
            }
            const int left = item.startFrame * *mFrameWidth + 2;
            const int right = (item.endFrame + 1) * *mFrameWidth - 2;
            const int top = trackIndex * kRowHeight + 6;
            const int bottom = trackIndex * kRowHeight + kRowHeight - 6;
            if (pos.y() >= top && pos.y() <= bottom && pos.x() >= left - 2 && pos.x() <= right + 2)
            {
                return index;
            }
        }
        return -1;
    }
    QVector<EventItem> *mEvents;
    QVector<TrackItem> *mTracks;
    int *mSelectedIndex;
    int *mSelectedTrack;
    int *mCurrentFrame;
    int *mMaxFrames;
    int *mFrameWidth;
    std::function<void(int)> mSelectCallback;
    std::function<void(int)> mFrameCallback;
    std::function<void()> mChangedCallback;
    std::function<void(int)> mEditCallback;
    DragMode mDragMode = DragNone;
    int mDragIndex = -1;
    int mDragOriginFrame = 0;
    int mOriginalStart = 0;
    int mOriginalEnd = 0;
};
class EventRuler : public QWidget
{
public:
    EventRuler(int *currentFrame, int *maxFrames, int *frameWidth, std::function<void(int)> frameCallback, QWidget *parent = nullptr) : QWidget(parent),
          mCurrentFrame(currentFrame),
          mMaxFrames(maxFrames),
          mFrameWidth(frameWidth),
          mFrameCallback(std::move(frameCallback))
    {
        setMinimumHeight(kRulerHeight);
        setMaximumHeight(kRulerHeight);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    void setScrollOffset(int value)
    {
        if (mScrollOffset != value)
        {
            mScrollOffset = value;
            update();
        }
    }
protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        painter.fillRect(event->rect(), QColor(246, 247, 249));
        const int firstFrame = qMax(0, mScrollOffset / *mFrameWidth);
        const int lastFrame = qMin(*mMaxFrames, (mScrollOffset + width()) / *mFrameWidth + 1);
        for (int frame = firstFrame; frame <= lastFrame; ++frame)
        {
            const int x = frame * *mFrameWidth - mScrollOffset;
            if (frame % 5 == 0)
            {
                painter.setPen(QColor(160, 166, 174));
                painter.drawLine(x, 13, x, height() - 1);
                painter.setPen(QColor(55, 61, 69));
                painter.drawText(QRect(x - 18, 0, 36, 14), Qt::AlignHCenter | Qt::AlignVCenter, QString::number(frame));
            }
            else
            {
                painter.setPen(QColor(195, 200, 207));
                painter.drawLine(x, 21, x, height() - 1);
            }
        }
        const int currentX = *mCurrentFrame * *mFrameWidth - mScrollOffset;
        painter.setPen(QPen(QColor(220, 55, 55), 2));
        painter.drawLine(currentX, 0, currentX, height());
        painter.setPen(QColor(197, 202, 209));
        painter.drawLine(0, height() - 1, width(), height() - 1);
    }
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            mFrameCallback(clampFrame((event->position().toPoint().x() + mScrollOffset) / *mFrameWidth, *mMaxFrames));
        }
    }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (event->buttons() & Qt::LeftButton)
        {
            mFrameCallback(clampFrame((event->position().toPoint().x() + mScrollOffset) / *mFrameWidth, *mMaxFrames));
        }
    }
private:
    int *mCurrentFrame;
    int *mMaxFrames;
    int *mFrameWidth;
    int mScrollOffset = 0;
    std::function<void(int)> mFrameCallback;
};
EventEditor::EventEditor(QWidget *parent) : QMainWindow(parent)
{
    ui.setupUi(this);
    setWindowTitle(QStringLiteral("EventEditor"));
    resize(1280, 780);
    setMinimumSize(850, 520);
    mTracks.append(TrackItem{QStringLiteral("Track 1")});
    QWidget *central = ui.centralWidget;
    auto *outerLayout = new QVBoxLayout(central);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    auto *tabs = new QTabWidget(central);
    outerLayout->addWidget(tabs);
    auto *eventsPage = new QWidget(tabs);
    auto *eventsLayout = new QVBoxLayout(eventsPage);
    eventsLayout->setContentsMargins(0, 0, 0, 0);
    eventsLayout->setSpacing(0);
    tabs->addTab(eventsPage, QStringLiteral("Events"));
    auto *headerRow = new QWidget(eventsPage);
    auto *headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);
    auto *nameHeader = new QLabel(QStringLiteral("TRACK"), headerRow);
    nameHeader->setFixedWidth(kTitleWidth);
    nameHeader->setMinimumHeight(kRulerHeight);
    nameHeader->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    nameHeader->setStyleSheet("QLabel { background: #e9edf2; color: #333b45; padding-left: 10px;" "font-weight: bold; border-bottom: 1px solid #c5cbd3; }");
    mRuler = new EventRuler(&mCurrentFrame, &mMaxFrames, &mFrameWidth,[this](int frame) 
    {
        mCurrentFrame = frame;
        updateFrameControls();
        refreshViews(false);
        updateStatus();
    }, headerRow);
    headerLayout->addWidget(nameHeader);
    headerLayout->addWidget(mRuler, 1);
    eventsLayout->addWidget(headerRow);
    auto *timelineRow = new QWidget(eventsPage);
    auto *timelineLayout = new QHBoxLayout(timelineRow);
    timelineLayout->setContentsMargins(0, 0, 0, 0);
    timelineLayout->setSpacing(0);
    mNamesScroll = new QScrollArea(timelineRow);
    mNamesScroll->setFrameShape(QFrame::NoFrame);
    mNamesScroll->setFixedWidth(kTitleWidth);
    mNamesScroll->setWidgetResizable(false);
    mNamesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mNamesScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mTimelineScroll = new QScrollArea(timelineRow);
    mTimelineScroll->setFrameShape(QFrame::NoFrame);
    mTimelineScroll->setWidgetResizable(false);
    mTimelineScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    mTimelineScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    mTimelineScroll->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    mNamesCanvas = new EventNamesCanvas(&mTracks, &mSelectedTrack, [this](int trackIndex)
    {
        mSelectedTrack = trackIndex;
        mSelectedIndex = -1;
        refreshViews(false);
        updateStatus();
    });
    mTimelineCanvas = new EventTimelineCanvas(&mEvents, &mTracks, &mSelectedIndex, &mSelectedTrack,
        &mCurrentFrame, &mMaxFrames, &mFrameWidth,
        [this](int index) {
            mSelectedIndex = index;
            if (index >= 0 && index < mEvents.size())
            {
                mSelectedTrack = mEvents.at(index).trackIndex;
            }
            refreshViews(false);
            updateStatus();
        },
        [this](int frame) {
            mCurrentFrame = frame;
            updateFrameControls();
            refreshViews(false);
            updateStatus();
        },
        [this]() {
            refreshViews(false);
            markModified();
            updateStatus();
        },
        [this](int index) { editEvent(index); });   
    mNamesScroll->setWidget(mNamesCanvas);
    mTimelineScroll->setWidget(mTimelineCanvas);
    timelineLayout->addWidget(mNamesScroll);
    timelineLayout->addWidget(mTimelineScroll, 1);
    eventsLayout->addWidget(timelineRow, 1);
    connect(mTimelineScroll->verticalScrollBar(), &QScrollBar::valueChanged, mNamesScroll->verticalScrollBar(), &QScrollBar::setValue);
    connect(mTimelineScroll->verticalScrollBar(), &QScrollBar::rangeChanged,
            this, [this](int minimum, int maximum) {
                QScrollBar *namesBar = mNamesScroll->verticalScrollBar();
                namesBar->setRange(minimum, maximum);
                namesBar->setPageStep(mTimelineScroll->verticalScrollBar()->pageStep());
            });
    connect(mTimelineScroll->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int value) {
                mRuler->setScrollOffset(value);
            });
    auto *footer = new QWidget(eventsPage);
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(8, 6, 8, 6);
    footerLayout->setSpacing(8);
    footerLayout->addWidget(new QLabel(QStringLiteral("Current frame:"), footer));
    mCurrentFrameSpin = new QSpinBox(footer);
    mCurrentFrameSpin->setRange(0, mMaxFrames - 1);
    mCurrentFrameSpin->setFixedWidth(90);
    footerLayout->addWidget(mCurrentFrameSpin);
    footerLayout->addSpacing(12);
    footerLayout->addWidget(new QLabel(QStringLiteral("Total frames:"), footer));
    mMaxFramesSpin = new QSpinBox(footer);
    mMaxFramesSpin->setRange(kMinimumFrames, kMaximumFrames);
    mMaxFramesSpin->setValue(mMaxFrames);
    mMaxFramesSpin->setFixedWidth(100);
    footerLayout->addWidget(mMaxFramesSpin);
    footerLayout->addSpacing(18);
    footerLayout->addWidget(new QLabel(QStringLiteral("Zoom:"), footer));
    mZoomSlider = new QSlider(Qt::Horizontal, footer);
    mZoomSlider->setRange(8, 32);
    mZoomSlider->setValue(mFrameWidth);
    mZoomSlider->setFixedWidth(150);
    mZoomSlider->setTickInterval(4);
    footerLayout->addWidget(mZoomSlider);
    mZoomLabel = new QLabel(QStringLiteral("100%"), footer);
    mZoomLabel->setMinimumWidth(48);
    footerLayout->addWidget(mZoomLabel);
    footerLayout->addStretch(1);
    eventsLayout->addWidget(footer);

    auto *startEventPage = new QWidget(tabs);
    auto *startEventLayout = new QVBoxLayout(startEventPage);
    auto *startEventControls = new QHBoxLayout();
    startEventControls->addWidget(new QLabel(QStringLiteral("Rows loaded when the level starts"), startEventPage));
    startEventControls->addStretch(1);
    auto *addStartEventButton = new QPushButton(QStringLiteral("Add row"), startEventPage);
    auto *removeStartEventButton = new QPushButton(QStringLiteral("Remove selected"), startEventPage);
    startEventControls->addWidget(addStartEventButton);
    startEventControls->addWidget(removeStartEventButton);
    startEventLayout->addLayout(startEventControls);
    mStartEventTable = new QTableWidget(0, 3, startEventPage);
    mStartEventTable->setHorizontalHeaderLabels({QStringLiteral("String"), QStringLiteral("Int 1"), QStringLiteral("Int 2")});
    mStartEventTable->horizontalHeaderItem(0)->setToolTip(QStringLiteral("Nombre del recurso para Int 1 = 1 (modelo) o 2 (colisiones). Déjalo vacío para Int 1 = 0 (pista inicial)."));
    mStartEventTable->horizontalHeaderItem(1)->setToolTip(QStringLiteral("0 = pista inicial; 1 = cargar modelo; 2 = cargar colisiones."));
    mStartEventTable->horizontalHeaderItem(2)->setToolTip(QStringLiteral("Con Int 1 = 0: ID de pista inicial (0 = ninguna; las pistas empiezan en 1). Con 1/2: ID de modelo o slot de colisiones."));
    mStartEventTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mStartEventTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mStartEventTable->setEditTriggers(QAbstractItemView::AllEditTriggers);
    mStartEventTable->setTabKeyNavigation(true);
    mStartEventTable->setAlternatingRowColors(true);
    mStartEventTable->setItemDelegateForColumn(1, new IntegerItemDelegate(mStartEventTable));
    mStartEventTable->setItemDelegateForColumn(2, new IntegerItemDelegate(mStartEventTable));
    mStartEventTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    mStartEventTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    mStartEventTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    mStartEventTable->setColumnWidth(1, 140);
    mStartEventTable->setColumnWidth(2, 140);
    mStartEventTable->verticalHeader()->setDefaultSectionSize(30);
    startEventLayout->addWidget(mStartEventTable, 1);
    tabs->addTab(startEventPage, QStringLiteral("StartEvent"));
    connect(addStartEventButton, &QPushButton::clicked, this, [this]() { addStartEventRow(); });
    connect(removeStartEventButton, &QPushButton::clicked, this, [this]() { removeSelectedStartEventRows(); });
    connect(mStartEventTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item)
    {
        if (!item || item->row() < 0 || item->row() >= mStartEvents.size())
        {
            return;
        }
        StartEventItem &startEvent = mStartEvents[item->row()];
        if (item->column() == 0)
        {
            startEvent.stringValue = item->text();
        }
        else if (item->column() == 1)
        {
            startEvent.firstInt = item->data(Qt::EditRole).toInt();
        }
        else if (item->column() == 2)
        {
            startEvent.secondInt = item->data(Qt::EditRole).toInt();
        }
        markModified();
    });
    connect(mCurrentFrameSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int frame) {
                mCurrentFrame = frame;
                refreshViews(false);
                updateStatus();
            });
    connect(mMaxFramesSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int frames) {
                mMaxFrames = qBound(kMinimumFrames, frames, kMaximumFrames);
                for (EventItem &item : mEvents)
                {
                    item.startFrame = clampFrame(item.startFrame, mMaxFrames);
                    item.endFrame = qBound(item.startFrame, item.endFrame, mMaxFrames - 1);
                }
                mCurrentFrame = clampFrame(mCurrentFrame, mMaxFrames);
                mCurrentFrameSpin->setRange(0, mMaxFrames - 1);
                refreshViews(true);
                markModified();
                updateStatus();
            });
    connect(mZoomSlider, &QSlider::valueChanged, this, [this](int width) { setFrameWidth(width); });
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    QAction *newAction = fileMenu->addAction(QStringLiteral("&New"));
    newAction->setShortcut(QKeySequence::New);
    QAction *openAction = fileMenu->addAction(QStringLiteral("&Open..."));
    openAction->setShortcut(QKeySequence::Open);
    QAction *saveAction = fileMenu->addAction(QStringLiteral("&Save"));
    saveAction->setShortcut(QKeySequence::Save);
    QAction *saveAsAction = fileMenu->addAction(QStringLiteral("Save &As..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    fileMenu->addSeparator();
    QAction *exportBytecodeAction = fileMenu->addAction(QStringLiteral("Export OhanaScript bytecode..."));
    QAction *exitAction = fileMenu->addAction(QStringLiteral("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    ui.mainToolBar->setWindowTitle(QStringLiteral("File"));
    ui.mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui.mainToolBar->addAction(newAction);
    ui.mainToolBar->addAction(openAction);
    ui.mainToolBar->addAction(saveAction);
    ui.mainToolBar->addAction(saveAsAction);
    ui.mainToolBar->addSeparator();
    ui.mainToolBar->addAction(exitAction);
    QMenu *editMenu = menuBar()->addMenu(QStringLiteral("&Event"));
    QAction *addAction = editMenu->addAction(QStringLiteral("&Add event"));
    addAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+N")));
    QAction *editAction = editMenu->addAction(QStringLiteral("&Edit selected event"));
    editAction->setShortcut(QKeySequence(Qt::Key_F2));
    QAction *duplicateAction = editMenu->addAction(QStringLiteral("&Duplicate event"));
    duplicateAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    QAction *deleteAction = editMenu->addAction(QStringLiteral("&Delete event"));
    deleteAction->setShortcut(QKeySequence::Delete);
    mCopyEventAction = editMenu->addAction(QStringLiteral("&Copy event"));
    mCopyEventAction->setShortcut(QKeySequence::Copy);
    mCopyEventAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    mPasteEventAction = editMenu->addAction(QStringLiteral("&Paste event"));
    mPasteEventAction->setShortcut(QKeySequence::Paste);
    mPasteEventAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    timelineRow->addAction(mCopyEventAction);
    timelineRow->addAction(mPasteEventAction);
    QToolBar *eventToolbar = addToolBar(QStringLiteral("Events"));
    eventToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    eventToolbar->addAction(addAction);
    eventToolbar->addAction(editAction);
    eventToolbar->addAction(duplicateAction);
    eventToolbar->addAction(deleteAction);
    eventToolbar->addSeparator();
    eventToolbar->addAction(mCopyEventAction);
    eventToolbar->addAction(mPasteEventAction);
    QMenu *trackMenu = menuBar()->addMenu(QStringLiteral("&Track"));
    QAction *addTrackAction = trackMenu->addAction(QStringLiteral("&Add track"));
    mRemoveTrackAction = trackMenu->addAction(QStringLiteral("&Remove selected track"));
    eventToolbar->addSeparator();
    eventToolbar->addAction(addTrackAction);
    eventToolbar->addAction(mRemoveTrackAction);
    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    QAction *zoomSmall = viewMenu->addAction(QStringLiteral("Zoom &S"));
    QAction *zoomMedium = viewMenu->addAction(QStringLiteral("Zoom &M"));
    QAction *zoomLarge = viewMenu->addAction(QStringLiteral("Zoom &L"));
    connect(zoomSmall, &QAction::triggered, this, [this]() { mZoomSlider->setValue(10); });
    connect(zoomMedium, &QAction::triggered, this, [this]() { mZoomSlider->setValue(16); });
    connect(zoomLarge, &QAction::triggered, this, [this]() { mZoomSlider->setValue(24); });
    connect(newAction, &QAction::triggered, this, [this]() { newFile(); });
    connect(openAction, &QAction::triggered, this, [this]() { openFile(); });
    connect(saveAction, &QAction::triggered, this, [this]() { saveFile(); });
    connect(saveAsAction, &QAction::triggered, this, [this]() { saveFileAs(); });
    connect(exportBytecodeAction, &QAction::triggered, this, [this]() { exportBytecode(); });
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    connect(addTrackAction, &QAction::triggered, this, [this]() { addTrack(); });
    connect(mRemoveTrackAction, &QAction::triggered, this, [this]() { removeTrack(); });
    connect(addAction, &QAction::triggered, this, [this]() { addEvent(); });
    connect(editAction, &QAction::triggered, this, [this]() { editSelectedEvent(); });
    connect(duplicateAction, &QAction::triggered, this, [this]() { duplicateSelectedEvent(); });
    connect(deleteAction, &QAction::triggered, this, [this]() { deleteSelectedEvent(); });
    connect(mCopyEventAction, &QAction::triggered, this, [this]() { copySelectedEvent(); });
    connect(mPasteEventAction, &QAction::triggered, this, [this]() { pasteEvent(); });
    statusBar()->showMessage(QStringLiteral("Ready — add an event to start editing"));
    refreshViews(true);
    refreshStartEventTable();
    updateFrameControls();
    updateWindowTitle();
}
EventEditor::~EventEditor() = default;
void EventEditor::refreshViews(bool resizeContents)
{
    if (mCopyEventAction)
    {
        mCopyEventAction->setEnabled(mSelectedIndex >= 0 && mSelectedIndex < mEvents.size());
    }
    if (mPasteEventAction)
    {
        mPasteEventAction->setEnabled(mHasEventClipboard);
    }
    if (mRemoveTrackAction)
    {
        mRemoveTrackAction->setEnabled(mTracks.size() > 1);
    }
    if (resizeContents)
    {
        const int rows = qMax(kMinimumVisibleRows, static_cast<int>(mTracks.size()));
        mNamesCanvas->setFixedSize(kTitleWidth, rows * kRowHeight);
        mTimelineCanvas->setFixedSize(mMaxFrames * mFrameWidth + 40, rows * kRowHeight);
        QScrollBar *timelineBar = mTimelineScroll->verticalScrollBar();
        QScrollBar *namesBar = mNamesScroll->verticalScrollBar();
        namesBar->setRange(timelineBar->minimum(), timelineBar->maximum());
        namesBar->setPageStep(timelineBar->pageStep());
        namesBar->setValue(timelineBar->value());
    }
    mNamesCanvas->update();
    mTimelineCanvas->update();
    mRuler->update();
    updateFrameControls();
}
void EventEditor::appendStartEventRow(const StartEventItem &item)
{
    const int row = mStartEventTable->rowCount();
    mStartEventTable->insertRow(row);
    const QSignalBlocker blocker(mStartEventTable);
    mStartEventTable->setItem(row, 0, new QTableWidgetItem(item.stringValue));
    auto *firstInt = new QTableWidgetItem();
    firstInt->setData(Qt::EditRole, item.firstInt);
    firstInt->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mStartEventTable->setItem(row, 1, firstInt);
    auto *secondInt = new QTableWidgetItem();
    secondInt->setData(Qt::EditRole, item.secondInt);
    secondInt->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mStartEventTable->setItem(row, 2, secondInt);
}
void EventEditor::refreshStartEventTable()
{
    if (!mStartEventTable)
    {
        return;
    }
    const QSignalBlocker blocker(mStartEventTable);
    mStartEventTable->setRowCount(0);
    for (const StartEventItem &item : mStartEvents)
    {
        appendStartEventRow(item);
    }
}
void EventEditor::addStartEventRow()
{
    const StartEventItem item{};
    mStartEvents.append(item);
    appendStartEventRow(item);
    const int row = mStartEvents.size() - 1;
    mStartEventTable->setCurrentCell(row, 0);
    mStartEventTable->editItem(mStartEventTable->item(row, 0));
    markModified();
}
void EventEditor::removeSelectedStartEventRows()
{
    QModelIndexList selectedRows = mStartEventTable->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
    {
        return;
    }
    std::sort(selectedRows.begin(), selectedRows.end(), [](const QModelIndex &left, const QModelIndex &right)
    {
        return left.row() > right.row();
    });
    int previousRow = -1;
    for (const QModelIndex &index : selectedRows)
    {
        const int row = index.row();
        if (row == previousRow || row < 0 || row >= mStartEvents.size())
        {
            continue;
        }
        mStartEvents.removeAt(row);
        mStartEventTable->removeRow(row);
        previousRow = row;
    }
    markModified();
}
void EventEditor::updateFrameControls()
{
    if (mCurrentFrameSpin)
    {
        const QSignalBlocker blocker(mCurrentFrameSpin);
        mCurrentFrameSpin->setRange(0, mMaxFrames - 1);
        mCurrentFrameSpin->setValue(mCurrentFrame);
    }
    if (mMaxFramesSpin)
    {
        const QSignalBlocker blocker(mMaxFramesSpin);
        mMaxFramesSpin->setValue(mMaxFrames);
    }
}
void EventEditor::updateWindowTitle()
{
    QString title = QStringLiteral("EventEditor");
    if (!mCurrentFile.isEmpty())
    {
        title += QStringLiteral(" — ") + QFileInfo(mCurrentFile).fileName();
    }
    else
    {
        title += QStringLiteral(" — Untitled");
    }
    if (mModified)
    {
        title += QStringLiteral(" *");
    }
    setWindowTitle(title);
}
void EventEditor::updateStatus()
{
    if (mSelectedIndex >= 0 && mSelectedIndex < mEvents.size())
    {
        const EventItem &item = mEvents.at(mSelectedIndex);
        const QString trackName = item.trackIndex >= 0 && item.trackIndex < mTracks.size()
            ? mTracks.at(item.trackIndex).name
            : QStringLiteral("Track");
        statusBar()->showMessage(QStringLiteral("%1  |  %2  |  %3  |  Frames %4–%5  |  Current frame %6")
            .arg(trackName, item.name, item.type)
            .arg(item.startFrame)
            .arg(item.endFrame)
            .arg(mCurrentFrame));
    }
    else
    {
        statusBar()->showMessage(QStringLiteral("%1 event(s) across %2 track(s)  |  Current frame: %3")
            .arg(mEvents.size()).arg(mTracks.size()).arg(mCurrentFrame));
    }
}
void EventEditor::setFrameWidth(int width)
{
    width = qBound(8, width, 32);
    if (mFrameWidth == width)
    {
        return;
    }
    const int oldWidth = mFrameWidth;
    const int oldScroll = mTimelineScroll->horizontalScrollBar()->value();
    mFrameWidth = width;
    refreshViews(true);
    mTimelineScroll->horizontalScrollBar()->setValue(oldScroll * mFrameWidth / oldWidth);
    mZoomLabel->setText(QStringLiteral("%1%").arg(qRound(mFrameWidth * 100.0 / 16.0)));
}
void EventEditor::markModified()
{
    mModified = true;
    updateWindowTitle();
}
bool EventEditor::confirmDiscardChanges()
{
    if (!mModified)
    {
        return true;
    }
    const QMessageBox::StandardButton answer = QMessageBox::warning(this, QStringLiteral("Unsaved changes"), QStringLiteral("This file has unsaved changes. Save them before continuing?"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save)
    {
        return saveFile();
    }
    if (answer == QMessageBox::Discard)
    {
        return true;
    }
    return false;
}
void EventEditor::newFile()
{
    if (!confirmDiscardChanges())
    {
        return;
    }
    mTracks.clear();
    mTracks.append(TrackItem{QStringLiteral("Track 1")});
    mEvents.clear();
    mStartEvents.clear();
    mSelectedIndex = -1;
    mSelectedTrack = 0;
    mCurrentFrame = 0;
    mMaxFrames = 240;
    mCurrentFile.clear();
    mModified = false;
    refreshViews(true);
    refreshStartEventTable();
    updateWindowTitle();
    updateStatus();
}
void EventEditor::openFile()
{
    if (!confirmDiscardChanges())
    {
        return;
    }
    const QString fileName = QFileDialog::getOpenFileName(this, QStringLiteral("Open event file"), QString(), QStringLiteral("Event files (*.events.json *.json);;All files (*.*)"));
    if (!fileName.isEmpty())
    {
        readFile(fileName);
    }
}
bool EventEditor::saveFile()
{
    if (mCurrentFile.isEmpty())
    {
        return saveFileAs();
    }
    return writeFile(mCurrentFile);
}
bool EventEditor::saveFileAs()
{
    const QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("Save event file"), mCurrentFile.isEmpty() ? QStringLiteral("events.events.json") : mCurrentFile, QStringLiteral("Event files (*.events.json *.json);;JSON files (*.json);;All files (*.*)"));
    if (fileName.isEmpty())
    {
        return false;
    }
    return writeFile(fileName);
}
QJsonObject EventEditor::projectJson(const QString &fileName) const
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("EventEditor"));
    root.insert(QStringLiteral("version"), 7);
    root.insert(QStringLiteral("maxFrames"), mMaxFrames);
    QJsonArray trackArray;
    for (const TrackItem &item : mTracks)
    {
        QJsonObject object;
        object.insert(QStringLiteral("name"), item.name);
        trackArray.append(object);
    }
    root.insert(QStringLiteral("tracks"), trackArray);
    QJsonArray eventArray;
    for (const EventItem &item : mEvents)
    {
        QJsonObject object;
        object.insert(QStringLiteral("name"), item.name);
        object.insert(QStringLiteral("type"), item.type);
        object.insert(QStringLiteral("track"), item.trackIndex);
        object.insert(QStringLiteral("startFrame"), item.startFrame);
        object.insert(QStringLiteral("endFrame"), item.endFrame);
        if (item.type == QStringLiteral("Animation"))
        {
            object.insert(QStringLiteral("mdlId"), item.mdlId);
            object.insert(QStringLiteral("anmId"), item.anmId);
            object.insert(QStringLiteral("argument"), item.argument);
        }
        else
        {
            object.insert(QStringLiteral("description"), item.description);
        }
        if (!item.scriptFilePath.isEmpty())
        {
            const QDir projectDirectory(QFileInfo(fileName).absolutePath());
            QFileInfo scriptInfo(item.scriptFilePath);
            const QString absoluteScriptPath = scriptInfo.isAbsolute() ? scriptInfo.absoluteFilePath() : projectDirectory.absoluteFilePath(item.scriptFilePath);
            object.insert(QStringLiteral("scriptFile"), projectDirectory.relativeFilePath(absoluteScriptPath));
        }
        else if (!item.script.isEmpty())
        {
            object.insert(QStringLiteral("script"), item.script);
        }
        if (item.type == QStringLiteral("Camera"))
        {
            QJsonObject camera;
            camera.insert(QStringLiteral("start"), transformToJson(item.cameraStart));
            camera.insert(QStringLiteral("end"), transformToJson(item.cameraEnd));
            object.insert(QStringLiteral("camera"), camera);
        }
        eventArray.append(object);
    }
    root.insert(QStringLiteral("events"), eventArray);
    QJsonArray startEventArray;
    for (const StartEventItem &item : mStartEvents)
    {
        QJsonObject object;
        object.insert(QStringLiteral("string"), item.stringValue);
        object.insert(QStringLiteral("int1"), item.firstInt);
        object.insert(QStringLiteral("int2"), item.secondInt);
        startEventArray.append(object);
    }
    root.insert(QStringLiteral("StartEvent"), startEventArray);
    return root;
}
bool EventEditor::writeFile(const QString &fileName)
{
    const QJsonObject root = projectJson(fileName);
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        QMessageBox::critical(this, QStringLiteral("Save failed"), QStringLiteral("Could not write the file:\n%1") .arg(file.errorString()));
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    mCurrentFile = fileName;
    mModified = false;
    updateWindowTitle();
    statusBar()->showMessage(QStringLiteral("Saved %1").arg(fileName), 4000);
    return true;
}
bool EventEditor::exportBytecode(const QString &outputFile, QString *errorMessage)
{
    QString defaultName = QStringLiteral("events.fest");
    if (!mCurrentFile.isEmpty())
    {
        const QFileInfo currentInfo(mCurrentFile);
        QString festFileName = currentInfo.fileName();
        const QString eventJsonSuffix = QStringLiteral(".events.json");
        if (festFileName.endsWith(eventJsonSuffix, Qt::CaseInsensitive))
        {
            festFileName.chop(eventJsonSuffix.size());
        }
        else
        {
            festFileName = currentInfo.completeBaseName();
        }
        defaultName = currentInfo.absolutePath() + QLatin1Char('/') + festFileName + QStringLiteral(".fest");
    }
    QString fileName = outputFile;
    if (fileName.isEmpty())
    {
        fileName = QFileDialog::getSaveFileName(this, QStringLiteral("Export OhanaScript event package"), defaultName,
            QStringLiteral("OhanaScript event packages (*.fest);;All files (*.*)"));
    }
    if (fileName.isEmpty())
    {
        return false;
    }
    if (!fileName.endsWith(QStringLiteral(".fest"), Qt::CaseInsensitive))
    {
        fileName += QStringLiteral(".fest");
    }
    const auto failExport = [this, errorMessage](const QString &message)
    {
        if (errorMessage)
        {
            *errorMessage = message;
        }
        else
        {
            QMessageBox::critical(this, QStringLiteral("OhanaScript compilation failed"), message);
        }
        return false;
    };

    try
    {
        std::string source;
        auto addGlobal = [&](const QString &name, const QString &expression)
        {
            source += name.toUtf8().toStdString();
            source += " = ";
            source += expression.toUtf8().toStdString();
            source += '\n';
        };
        auto addStringGlobal = [&](const QString &name, const QString &value)
        {
            const QByteArray literal = QByteArray::fromStdString(ohanaStringLiteral(value));
            addGlobal(name, QString::fromUtf8(literal.constData(), literal.size()));
        };
        auto addIntegerGlobal = [&](const QString &name, int value)
        {
            addGlobal(name, QString::number(value));
        };
        auto addFloatGlobal = [&](const QString &name, double value)
        {
            addGlobal(name, ohanaFloatLiteral(value));
        };

        addIntegerGlobal(QStringLiteral("__event_editor_max_frames"), mMaxFrames);
        addIntegerGlobal(QStringLiteral("__event_editor_track_count"), static_cast<int>(mTracks.size()));

        for (int index = 0; index < mEvents.size(); ++index)
        {
            const EventItem &item = mEvents.at(index);
            const QString prefix = QStringLiteral("__event_editor_timeline_%1_").arg(index);
            addIntegerGlobal(prefix + QStringLiteral("track"), item.trackIndex);

            addIntegerGlobal(prefix + QStringLiteral("start_frame"), item.startFrame);
            addIntegerGlobal(prefix + QStringLiteral("end_frame"), item.endFrame);
            if (item.type == QStringLiteral("Animation"))
            {
                addIntegerGlobal(prefix + QStringLiteral("mdl_id"), item.mdlId);
                addIntegerGlobal(prefix + QStringLiteral("anm_id"), item.anmId);
                addIntegerGlobal(prefix + QStringLiteral("argument"), item.argument);
            }
            else
            {
                addStringGlobal(prefix + QStringLiteral("description"), item.description);
            }

            if (item.type == QStringLiteral("Camera"))
            {
                const std::array<QString, 3> axes = {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("z")};
                for (std::size_t axis = 0; axis < axes.size(); ++axis)
                {
                    addFloatGlobal(prefix + QStringLiteral("camera_start_position_") + axes[axis], item.cameraStart.position[axis]);
                    addFloatGlobal(prefix + QStringLiteral("camera_start_rotation_") + axes[axis], item.cameraStart.rotation[axis]);
                    addFloatGlobal(prefix + QStringLiteral("camera_end_position_") + axes[axis], item.cameraEnd.position[axis]);
                    addFloatGlobal(prefix + QStringLiteral("camera_end_rotation_") + axes[axis], item.cameraEnd.rotation[axis]);
                }
            }
        }
        const QRegularExpression eventDeclaration(QStringLiteral("(?m)^\\s*evento\\s+([^\\s:]+)\\s*:"));
        const QRegularExpression functionDeclaration(QStringLiteral("(?m)^\\s*(?:funcion|función|func)\\s+"));
        const QRegularExpression functionNameDeclaration(QStringLiteral("(?m)^\\s*(?:funcion|función|func)\\s+([^\\s(:]+)"));
        struct TrackEventFunction
        {
            int StartFrame = 0;
            QString Name;
        };
        QVector<QVector<TrackEventFunction>> trackEventFunctions(mTracks.size());
        for (int index = 0; index < mEvents.size(); ++index)
        {
            const EventItem &item = mEvents.at(index);
            QString code;
            if (!item.scriptFilePath.isEmpty())
            {
                QFile scriptFile(item.scriptFilePath);
                if (!scriptFile.open(QIODevice::ReadOnly))
                {
                    return failExport(QStringLiteral("Could not read script file '%1':\n%2")
                        .arg(item.scriptFilePath, scriptFile.errorString()));
                }
                const QByteArray scriptBytes = scriptFile.readAll();
                if (scriptFile.error() != QFileDevice::NoError)
                {
                    return failExport(QStringLiteral("Could not read script file '%1':\n%2")
                        .arg(item.scriptFilePath, scriptFile.errorString()));
                }
                code = QString::fromUtf8(scriptBytes);
            }
            else
            {
                code = item.script;
            }
            code = code.trimmed();
            if (code.isEmpty())
            {
                continue;
            }

            if (!eventDeclaration.match(code).hasMatch() && !functionDeclaration.match(code).hasMatch())
            {
                const QString handlerName = QStringLiteral("__event_editor_timeline_%1").arg(index);
                source += "funcion ";
                source += handlerName.toUtf8().toStdString();
                source += "():\n";
                const QStringList lines = code.split(QLatin1Char('\n'));
                for (const QString &line : lines)
                {
                    if (!line.isEmpty())
                    {
                        source += "    ";
                        source += line.toUtf8().toStdString();
                    }
                    source += '\n';
                }
                if (item.type == QStringLiteral("Script") && item.trackIndex >= 0 && item.trackIndex < trackEventFunctions.size())
                {
                    trackEventFunctions[item.trackIndex].append(TrackEventFunction{ item.startFrame, handlerName });
                }
            }
            else
            {
                if (item.type == QStringLiteral("Script") && item.trackIndex >= 0 && item.trackIndex < trackEventFunctions.size())
                {
                    const QString handlerName = QStringLiteral("__event_editor_timeline_%1").arg(index);
                    const QRegularExpressionMatch eventMatch = eventDeclaration.match(code);
                    if (eventMatch.hasMatch())
                    {
                        const QString handlerDeclaration = QStringLiteral("funcion %1():").arg(handlerName);
                        code.replace(eventMatch.capturedStart(0), eventMatch.capturedLength(0), handlerDeclaration);
                        trackEventFunctions[item.trackIndex].append(TrackEventFunction{ item.startFrame, handlerName });
                    }
                    else
                    {
                        const QRegularExpressionMatch functionMatch = functionNameDeclaration.match(code);
                        if (functionMatch.hasMatch())
                        {
                            code.replace(functionMatch.capturedStart(1), functionMatch.capturedLength(1), handlerName);
                            trackEventFunctions[item.trackIndex].append(TrackEventFunction{ item.startFrame, handlerName });
                        }
                    }
                }
                source += code.toUtf8().toStdString();
                source += '\n';
            }
        }
        for (int trackIndex = 0; trackIndex < trackEventFunctions.size(); ++trackIndex)
        {
            source += "funcion __event_editor_track_";
            source += QString::number(trackIndex + 1).toUtf8().toStdString();
            source += "():\n";
            QVector<TrackEventFunction> &eventFunctions = trackEventFunctions[trackIndex];
            std::stable_sort(eventFunctions.begin(), eventFunctions.end(), [](const TrackEventFunction &left, const TrackEventFunction &right)
            {
                return left.StartFrame < right.StartFrame;
            });
            if (eventFunctions.isEmpty())
            {
                source += "    devolver\n";
                continue;
            }
            for (const TrackEventFunction &eventFunction : eventFunctions)
            {
                source += "    ";
                source += eventFunction.Name.toUtf8().toStdString();
                source += "()\n";
            }
        }
        const std::string utf8Source = source;
        ohana::Lexer lexer(utf8Source);
        ohana::Parser parser(lexer.tokenize());
        ohana::Program program = parser.parse();
        ohana::Compiler compiler;
        ohana::Chunk chunk = compiler.compile(program);
        for (const StartEventItem &item : mStartEvents)
        {
            chunk.strings.push_back(item.stringValue.toUtf8().toStdString());
        }
        std::ostringstream bytecodeStream(std::ios::out | std::ios::binary);
        std::unordered_map<std::string, std::uint32_t> stringPoolIndices;
        ohana::escribir_bytecode(chunk, bytecodeStream, stringPoolIndices);
        const std::string bytecode = bytecodeStream.str();
        const auto appendU32 = [](QByteArray &target, quint32 value)
        {
            for (int byte = 0; byte < 4; ++byte)
            {
                target.append(static_cast<char>((value >> (byte * 8)) & 0xff));
            }
        };

        QByteArray startEventRegion;
        if (static_cast<quint64>(mStartEvents.size()) > UINT32_MAX)
        {
            throw std::runtime_error("demasiadas filas de StartEvent para el formato");
        }
        appendU32(startEventRegion, static_cast<quint32>(mStartEvents.size()));
        for (const StartEventItem &item : mStartEvents)
        {
            const std::string value = item.stringValue.toUtf8().toStdString();
            const auto stringIndex = stringPoolIndices.find(value);
            if (stringIndex == stringPoolIndices.end())
            {
                throw std::runtime_error("no se encontró un string StartEvent en el string pool");
            }
            appendU32(startEventRegion, stringIndex->second);
            appendU32(startEventRegion, static_cast<quint32>(item.firstInt));
            appendU32(startEventRegion, static_cast<quint32>(item.secondInt));
        }
        if (static_cast<quint64>(startEventRegion.size()) > UINT32_MAX ||
            bytecode.size() > static_cast<std::size_t>(UINT32_MAX))
        {
            throw std::runtime_error("el paquete excede el límite de tamaño del formato");
        }

        QByteArray package;
        package.append("FEST", 4);
        appendU32(package, 2);
        appendU32(package, static_cast<quint32>(startEventRegion.size()));
        appendU32(package, static_cast<quint32>(bytecode.size()));
        package.append(startEventRegion);
        package.append(bytecode.data(), static_cast<qsizetype>(bytecode.size()));

        QSaveFile output(fileName);
        if (!output.open(QIODevice::WriteOnly) || output.write(package) != package.size() || !output.commit())
        {
            const QString message = QStringLiteral("Could not write the package:\n%1").arg(output.errorString());
            if (errorMessage)
            {
                *errorMessage = message;
            }
            else
            {
                QMessageBox::critical(this, QStringLiteral("Export failed"), message);
            }
            return false;
        }
        if (errorMessage)
        {
            errorMessage->clear();
        }
        else
        {
            statusBar()->showMessage(QStringLiteral("Exported OhanaScript bytecode package: %1").arg(fileName), 5000);
        }
        return true;
    }
    catch (const std::exception &error)
    {
        const QString message = QString::fromUtf8(error.what());
        if (errorMessage)
        {
            *errorMessage = message;
        }
        else
        {
            QMessageBox::critical(this, QStringLiteral("OhanaScript compilation failed"), message);
        }
        return false;
    }
}
bool EventEditor::readFile(const QString &fileName, QString *errorMessage)
{
    auto fail = [this, errorMessage](const QString &title, const QString &message)
    {
        if (errorMessage)
        {
            *errorMessage = message;
        }
        else
        {
            QMessageBox::critical(this, title, message);
        }
        return false;
    };
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        return fail(QStringLiteral("Open failed"), QStringLiteral("Could not read the file:\n%1").arg(file.errorString()));
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return fail(QStringLiteral("Invalid event file"), QStringLiteral("The file is not a valid EventEditor JSON file:\n%1").arg(parseError.errorString()));
    }
    const QJsonObject root = document.object();
    const QJsonValue eventValue = root.value(QStringLiteral("events"));
    if (!eventValue.isArray())
    {
        return fail(QStringLiteral("Invalid event file"), QStringLiteral("The file does not contain an 'events' array."));
    }
    const QJsonValue startEventValue = root.value(QStringLiteral("StartEvent"));
    if (!startEventValue.isUndefined() && !startEventValue.isArray())
    {
        return fail(QStringLiteral("Invalid event file"), QStringLiteral("The 'StartEvent' value must be an array."));
    }
    QVector<TrackItem> loadedTracks;
    const QJsonValue trackValue = root.value(QStringLiteral("tracks"));
    if (trackValue.isArray())
    {
        const QJsonArray trackArray = trackValue.toArray();
        for (int index = 0; index < trackArray.size(); ++index)
        {
            const QJsonValue value = trackArray.at(index);
            TrackItem track;
            if (value.isObject())
            {
                track.name = value.toObject().value(QStringLiteral("name")).toString();
            }
            else
            {
                track.name = value.toString();
            }
            if (track.name.isEmpty())
            {
                track.name = QStringLiteral("Track %1").arg(index + 1);
            }
            loadedTracks.append(track);
        }
    }
    if (loadedTracks.isEmpty())
    {
        loadedTracks.append(TrackItem{QStringLiteral("Track 1")});
    }
    const int newMaxFrames = qBound(kMinimumFrames, root.value(QStringLiteral("maxFrames")).toInt(240), kMaximumFrames);
    QVector<EventItem> loadedEvents;
    QVector<StartEventItem> loadedStartEvents;
    const QJsonArray eventArray = eventValue.toArray();
    for (const QJsonValue &value : eventArray)
    {
        if (!value.isObject())
        {
            continue;
        }
        const QJsonObject object = value.toObject();
        EventItem item;
        item.name = object.value(QStringLiteral("name")).toString(QStringLiteral("Event"));
        item.type = normalizeEventType(object.value(QStringLiteral("type")).toString(QStringLiteral("Animation")));
        item.startFrame = clampFrame(object.value(QStringLiteral("startFrame")).toInt(0), newMaxFrames);
        item.endFrame = qBound(item.startFrame, object.value(QStringLiteral("endFrame")).toInt(item.startFrame), newMaxFrames - 1);
        item.trackIndex = qBound(0, object.value(QStringLiteral("track")).toInt(0), static_cast<int>(loadedTracks.size()) - 1);
        item.mdlId = object.value(QStringLiteral("mdlId")).toInt(0);
        item.anmId = object.value(QStringLiteral("anmId")).toInt(0);
        item.argument = object.value(QStringLiteral("argument")).toInt(0);
        item.color = eventColor(loadedEvents.size());
        item.description = object.value(QStringLiteral("description")).toString();
        item.scriptFilePath = object.value(QStringLiteral("scriptFile")).toString();
        if (!item.scriptFilePath.isEmpty())
        {
            const QFileInfo scriptInfo(item.scriptFilePath);
            const QDir projectDirectory(QFileInfo(fileName).absolutePath());
            item.scriptFilePath = scriptInfo.isAbsolute()
                ? scriptInfo.absoluteFilePath()
                : projectDirectory.absoluteFilePath(item.scriptFilePath);
            item.scriptFilePath = QDir::cleanPath(item.scriptFilePath);
        }
        item.script = object.value(QStringLiteral("script")).toString();
        const QJsonObject camera = object.value(QStringLiteral("camera")).toObject();
        readTransform(camera.value(QStringLiteral("start")), item.cameraStart);
        readTransform(camera.value(QStringLiteral("end")), item.cameraEnd);
        loadedEvents.append(item);
    }
    const QJsonArray startEventArray = startEventValue.toArray();
    for (const QJsonValue &value : startEventArray)
    {
        if (!value.isObject())
        {
            continue;
        }
        const QJsonObject object = value.toObject();
        StartEventItem item;
        item.stringValue = object.value(QStringLiteral("string")).toString();
        item.firstInt = object.value(QStringLiteral("int1")).toInt(0);
        item.secondInt = object.value(QStringLiteral("int2")).toInt(0);
        loadedStartEvents.append(item);
    }
    mTracks = loadedTracks;
    mEvents = loadedEvents;
    mStartEvents = loadedStartEvents;
    mMaxFrames = newMaxFrames;
    mCurrentFrame = clampFrame(mCurrentFrame, mMaxFrames);
    mSelectedIndex = mEvents.isEmpty() ? -1 : 0;
    mSelectedTrack = mSelectedIndex >= 0 ? mEvents.at(mSelectedIndex).trackIndex : 0;
    mCurrentFile = fileName;
    mModified = false;
    refreshViews(true);
    refreshStartEventTable();
    updateFrameControls();
    updateWindowTitle();
    updateStatus();
    statusBar()->showMessage(QStringLiteral("Opened %1").arg(fileName), 4000);
    return true;
}
bool EventEditor::compileFileToFest(const QString &inputFile, const QString &outputFile, QString *errorMessage)
{
    if (errorMessage)
    {
        errorMessage->clear();
    }
    EventEditor editor;
    if (!editor.readFile(inputFile, errorMessage))
    {
        return false;
    }
    return editor.exportBytecode(outputFile, errorMessage);
}
void EventEditor::addTrack()
{
    TrackItem track;
    track.name = QStringLiteral("Track %1").arg(mTracks.size() + 1);
    mTracks.append(track);
    mSelectedTrack = mTracks.size() - 1;
    mSelectedIndex = -1;
    refreshViews(true);
    markModified();
    updateStatus();
}
void EventEditor::removeTrack()
{
    if (mTracks.size() <= 1)
    {
        return;
    }

    const int trackIndex = qBound(0, mSelectedTrack, static_cast<int>(mTracks.size()) - 1);
    int eventsToRemove = 0;
    for (const EventItem &item : mEvents)
    {
        if (item.trackIndex == trackIndex)
        {
            ++eventsToRemove;
        }
    }

    const QString trackName = mTracks.at(trackIndex).name;
    const QString prompt = eventsToRemove == 0
        ? QStringLiteral("Remove '%1'?").arg(trackName)
        : QStringLiteral("Remove '%1' and its %2 event(s)?").arg(trackName).arg(eventsToRemove);
    const auto answer = QMessageBox::question(this, QStringLiteral("Remove track"), prompt,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
    {
        return;
    }

    for (int index = mEvents.size() - 1; index >= 0; --index)
    {
        if (mEvents.at(index).trackIndex == trackIndex)
        {
            mEvents.removeAt(index);
        }
        else if (mEvents[index].trackIndex > trackIndex)
        {
            --mEvents[index].trackIndex;
        }
    }
    mTracks.removeAt(trackIndex);
    mSelectedTrack = qMin(trackIndex, static_cast<int>(mTracks.size()) - 1);
    mSelectedIndex = -1;

    refreshViews(true);
    markModified();
    updateStatus();
}
void EventEditor::addEvent()
{
    const int trackIndex = qBound(0, mSelectedTrack, static_cast<int>(mTracks.size()) - 1);
    auto isFrameOccupied = [this, trackIndex](int frame)
    {
        for (const EventItem &existing : mEvents)
        {
            if (existing.trackIndex == trackIndex && frame >= existing.startFrame && frame <= existing.endFrame)
            {
                return true;
            }
        }
        return false;
    };

    int startFrame = -1;
    for (int frame = clampFrame(mCurrentFrame, mMaxFrames); frame < mMaxFrames; ++frame)
    {
        if (!isFrameOccupied(frame))
        {
            startFrame = frame;
            break;
        }
    }
    if (startFrame < 0)
    {
        for (int frame = 0; frame < clampFrame(mCurrentFrame, mMaxFrames); ++frame)
        {
            if (!isFrameOccupied(frame))
            {
                startFrame = frame;
                break;
            }
        }
    }
    if (startFrame < 0)
    {
        QMessageBox::information(this, QStringLiteral("Track is full"),
            QStringLiteral("The selected track has no free frames. Move an event or increase the timeline length first."));
        return;
    }

    EventItem item;
    item.name = QStringLiteral("Event %1").arg(mEvents.size() + 1);
    item.type = QStringLiteral("Animation");
    item.startFrame = startFrame;
    item.trackIndex = trackIndex;
    item.endFrame = qMin(mMaxFrames - 1, startFrame + 9);
    for (const EventItem &existing : mEvents)
    {
        if (existing.trackIndex == trackIndex && existing.startFrame > startFrame)
        {
            item.endFrame = qMin(item.endFrame, existing.startFrame - 1);
        }
    }
    item.color = eventColor(mEvents.size());
    mEvents.append(item);
    mSelectedIndex = mEvents.size() - 1;
    mSelectedTrack = item.trackIndex;
    mCurrentFrame = item.startFrame;
    if (!editEvent(mSelectedIndex))
    {
        mEvents.removeAt(mSelectedIndex);
        mSelectedIndex = mEvents.isEmpty() ? -1 : mEvents.size() - 1;
        if (mSelectedIndex >= 0)
        {
            mSelectedTrack = mEvents.at(mSelectedIndex).trackIndex;
        }
        refreshViews(true);
        updateStatus();
    }
}
void EventEditor::copySelectedEvent()
{
    if (mSelectedIndex < 0 || mSelectedIndex >= mEvents.size())
    {
        return;
    }
    mEventClipboard = mEvents.at(mSelectedIndex);
    mHasEventClipboard = true;
    refreshViews(false);
    statusBar()->showMessage(QStringLiteral("Copied event: %1").arg(mEventClipboard.name), 3000);
}
void EventEditor::pasteEvent()
{
    if (!mHasEventClipboard || mTracks.isEmpty())
    {
        return;
    }

    const int duration = mEventClipboard.endFrame - mEventClipboard.startFrame + 1;
    if (duration <= 0 || duration > mMaxFrames)
    {
        QMessageBox::information(this, QStringLiteral("Cannot paste event"),
            QStringLiteral("The copied event is longer than the current timeline. Increase the timeline length first."));
        return;
    }

    const int trackIndex = qBound(0, mSelectedTrack, static_cast<int>(mTracks.size()) - 1);
    const int firstFrame = clampFrame(mCurrentFrame, mMaxFrames);
    const int lastPossibleStart = mMaxFrames - duration;
    auto rangeIsFree = [this, trackIndex, duration](int startFrame)
    {
        const int endFrame = startFrame + duration - 1;
        for (const EventItem &existing : mEvents)
        {
            if (existing.trackIndex == trackIndex && existing.startFrame <= endFrame && existing.endFrame >= startFrame)
            {
                return false;
            }
        }
        return true;
    };

    int startFrame = -1;
    for (int frame = firstFrame; frame <= lastPossibleStart; ++frame)
    {
        if (rangeIsFree(frame))
        {
            startFrame = frame;
            break;
        }
    }
    if (startFrame < 0)
    {
        for (int frame = 0; frame < firstFrame && frame <= lastPossibleStart; ++frame)
        {
            if (rangeIsFree(frame))
            {
                startFrame = frame;
                break;
            }
        }
    }
    if (startFrame < 0)
    {
        QMessageBox::information(this, QStringLiteral("Track is full"),
            QStringLiteral("There is no free space for this event on the selected track."));
        return;
    }

    EventItem item = mEventClipboard;
    item.name += QStringLiteral(" copy");
    item.startFrame = startFrame;
    item.endFrame = startFrame + duration - 1;
    item.trackIndex = trackIndex;
    item.color = eventColor(mEvents.size());
    mEvents.append(item);
    mSelectedIndex = mEvents.size() - 1;
    mSelectedTrack = trackIndex;
    mCurrentFrame = startFrame;
    refreshViews(true);
    markModified();
    updateStatus();
}
void EventEditor::editSelectedEvent()
{
    editEvent(mSelectedIndex);
}
bool EventEditor::editEvent(int index)
{
    if (index < 0 || index >= mEvents.size())
    {
        return false;
    }
    EventItem item = mEvents.at(index);
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Edit event"));
    dialog.setMinimumWidth(620);
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout();
    auto *nameEdit = new QLineEdit(item.name, &dialog);
    auto *typeCombo = new QComboBox(&dialog);
    typeCombo->addItems({QStringLiteral("Animation"), QStringLiteral("Audio"), QStringLiteral("Camera"), QStringLiteral("Script")});
    item.type = normalizeEventType(item.type);
    if (typeCombo->findText(item.type) < 0)
    {
        typeCombo->addItem(item.type);
    }
    typeCombo->setCurrentText(item.type);
    auto *trackCombo = new QComboBox(&dialog);
    for (const TrackItem &track : mTracks)
    {
        trackCombo->addItem(track.name);
    }
    trackCombo->setCurrentIndex(qBound(0, item.trackIndex, trackCombo->count() - 1));
    auto *startSpin = new QSpinBox(&dialog);
    startSpin->setRange(0, mMaxFrames - 1);
    startSpin->setValue(item.startFrame);
    auto *endSpin = new QSpinBox(&dialog);
    endSpin->setRange(0, mMaxFrames - 1);
    endSpin->setValue(item.endFrame);
    auto *descriptionEdit = new QLineEdit(item.description, &dialog);
    auto *mdlIdSpin = new QSpinBox(&dialog);
    mdlIdSpin->setRange(0, (std::numeric_limits<int>::max)());
    mdlIdSpin->setValue(item.mdlId);
    auto *anmIdSpin = new QSpinBox(&dialog);
    anmIdSpin->setRange(0, (std::numeric_limits<int>::max)());
    anmIdSpin->setValue(item.anmId);
    auto *argumentSpin = new QSpinBox(&dialog);
    argumentSpin->setRange((std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)());
    argumentSpin->setValue(item.argument);
    auto *scriptFilePathEdit = new QLineEdit(item.scriptFilePath, &dialog);
    scriptFilePathEdit->setReadOnly(true);
    scriptFilePathEdit->setPlaceholderText(QStringLiteral("Create or open an OhanaScript source file"));
    scriptFilePathEdit->setToolTip(item.scriptFilePath);
    auto *scriptFileRow = new QWidget(&dialog);
    auto *scriptFileLayout = new QHBoxLayout(scriptFileRow);
    scriptFileLayout->setContentsMargins(0, 0, 0, 0);
    scriptFileLayout->setSpacing(6);
    scriptFileLayout->addWidget(scriptFilePathEdit, 1);
    auto *createScriptButton = new QPushButton(QStringLiteral("Create..."), scriptFileRow);
    auto *openScriptButton = new QPushButton(QStringLiteral("Open..."), scriptFileRow);
    scriptFileLayout->addWidget(createScriptButton);
    scriptFileLayout->addWidget(openScriptButton);
    auto suggestedScriptPath = [&]()
    {
        if (!scriptFilePathEdit->text().isEmpty())
        {
            return scriptFilePathEdit->text();
        }
        const QString directory = mCurrentFile.isEmpty()
            ? QDir::currentPath()
            : QFileInfo(mCurrentFile).absolutePath();
        QString baseName = nameEdit->text().trimmed();
        baseName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]+")), QStringLiteral("_"));
        if (baseName.isEmpty())
        {
            baseName = QStringLiteral("script");
        }
        return QDir(directory).filePath(baseName + QStringLiteral(".ohs"));
    };
    connect(createScriptButton, &QPushButton::clicked, &dialog, [&]()
    {
        QString fileName = QFileDialog::getSaveFileName(&dialog, QStringLiteral("Create OhanaScript source"),
            suggestedScriptPath(), QStringLiteral("OhanaScript source (*.ohs);;All files (*.*)"));
        if (fileName.isEmpty())
        {
            return;
        }
        if (QFileInfo(fileName).suffix().isEmpty())
        {
            fileName += QStringLiteral(".ohs");
        }
        QSaveFile scriptFile(fileName);
        if (!scriptFile.open(QIODevice::WriteOnly))
        {
            QMessageBox::critical(&dialog, QStringLiteral("Create script failed"), QStringLiteral("Could not create the script file:\n%1").arg(scriptFile.errorString()));
            return;
        }
        const QString initialSource = item.script.isEmpty() ? QStringLiteral("evento main:\n    variable listo = verdadero\n") : item.script;
        const QByteArray sourceBytes = initialSource.toUtf8();
        if (scriptFile.write(sourceBytes) != sourceBytes.size() || !scriptFile.commit())
        {
            QMessageBox::critical(&dialog, QStringLiteral("Create script failed"), QStringLiteral("Could not save the script file:\n%1").arg(scriptFile.errorString()));
            return;
        }
        const QString absolutePath = QFileInfo(fileName).absoluteFilePath();
        scriptFilePathEdit->setText(absolutePath);
        scriptFilePathEdit->setToolTip(absolutePath);
    });
    connect(openScriptButton, &QPushButton::clicked, &dialog, [&]()
    {
        const QString fileName = QFileDialog::getOpenFileName(&dialog, QStringLiteral("Open OhanaScript source"), suggestedScriptPath(), QStringLiteral("OhanaScript source (*.ohs *.ohana *.oscript *.txt);;All files (*.*)"));
        if (fileName.isEmpty())
        {
            return;
        }
        QFile scriptFile(fileName);
        if (!scriptFile.open(QIODevice::ReadOnly))
        {
            QMessageBox::critical(&dialog, QStringLiteral("Open script failed"), QStringLiteral("Could not read the script file:\n%1").arg(scriptFile.errorString()));
            return;
        }
        scriptFile.readAll();
        if (scriptFile.error() != QFileDevice::NoError)
        {
            QMessageBox::critical(&dialog, QStringLiteral("Open script failed"), QStringLiteral("Could not read the script file:\n%1").arg(scriptFile.errorString()));
            return;
        }
        const QString absolutePath = QFileInfo(fileName).absoluteFilePath();
        scriptFilePathEdit->setText(absolutePath);
        scriptFilePathEdit->setToolTip(absolutePath);
    });
    struct CameraEditors
    {
        std::array<QDoubleSpinBox *, 3> position{};
        std::array<QDoubleSpinBox *, 3> rotation{};
    };
    CameraEditors startCameraEditors;
    CameraEditors endCameraEditors;
    auto *cameraGroup = new QGroupBox(QStringLiteral("Camera movement"), &dialog);
    auto *cameraLayout = new QHBoxLayout(cameraGroup);
    auto addCameraTransform = [&](const QString &title, const CameraTransform &transform, CameraEditors &editors)
    {
        auto *transformGroup = new QGroupBox(title, cameraGroup);
        auto *transformLayout = new QVBoxLayout(transformGroup);
        auto addVectorFields = [&](const QString &vectorTitle, const std::array<double, 3> &values, std::array<QDoubleSpinBox *, 3> &spins, bool isRotation)
        {
            auto *vectorGroup = new QGroupBox(vectorTitle, transformGroup);
            auto *vectorForm = new QFormLayout(vectorGroup);
            const QStringList axes = {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
            for (int axis = 0; axis < 3; ++axis)
            {
                auto *spin = new QDoubleSpinBox(vectorGroup);
                spin->setRange(isRotation ? -360000.0 : -1000000.0, isRotation ? 360000.0 : 1000000.0);
                spin->setDecimals(3);
                spin->setSingleStep(isRotation ? 1.0 : 0.1);
                spin->setKeyboardTracking(false);
                spin->setValue(values[static_cast<std::size_t>(axis)]);
                if (isRotation)
                {
                    spin->setSuffix(QStringLiteral("°"));
                }
                spins[static_cast<std::size_t>(axis)] = spin;
                vectorForm->addRow(axes[axis] + QStringLiteral(":"), spin);
            }
            transformLayout->addWidget(vectorGroup);
        };
        addVectorFields(QStringLiteral("Position"), transform.position, editors.position, false);
        addVectorFields(QStringLiteral("Rotation"), transform.rotation, editors.rotation, true);
        cameraLayout->addWidget(transformGroup);
    };
    addCameraTransform(QStringLiteral("Start"), item.cameraStart, startCameraEditors);
    addCameraTransform(QStringLiteral("End"), item.cameraEnd, endCameraEditors);
    auto *colorButton = new QPushButton(QStringLiteral("Choose color…"), &dialog);
    QColor selectedColor = item.color;
    auto updateColorButton = [colorButton, &selectedColor]() 
    {
        colorButton->setStyleSheet(QStringLiteral("QPushButton { background-color: %1; color: %2; padding: 5px; }")
            .arg(selectedColor.name(), selectedColor.lightness() < 140 ? QStringLiteral("white") : QStringLiteral("#202020")));
        colorButton->setText(selectedColor.name(QColor::HexRgb));
    };
    updateColorButton();
    connect(colorButton, &QPushButton::clicked, &dialog, [&]()     
    {
        const QColor picked = QColorDialog::getColor(selectedColor, &dialog, QStringLiteral("Event color"));
        if (picked.isValid())
        {
            selectedColor = picked;
            updateColorButton();
        }
    });
    form->addRow(QStringLiteral("Name:"), nameEdit);
    form->addRow(QStringLiteral("Type:"), typeCombo);
    form->addRow(QStringLiteral("Track:"), trackCombo);
    form->addRow(QStringLiteral("Start frame:"), startSpin);
    form->addRow(QStringLiteral("End frame:"), endSpin);
    form->addRow(QStringLiteral("MdlID:"), mdlIdSpin);
    form->addRow(QStringLiteral("AnmID:"), anmIdSpin);
    form->addRow(QStringLiteral("Argument (int):"), argumentSpin);
    form->addRow(QStringLiteral("Description:"), descriptionEdit);
    form->addRow(QStringLiteral("OhanaScript file:"), scriptFileRow);
    form->addRow(cameraGroup);
    QWidget *descriptionLabel = form->labelForField(descriptionEdit);
    QWidget *mdlIdLabel = form->labelForField(mdlIdSpin);
    QWidget *anmIdLabel = form->labelForField(anmIdSpin);
    QWidget *argumentLabel = form->labelForField(argumentSpin);
    QWidget *scriptLabel = form->labelForField(scriptFileRow);
    auto updateTypeFields = [typeCombo, descriptionEdit, descriptionLabel, mdlIdSpin, mdlIdLabel, anmIdSpin, anmIdLabel, argumentSpin, argumentLabel, scriptFileRow, scriptLabel, cameraGroup, &dialog]()
    {
        const bool isAnimation = typeCombo->currentText() == QStringLiteral("Animation");
        const bool isScript = typeCombo->currentText() == QStringLiteral("Script");
        const bool isCamera = typeCombo->currentText() == QStringLiteral("Camera");
        const bool showDescription = !isScript && !isAnimation;
        descriptionEdit->setVisible(showDescription);
        descriptionLabel->setVisible(showDescription);
        mdlIdSpin->setVisible(isAnimation);
        mdlIdLabel->setVisible(isAnimation);
        anmIdSpin->setVisible(isAnimation);
        anmIdLabel->setVisible(isAnimation);
        argumentSpin->setVisible(isAnimation);
        argumentLabel->setVisible(isAnimation);
        scriptFileRow->setVisible(isScript);
        scriptLabel->setVisible(isScript);
        cameraGroup->setVisible(isCamera);
        dialog.adjustSize();
    };
    connect(typeCombo, &QComboBox::currentTextChanged, &dialog, updateTypeFields);
    updateTypeFields();
    form->addRow(QStringLiteral("Color:"), colorButton);
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
    {
        return false;
    }
    item.name = nameEdit->text().trimmed();
    if (item.name.isEmpty())
    {
        item.name = QStringLiteral("Event");
    }
    item.type = typeCombo->currentText();
    item.trackIndex = qBound(0, trackCombo->currentIndex(), static_cast<int>(mTracks.size()) - 1);
    mSelectedTrack = item.trackIndex;
    item.startFrame = qMin(startSpin->value(), endSpin->value());
    item.endFrame = qMax(startSpin->value(), endSpin->value());
    item.description = descriptionEdit->text();
    item.mdlId = mdlIdSpin->value();
    item.anmId = anmIdSpin->value();
    item.argument = argumentSpin->value();
    const QString selectedScriptPath = scriptFilePathEdit->text().trimmed();
    if (!selectedScriptPath.isEmpty())
    {
        item.scriptFilePath = QFileInfo(selectedScriptPath).absoluteFilePath();
        item.script.clear();
    }
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
        item.cameraStart.position[axis] = startCameraEditors.position[axis]->value();
        item.cameraStart.rotation[axis] = startCameraEditors.rotation[axis]->value();
        item.cameraEnd.position[axis] = endCameraEditors.position[axis]->value();
        item.cameraEnd.rotation[axis] = endCameraEditors.rotation[axis]->value();
    }
    item.color = selectedColor;
    mEvents[index] = item;
    mSelectedIndex = index;
    refreshViews(true);
    markModified();
    updateStatus();
    return true;
}
void EventEditor::deleteSelectedEvent()
{
    if (mSelectedIndex < 0 || mSelectedIndex >= mEvents.size())
    {
        return;
    }
    const QString name = mEvents.at(mSelectedIndex).name;
    const auto answer = QMessageBox::question(this, QStringLiteral("Delete event"), QStringLiteral("Delete '%1'?").arg(name), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
    {
        return;
    }
    mEvents.removeAt(mSelectedIndex);
    if (mEvents.isEmpty())
    {
        mSelectedIndex = -1;
    }
    else
    {
        mSelectedIndex = qMin(mSelectedIndex, mEvents.size() - 1);
        mSelectedTrack = mEvents.at(mSelectedIndex).trackIndex;
    }
    refreshViews(true);
    markModified();
    updateStatus();
}
void EventEditor::duplicateSelectedEvent()
{
    if (mSelectedIndex < 0 || mSelectedIndex >= mEvents.size())
    {
        return;
    }
    EventItem copy = mEvents.at(mSelectedIndex);
    copy.name += QStringLiteral(" copy");
    const int length = copy.endFrame - copy.startFrame;
    copy.startFrame = qMin(mMaxFrames - 1 - length, copy.startFrame + 1);
    copy.endFrame = qMin(mMaxFrames - 1, copy.startFrame + length);
    copy.color = eventColor(mEvents.size());
    mEvents.insert(mSelectedIndex + 1, copy);
    mSelectedTrack = copy.trackIndex;
    ++mSelectedIndex;
    refreshViews(true);
    markModified();
    updateStatus();
}
void EventEditor::closeEvent(QCloseEvent *event)
{
    if (confirmDiscardChanges())
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}