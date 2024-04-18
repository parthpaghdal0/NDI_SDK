#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include <QPixmap>

Q_GUI_EXPORT QPixmap qt_pixmapFromWinHBITMAP(HBITMAP bitmap, int hbitmapFormat=0);

QPixmap grabWindow(WId window, QRect screenGeometry)
{
    QSize windowSize;
    int x = 0;
    int y = 0;
    HWND hwnd = reinterpret_cast<HWND>(window);
    if (hwnd) {
        RECT r;
        GetClientRect(hwnd, &r);
        windowSize = QSize(r.right - r.left, r.bottom - r.top);
    } else {
        // Grab current screen. The client rectangle of GetDesktopWindow() is the
        // primary screen, but it is possible to grab other screens from it.
        hwnd = GetDesktopWindow();
        windowSize = screenGeometry.size();
        x += screenGeometry.x();
        y += screenGeometry.y();
    }

    int width = windowSize.width();
    int height = windowSize.height();

    // Create and setup bitmap
    HDC display_dc = GetDC(nullptr);
    HDC bitmap_dc = CreateCompatibleDC(display_dc);
    HBITMAP bitmap = CreateCompatibleBitmap(display_dc, width, height);
    HGDIOBJ null_bitmap = SelectObject(bitmap_dc, bitmap);

    // copy data
    HDC window_dc = GetDC(hwnd);
    BitBlt(bitmap_dc, 0, 0, width, height, window_dc, x, y, SRCCOPY);

    // clean up all but bitmap
    ReleaseDC(hwnd, window_dc);
    SelectObject(bitmap_dc, null_bitmap);
    DeleteDC(bitmap_dc);

    const QPixmap pixmap = qt_pixmapFromWinHBITMAP(bitmap);

    DeleteObject(bitmap);
    ReleaseDC(nullptr, display_dc);

    return pixmap;
}

void RGBtoYUV(unsigned char r, unsigned char g, unsigned char b,
              unsigned char& y, unsigned char& u, unsigned char& v) {
    y = (( 66 * r + 129 * g +  25 * b + 128) / 256) +  16;
    u = ((-38 * r -  74 * g + 112 * b + 128) / 256) + 128;
    v = ((112 * r -  94 * g -  18 * b + 128) / 256) + 128;
}

// Function to convert QPixmap to UYVY format
std::vector<unsigned char> convertQPixmapToUYVY(const QPixmap& pixmap) {
    std::vector<unsigned char> uyvyData;

    if (pixmap.isNull()) {
        return uyvyData; // Return empty vector if the pixmap is null
    }

    QImage image = pixmap.toImage();
    int width = image.width();
    int height = image.height();

    for (int y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < width; x += 2) {
            QRgb pixel1 = line[x];
            QRgb pixel2 = line[x + 1];

            unsigned char r1 = qRed(pixel1);
            unsigned char g1 = qGreen(pixel1);
            unsigned char b1 = qBlue(pixel1);
            unsigned char r2 = qRed(pixel2);
            unsigned char g2 = qGreen(pixel2);
            unsigned char b2 = qBlue(pixel2);

            unsigned char y1, u1, v1, y2, u2, v2;
            RGBtoYUV(r1, g1, b1, y1, u1, v1);
            RGBtoYUV(r2, g2, b2, y2, u2, v2);

            // Pack YUV values into UYVY format
            uyvyData.push_back(u1);
            uyvyData.push_back(y1);
            uyvyData.push_back(v1);
            uyvyData.push_back(y2);
        }
    }

    return uyvyData;
}

#endif // UTILS_H

