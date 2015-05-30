#include "bakaengine.h"

#include <QMessageBox>
#include <QDir>

#include "ui/mainwindow.h"
#include "ui_mainwindow.h"
#include "settings.h"
#include "mpvhandler.h"
#include "gesturehandler.h"
#include "overlayhandler.h"
#include "updatemanager.h"
#include "widgets/dimdialog.h"
#include "util.h"

BakaEngine::BakaEngine(QObject *parent):
    QObject(parent),
    window(static_cast<MainWindow*>(parent)),
    mpv(new MpvHandler(window->ui->mpvFrame->winId(), this)),
    gesture(new GestureHandler(this)),
    overlay(new OverlayHandler(this)),
    update(new UpdateManager(this)),
    // note: trayIcon does not work in my environment--known qt bug
    // see: https://bugreports.qt-project.org/browse/QTBUG-34364
    sysTrayIcon(new QSystemTrayIcon(window->windowIcon(), this)),
    // todo: tray menu/tooltip
    translator(nullptr),
    qtTranslator(nullptr)
{
    if(Util::DimLightsSupported())
        dimDialog = new DimDialog(window, nullptr);
    else
    {
        dimDialog = nullptr;
        window->ui->action_Dim_Lights->setEnabled(false);
    }

    connect(mpv, &MpvHandler::messageSignal,
            [=](QString msg)
            {
                Print(msg, "mpv");
            });
    connect(update, &UpdateManager::messageSignal,
            [=](QString msg)
            {
                Print(msg, "update");
            });
}

BakaEngine::~BakaEngine()
{
    if(translator != nullptr)
        delete translator;
    if(qtTranslator != nullptr)
        delete qtTranslator;
    if(dimDialog != nullptr)
        delete dimDialog;
    delete update;
    delete overlay;
    delete gesture;
    delete mpv;
}

void BakaEngine::LoadSettings()
{
    QFile f(Util::SettingsLocation());
    f.open(QIODevice::ReadOnly | QIODevice::Text);
    QString l = f.readLine();
    f.close();
    if(l.startsWith("{"))
        Load2_0_3();
    else
    {
        Settings *settings = new Settings(Util::SettingsLocation(), this);
        settings->Load();
        QString version;
        if(settings->isEmpty()) // empty settings
        {
            version = "2.0.2"; // current version

            // populate initially
    #if defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
            settings->beginGroup("mpv");
            settings->setValue("af", "scaletempo");
            settings->setValue("vo", "vdpau,opengl-hq");
            settings->setValue("hwdec", "auto");
            settings->endGroup();
    #endif
        }
        else
        {
            settings->beginGroup("baka-mplayer");
            version = settings->value("version", "1.9.9"); // defaults to the first version without version info in settings
            settings->endGroup();
        }

        if(version == "2.0.2") Load2_0_2(settings);
        else if(version == "2.0.1") { Load2_0_1(settings); settings->clear(); SaveSettings(); }
        else if(version == "2.0.0") { Load2_0_0(settings); settings->clear(); SaveSettings(); }
        else if(version == "1.9.9") { Load1_9_9(settings); settings->clear(); SaveSettings(); }
        else
        {
            Load2_0_2(settings);
            window->ui->action_Preferences->setEnabled(false);
            QMessageBox::information(window, tr("Settings version not recognized"), tr("The settings file was made by a newer version of baka-mplayer; please upgrade this version or seek assistance from the developers.\nSome features may not work and changed settings will not be saved."));
        }
        delete settings;
    }
}

void BakaEngine::Command(QString command)
{
    if(command == QString())
        return;
    QStringList args = command.split(" ");
    if(!args.empty())
    {
        if(args.front() == "baka") // implicitly understood
            args.pop_front();

        if(!args.empty())
        {
            auto iter = BakaCommandMap.find(args.front());
            if(iter != BakaCommandMap.end())
            {
                args.pop_front();
                (this->*(iter->first))(args); // execute command
            }
            else
                InvalidCommand(args.join(' '));
        }
        else
            RequiresParameters("baka");
    }
    else
        InvalidCommand(args.join(' '));
}

void BakaEngine::Print(QString what, QString who)
{
    window->ui->outputTextEdit->moveCursor(QTextCursor::End);
    window->ui->outputTextEdit->insertPlainText(QString("[%0]: %1\n").arg(who, what));
}

void BakaEngine::InvalidCommand(QString command)
{
    Print(tr("invalid command '%0'").arg(command));
}

void BakaEngine::InvalidParameter(QString parameter)
{
    Print(tr("invalid parameter '%0'").arg(parameter));
}

void BakaEngine::RequiresParameters(QString what)
{
    Print(tr("'%0' requires parameters").arg(what));
}
