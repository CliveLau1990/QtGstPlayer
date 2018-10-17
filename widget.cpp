#include "widget.h"
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QToolButton>
#include <QStyle>

void Widget::handle_message (CustomData *data, GstMessage *msg)
{
  GError *err;
  gchar *debug_info;

  switch (GST_MESSAGE_TYPE (msg))
  {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error (msg, &err, &debug_info);
      qCritical() << "Error received from element " << QString(GST_OBJECT_NAME (msg->src)) << ":" << QString(err->message);
      qCritical() << "Debugging information:" << QString(debug_info);
      g_clear_error (&err);
      g_free (debug_info);
      if(queryTimer->isActive())
      {
          queryTimer->stop();
      }
      break;
    case GST_MESSAGE_EOS:
      qInfo ("End-Of-Stream reached.\n");
      if(queryTimer->isActive())
      {
          queryTimer->stop();
      }

      break;
    case GST_MESSAGE_DURATION:
      data->duration = GST_CLOCK_TIME_NONE;
      break;
    case GST_MESSAGE_STATE_CHANGED:
    {
      GstState old_state, new_state, pending_state;
      gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
      qInfo() << "GST_MESSAGE_STATE_CHANGED is called!!!";
      if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->playbin2))
      {
        g_print ("Pipeline state changed from %s to %s:\n",gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));

        /* Remember whether we are in the PLAYING state or not */
        data->playing = (data->playbin2->current_state == GST_STATE_PLAYING);

        if (data->playing)
        {
          /* We just moved to PLAYING. Check if seeking is possible */
           qInfo() << "data->playing is called!!";
          GstQuery *query;
          gint64 start, end;
          query = gst_query_new_seeking (GST_FORMAT_TIME);
          if (gst_element_query (data->playbin2, query))
          {
            gst_query_parse_seeking (query, NULL, &data->seek_enabled, &start, &end);
            if (data->seek_enabled) {
              g_print ("Seeking is ENABLED from %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT "\n",
                  GST_TIME_ARGS (start), GST_TIME_ARGS (end));
            }
            else
            {
              g_print ("Seeking is DISABLED for this stream.\n");
            }
          }
          else
          {
            g_printerr ("Seeking query failed.");
          }
          gst_query_unref (query);
        }
      }
    } break;
    default:
      /* We should not reach here */
      g_printerr ("Unexpected message received.\n");
      break;
  }
  gst_message_unref (msg);
}

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    data = new CustomData;
    muteFlag = true;

    /* Initialize our data structure */
    memset (data, 0, sizeof (CustomData));
    data->duration = GST_CLOCK_TIME_NONE;

    /* Create the elements */
    data->playbin2 = gst_element_factory_make ("playbin", "playbin2");

    if (!data->playbin2)
    {
      qWarning("Not all elements could be created.\n");
      return ;
    }

    /* Set the URI to play */
    g_object_set(data->playbin2, "uri", uri.toUtf8().data(), NULL);

    /* Create the GUI */
    createUi(data);

    /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
    bus = gst_element_get_bus (data->playbin2);
    queryTimer = new QTimer;
    connect(queryTimer,SIGNAL(timeout()),this,SLOT(slotTimerout()));
}

void Widget::slotTimerout()
{
    GstMessage *msg;
    msg = gst_bus_timed_pop_filtered (data->bus, GST_CLOCK_TIME_NONE,
        GstMessageType(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_DURATION));

    /* Parse message */
    if (msg != NULL)
    {
       qInfo() << "msg != NULL";
       handle_message (data, msg);
    }
    refresh_ui(data);
    expose_cb(displayWnd,NULL,data);
    analyze_streams(data);
}

Widget::~Widget()
{
}

void Widget::closeEvent(QCloseEvent *)
{
    if(queryTimer->isActive())
    {
        queryTimer->stop();
    }
    delete_event_cb(NULL,NULL,data);

    /* Free resources */
    gst_element_set_state (data->playbin2, GST_STATE_NULL);
    gst_object_unref (data->playbin2);

    if(data != NULL)
    {
        delete data;
        data = NULL;
    }
}

/* This function is called when the GUI toolkit creates the physical window that will hold the video.
 * At this point we can retrieve its handler (which has a different meaning depending on the windowing system)
 * and pass it to GStreamer through the XOverlay interface. */

