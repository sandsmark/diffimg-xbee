
/******************************************************************************
   DiffImg: image difference viewer
   Copyright(C) 2011-2014  xbee@xbee.net

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *******************************************************************************/

#include <QApplication>
//#include <QPlastiqueStyle>
#include <QMessageBox>
#include <QDesktopWidget>
#include <QtCore/QFileInfo>
#include <QDebug>

#include "DiffImgWindow.h"
#include "AppSettings.h"
#include "MiscFunctions.h"
#include "MetricsRegistering.h"
//#include "FormatsRegistering.h"
#include "MetricsManager.h"
#include "BaseMetric.h"

const int EXIT_OK = 0;
const int EXIT_NOK = 1;

#ifdef Q_OS_UNIX
#include <time.h>
#endif

// functions for console mode (in order to get the message)
//-----------------------------------------------------------

#ifdef Q_OS_WIN

#include <QtCore/QDir>
#include <QtCore/QString>
#include <QtCore/QLibrary>

#include <windows.h>
#include <shellapi.h>
#include <process.h>

// console related includes
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <fstream>

/**
   try to attach to the parents console
   \return true if console has been attached, false otherwise
 */
typedef BOOL (WINAPI *attachConsolePtr)(DWORD dwProcessId);
static attachConsolePtr attachConsole = 0;
static bool attachConsoleResolved = false;
static bool attachToConsole()
{
    if (!attachConsoleResolved) {
        attachConsoleResolved = true;
        attachConsole = (attachConsolePtr) QLibrary::resolve(QLatin1String("kernel32"), "AttachConsole");
    }

    return attachConsole ? attachConsole(~0U) != 0 : false;
}

/**
   redirect stdout, stderr and
   cout, wcout, cin, wcin, wcerr, cerr, wclog and clog to console
 */
static void redirectToConsole()
{
    int hCrt;
    FILE *hf;

//	int i;

    hCrt = _open_osfhandle((intptr_t) GetStdHandle(STD_INPUT_HANDLE), _O_TEXT);

    if (hCrt != -1) {
        hf = _fdopen(hCrt, "r");
        *stdin = *hf;
        /*i =*/ setvbuf(stdin, nullptr, _IONBF, 0);
    }

    hCrt = _open_osfhandle((intptr_t) GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);

    if (hCrt != -1) {
        hf = _fdopen(hCrt, "w");
        *stdout = *hf;
        /*i =*/ setvbuf(stdout, nullptr, _IONBF, 0);
    }

    hCrt = _open_osfhandle((intptr_t) GetStdHandle(STD_ERROR_HANDLE), _O_TEXT);

    if (hCrt != -1) {
        hf = _fdopen(hCrt, "w");
        *stderr = *hf;
        /*i =*/ setvbuf(stderr, nullptr, _IONBF, 0);
    }

    // make cout, wcout, cin, wcin, wcerr, cerr, wclog and clog
    // point to console as well
    std::ios::sync_with_stdio();
}

#endif

// extract from QTest ...
void qSleep(int ms)
{
#ifdef Q_OS_WIN
    Sleep(uint(ms));
#else
    struct timespec ts = {
        ms / 1000, (ms % 1000) * 1000 * 1000
    };
    nanosleep(&ts, nullptr);
#endif
}

void listMetrics()
{
    QTextStream out(stdout);
    out << Qt::endl;
    out << QObject::tr("Image Difference tool.") << Qt::endl;
    out << Qt::endl;
    out << QObject::tr("Available metrics:") << Qt::endl;
    out << Qt::endl;

    // metrics registering
    MetricsRegistering::registerAll();

    //
    const QList<BaseMetric *> &list = MetricsManager::getMetrics();

    foreach (BaseMetric *met, list) {
        out << met->getType() << ": " << met->getName() << Qt::endl;

        // input parameters
        const QList<MetricParam *> &listInput = met->getInputParams();

        if (!listInput.isEmpty()) {
            out << QObject::tr("\tInput parameters:") << Qt::endl;

            foreach (MetricParam *param, listInput) {
                out << "\t\t" << param->type << " = " << param->threshold.toString() << " (" << param->name << ")" << Qt::endl;
            }
        }

        // output parameters
        const QList<MetricParam *> &listOutput = met->getOutputParams();
        out << QObject::tr("\tOutput parameters:") << Qt::endl;

        foreach (MetricParam *param, listOutput) {
            out << "\t\t" << param->type << " = " << param->threshold.toString() << " (" << param->name << ")" << Qt::endl;
        }

        out << Qt::endl;
    }

    out << Qt::endl;
    out << QObject::tr("How to use batch mode:") << Qt::endl;
    out << QObject::tr("Example 1: --batch --metric PerChannelMeanMetric img1 img2: Use of per channel mean metric", "Do not translate parameters, only comments") << Qt::endl;
    out << QObject::tr("Example 2: --batch --metric PerChannelMetric --threshold ErrorNum=250 img1 img2: Use of per channel metric and exit with error if pixel with difference number is upper to 250", "Do not translate parameters, only comments") << Qt::endl;
    out << QObject::tr("Example 3: --batch --metric PerceptualMetric --in Gamma=1.8 --in FOV=75 --threshold ErrorNum=250 img1 img2: Use of perceptual metric with some input parameters set and exit with error if pixel with difference number is upper to 250", "Do not translate parameters, only comments") << Qt::endl;
    out << Qt::endl;
}

