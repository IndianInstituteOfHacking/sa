// ============================================================================
//  RootBrowser Installer — Windows 10/11 (64-bit) — With Admin Elevation
//
//  ZERO pre-requisites. Downloads and installs:
//    · Git for Windows
//    · Python 3.11
//    · Visual Studio 2022 Build Tools (with UAC)
//    · Qt 6.6 (MSVC 2019 64-bit)
//    · Tor Expert Bundle
//    · RootBrowser source (from GitHub)
//    · Compiles + bundles + creates shortcuts
//
//  Build (Windows, MSVC):
//    cl /std:c++17 /Zc:__cplusplus /permissive- /DNOMINMAX /DWIN32_LEAN_AND_MEAN ^
//       installer_windows.cpp ^
//       /Fe:RootBrowser-Setup.exe ^
//       /I "%QTDIR%\include" ^
//       /I "%QTDIR%\include\QtWidgets" ^
//       /I "%QTDIR%\include\QtGui" ^
//       /I "%QTDIR%\include\QtCore" ^
//       /link /SUBSYSTEM:WINDOWS ^
//       /LIBPATH:"%QTDIR%\lib" ^
//       Qt6Widgets.lib Qt6Gui.lib Qt6Core.lib ^
//       user32.lib shell32.lib advapi32.lib
// ============================================================================

// ── Windows macro fixes (MUST come before any other includes)
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
    #define UNICODE
#endif
#ifndef _UNICODE
    #define _UNICODE
#endif

#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScrollBar>
#include <QTimer>
#include <QFont>
#include <QFontDatabase>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QStackedWidget>
#include <QScrollArea>
#include <QFrame>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVariantAnimation>
#include <QElapsedTimer>
#include <QDateTime>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QCoreApplication>
#include <QThread>
#include <QTextStream>
#include <functional>
#include <cmath>
#include <algorithm>

// ── Windows headers (after NOMINMAX)
#include <windows.h>
#include <shellapi.h>

// ============================================================================
//  Config
// ============================================================================
static const char* kAppName     = "RootBrowser Setup";
static const char* kGitHubUrl   = "https://github.com/IndianInstituteOfHacking/mk.git";
static const char* kDisplayName = "RootBrowser";
static const char* kVersion     = "1.0.0";

static const char* kQtVersion   = "6.6.0";
static const char* kQtArch      = "win64_msvc2019_64";

static const char* kTorBundleUrl =
    "https://archive.torproject.org/tor-package-archive/torbrowser/13.0.6/"
    "tor-expert-bundle-windows-x86_64-13.0.6.tar.gz";

// ============================================================================
//  Elevation
// ============================================================================
static bool isRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0,
                                 &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

static bool relaunchAsAdmin() {
    wchar_t exePath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH))
        return false;

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd         = nullptr;
    sei.lpVerb       = L"runas";
    sei.lpFile       = exePath;
    sei.lpParameters = L"--elevated";
    sei.lpDirectory  = nullptr;
    sei.nShow        = SW_NORMAL;

    if (!ShellExecuteExW(&sei))
        return false;

    if (sei.hProcess)
        CloseHandle(sei.hProcess);

    return true;
}

// ============================================================================
//  Palette
// ============================================================================
namespace Col {
    const QColor bg          = QColor("#0b0c10");
    const QColor bgSubtle    = QColor("#0e1014");
    const QColor surface     = QColor("#13151b");
    const QColor surfaceAlt  = QColor("#171a21");
    const QColor surfaceHi   = QColor("#1d2028");
    const QColor border      = QColor("#242832");
    const QColor borderSoft  = QColor("#1c1f27");
    const QColor textPrimary = QColor("#e6e8ee");
    const QColor textBright  = QColor("#f7f8fb");
    const QColor textMuted   = QColor("#8a90a0");
    const QColor textFaint   = QColor("#565b69");
    const QColor accent      = QColor("#5a8cd8");
    const QColor accentHi    = QColor("#74a3e8");
    const QColor accentLo    = QColor("#3d6bb8");
    const QColor success     = QColor("#4a9a6e");
    const QColor danger      = QColor("#b85858");
    const QColor warning     = QColor("#b8904a");
    const QColor logoGreen   = QColor("#3cc47a");
    const QColor logoTeal    = QColor("#2ba9a0");
    const QColor logoCyan    = QColor("#4db8d8");
    const QColor logoBg      = QColor("#0a1420");
    const QColor logoGrid    = QColor("#1e2a38");
}

// ============================================================================
//  Paths
// ============================================================================
struct Paths {
    static QString installDir() {
        return "C:\\RootBrowser";
    }
    static QString workDir() {
        return "C:\\RootBrowser\\_build";
    }
    static QString qtDir() {
        return "C:\\RootBrowser\\Qt\\" + QString(kQtVersion) + "\\msvc2019_64";
    }
    static QString torDir() {
        return "C:\\RootBrowser\\tor";
    }
    static QString binDir() {
        return "C:\\RootBrowser\\bin";
    }
    static QString logFile() {
        return "C:\\RootBrowser\\install.log";
    }
    static QString stateFile() {
        return "C:\\RootBrowser\\.state";
    }
};

// ============================================================================
//  Icon painting
// ============================================================================
enum class IconKind {
    Git, Qt, Globe, Tor, Hammer, Package, Rocket, Broom,
    VisualStudio, Python, Download, Check
};