void Widget::realize_cb(QWidget *widget, CustomData *data)
{
   guintptr window_handle;
   window_handle = (guintptr)(widget->winId());
   gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(data->playbin2),window_handle);
}

/*This funcion is called when the PLAY button is clicked!*/
void Widget::playButtonClicked(QPushButton *button, CustomData *data)
{
   Q_UNUSED(button);
   GstStateChangeReturn ret;
   ret = gst_element_set_state (data->playbin2, GST_STATE_PLAYING);
   if (ret == GST_STATE_CHANGE_FAILURE)
   {
     g_printerr ("Unable to set the pipeline to the playing state.\n");
     gst_object_unref (data->playbin2);
     QMessageBox::information(this,tr("Tips"),tr("Failed to render video!"),1);
     data->playbin2 = gst_element_factory_make ("playbin", "playbin2");
     realize_cb(displayWnd,data);
     if (!data->playbin2)
     {
       qWarning("Not all elements could be created.\n");
       return ;
     }
     return;
   }
   else
   {
       if(!queryTimer->isActive())
       {
           queryTimer->start(100*3);
       }
   }

   g_object_set(G_OBJECT(data->playbin2), "volume", 50*1.0/100, NULL);
   g_object_set(G_OBJECT(data->playbin2), "mute", FALSE, NULL);

}

/* This function is called when the PAUSE button is clicked */
void Widget::plauseButtonClicked(QPushButton *button, CustomData *data)
{
    Q_UNUSED(button);
    if(uri == "")
    {
        return;
    }
    GstStateChangeReturn ret;
    ret = gst_element_set_state(data->playbin2,GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
      g_printerr ("Unable to set the pipeline to the plauseing state.\n");
      gst_object_unref (data->playbin2);
      QMessageBox::information(this,tr("Tips"),tr("Failed to pause!"),1);
      data->playbin2 = gst_element_factory_make ("playbin", "playbin2");
      realize_cb(displayWnd,data);
      if (!data->playbin2)
      {
        qWarning("Not all elements could be created.\n");
        return ;
      }
      return;
    }
}

/* This function is called when the STOP button is clicked */
void Widget::stopButtonClicked(QPushButton *button, CustomData *data)
{
    Q_UNUSED(button);
    if(uri == "")
    {
        return;
    }
    GstStateChangeReturn ret;
    if(data != NULL && data->playbin2 != NULL)
    {
      ret = gst_element_set_state (data->playbin2, GST_STATE_READY);  ;
      if (ret == GST_STATE_CHANGE_FAILURE)
      {
        g_printerr ("Unable to set the pipeline to the stopping state.\n");
        gst_object_unref (data->playbin2);
        return;
      }
      else
      {
          startBtn->setText("播放");
      }
    }
}

/* This function is called when the main window is closed */
void Widget::delete_event_cb(QWidget *widget, QEvent *event, CustomData *data)
{
    Q_UNUSED(widget);
    Q_UNUSED(event);
    if(data != NULL)
    {
      stopButtonClicked(NULL,data);
    }
}

/* This function is called everytime the video window needs to be redrawn (due to damage/exposure,
 * rescaling, etc). GStreamer takes care of this in the PAUSED and PLAYING states, otherwise,
 * we simply draw a black rectangle to avoid garbage showing up. */
bool Widget::expose_cb(QWidget *widget, QEvent *event, CustomData *data)
{
    Q_UNUSED(widget);
    Q_UNUSED(event);
    if(data->playbin2->current_state == GST_STATE_PLAYING)
    {
        qInfo() << "expose_cb is called!";
        renderWnd->setCurrentIndex(0);
    }
    else if(data->playbin2->current_state == GST_STATE_READY)
    {
        renderWnd->setCurrentIndex(1);
        slider->setValue(0);
        volumeSlider->setValue(50);
    }
    return false;
}

/* This function is called when the slider changes its position. We perform a seek to the
 * new position here. */