void usage()
{
    QTextStream out(stdout);
    out << Qt::endl;
    out << QObject::tr("Image Difference tool.") << Qt::endl;
    out << Qt::endl;
    out << QObject::tr("Usage: ") << QApplication::arguments().at(0) << " " << QObject::tr("[options]") << " " << QObject::tr("img1 img2") << Qt::endl;
    out << QObject::tr("") << Qt::endl;
    out << QObject::tr("Following options are available:") << Qt::endl;
    out << QObject::tr(" --list                 : list all metrics with their parameters.") << Qt::endl;
    out << QObject::tr(" --portable             : use settings file location near the executable (for portable use).") << Qt::endl;
    out << QObject::tr("") << Qt::endl;
    out << QObject::tr(" --batch                : batch mode. Differences are computed without GUI and return code is used as result.") << Qt::endl;
    out << QObject::tr(" --output filename      : save difference image. The difference is only saved when the input files aren't strictly identical.") << Qt::endl;
    out << QObject::tr(" --metric name          : metric tag name. Display list of metrics with --list option.") << Qt::endl;
    out << QObject::tr(" --in  name=value       : input parameter update.") << Qt::endl;
    out << QObject::tr(" --threshold name=value : output parameter used to define comparison batch mode. If you set several threshold parameters, only the last one will be kept.") << Qt::endl;

}

// --batch --metric PerceptualMetric --in Gamma=1.8 --in FOV=75 --threshold ErrorNum=250 D:\developpements\qt4\DevAMoi\diffimg2.0\test\img1\IMG_0149.png D:\developpements\qt4\DevAMoi\diffimg2.0\test\img2\IMG_0149.png

int makeDiffBatch(const QString &file1, const QString &file2, const QString &metric, const QMap<QString, QString> &inParams, const QMap<QString, QString> &outParams, const QString &outFile)
{
    if (!QFileInfo(file1).exists() || !QFileInfo(file2).exists()) {
        qWarning() << QObject::tr("You must set valid filename files as arguments in batch mode!!");
        return EXIT_NOK;
    }

    // metrics registering
    MetricsRegistering::registerAll();
    //FormatsRegistering::registerAll();

    //
    //
    BaseMetric *met =  MetricsManager::getMetricByType(metric);
    /*
    const QList<BaseMetric *> &list = MetricsManager::getMetrics();

    bool found = false;
    foreach (met, list)
    {
        if (met->getType() == metric)
        {
            found = true;
            break;
        }
    }
    */

    if (!met) {
        qWarning() << QObject::tr("Metric %1 unknown!!").arg(metric);
        return EXIT_NOK;
    }

    // set input parameters
    const QList<MetricParam *> &listInput = met->getInputParams();

    if (!inParams.isEmpty() && listInput.isEmpty()) {
        qWarning() << QObject::tr("Metric %1 has no input parameter!!").arg(met->getType());
        return EXIT_NOK;
    }

    if (!inParams.isEmpty() && !listInput.isEmpty()) {
        foreach (MetricParam *param, listInput) {
            foreach (const QString &key, inParams.keys()) {
                if (param->type == key) {
                    param->setThreshold(inParams[key]);
                }
            }
        }
    }

    const QList<MetricParam *> &listOutput = met->getOutputParams();

    // clear saved settings
    foreach (MetricParam *param, listOutput) {
        param->reset();
        param->used = false;
    }

    if (!outParams.isEmpty()) {
        foreach (MetricParam *param, listOutput) {
            foreach (const QString &key, outParams.keys()) {
                if (param->type == key) {
                    param->setThreshold(outParams[key]);
                    param->used = true;
                }
            }
        }
    }

    met->checkDifferences(file1, file2);

    if (!met->isValid()) {
        qWarning() << QObject::tr("Can't display difference, more information in the log panel ...");
        return EXIT_NOK;
    }

    if (!met->getPixelError()) {
        qDebug() << QObject::tr("Files are identical");
        return EXIT_OK;
    }

    // display stats
    QTextStream out(stdout);

    foreach (MetricParam *param, listInput) {
        out << "  " << param->type << " = " << param->threshold.toString() << Qt::endl;
    }

    foreach (MetricParam *param, listOutput) {
        if (param->used) {
            out << "* ";
        } else {
            out << "  ";
        }

        out << param->type << " = " << param->value.toString() << " (threshold = " << param->threshold.toString() << ")" << Qt::endl;
    }

    if (!outFile.isEmpty()) {
        if (!met->saveDifference(outFile)) {
            out << QObject::tr("Can't save difference image in %1").arg(outFile) << Qt::endl;
        }
    }

    if (met->selectedStatsIsValid()) {
        out << QObject::tr("Files are identical (under the threshold)") << Qt::endl;
        return EXIT_OK;
    }

    return EXIT_NOK;
}

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(diffimg);

    QApplication app(argc, argv);

    // add possible image plugin path