static void paintIcon(QPainter& p, IconKind k, const QRectF& r, const QColor& c) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const qreal s = r.width();
    auto P = [&](qreal x, qreal y) { return QPointF(r.x() + x * s, r.y() + y * s); };
    QPen pen(c, (std::max)(static_cast<qreal>(1.4), s * 0.10),
             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (k) {
    case IconKind::Git:
        p.drawEllipse(P(.28,.25), .09*s, .09*s);
        p.drawEllipse(P(.28,.75), .09*s, .09*s);
        p.drawEllipse(P(.72,.5),  .09*s, .09*s);
        p.drawLine(P(.28,.34), P(.28,.66));
        p.drawLine(P(.28,.58), P(.63,.5));
        break;
    case IconKind::Qt:
        p.drawRoundedRect(QRectF(r.x()+.2*s, r.y()+.2*s, .6*s, .6*s), 3, 3);
        p.setBrush(c);
        p.drawEllipse(P(.5,.5), .08*s, .08*s);
        break;
    case IconKind::Globe:
        p.drawEllipse(P(.5,.5), .35*s, .35*s);
        p.drawLine(P(.15,.5), P(.85,.5));
        p.drawEllipse(P(.5,.5), .15*s, .35*s);
        break;
    case IconKind::Tor:
        p.drawEllipse(P(.5,.5), .35*s, .35*s);
        p.drawEllipse(P(.5,.5), .20*s, .20*s);
        p.setBrush(c);
        p.drawEllipse(P(.5,.5), .06*s, .06*s);
        break;
    case IconKind::Hammer:
        p.drawLine(P(.3,.3), P(.7,.7));
        p.drawLine(P(.24,.36), P(.36,.24));
        p.drawLine(P(.7,.6), P(.84,.74));
        p.drawLine(P(.6,.3), P(.7,.2));
        p.drawLine(P(.74,.24), P(.84,.34));
        break;
    case IconKind::Package:
        p.drawLine(P(.2,.4), P(.5,.22));
        p.drawLine(P(.5,.22), P(.8,.4));
        p.drawLine(P(.8,.4), P(.8,.75));
        p.drawLine(P(.8,.75), P(.5,.9));
        p.drawLine(P(.5,.9), P(.2,.75));
        p.drawLine(P(.2,.75), P(.2,.4));
        p.drawLine(P(.2,.4), P(.5,.58));
        p.drawLine(P(.5,.58), P(.8,.4));
        p.drawLine(P(.5,.58), P(.5,.9));
        break;
    case IconKind::Rocket:
        p.drawLine(P(.5,.16), P(.68,.5));
        p.drawLine(P(.5,.16), P(.32,.5));
        p.drawLine(P(.32,.5), P(.5,.62));
        p.drawLine(P(.68,.5), P(.5,.62));
        p.drawLine(P(.42,.68), P(.58,.68));
        p.drawLine(P(.44,.78), P(.56,.78));
        break;
    case IconKind::Broom:
        p.drawLine(P(.6,.2), P(.4,.55));
        p.drawLine(P(.32,.55), P(.68,.55));
        p.drawLine(P(.32,.55), P(.26,.8));
        p.drawLine(P(.68,.55), P(.74,.8));
        p.drawLine(P(.26,.8), P(.74,.8));
        p.drawLine(P(.42,.62), P(.4,.78));
        p.drawLine(P(.5,.62), P(.5,.78));
        p.drawLine(P(.58,.62), P(.6,.78));
        break;
    case IconKind::VisualStudio:
        p.drawLine(P(.2,.3), P(.2,.7));
        p.drawLine(P(.2,.3), P(.5,.5));
        p.drawLine(P(.2,.7), P(.5,.5));
        p.drawLine(P(.8,.3), P(.8,.7));
        p.drawLine(P(.8,.3), P(.5,.5));
        p.drawLine(P(.8,.7), P(.5,.5));
        break;
    case IconKind::Python:
        p.drawEllipse(QRectF(r.x()+.22*s, r.y()+.14*s, .4*s, .5*s));
        p.drawEllipse(QRectF(r.x()+.38*s, r.y()+.36*s, .4*s, .5*s));
        break;
    case IconKind::Download:
        p.drawLine(P(.5,.18), P(.5,.62));
        p.drawLine(P(.32,.48), P(.5,.66));
        p.drawLine(P(.68,.48), P(.5,.66));
        p.drawLine(P(.22,.78), P(.78,.78));
        break;
    case IconKind::Check:
        {
            QPointF pts[3] = {P(.22,.5), P(.42,.72), P(.8,.28)};
            p.drawPolyline(pts, 3);
        }
        break;
    }

    p.restore();
}

// ============================================================================
//  MainLogo
// ============================================================================
class MainLogo : public QWidget {
public:
    explicit MainLogo(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
    QPixmap render(int size) const {
        QPixmap pm(256, 256);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal cx = 128, cy = 128;

        QLinearGradient bgG(0, 0, 256, 256);
        bgG.setColorAt(0, QColor("#0e1a26"));
        bgG.setColorAt(1, Col::logoBg);
        p.setBrush(bgG);
        p.setPen(QPen(QColor("#18293a"), 2));
        p.drawRoundedRect(QRectF(4, 4, 248, 248), 52, 52);

        p.setPen(QPen(Col::logoGrid, 1.6));
        p.setBrush(Qt::NoBrush);
        const qreal globeR = 78;
        p.drawEllipse(QPointF(cx, cy), globeR, globeR);
        for (qreal y = -globeR; y <= globeR; y += globeR/2.2) {
            const qreal r = std::sqrt((std::max)(0.0, globeR*globeR - y*y));
            p.drawLine(QPointF(cx - r, cy + y), QPointF(cx + r, cy + y));
        }
        for (qreal x = -globeR; x <= globeR; x += globeR/3.0) {
            const qreal t = x / globeR;
            const qreal rx = std::abs(t) * globeR;
            p.drawEllipse(QPointF(cx, cy), rx, globeR);
        }

        const QPointF glowCenter(cx, cy);
        QRadialGradient innerGlow(glowCenter, globeR);
        innerGlow.setColorAt(0, QColor(Col::logoTeal.red(), Col::logoTeal.green(),
                                        Col::logoTeal.blue(), 30));
        innerGlow.setColorAt(1, QColor(0, 0, 0, 0));
        p.setBrush(innerGlow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(cx, cy), globeR, globeR);

        p.setBrush(QColor("#081018"));
        p.setPen(QPen(QColor("#1a2a38"), 1.5));
        p.drawEllipse(QPointF(cx, cy), 52, 52);

        QLinearGradient playG(QPointF(cx - 20, cy - 20), QPointF(cx + 24, cy + 20));
        playG.setColorAt(0, Col::logoGreen);
        playG.setColorAt(1, Col::logoTeal);
        QPainterPath chv;
        chv.moveTo(cx - 18, cy - 26);
        chv.lineTo(cx + 22, cy);
        chv.lineTo(cx - 18, cy + 26);
        chv.lineTo(cx - 18, cy + 6);
        chv.lineTo(cx + 4,  cy);
        chv.lineTo(cx - 18, cy - 6);
        chv.closeSubpath();
        p.setBrush(playG);
        p.setPen(Qt::NoPen);
        p.drawPath(chv);

        auto drawArc = [&](qreal startDeg, qreal spanDeg, const QColor& c) {
            QPen arcPen(c, 10, Qt::SolidLine, Qt::RoundCap);
            p.setPen(arcPen);
            p.setBrush(Qt::NoBrush);
            const qreal arcR = 96;
            QRectF rect(cx - arcR, cy - arcR, arcR * 2, arcR * 2);
            p.drawArc(rect, int(startDeg * 16), int(spanDeg * 16));
        };
        drawArc(35, 60, Col::logoTeal);
        drawArc(215, 60, Col::logoTeal);

        p.setPen(Qt::NoPen);
        p.setBrush(Col::logoGreen);
        p.drawEllipse(QPointF(cx, cy - globeR - 18), 8, 8);
        p.setBrush(Col::logoCyan);
        p.drawEllipse(QPointF(cx - globeR - 18, cy), 6, 6);
        p.drawEllipse(QPointF(cx + globeR + 18, cy), 6, 6);

        return pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.drawPixmap(0, 0, render(width()));
    }
};

// ============================================================================
//  Background
// ============================================================================
class Background : public QWidget {
public:
    explicit Background(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        QLinearGradient g(0, 0, 0, height());
        g.setColorAt(0, Col::bgSubtle);
        g.setColorAt(1, Col::bg);
        p.fillRect(rect(), g);
        const QPointF c(width() * 0.5, height() * 0.2);
        QRadialGradient rg(c, (std::max)(width(), height()) * 0.7);
        rg.setColorAt(0, QColor(Col::accent.red(), Col::accent.green(),
                                Col::accent.blue(), 10));
        rg.setColorAt(1, QColor(0, 0, 0, 0));
        p.fillRect(rect(), rg);
    }
};

// ============================================================================
//  Custom Button
// ============================================================================
class Button : public QPushButton {
public:
    Button(const QString& text, bool primary, QWidget* parent = nullptr)
        : QPushButton(text, parent), primary_(primary)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setFixedHeight(42);
        setMinimumWidth(180);
        anim_ = new QVariantAnimation(this);
        anim_->setDuration(140);
        anim_->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim_, &QVariantAnimation::valueChanged, this,
                [this](const QVariant& v){ animT_ = v.toReal(); update(); });
    }
