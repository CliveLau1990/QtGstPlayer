#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <gst/gst.h>
#include <gst/audio/streamvolume.h>
#include <gst/video/videooverlay.h>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QTimer>
#include <QStackedWidget>
#include "videowidget.h"
#include "playercontrols.h"

/* Structure to contain all our information, so we can pass it around */
typedef struct _CustomData {
  GstElement *playbin2;           /* Our one and only pipeline */
  QSlider *slider;              /* Slider widget to keep track of current position */
  QLabel  *streams_list;        /* Text widget to display info about the streams */
  gulong slider_update_signal_id; /* Signal ID for the slider update signal */

  GstState state;                 /* Current state of the pipeline */
  gint64 duration;                /* Duration of the clip, in nanoseconds */
  gboolean playing;              /* Are we in the PLAYING state? */
  gboolean seek_enabled;         /* Is seeking enabled for this media? */
  GstBus *bus;
} CustomData;

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = 0);
    ~Widget();
    bool refresh_ui (CustomData *data);

protected:
    void closeEvent(QCloseEvent *); // 窗口关闭时候应做的处理,退出应用程序。
    void resizeEvent(QResizeEvent *event);

private slots:
    void slotOpenButtonClicked();
    void slotPlayButtonClicked();
    void slotPaluseButtonClicked();
    void slotStopButtonClicked();
    void seek(int seconds);
    void slotTimerout();
    void slotmuteButtonClicked();
    void slotVolumeChange(int);

public slots:
    void slotFullScreen(bool flag);

private:
    void playButtonClicked(QPushButton *button, CustomData *data);
    void plauseButtonClicked(QPushButton *button, CustomData *data);
    void stopButtonClicked(QPushButton *button,CustomData *data);
    void delete_event_cb (QWidget *widget, QEvent *event, CustomData *data);
    bool expose_cb(QWidget *widget, QEvent *event, CustomData *data);
    void createUi(CustomData *data);
    void slider_cb (CustomData *data);
    void analyze_streams(CustomData *data);
    void realize_cb (QWidget *widget, CustomData *data);
    void handle_message (CustomData *data, GstMessage *msg);

private:
    VideoWidget *displayWnd;
    QLabel *backgroundWnd;
    QStackedWidget *renderWnd;
    QHBoxLayout *timeLayout;
    QLabel *timeLabel;
    QSlider *slider;
    QLabel *infoLabel;
    QPushButton *openBtn;
    QPushButton *startBtn;
    QPushButton *stopBtn;
    QPushButton *plauseBtn;
    QAbstractButton *muteButton;
    QSlider  *volumeSlider;
    PlayerControls *playButtonControl;
    QVBoxLayout *mainLayout;
    QHBoxLayout *buttonLayout;
    CustomData *data;
    QTimer   *queryTimer;
    QString  uri;
    GstBus *bus;

    bool   muteFlag;
};

#endif // WIDGET_H