#ifdef Q_WS_MAC
    app.addLibraryPath(QApplication::applicationDirPath() + "/../plugins");
#endif
    app.addLibraryPath(QApplication::applicationDirPath() + "./plugins");

    // update some application infos (use by some platform for temp storage, ...)
    MiscFunctions::updateApplicationIdentity();

    // set default language for application (computed or saved)
    MiscFunctions::setDefaultLanguage();

    // modifying base look
#if defined(Q_WS_WIN) || defined(Q_WS_MAC)
    //QApplication::setStyle(new QPlastiqueStyle);
#endif

    // check for special argument
    bool forceResetConfig = false;
    bool batchMode = false;
    QString outfile;
    QStringList args = QApplication::arguments();
    QStringList files;
    QString metric = "PerChannelMetric";
    QMap<QString, QString> inParams;
    QMap<QString, QString> outParams;
    bool errorInParameters = false;

    for (int i = 1; i < args.count(); ++i) {
        const QString arg = args.at(i);

        if (arg == "--reset-config") {
            forceResetConfig = true;
        } else if (arg == "--list") { // list all metrics & parameters
            listMetrics();
            return EXIT_OK;
        } else if (arg == "--batch") {
            batchMode = true;
        } else if (arg == "--metric") {
            metric = args.at(++i);
        } else if (arg == "--in") {
            QStringList in = args.at(++i).split("=");

            if (in.size() == 2) {
                inParams[in[0]] = in[1];
            } else {
                errorInParameters = true;
            }
        } else if (arg == "--threshold") {
            QStringList out = args.at(++i).split("=");

            if (out.size() == 2) {
                outParams[out[0]] = out[1];
            } else {
                errorInParameters = true;
            }
        } else if (arg == "--output") {
            outfile = args.value(++i);
        } else if (arg == "--help") {
            usage();
            return EXIT_OK;
        } else if (arg == "--portable") {
            QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, QApplication::applicationDirPath());
            QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, QApplication::applicationDirPath());
            QSettings::setDefaultFormat(QSettings::IniFormat);
        } else {
            files << arg;
        }
    }

    if (files.size() == 1) { // must have two parameters
        files << "";
    }

    if (errorInParameters) {
        qWarning() << QObject::tr("Error in options!");
        usage();
        return EXIT_NOK;
    }

    // check batch mode
    if (batchMode) {
#ifdef Q_OS_WIN // console in win32

        if (attachToConsole()) {
            redirectToConsole();
        }

#endif

        if (files.size() == 2) {
            return makeDiffBatch(files[0], files[1], metric, inParams, outParams, outfile);
        } else {
            qWarning() << QObject::tr("You must set at least two files as arguments in batch mode!!");
            usage();
            return EXIT_NOK;
        }
    }

    AppSettings settings;

    settings.beginGroup("Application");

    // reset the saved configuration if needed
    bool resetConfig = settings.value("resetConfig", false).toBool();

    if (resetConfig || forceResetConfig) {
        settings.clear();
        settings.sync();
    }

    settings.endGroup();

    DiffImgWindow w;

    QObject::connect(&app, SIGNAL(lastWindowClosed()), &app, SLOT(quit()));
    w.show();

    if (!files.isEmpty()) {
        w.setFiles(files[0], files[1]);
    }

    w.load(); // load files or dialog

    return app.exec();
}
