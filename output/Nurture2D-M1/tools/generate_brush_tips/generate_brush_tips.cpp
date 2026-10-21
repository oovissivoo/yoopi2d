/*
 * generate_brush_tips.cpp
 *
 * Compile-time tool — run by qmake as a PRE_TARGETDEPS step.
 * Generates 4 CosmicDefaultTip PNG masks into ../../BrushTips/.
 * The app embeds them via app.qrc; zero runtime generation cost.
 *
 * Tips:
 *   CosmicDefaultTip_Hardest.png  — pixelated, no AA (hard binary circle)
 *   CosmicDefaultTip_Hard.png     — tight smoothstep falloff (85% opaque radius)
 *   CosmicDefaultTip_Medium.png   — balanced falloff (55% opaque radius, pencil default)
 *   CosmicDefaultTip_Soft.png     — wide feather (10% opaque radius)
 */

#include <QCoreApplication>
#include <QImage>
#include <QDir>
#include <QString>
#include <cmath>
#include <cstdio>

static QImage bakeHardest(int res)
{
    // Binary circle — pixel is fully opaque if inside radius, fully transparent outside.
    // No smoothstep, no AA. Gives a crisp pixelated stamp.
    QImage img(res, res, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    const double cx = (res - 1) * 0.5;
    const double cy = (res - 1) * 0.5;
    const double r2 = cx * cx;
    for (int y = 0; y < res; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < res; ++x) {
            const double dx = x - cx;
            const double dy = y - cy;
            row[x] = (dx * dx + dy * dy <= r2) ? qRgba(255, 255, 255, 255) : 0;
        }
    }
    return img;
}

static QImage bakeSmoothTip(int res, double hardness)
{
    // Radial smoothstep falloff.
    // hardness: fraction of radius that stays fully opaque [0..1].
    // 1.0 = all opaque (same as hardest but AA'd edge), 0.0 = full feather.
    QImage img(res, res, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    const double cx = (res - 1) * 0.5;
    const double cy = (res - 1) * 0.5;
    const double r  = cx;
    for (int y = 0; y < res; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < res; ++x) {
            const double dx = x - cx;
            const double dy = y - cy;
            const double dist = std::sqrt(dx * dx + dy * dy) / r; // 0=center, 1=edge
            if (dist >= 1.0) { row[x] = 0; continue; }
            double alpha;
            if (dist <= hardness) {
                alpha = 1.0;
            } else {
                const double t = (dist - hardness) / (1.0 - hardness);
                alpha = 1.0 - t * t * (3.0 - 2.0 * t); // smoothstep
            }
            const int a = static_cast<int>(alpha * 255.0 + 0.5);
            row[x] = qRgba(a, a, a, a); // premultiplied white
        }
    }
    return img;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // Output dir is passed as first argument, defaults to ../../BrushTips relative to tool
    const QString outDir = (argc > 1)
        ? QString::fromLocal8Bit(argv[1])
        : QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../../BrushTips");

    if (!QDir().mkpath(outDir)) {
        fprintf(stderr, "generate_brush_tips: failed to create output dir: %s\n",
                qPrintable(outDir));
        return 1;
    }

    constexpr int kRes = 256;

    struct TipDef { const char* name; bool hardest; double hardness; };
    const TipDef tips[] = {
        { "CosmicDefaultTip_Hardest.png", true,  0.0  },
        { "CosmicDefaultTip_Hard.png",    false, 0.85 },
        { "CosmicDefaultTip_Medium.png",  false, 0.55 },
        { "CosmicDefaultTip_Soft.png",    false, 0.10 },
    };

    for (const auto& tip : tips) {
        const QString path = outDir + "/" + tip.name;
        const QImage img = tip.hardest ? bakeHardest(kRes) : bakeSmoothTip(kRes, tip.hardness);
        if (!img.save(path, "PNG")) {
            fprintf(stderr, "generate_brush_tips: failed to write %s\n", qPrintable(path));
            return 1;
        }
        printf("generate_brush_tips: wrote %s\n", qPrintable(path));
    }
    return 0;
}
