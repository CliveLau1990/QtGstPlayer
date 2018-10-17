#include "widget.h"
#include <QApplication>
#include <gst/gst.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    gst_init (&argc, &argv);
    Widget w;
    w.show();

    return a.exec();
}

#if 0
if (m_pipeline) {
       QGst::StreamVolumePtr svp =
           m_pipeline.dynamicCast<QGst::StreamVolume>();
       if (svp) {
           return svp->volume(QGst::StreamVolumeFormatCubic) * 10;
       }
   }
#endif