protected:
    bool event(QEvent* e) override {
        if (e->type() == QEvent::Enter) animate(1.0);
        if (e->type() == QEvent::Leave) animate(0.0);
        return QPushButton::event(e);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const bool en = isEnabled();
        const bool prs = isDown();
        const qreal t = animT_;
        QColor bg, fg, border;
        if (!en) { bg = Col::surface; fg = Col::textFaint; border = Col::borderSoft; }
        else if (primary_) {
            bg = prs ? Col::accentLo
                     : QColor::fromRgbF(
                         Col::accent.redF()   + (Col::accentHi.redF()   - Col::accent.redF())   * t,
                         Col::accent.greenF() + (Col::accentHi.greenF() - Col::accent.greenF()) * t,
                         Col::accent.blueF()  + (Col::accentHi.blueF()  - Col::accent.blueF())  * t);
            fg = Col::textBright;
            border = Qt::transparent;
        } else {
            bg = prs ? Col::surfaceHi
                     : QColor::fromRgbF(
                         Col::surface.redF()   + (Col::surfaceAlt.redF()   - Col::surface.redF())   * t,
                         Col::surface.greenF() + (Col::surfaceAlt.greenF() - Col::surface.greenF()) * t,
                         Col::surface.blueF()  + (Col::surfaceAlt.blueF()  - Col::surface.blueF())  * t);
            fg = Col::textPrimary;
            border = Col::border;
        }
        QRectF r = rect().adjusted(1, 1, -1, -1);
        p.setPen(QPen(border, 1));
        p.setBrush(bg);
        p.drawRoundedRect(r, 8, 8);
        QFont f = font();
        f.setPixelSize(13);
        f.setWeight(primary_ ? QFont::DemiBold : QFont::Normal);
        p.setFont(f);
        p.setPen(fg);
        p.drawText(rect(), Qt::AlignCenter, text());
    }
private:
    void animate(qreal v) {
        anim_->stop(); anim_->setStartValue(animT_); anim_->setEndValue(v); anim_->start();
    }
    bool primary_;
    qreal animT_ = 0.0;
    QVariantAnimation* anim_ = nullptr;
};

// ============================================================================
//  Window Controls
// ============================================================================
class WindowControls : public QWidget {
public:
    std::function<void()> onMin, onMax, onClose;
    explicit WindowControls(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(126, 34);
        setMouseTracking(true);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        for (int i = 0; i < 3; ++i) {
            QRectF r(i * 42, 0, 38, 34);
            bool hov = (hover_ == i);
            if (hov) {
                p.setPen(Qt::NoPen);
                p.setBrush(i == 2 ? Col::danger : Col::surfaceHi);
                p.drawRoundedRect(r.adjusted(5, 6, -5, -6), 6, 6);
            }
            p.setPen(QPen(hov ? Qt::white : Col::textMuted, 1.3,
                          Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            QPointF c = r.center();
            if (i == 0) p.drawLine(QPointF(c.x()-5, c.y()+3), QPointF(c.x()+5, c.y()+3));
            else if (i == 1) p.drawRoundedRect(QRectF(c.x()-4.5, c.y()-4.5, 9, 9), 1.2, 1.2);
            else {
                p.drawLine(QPointF(c.x()-4, c.y()-4), QPointF(c.x()+4, c.y()+4));
                p.drawLine(QPointF(c.x()+4, c.y()-4), QPointF(c.x()-4, c.y()+4));
            }
        }
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        int h = -1;
        for (int i = 0; i < 3; ++i)
            if (QRectF(i*42, 0, 38, 34).contains(e->position())) { h = i; break; }
        if (h != hover_) { hover_ = h; setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); }
    }
    void leaveEvent(QEvent*) override { hover_ = -1; update(); }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() != Qt::LeftButton) return;
        int h = -1;
        for (int i = 0; i < 3; ++i)
            if (QRectF(i*42, 0, 38, 34).contains(e->position())) { h = i; break; }
        if (h == 0 && onMin)   onMin();
        if (h == 1 && onMax)   onMax();
        if (h == 2 && onClose) onClose();
    }
private:
    int hover_ = -1;
};

// ============================================================================
//  Logger
// ============================================================================
class Logger {
public:
    static Logger& instance() { static Logger l; return l; }

