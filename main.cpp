
#include "widget.h"

#include <QApplication>
#include <Processing.NDI.Lib.h>

#ifdef _WIN32
#ifdef _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x64.lib")
#else // _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x86.lib")
#endif // _WIN64
#endif // _WIN32


int main(int argc, char *argv[])
{
    if (!NDIlib_initialize())
        return 0;

    QApplication a(argc, argv);
    Widget w;

    const int radius = 10;

    QPainterPath path;
    path.addRoundedRect(w.rect(), radius, radius);
    QRegion mask = QRegion(path.toFillPolygon().toPolygon());
    w.setMask(mask);

    w.show();
    int ret = a.exec();

    NDIlib_destroy();
    return ret;
}