void Widget::slider_cb(CustomData *data)
{
    double value = data->slider->value();
    gst_element_seek_simple(data->playbin2,GST_FORMAT_TIME,GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),(gint64)(value * GST_SECOND));
}

 void Widget::createUi (CustomData *data)
 {
    setWindowTitle(tr("MultiPlayer"));
    setWindowIcon(QIcon(":/new/image/Image/icon_fronware.png"));
    resize(600,500);
    setStyleSheet("background-color: white;");

     mainLayout = new QVBoxLayout(this);
     mainLayout->setContentsMargins(5,5,5,5);
     mainLayout->setSpacing(5);

     displayWnd = new VideoWidget;

     backgroundWnd = new QLabel;
     backgroundWnd->setStyleSheet("background-color: black;");

     renderWnd = new QStackedWidget;
     renderWnd->addWidget(displayWnd);
     renderWnd->addWidget(backgroundWnd);
     renderWnd->setCurrentIndex(1);

     slider = new QSlider(Qt::Horizontal);
     slider->setFixedHeight(15);
     timeLabel = new QLabel;
     timeLabel->setFixedHeight(15);
     timeLabel->setStyleSheet("background-color: #ffffff;color:black; font-family:\"STXihei\";font-size: 10px;");
     timeLabel->setText("00:00:00/00:00:00");
     timeLayout = new QHBoxLayout;
     timeLayout->setContentsMargins(0,0,0,0);
     timeLayout->setSpacing(5);
     timeLayout->addWidget(slider,5);
     timeLayout->addWidget(timeLabel,1);


     infoLabel = new QLabel;
     infoLabel->setFixedHeight(25);
     infoLabel->setStyleSheet("background-color: white;color:black; font-family:\"STXihei\";font-size: 15px;");


     openBtn = new QPushButton;
     openBtn->setFixedSize(75,25);
     openBtn->setText("OpenFile");
     openBtn->setStyleSheet(" QPushButton{border: 1px solid #C0C0C0; \
                            background-color: white;\
                            border-style: solid;\
                            border-radius:0px;\
                            color:black;\
                            font-family:\"STXihei\";\
                            font-size: 15px;\
                            padding:0 0px;\
                            margin:0 0px;\
                            }\
                            QPushButton:hover{border: 1px solid #C0C0C0; \
                             background-color: #ececec;\
                             border-style: solid;\
                             border-radius:0px;\
                             color:black;\
                             font-family:\"STXihei\";\
                             font-size: 15px;\
                             padding:0 0px;\
                             margin:0 0px;\
                             }\
                            ");

     startBtn = new QPushButton;
     startBtn->setFixedSize(45,25);
     startBtn->setText("播放");
     startBtn->setStyleSheet(openBtn->styleSheet());

     stopBtn = new QPushButton;
     stopBtn->setFixedSize(45,25);
     stopBtn->setText("停止");
     stopBtn->setStyleSheet(openBtn->styleSheet());

     plauseBtn = new QPushButton;
     plauseBtn->setFixedSize(45,25);
     plauseBtn->setText("暂停");
     plauseBtn->setStyleSheet(openBtn->styleSheet());
     plauseBtn->setVisible(false);

     muteButton = new QToolButton;
     muteButton->setFixedSize(45,25);
     muteButton->setIcon(style()->standardIcon(QStyle::SP_MediaVolume));

     volumeSlider = new QSlider(Qt::Horizontal);
     volumeSlider->setFixedSize(60,15);
     volumeSlider->setRange(0,100);

     buttonLayout = new QHBoxLayout;
     buttonLayout->setContentsMargins(0,0,0,0);
     buttonLayout->setSpacing(15);
     buttonLayout->addStretch();
     buttonLayout->addWidget(openBtn);
     buttonLayout->addWidget(startBtn);
     buttonLayout->addWidget(plauseBtn);
     buttonLayout->addWidget(stopBtn);
     buttonLayout->addWidget(muteButton);
     buttonLayout->addWidget(volumeSlider);
     buttonLayout->addStretch();

     mainLayout->addWidget(renderWnd,5);
     mainLayout->addWidget(infoLabel,1);
     mainLayout->addLayout(timeLayout);
     mainLayout->addLayout(buttonLayout);

     realize_cb(displayWnd,data);

     connect(openBtn,SIGNAL(clicked()),this,SLOT(slotOpenButtonClicked()));
     connect(startBtn,SIGNAL(clicked()),this,SLOT(slotPlayButtonClicked()));
     connect(plauseBtn,SIGNAL(clicked()),this,SLOT(slotPaluseButtonClicked()));
     connect(stopBtn,SIGNAL(clicked()),this,SLOT(slotStopButtonClicked()));
     connect(displayWnd,SIGNAL(fullScreenSignal(bool)),this,SLOT(slotFullScreen(bool)));
     connect(muteButton,SIGNAL(clicked()),this,SLOT(slotmuteButtonClicked()));
     connect(volumeSlider,SIGNAL(sliderMoved(int)),this,SLOT(slotVolumeChange(int)));

     slider->setRange(0,100);
     volumeSlider->setValue(50);
     data->slider = slider;
     connect(slider,SIGNAL(sliderMoved(int)),this,SLOT(seek(int)));
     data->streams_list = infoLabel;
}

 void Widget::slotOpenButtonClicked()
 {
     QString fileName = QFileDialog::getOpenFileName(this, tr("Please choose video file"), tr("/"));
     qInfo() << "fileName is" << fileName;
     if(fileName == "")
     {
         return;
     }
     data->streams_list->setText(fileName);
     uri = "file:///" + fileName;
     stopButtonClicked(NULL,data);
     g_object_set(data->playbin2, "uri", uri.toUtf8().data(), NULL);
     playButtonClicked(NULL,data);
 }

 void Widget::seek(int seconds)
 {
     Q_UNUSED(seconds);
     slider_cb(data);
 }

 void Widget::slotPlayButtonClicked()
 {
     qInfo() << "slotPlayButtonClicked is called!!";
     if(uri == "")
     {
         return;
     }
     if(data->playbin2->current_state == GST_STATE_PLAYING)
     {
         plauseButtonClicked(NULL,data);
     }
     else
     {
         playButtonClicked(NULL,data);
     }
 }

 void Widget::slotPaluseButtonClicked()
 {
     qInfo() << "slotPaluseButtonClicked!!!";
     plauseButtonClicked(NULL,data);
 }

 void Widget::slotStopButtonClicked()
 {
     qInfo() << "slotStopButtonClicked is called!!";
     stopButtonClicked(NULL,data);
 }

 /* This function is called periodically to refresh the GUI */
 bool Widget::refresh_ui(CustomData *data)
 {
     GstFormat fmt = GST_FORMAT_TIME;
     gint64 current = -1;

     if (data->playbin2->current_state < GST_STATE_PAUSED)
     {
         return TRUE;
      }

     if(data->playbin2->current_state == GST_STATE_PLAYING)
     {
         startBtn->setText("暂停");
     }
     else if(data->playbin2->current_state == GST_STATE_PAUSED)
     {
         startBtn->setText("播放");
     }
     else
     {
         startBtn->setText("播放");
     }

     if (!GST_CLOCK_TIME_IS_VALID (data->duration))
     {
       if (!gst_element_query_duration (data->playbin2, fmt, &data->duration))
       {
          qWarning("Could not query current duration.\n");
          return FALSE;
       }
       else
       {
         /* Set the range of the slider to the clip duration, in SECONDS */
          data->slider->setRange(0,(gdouble)data->duration/GST_SECOND);
       }
     }

     qInfo() << "the playbin duration is" << QString::number(data->duration);

     if(data->playbin2->current_state == GST_STATE_PLAYING )
     {
         GstQuery *query;
         gint64 start, end;
         query = gst_query_new_seeking (GST_FORMAT_TIME);
         if (gst_element_query (data->playbin2, query))
         {
           gst_query_parse_seeking (query, NULL, &data->seek_enabled, &start, &end);
           if (data->seek_enabled)
           {
             g_print ("Seeking is ENABLED from %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT "\n",
                 GST_TIME_ARGS (start), GST_TIME_ARGS (end));
             if(data->duration != end)
             {
               data->duration = end;
               slider->setRange(0,(gdouble)data->duration/GST_SECOND);
             }
           }
           else
           {
             g_print ("Seeking is DISABLED for this stream.\n");
           }
         }
         else
         {
           g_printerr ("Seeking query failed.");
         }
         gst_query_unref (query);
     }

     if (gst_element_query_position (data->playbin2, fmt, &current))
     {
         gchar *infor;
         infor  = g_strdup_printf("%" GST_TIME_FORMAT " / %" GST_TIME_FORMAT "\r",
                  GST_TIME_ARGS (current), GST_TIME_ARGS (data->duration));
         if(current == data->duration)
         {
             if(queryTimer->isActive())
             {
                 queryTimer->stop();
             }
             stopButtonClicked(NULL,data);
         }
         timeLabel->setText(QString(infor));
         if(!slider->isSliderDown())
         {
            slider->setValue(current/GST_SECOND);
         }
     }
     return TRUE;
 }

 /* Extract metadata from all the streams and write it to the text widget in the GUI */
 void Widget::analyze_streams(CustomData *data)
 {
     gint i;
     GstTagList *tags;
     gchar *str, *total_str;
     guint rate;
     gint n_video, n_audio, n_text;

     /* Read some properties */
     g_object_get (data->playbin2, "n-video", &n_video, NULL);
     g_object_get (data->playbin2, "n-audio", &n_audio, NULL);
     g_object_get (data->playbin2, "n-text", &n_text, NULL);

     QString infor;
     for (i = 0; i < n_video; i++)
     {
       tags = NULL;
       /* Retrieve the stream's video tags */
       g_signal_emit_by_name (data->playbin2, "get-video-tags", i, &tags);
       if (tags)
       {
         total_str = g_strdup_printf ("video stream %d:", i);
         infor += QString(total_str);
         g_free (total_str);
         gst_tag_list_get_string (tags, GST_TAG_VIDEO_CODEC, &str);
         total_str = g_strdup_printf ("  codec: %s", str ? str : "unknown");
         infor += QString(total_str);
         g_free (total_str);
         g_free (str);
         gst_tag_list_free (tags);
       }
     }

     for (i = 0; i < n_audio; i++)
     {
       tags = NULL;
       g_signal_emit_by_name (data->playbin2, "get-audio-tags", i, &tags);
       if (tags)
       {
         total_str = g_strdup_printf ("audio stream %d:", i);
         infor += QString(total_str);
         g_free (total_str);
         if (gst_tag_list_get_string (tags, GST_TAG_AUDIO_CODEC, &str))
         {
           total_str = g_strdup_printf ("  codec: %s", str);
           infor += QString(total_str);
           g_free (total_str);
           g_free (str);
         }
         if (gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str))
         {
           total_str = g_strdup_printf ("  language: %s", str);
           infor += QString(total_str);
           g_free (total_str);
           g_free (str);
         }
         if (gst_tag_list_get_uint (tags, GST_TAG_BITRATE, &rate)) {
           total_str = g_strdup_printf ("  bitrate: %d", rate);
           infor += QString(total_str);
           g_free (total_str);
         }
         gst_tag_list_free (tags);
       }
     }

     for (i = 0; i < n_text; i++)
     {
       tags = NULL;
       /* Retrieve the stream's subtitle tags */
       g_signal_emit_by_name (data->playbin2, "get-text-tags", i, &tags);
       if (tags)
       {
         total_str = g_strdup_printf ("subtitle stream %d:", i);
         infor += QString(total_str);
         g_free (total_str);
         if (gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str))
         {
           total_str = g_strdup_printf ("  language: %s", str);
           infor += QString(total_str);
           g_free (total_str);
           g_free (str);
         }
         gst_tag_list_free (tags);
       }
     }
 }

 void Widget::resizeEvent(QResizeEvent *event)
 {
     if(this->width() != 600)
     {
         this->slider->resize(QSize(this->width() - 100, 15));
     }
     QWidget::resizeEvent(event);
 }

 void Widget::slotFullScreen(bool flag)
 {

     qInfo() << "receive fullscreen signal signal is" << QString::number(flag);
     if(!this->isFullScreen())
     {
         mainLayout->setContentsMargins(0,0,0,0);
         flag = true;
     }
     else
     {
         mainLayout->setContentsMargins(5,5,5,5);
         flag = false;
     }
     this->infoLabel->setVisible(!flag);
     this->slider->setVisible(!flag);
     this->openBtn->setVisible(!flag);
     this->startBtn->setVisible(!flag);
     this->plauseBtn->setVisible(!flag);
     this->stopBtn->setVisible(!flag);
     this->timeLabel->setVisible(!flag);
     this->muteButton->setVisible(!flag);
     this->volumeSlider->setVisible(!flag);
     if(flag == false)
     {
        this->showNormal();
     }
     else
     {
         this->showFullScreen();
     }
 }

 void Widget::slotmuteButtonClicked()
 {
     plauseButtonClicked(NULL,data);
     if(muteFlag)
     {
         muteButton->setIcon(style()->standardIcon(QStyle::SP_MediaVolumeMuted));
         g_object_set(G_OBJECT(data->playbin2), "mute", TRUE, NULL);
     }
     else
     {
          muteButton->setIcon(style()->standardIcon(QStyle::SP_MediaVolume));
          g_object_set(G_OBJECT(data->playbin2), "mute", FALSE, NULL);
     }
     muteFlag = !muteFlag;
     playButtonClicked(NULL,data);
 }

 void Widget::slotVolumeChange(int pos)
 {
     g_object_set(G_OBJECT(data->playbin2), "volume", pos*1.0/100, NULL);
 }