    void init(const QString& filePath) {
        filePath_ = filePath;
        QDir().mkpath(QFileInfo(filePath_).absolutePath());
        QFile f(filePath_);
        if (f.open(QIODevice::WriteOnly | QIODevice::Append)) {
            QTextStream ts(&f);
            ts << "\n\n=== Session " << QDateTime::currentDateTime().toString(Qt::ISODate)
               << " ===\n";
        }
    }

    void log(const QString& level, const QString& msg) {
        const QString line = QString("[%1] [%2] %3")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"), level, msg);
        if (!filePath_.isEmpty()) {
            QFile f(filePath_);
            if (f.open(QIODevice::WriteOnly | QIODevice::Append)) {
                QTextStream ts(&f);
                ts << line << "\n";
            }
        }
    }

    void info(const QString& m)  { log("INFO", m); }
    void warn(const QString& m)  { log("WARN", m); }
    void error(const QString& m) { log("ERROR", m); }
    void step(const QString& m)  { log("STEP", m); }

private:
    QString filePath_;
};

// ============================================================================
//  Installer
// ============================================================================
class Installer : public QWidget {
public:
    Installer() {
        setWindowTitle(kAppName);
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        setMinimumSize(920, 640);
        resize(1080, 760);

        Logger::instance().init(Paths::logFile());

        buildUi();
        centerOnScreen();
    }

protected:
    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        if (bg_)       bg_->setGeometry(rect());
        if (stack_)    stack_->setGeometry(rect());
        if (controls_) controls_->move(width()-controls_->width()-10, 8);
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && e->position().y() < 46) {
            dragPos_ = e->globalPosition().toPoint() - frameGeometry().topLeft();
            dragging_ = true;
        }
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (dragging_ && (e->buttons() & Qt::LeftButton))
            move(e->globalPosition().toPoint() - dragPos_);
    }
    void mouseReleaseEvent(QMouseEvent*) override { dragging_ = false; }

