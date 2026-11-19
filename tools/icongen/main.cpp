// Build-time tool: renders the app icon SVG into a multi-size Windows .ico
// (PNG-compressed entries, supported since Windows Vista).
//
//   can_icongen <input.svg> <output.ico>

#include <QBuffer>
#include <QFile>
#include <QCoreApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>

#include <cstdio>
#include <vector>

namespace {

void putU16(QByteArray &b, quint16 v)
{
    b.append(static_cast<char>(v & 0xFF));
    b.append(static_cast<char>(v >> 8));
}

void putU32(QByteArray &b, quint32 v)
{
    for (int i = 0; i < 4; ++i)
        b.append(static_cast<char>((v >> (8 * i)) & 0xFF));
}

} // namespace

int main(int argc, char *argv[])
{
    // No GUI platform plugin: rendering SVG paths into a QImage needs none,
    // and the tool must never wait on a dialog during a build.
    QCoreApplication app(argc, argv);
    if (argc != 3) {
        std::fprintf(stderr, "usage: can_icongen <input.svg> <output.ico>\n");
        return 2;
    }
    QSvgRenderer svg(QString::fromLocal8Bit(argv[1]));
    if (!svg.isValid()) {
        std::fprintf(stderr, "can_icongen: cannot read %s\n", argv[1]);
        return 1;
    }
    const int sizes[] = {16, 20, 24, 32, 40, 48, 64, 128, 256};
    std::vector<QByteArray> pngs;
    for (int s : sizes) {
        QImage img(s, s, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        svg.render(&p, QRectF(0, 0, s, s));
        p.end();
        QByteArray png;
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        pngs.push_back(png);
    }
    QByteArray ico;
    putU16(ico, 0);                                   // reserved
    putU16(ico, 1);                                   // type: icon
    putU16(ico, static_cast<quint16>(pngs.size()));   // image count
    quint32 offset = 6 + 16 * static_cast<quint32>(pngs.size());
    for (std::size_t i = 0; i < pngs.size(); ++i) {
        const int s = sizes[i];
        ico.append(static_cast<char>(s >= 256 ? 0 : s)); // width (0 = 256)
        ico.append(static_cast<char>(s >= 256 ? 0 : s)); // height
        ico.append('\0');                                // palette
        ico.append('\0');                                // reserved
        putU16(ico, 1);                                  // planes
        putU16(ico, 32);                                 // bits per pixel
        putU32(ico, static_cast<quint32>(pngs[i].size()));
        putU32(ico, offset);
        offset += static_cast<quint32>(pngs[i].size());
    }
    for (const auto &png : pngs)
        ico.append(png);
    QFile out(QString::fromLocal8Bit(argv[2]));
    if (!out.open(QIODevice::WriteOnly) || out.write(ico) != ico.size()) {
        std::fprintf(stderr, "can_icongen: cannot write %s\n", argv[2]);
        return 1;
    }
    return 0;
}