#if 0
 m_audioSink = gst_element_factory_make("autoaudiosink", "audiosink");
       if (m_audioSink) {
       GstElement *audioSink = gst_element_factory_make("autoaudiosink", "audiosink");
       if (audioSink) {
           if (usePlaybinVolume()) {
               m_audioSink = audioSink;
               m_volumeElement = m_playbin;
           } else {
               m_volumeElement = gst_element_factory_make("volume", "volumeelement");
               if (m_volumeElement) {
                   m_audioSink = gst_bin_new("audio-output-bin");

                   gst_bin_add_many(GST_BIN(m_audioSink), m_volumeElement, audioSink, NULL);
                   gst_element_link(m_volumeElement, audioSink);

                   GstPad *pad = gst_element_get_static_pad(m_volumeElement, "sink");
                   gst_element_add_pad(GST_ELEMENT(m_audioSink), gst_ghost_pad_new("sink", pad));
                   gst_object_unref(GST_OBJECT(pad));
               } else {
                   m_audioSink = audioSink;
                   m_volumeElement = m_playbin;
               }
           }

           g_object_set(G_OBJECT(m_playbin), "audio-sink", m_audioSink, NULL);
           addAudioBufferProbe();
       }
       g_signal_connect(G_OBJECT(m_playbin), "notify::source", G_CALLBACK(playbinNotifySource), this);
       g_signal_connect(G_OBJECT(m_playbin), "element-added",  G_CALLBACK(handleElementAdded), this);

       // Init volume and mute state
       g_object_set(G_OBJECT(m_playbin), "volume", 1.0, NULL);
       g_object_set(G_OBJECT(m_playbin), "mute", FALSE, NULL);

       g_signal_connect(G_OBJECT(m_playbin), "notify::volume", G_CALLBACK(handleVolumeChange), this);
       g_signal_connect(G_OBJECT(m_playbin), "notify::mute", G_CALLBACK(handleMutedChange), this);
       if (usePlaybinVolume()) {
           updateVolume();
           updateMuted();
           g_signal_connect(G_OBJECT(m_playbin), "notify::volume", G_CALLBACK(handleVolumeChange), this);
           g_signal_connect(G_OBJECT(m_playbin), "notify::mute", G_CALLBACK(handleMutedChange), this);
       }

       g_signal_connect(G_OBJECT(m_playbin), "video-changed", G_CALLBACK(handleStreamsChange), this);
       g_signal_connect(G_OBJECT(m_playbin), "audio-changed", G_CALLBACK(handleStreamsChange), this);
   if (m_volume != volume) {
       m_volume = volume;

       if (m_playbin) {
           //playbin2 allows to set volume and muted independently,
           g_object_set(G_OBJECT(m_playbin), "volume", m_volume/100.0, NULL);
       }
       if (m_volumeElement)
           g_object_set(G_OBJECT(m_volumeElement), "volume", m_volume / 100.0, NULL);

       emit volumeChanged(m_volume);
   }
   if (m_muted != muted) {
       m_muted = muted;

       g_object_set(G_OBJECT(m_playbin), "mute", m_muted ? TRUE : FALSE, NULL);
       if (m_volumeElement)
           g_object_set(G_OBJECT(m_volumeElement), "mute", m_muted ? TRUE : FALSE, NULL);

       emit mutedStateChanged(m_muted);
   }
}
#endif
