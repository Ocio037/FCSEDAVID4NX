#pragma once
#include <QtWidgets/QMainWindow>
#include <QColor>
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <array>
#include "ui_EventEditor.h"
struct CameraTransform
{
    std::array<double, 3> position{};
    std::array<double, 3> rotation{};
};
struct TrackItem
{
    QString name;
};
struct EventItem
{
    QString name;
    QString type;
    int startFrame = 0;
    int endFrame = 10;
    int trackIndex = 0;
    int mdlId = 0;
    int anmId = 0;
    int argument = 0;
    QColor color = QColor(84, 150, 220);
    QString description;
    QString scriptFilePath;
    QString script;
    CameraTransform cameraStart;
    CameraTransform cameraEnd;
};
class EventNamesCanvas;
class EventTimelineCanvas;
class EventRuler;
class QAction;
class QScrollArea;
class QSpinBox;
class QSlider;
class QLabel;
class QCloseEvent;
class QTableWidget;
struct StartEventItem
{
    QString stringValue;
    int firstInt = 0;
    int secondInt = 0;
};
class EventEditor : public QMainWindow
{
    Q_OBJECT
public:
    explicit EventEditor(QWidget *parent = nullptr);
    ~EventEditor() override;
    static bool compileFileToFest(const QString &inputFile, const QString &outputFile, QString *errorMessage = nullptr);
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void refreshViews(bool resizeContents = true);
    void updateFrameControls();
    void updateWindowTitle();
    void updateStatus();
    void setFrameWidth(int width);
    void markModified();
    bool confirmDiscardChanges();
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    bool writeFile(const QString &fileName);
    QJsonObject projectJson(const QString &fileName) const;
    bool exportBytecode(const QString &outputFile = QString(), QString *errorMessage = nullptr);
    bool readFile(const QString &fileName, QString *errorMessage = nullptr);
    void addTrack();
    void removeTrack();
    void addEvent();
    void copySelectedEvent();
    void pasteEvent();
    void editSelectedEvent();
    bool editEvent(int index);
    void deleteSelectedEvent();
    void duplicateSelectedEvent();
    void addStartEventRow();
    void removeSelectedStartEventRows();
    void refreshStartEventTable();
    void appendStartEventRow(const StartEventItem &item);
    Ui::EventEditorClass ui;
    QVector<TrackItem> mTracks;
    QVector<EventItem> mEvents;
    EventItem mEventClipboard;
    bool mHasEventClipboard = false;
    QVector<StartEventItem> mStartEvents;
    QString mCurrentFile;
    bool mModified = false;
    int mSelectedIndex = -1;
    int mSelectedTrack = 0;
    int mCurrentFrame = 0;
    int mMaxFrames = 240;
    int mFrameWidth = 16;
    EventNamesCanvas *mNamesCanvas = nullptr;
    EventTimelineCanvas *mTimelineCanvas = nullptr;
    EventRuler *mRuler = nullptr;
    QScrollArea *mNamesScroll = nullptr;
    QScrollArea *mTimelineScroll = nullptr;
    QSpinBox *mCurrentFrameSpin = nullptr;
    QSpinBox *mMaxFramesSpin = nullptr;
    QSlider *mZoomSlider = nullptr;
    QLabel *mZoomLabel = nullptr;
    QTableWidget *mStartEventTable = nullptr;
    QAction *mRemoveTrackAction = nullptr;
    QAction *mCopyEventAction = nullptr;
    QAction *mPasteEventAction = nullptr;
};