private:
    void buildUi() {
        bg_ = new Background(this);
        bg_->setGeometry(rect());
        bg_->lower();

        stack_ = new QStackedWidget(this);
        stack_->setStyleSheet("background:transparent;");
        stack_->setGeometry(rect());

        stack_->addWidget(buildWelcome());
        stack_->addWidget(buildRunning());
        stack_->addWidget(buildDone());
        stack_->addWidget(buildFailed());

        controls_ = new WindowControls(this);
        controls_->onMin   = [this]{ showMinimized(); };
        controls_->onMax   = [this]{ if (isMaximized()) showNormal(); else showMaximized(); };
        controls_->onClose = [this]{ close(); };
        controls_->move(width()-controls_->width()-10, 8);
    }

    void centerOnScreen() {
        if (auto* s = QGuiApplication::primaryScreen()) {
            const QRect g = s->availableGeometry();
            move(g.center().x() - width()/2, g.center().y() - height()/2);
        }
    }

    QWidget* buildWelcome() {
        auto* page = new QWidget;
        page->setStyleSheet("background:transparent;");
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(72, 56, 72, 56);
        v->setSpacing(0);
        v->addStretch(2);

        auto* logoRow = new QHBoxLayout;
        logoRow->addStretch(1);
        auto* logo = new MainLogo(page);
        logo->setFixedSize(140, 140);
        logoRow->addWidget(logo);
        logoRow->addStretch(1);
        v->addLayout(logoRow);
        v->addSpacing(28);

        auto* title = new QLabel("RootBrowser", page);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(
            QString("color:%1;font-size:48px;font-weight:200;"
                    "letter-spacing:-1.2px;background:transparent;")
                .arg(Col::textBright.name()));
        v->addWidget(title);

        auto* version = new QLabel(QString("Windows Setup · v%1").arg(kVersion), page);
        version->setAlignment(Qt::AlignCenter);
        version->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:600;letter-spacing:3px;"
                    "background:transparent;").arg(Col::textMuted.name()));
        v->addSpacing(8);
        v->addWidget(version);

        v->addSpacing(26);
        auto* desc = new QLabel(
            "A modern, cross-platform web browser built with Qt6 and Chromium.", page);
        desc->setAlignment(Qt::AlignCenter);
        desc->setStyleSheet(
            QString("color:%1;font-size:13.5px;background:transparent;")
                .arg(Col::textMuted.name()));
        v->addWidget(desc);

        v->addSpacing(4);
        auto* desc2 = new QLabel(
            "This installer requires administrator privileges.\n"
            "It will download and set up everything automatically — no prior tools required.",
            page);
        desc2->setAlignment(Qt::AlignCenter);
        desc2->setStyleSheet(
            QString("color:%1;font-size:12px;background:transparent;line-height:1.6;")
                .arg(Col::textFaint.name()));
        v->addWidget(desc2);

        v->addSpacing(40);
        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(10);
        btnRow->addStretch(1);
        auto* cancel = new Button("Cancel", false, page);
        auto* next   = new Button("Install Now", true, page);
        btnRow->addWidget(cancel);
        btnRow->addWidget(next);
        btnRow->addStretch(1);
        v->addLayout(btnRow);

        v->addStretch(3);
        auto* footer = new QLabel(
            "Requires ~6 GB disk space · Internet connection · Runs as Administrator",
            page);
        footer->setAlignment(Qt::AlignCenter);
        footer->setStyleSheet(
            QString("color:%1;font-size:11px;background:transparent;")
                .arg(Col::textFaint.name()));
        v->addWidget(footer);

        connect(cancel, &QPushButton::clicked, this, &QWidget::close);
        connect(next, &QPushButton::clicked, this, [this]{
            stack_->setCurrentIndex(1);
            startInstall();
        });
        return page;
    }

    QWidget* buildRunning() {
        auto* page = new QWidget;
        page->setStyleSheet("background:transparent;");
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(72, 48, 72, 36);
        v->setSpacing(0);

        auto* heading = new QLabel("Installing RootBrowser", page);
        heading->setAlignment(Qt::AlignCenter);
        heading->setStyleSheet(
            QString("color:%1;font-size:19px;font-weight:400;background:transparent;")
                .arg(Col::textBright.name()));
        v->addWidget(heading);

        stepLabel_ = new QLabel("Preparing…", page);
        stepLabel_->setAlignment(Qt::AlignCenter);
        stepLabel_->setStyleSheet(
            QString("color:%1;font-size:12px;background:transparent;")
                .arg(Col::accent.name()));
        v->addSpacing(6);
        v->addWidget(stepLabel_);

        v->addSpacing(16);
        progress_ = new QProgressBar(page);
        progress_->setRange(0, 100);
        progress_->setValue(0);
        progress_->setTextVisible(false);
        progress_->setFixedHeight(6);
        progress_->setStyleSheet(
            QString("QProgressBar{background:%1;border:none;border-radius:3px;}"
                    "QProgressBar::chunk{background:%2;border-radius:3px;}")
                .arg(Col::surfaceHi.name(), Col::accent.name()));
        v->addWidget(progress_);

        v->addSpacing(18);
        terminal_ = new QPlainTextEdit(page);
        terminal_->setReadOnly(true);
        terminal_->setFrameStyle(QFrame::NoFrame);
        QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        mono.setPixelSize(11.5);
        terminal_->setFont(mono);
        terminal_->setStyleSheet(
            QString("QPlainTextEdit{background:%1;color:%2;"
                    "border:1px solid %3;border-radius:8px;padding:12px;}")
                .arg(Col::surface.name(), Col::textPrimary.name(), Col::border.name()));
        terminal_->verticalScrollBar()->setStyleSheet(
            "QScrollBar:vertical{background:transparent;width:8px;}"
            "QScrollBar::handle:vertical{background:#242832;border-radius:4px;min-height:30px;}"
            "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");
        v->addWidget(terminal_, 1);

        v->addSpacing(12);
        statusLabel_ = new QLabel("Starting…", page);
        statusLabel_->setAlignment(Qt::AlignCenter);
        statusLabel_->setStyleSheet(
            QString("color:%1;font-size:11.5px;background:transparent;")
                .arg(Col::textMuted.name()));
        v->addWidget(statusLabel_);

        return page;
    }

    QWidget* buildDone() {
        auto* page = new QWidget;
        page->setStyleSheet("background:transparent;");
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(72, 56, 72, 56);
        v->setSpacing(0);
        v->addStretch(2);

        auto* icon = new QLabel(page);
        icon->setFixedSize(64, 64);
        icon->setAlignment(Qt::AlignCenter);
        icon->setPixmap(makeSuccessIcon(64));
        v->addWidget(icon, 0, Qt::AlignHCenter);
        v->addSpacing(18);

        auto* title = new QLabel("Installation complete", page);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(
            QString("color:%1;font-size:26px;font-weight:300;"
                    "letter-spacing:-0.3px;background:transparent;")
                .arg(Col::textBright.name()));
        v->addWidget(title);
        v->addSpacing(6);

        auto* sub = new QLabel("RootBrowser is now installed on your system.", page);
        sub->setAlignment(Qt::AlignCenter);
        sub->setStyleSheet(
            QString("color:%1;font-size:12.5px;background:transparent;")
                .arg(Col::textMuted.name()));
        v->addWidget(sub);
        v->addSpacing(26);

        auto* info = new QLabel(page);
        info->setTextFormat(Qt::RichText);
        info->setText(QString(
            "<div style='color:%1;font-size:12px;line-height:1.9;'>"
            "<b style='color:%2'>How to launch</b><br>"
            "&nbsp;&nbsp;·&nbsp; Start Menu — search <b>RootBrowser</b><br>"
            "&nbsp;&nbsp;·&nbsp; Desktop shortcut<br>"
            "&nbsp;&nbsp;·&nbsp; Direct — "
            "<span style='font-family:monospace;background:%3;padding:2px 8px;"
            "border-radius:4px;'>C:\\RootBrowser\\bin\\RootBrowser.exe</span><br>"
            "<br><b style='color:%2'>Installation log</b><br>"
            "&nbsp;&nbsp;<span style='font-family:monospace;font-size:11px;'>%4</span>"
            "</div>")
            .arg(Col::textPrimary.name(), Col::textBright.name(),
                 Col::surfaceHi.name(), Paths::logFile()));
        info->setStyleSheet(
            QString("QLabel{background:%1;border:1px solid %2;"
                    "border-radius:8px;padding:16px 20px;}")
                .arg(Col::surface.name(), Col::borderSoft.name()));
        info->setMaximumWidth(560);
        auto* cardRow = new QHBoxLayout;
        cardRow->addStretch(1);
        cardRow->addWidget(info);
        cardRow->addStretch(1);
        v->addLayout(cardRow);
        v->addSpacing(26);

        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(10);
        btnRow->addStretch(1);
        auto* launch = new Button("Launch RootBrowser", true, page);
        auto* openFolder = new Button("Open folder", false, page);
        auto* close = new Button("Close", false, page);
        btnRow->addWidget(close);
        btnRow->addWidget(openFolder);
        btnRow->addWidget(launch);
        btnRow->addStretch(1);
        v->addLayout(btnRow);
        v->addStretch(3);

        connect(close, &QPushButton::clicked, this, &QWidget::close);
        connect(openFolder, &QPushButton::clicked, this, []{
            QDesktopServices::openUrl(QUrl::fromLocalFile(Paths::installDir()));
        });
        connect(launch, &QPushButton::clicked, this, []{
            QProcess::startDetached(Paths::binDir() + "/RootBrowser.exe", {});
        });
        return page;
    }

    QWidget* buildFailed() {
        auto* page = new QWidget;
        page->setStyleSheet("background:transparent;");
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(72, 56, 72, 56);
        v->setSpacing(0);
        v->addStretch(2);

        auto* icon = new QLabel(page);
        icon->setFixedSize(64, 64);
        icon->setAlignment(Qt::AlignCenter);
        icon->setPixmap(makeErrorIcon(64));
        v->addWidget(icon, 0, Qt::AlignHCenter);
        v->addSpacing(18);

        auto* title = new QLabel("Installation failed", page);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(
            QString("color:%1;font-size:26px;font-weight:300;"
                    "letter-spacing:-0.3px;background:transparent;")
                .arg(Col::danger.name()));
        v->addWidget(title);
        v->addSpacing(6);

        failedSubLabel_ = new QLabel("Something went wrong. Check the log below.", page);
        failedSubLabel_->setAlignment(Qt::AlignCenter);
        failedSubLabel_->setWordWrap(true);
        failedSubLabel_->setStyleSheet(
            QString("color:%1;font-size:12.5px;background:transparent;")
                .arg(Col::textMuted.name()));
        v->addWidget(failedSubLabel_);
        v->addSpacing(18);

        failedLog_ = new QPlainTextEdit(page);
        failedLog_->setReadOnly(true);
        failedLog_->setMaximumHeight(220);
        QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        mono.setPixelSize(11);
        failedLog_->setFont(mono);
        failedLog_->setStyleSheet(
            QString("QPlainTextEdit{background:%1;color:%2;"
                    "border:1px solid %3;border-radius:8px;padding:12px;}")
                .arg(Col::surface.name(), Col::textMuted.name(), Col::border.name()));
        v->addWidget(failedLog_);
        v->addSpacing(18);

        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(10);
        btnRow->addStretch(1);
        auto* close = new Button("Close", false, page);
        auto* openLog = new Button("Open log file", false, page);
        auto* retry = new Button("Retry", true, page);
        btnRow->addWidget(close);
        btnRow->addWidget(openLog);
        btnRow->addWidget(retry);
        btnRow->addStretch(1);
        v->addLayout(btnRow);
        v->addStretch(3);

        connect(close, &QPushButton::clicked, this, &QWidget::close);
        connect(openLog, &QPushButton::clicked, this, []{
            QDesktopServices::openUrl(QUrl::fromLocalFile(Paths::logFile()));
        });
        connect(retry, &QPushButton::clicked, this, [this]{
            if (terminal_) terminal_->clear();
            if (progress_) progress_->setValue(0);
            running_ = false;
            stack_->setCurrentIndex(1);
            startInstall();
        });
        return page;
    }

    void appendTerm(const QString& line, const QColor& c) {
        if (!terminal_) return;
        QString s = line;
        s.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;");
        terminal_->appendHtml(
            QString("<span style='color:%1'>%2</span>").arg(c.name(), s));
        auto* sb = terminal_->verticalScrollBar();
        sb->setValue(sb->maximum());
    }
    void setStatus(const QString& s, const QColor& c = Col::textMuted) {
        if (!statusLabel_) return;
        statusLabel_->setStyleSheet(
            QString("color:%1;font-size:11.5px;background:transparent;").arg(c.name()));
        statusLabel_->setText(s);
    }
    void setStepTitle(const QString& s) { if (stepLabel_) stepLabel_->setText(s); }

    struct Step {
        QString id;
        QString title;
        QStringList commands;
        int progress;
    };

    void startInstall() {
        if (running_) return;
        running_ = true;
        if (terminal_) terminal_->clear();
        if (progress_) progress_->setValue(0);
        Logger::instance().step("Installation started");

        appendTerm("RootBrowser Windows Installer", Col::textBright);
        appendTerm(QString("Install dir: %1").arg(Paths::installDir()), Col::textFaint);
        appendTerm(QString("Log file:    %1").arg(Paths::logFile()), Col::textFaint);
        appendTerm(QString("Admin:       %1").arg(isRunningAsAdmin() ? "YES" : "NO"),
                   isRunningAsAdmin() ? Col::success : Col::danger);
        appendTerm("", Col::textFaint);

        QDir().mkpath(Paths::installDir());
        QDir().mkpath(Paths::workDir());
        QDir().mkpath(Paths::binDir());
        QDir().mkpath(Paths::torDir());

        QVector<Step> steps = {

            {"git", "Installing Git for Windows",
             {
                 "where git >nul 2>&1 && (echo Git already installed && exit 0)",
                 "winget install --id Git.Git -e --silent "
                 "--accept-package-agreements --accept-source-agreements "
                 "--disable-interactivity 2>&1",
                 "echo Git ready"
             },
             8},

            {"python", "Installing Python 3.11",
             {
                 "where python >nul 2>&1 && (echo Python already installed && exit 0)",
                 "winget install --id Python.Python.3.11 -e --silent "
                 "--accept-package-agreements --accept-source-agreements "
                 "--disable-interactivity 2>&1",
                 "echo Python ready"
             },
             14},

            {"aqt", "Installing aqtinstall (Qt installer tool)",
             {
                 "python -m pip install --upgrade pip --quiet 2>&1",
                 "python -m pip install aqtinstall --quiet 2>&1"
             },
             18},

            {"vs", "Installing Visual Studio Build Tools",
             {
                 "if exist \"C:\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat\" "
                 "(echo MSVC already installed && exit 0)",
                 QString("powershell -Command \"& { "
                         "Invoke-WebRequest -Uri "
                         "'https://aka.ms/vs/17/release/vs_buildtools.exe' "
                         "-OutFile '%1\\vs_buildtools.exe' -UseBasicParsing }\" 2>&1")
                     .arg(Paths::workDir()),
                 QString("start /wait \"\" \"%1\\vs_buildtools.exe\" "
                         "--quiet --wait --norestart --nocache "
                         "--installPath \"C:\\BuildTools\" "
                         "--add Microsoft.VisualStudio.Workload.VCTools "
                         "--add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 "
                         "--add Microsoft.VisualStudio.Component.Windows11SDK.22621 "
                         "--includeRecommended")
                     .arg(Paths::workDir()),
                 "if not exist \"C:\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat\" "
                 "(echo [ERROR] MSVC install failed && exit 1)"
             },
             42},

            {"qt", "Downloading Qt 6.6 (MSVC 2019 64-bit)",
             {
                 QString("if exist \"%1\\lib\\Qt6Core.lib\" "
                         "(echo Qt already present && exit 0)").arg(Paths::qtDir()),
                 QString("python -m aqt install-qt windows desktop %1 %2 "
                         "--outputdir \"%3\\Qt\" "
                         "--archives qtbase qtwebengine qtsvg 2>&1")
                     .arg(kQtVersion, kQtArch, Paths::workDir()),
                 QString("if not exist \"%1\\lib\\Qt6Core.lib\" "
                         "(echo [ERROR] Qt download failed && exit 1)")
                     .arg(Paths::qtDir())
             },
             68},

            {"tor", "Downloading Tor Expert Bundle",
             {
                 QString("if exist \"%1\\tor.exe\" (echo Tor already present && exit 0)")
                     .arg(Paths::torDir()),
                 QString("powershell -Command \"& { "
                         "Invoke-WebRequest -Uri '%1' "
                         "-OutFile '%2\\tor.tar.gz' -UseBasicParsing }\" 2>&1")
                     .arg(kTorBundleUrl, Paths::workDir()),
                 QString("tar -xzf \"%1\\tor.tar.gz\" -C \"%2\" --strip-components=1")
                     .arg(Paths::workDir(), Paths::torDir()),
                 QString("if exist \"%1\\tor.exe\" (echo Tor ready) else "
                         "(echo [WARN] Tor missing — private mode disabled)")
                     .arg(Paths::torDir())
             },
             78},

            {"clone", "Cloning RootBrowser source from GitHub",
             {
                 QString("if exist \"%1\\src\" rmdir /S /Q \"%1\\src\"")
                     .arg(Paths::workDir()),
                 QString("git clone --depth=1 %1 \"%2\\src\" 2>&1")
                     .arg(kGitHubUrl, Paths::workDir()),
                 QString("if not exist \"%1\\src\\main.cpp\" "
                         "(echo [ERROR] Source clone failed && exit 1)")
                     .arg(Paths::workDir())
             },
             84},

            {"compile", "Compiling RootBrowser with MSVC",
             {
                 QString(
                     "@echo off\n"
                     "call \"C:\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat\" >nul\n"
                     "cd /d \"%1\\src\"\n"
                     "cl /nologo /std:c++17 /Zc:__cplusplus /permissive- "
                     "/EHsc /O2 /MD /DNDEBUG /DNOMINMAX /DWIN32_LEAN_AND_MEAN ^\n"
                     "   /I \"%2\\include\" ^\n"
                     "   /I \"%2\\include\\QtCore\" ^\n"
                     "   /I \"%2\\include\\QtGui\" ^\n"
                     "   /I \"%2\\include\\QtWidgets\" ^\n"
                     "   /I \"%2\\include\\QtWebEngineWidgets\" ^\n"
                     "   /I \"%2\\include\\QtWebEngineCore\" ^\n"
                     "   /I \"%2\\include\\QtNetwork\" ^\n"
                     "   /Fe:\"%3\\RootBrowser.exe\" ^\n"
                     "   main.cpp connector.cpp viewpagesource.cpp webadblocker.cpp ^\n"
                     "   bookmarkstore.cpp bookmarkpage.cpp ^\n"
                     "   downloadmanager.cpp downloadpage.cpp downloadpanel.cpp ^\n"
                     "   historystore.cpp historypage.cpp ^\n"
                     "   settingsstore.cpp settingspage.cpp ^\n"
                     "   sessionstore.cpp findinpage.cpp ^\n"
                     "   torcontroller.cpp privatemodepage.cpp ^\n"
                     "   privatehomepage.cpp privatebrowser.cpp restoresessionpage.cpp ^\n"
                     "   /link /SUBSYSTEM:WINDOWS ^\n"
                     "   /LIBPATH:\"%2\\lib\" ^\n"
                     "   Qt6WebEngineWidgets.lib Qt6WebEngineCore.lib ^\n"
                     "   Qt6Widgets.lib Qt6Gui.lib Qt6Core.lib Qt6Network.lib ^\n"
                     "   Qt6WebChannel.lib Qt6Quick.lib Qt6Qml.lib ^\n"
                     "   user32.lib shell32.lib advapi32.lib ole32.lib\n"
                 ).arg(Paths::workDir(), Paths::qtDir(), Paths::binDir()),
                 QString("if not exist \"%1\\RootBrowser.exe\" "
                         "(echo [ERROR] Compile failed && exit 1)")
                     .arg(Paths::binDir())
             },
             92},

            {"deploy", "Bundling Qt dependencies (windeployqt)",
             {
                 QString("\"%1\\bin\\windeployqt.exe\" --release "
                         "--no-translations --no-system-d3d-compiler "
                         "--no-opengl-sw --webenginecore "
                         "\"%2\\RootBrowser.exe\" 2>&1")
                     .arg(Paths::qtDir(), Paths::binDir()),
                 QString("if not exist \"%1\\Qt6Core.dll\" "
                         "(echo [ERROR] windeployqt failed && exit 1)")
                     .arg(Paths::binDir())
             },
             96},

            {"torcopy", "Placing Tor beside browser",
             {
                 QString("if exist \"%1\\tor.exe\" ("
                         "xcopy /E /I /Y \"%1\" \"%2\\tor\" >nul && echo Tor placed)")
                     .arg(Paths::torDir(), Paths::binDir())
             },
             98},

            {"shortcuts", "Creating Start Menu shortcuts and PATH entry",
             {
                 QString("powershell -Command \"& { "
                         "$WshShell = New-Object -ComObject WScript.Shell; "
                         "$lnk = $WshShell.CreateShortcut("
                         "'C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\RootBrowser.lnk'); "
                         "$lnk.TargetPath = '%1\\RootBrowser.exe'; "
                         "$lnk.WorkingDirectory = '%1'; "
                         "$lnk.Save() }\" 2>&1")
                     .arg(Paths::binDir()),
                 QString("powershell -Command \"& { "
                         "$WshShell = New-Object -ComObject WScript.Shell; "
                         "$lnk = $WshShell.CreateShortcut("
                         "'$env:PUBLIC\\Desktop\\RootBrowser.lnk'); "
                         "$lnk.TargetPath = '%1\\RootBrowser.exe'; "
                         "$lnk.WorkingDirectory = '%1'; "
                         "$lnk.Save() }\" 2>&1")
                     .arg(Paths::binDir()),
                 QString("powershell -Command \"& { "
                         "$p = [Environment]::GetEnvironmentVariable('Path', 'Machine'); "
                         "if ($p -notlike '*%1*') {{ "
                         "[Environment]::SetEnvironmentVariable('Path', "
                         "$p + ';%1', 'Machine') }} }\" 2>&1")
                     .arg(Paths::binDir())
             },
             100},
        };

        runStepChain(steps, 0);
    }

    void runStepChain(const QVector<Step>& steps, int idx) {
        if (idx >= steps.size()) {
            running_ = false;
            Logger::instance().info("Installation complete");
            appendTerm("", Col::textPrimary);
            appendTerm("Installation completed successfully", Col::success);
            QFile::remove(Paths::stateFile());
            stack_->setCurrentIndex(2);
            return;
        }

        const Step& step = steps[idx];
        Logger::instance().step(step.title);

        setStepTitle(QString("Step %1 of %2 · %3")
                     .arg(idx + 1).arg(steps.size()).arg(step.title));

        appendTerm("", Col::textPrimary);
        appendTerm(QString("  > %1   [%2/%3]")
                    .arg(step.title).arg(idx + 1).arg(steps.size()),
                    Col::accent);

        QFile sf(Paths::stateFile());
        if (sf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            sf.write(step.id.toUtf8());
            sf.close();
        }

        runCommandList(step.commands, 0, [this, idx, step, steps](bool ok){
            if (!ok) {
                Logger::instance().error("Step failed: " + step.title);
                appendTerm(QString("  X Failed: %1").arg(step.title), Col::danger);
                setStatus("Failed · " + step.title, Col::danger);
                running_ = false;

                if (failedLog_ && terminal_)
                    failedLog_->setPlainText(terminal_->toPlainText());
                if (failedSubLabel_)
                    failedSubLabel_->setText(
                        QString("Failed at step %1 of %2: %3")
                            .arg(idx + 1).arg(steps.size()).arg(step.title));

                stack_->setCurrentIndex(3);
                return;
            }

            appendTerm(QString("  OK %1").arg(step.title), Col::success);
            if (progress_) progress_->setValue(step.progress);
            setStatus("Completed · " + step.title, Col::success);

            QTimer::singleShot(150, this, [this, idx, steps]{
                runStepChain(steps, idx + 1);
            });
        });
    }

    void runCommandList(const QStringList& cmds, int cmdIdx,
                        std::function<void(bool)> done)
    {
        if (cmdIdx >= cmds.size()) { done(true); return; }

        const QString cmd = cmds[cmdIdx].trimmed();
        if (cmd.isEmpty()) { runCommandList(cmds, cmdIdx + 1, done); return; }

        appendTerm(QString("  $ %1").arg(
            cmd.length() > 120 ? cmd.left(120) + "..." : cmd), Col::textFaint);

        auto* proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        proc->setWorkingDirectory(Paths::workDir());

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("PYTHONIOENCODING", "utf-8");
        env.insert("PYTHONUTF8", "1");
        proc->setProcessEnvironment(env);

        connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]{
            const QByteArray data = proc->readAllStandardOutput();
            const QString text = QString::fromUtf8(data);
            for (const QString& line : text.split('\n')) {
                const QString ln = line.trimmed();
                if (ln.isEmpty()) continue;
                QColor c = Col::textPrimary;
                if (ln.contains("[ERROR]") || ln.contains("error", Qt::CaseInsensitive)
                    || ln.contains("failed", Qt::CaseInsensitive))
                    c = Col::danger;
                else if (ln.contains("[WARN]") || ln.contains("warning", Qt::CaseInsensitive))
                    c = Col::warning;
                else if (ln.contains("OK") || ln.contains("ready")
                         || ln.contains("done") || ln.contains("complete"))
                    c = Col::success;
                else if (ln.contains("Downloading") || ln.contains("Extracting"))
                    c = Col::accent;

                appendTerm("    " + ln, c);
                Logger::instance().info(ln);
            }
        });

        connect(proc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this,
                [this, proc, cmds, cmdIdx, done](int code, QProcess::ExitStatus st){
            proc->deleteLater();

            if (code == 0 && st == QProcess::NormalExit) {
                runCommandList(cmds, cmdIdx + 1, done);
            } else {
                Logger::instance().error(QString("Command failed (exit %1): %2")
                                             .arg(code).arg(cmds[cmdIdx]));
                done(false);
            }
        });

        proc->start("cmd.exe", { "/C", cmd });
    }

    static QPixmap makeSuccessIcon(int size) {
        QPixmap pm(160, 160);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen); p.setBrush(Col::success);
        p.drawEllipse(QRectF(0, 0, 160, 160));
        p.setPen(QPen(Qt::white, 12, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawLine(48, 82, 70, 106);
        p.drawLine(70, 106, 114, 56);
        return pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    static QPixmap makeErrorIcon(int size) {
        QPixmap pm(160, 160);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen); p.setBrush(Col::danger);
        p.drawEllipse(QRectF(0, 0, 160, 160));
        p.setPen(QPen(Qt::white, 12, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(56, 56, 104, 104);
        p.drawLine(104, 56, 56, 104);
        return pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QStackedWidget* stack_ = nullptr;
    Background*     bg_ = nullptr;
    WindowControls* controls_ = nullptr;
    QPlainTextEdit* terminal_ = nullptr;
    QProgressBar*   progress_ = nullptr;
    QLabel*         statusLabel_ = nullptr;
    QLabel*         stepLabel_ = nullptr;
    QLabel*         failedSubLabel_ = nullptr;
    QPlainTextEdit* failedLog_ = nullptr;
    bool running_ = false;
    QPoint dragPos_;
    bool dragging_ = false;
};

// ============================================================================
//  main — with elevation check
// ============================================================================
int main(int argc, char** argv) {
    bool alreadyElevated = false;
    for (int i = 0; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == "--elevated") {
            alreadyElevated = true;
            break;
        }
    }

    if (!isRunningAsAdmin() && !alreadyElevated) {
        if (relaunchAsAdmin()) {
            return 0;
        }

        QApplication app(argc, argv);
        QMessageBox::critical(
            nullptr,
            "Administrator Rights Required",
            "RootBrowser requires administrator privileges to:\n\n"
            "  • Install Visual Studio Build Tools\n"
            "  • Download and configure Qt6\n"
            "  • Write to system directories\n\n"
            "Please accept the UAC prompt or run this installer as Administrator.");
        return 1;
    }

    QApplication app(argc, argv);
    app.setApplicationName(kAppName);
    app.setStyle("Fusion");

    QFont f = app.font();
    f.setFamilies({"Segoe UI Variable Text", "Segoe UI", "Inter", "Ubuntu",
                   "Noto Sans", "Cantarell", "DejaVu Sans"});
    f.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(f);

    QPalette pal;
    pal.setColor(QPalette::Window,          Col::bg);
    pal.setColor(QPalette::WindowText,      Col::textPrimary);
    pal.setColor(QPalette::Base,            Col::surface);
    pal.setColor(QPalette::Text,            Col::textPrimary);
    pal.setColor(QPalette::Button,          Col::surface);
    pal.setColor(QPalette::ButtonText,      Col::textPrimary);
    pal.setColor(QPalette::Highlight,       Col::accent);
    pal.setColor(QPalette::HighlightedText, Col::textBright);
    app.setPalette(pal);

    app.setStyleSheet(
        "QToolTip{background:#1d2028;color:#e6e8ee;border:1px solid #242832;"
        "padding:6px 10px;border-radius:6px;}");

    Installer w;
    w.show();
    return app.exec();
}
