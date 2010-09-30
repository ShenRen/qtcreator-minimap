/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "buildablehelperlibrary.h"

#include <QtCore/QFileInfo>
#include <QtCore/QCoreApplication>
#include <QtCore/QHash>
#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtCore/QDateTime>

#include <utils/environment.h>
#include <utils/synchronousprocess.h>

#include <QtGui/QDesktopServices>
#include <QDebug>

namespace Utils {

QString BuildableHelperLibrary::findSystemQt(const Utils::Environment &env)
{
    QStringList paths = env.path();
    foreach (const QString &path, paths) {
        foreach (const QString &possibleCommand, possibleQMakeCommands()) {
            const QFileInfo qmake(path + QLatin1Char('/') + possibleCommand);
            if (qmake.exists()) {
                if (!qtVersionForQMake(qmake.absoluteFilePath()).isNull()) {
                    return qmake.absoluteFilePath();
                }
            }
        }
    }
    return QString();
}

QString BuildableHelperLibrary::qtInstallDataDir(const QString &qmakePath)
{
    QProcess proc;
    proc.start(qmakePath, QStringList() << QLatin1String("-query") << QLatin1String("QT_INSTALL_DATA"));
    if (proc.waitForFinished())
        return QString(proc.readAll().trimmed());
    return QString();
}

QString BuildableHelperLibrary::qtVersionForQMake(const QString &qmakePath)
{
    if (qmakePath.isEmpty())
        return QString();

    QProcess qmake;
    qmake.start(qmakePath, QStringList(QLatin1String("--version")));
    if (!qmake.waitForStarted()) {
        qWarning("Cannot start '%s': %s", qPrintable(qmakePath), qPrintable(qmake.errorString()));
        return QString();
    }
    if (!qmake.waitForFinished())      {
        Utils::SynchronousProcess::stopProcess(qmake);
        qWarning("Timeout running '%s'.", qPrintable(qmakePath));
        return QString();
    }
    if (qmake.exitStatus() != QProcess::NormalExit) {
        qWarning("'%s' crashed.", qPrintable(qmakePath));
        return QString();
    }
    const QString output = QString::fromLocal8Bit(qmake.readAllStandardOutput());
    QRegExp regexp(QLatin1String("(QMake version|QMake version:)[\\s]*([\\d.]*)"), Qt::CaseInsensitive);
    regexp.indexIn(output);
    if (regexp.cap(2).startsWith(QLatin1String("2."))) {
        QRegExp regexp2(QLatin1String("Using Qt version[\\s]*([\\d\\.]*)"), Qt::CaseInsensitive);
        regexp2.indexIn(output);
        const QString version = regexp2.cap(1);
        return version;
    }
    return QString();
}

bool BuildableHelperLibrary::checkMinimumQtVersion(const QString &qtVersionString, int majorVersion, int minorVersion, int patchVersion)
{
    int major = -1;
    int minor = -1;
    int patch = -1;

    // check format
    QRegExp qtVersionRegex(QLatin1String("^\\d+\\.\\d+\\.\\d+$"));
    if (!qtVersionRegex.exactMatch(qtVersionString))
        return false;

    QStringList parts = qtVersionString.split(QLatin1Char('.'));
    major = parts.at(0).toInt();
    minor = parts.at(1).toInt();
    patch = parts.at(2).toInt();

    if (major == majorVersion) {
        if (minor == minorVersion) {
            if (patch >= patchVersion)
                return true;
        } else if (minor > minorVersion)
            return true;
    }

    return false;
}

QStringList BuildableHelperLibrary::possibleQMakeCommands()
{
    // On windows no one has renamed qmake, right?
#ifdef Q_OS_WIN
    return QStringList(QLatin1String("qmake.exe"));
#else
    // On unix some distributions renamed qmake to avoid clashes
    QStringList result;
    result << QLatin1String("qmake-qt4") << QLatin1String("qmake4") << QLatin1String("qmake");
    return result;
#endif
}

// Copy helper source files to a target directory, replacing older files.
bool BuildableHelperLibrary::copyFiles(const QString &sourcePath,
                                     const QStringList &files,
                                     const QString &targetDirectory,
                                     QString *errorMessage)
{
    if (!QDir().mkpath(targetDirectory)) {
        *errorMessage = QCoreApplication::translate("ProjectExplorer::DebuggingHelperLibrary", "The target directory %1 could not be created.").arg(targetDirectory);
        return false;
    }
    foreach (const QString &file, files) {
        const QString source = sourcePath + file;
        const QString dest = targetDirectory + file;
        const QFileInfo destInfo(dest);
        if (destInfo.exists()) {
            if (destInfo.lastModified() >= QFileInfo(source).lastModified())
                continue;
            if (!QFile::remove(dest)) {
                *errorMessage = QCoreApplication::translate("ProjectExplorer::DebuggingHelperLibrary", "The existing file %1 could not be removed.").arg(destInfo.absoluteFilePath());
                return false;
            }
        }
        if (!QFile::copy(source, dest)) {
            *errorMessage = QCoreApplication::translate("ProjectExplorer::DebuggingHelperLibrary", "The file %1 could not be copied to %2.").arg(source, dest);
            return false;
        }
    }
    return true;
}

// Helper: Run a build process with merged stdout/stderr
static inline bool runBuildProcessI(QProcess &proc,
                                    const QString &binary,
                                    const QStringList &args,
                                    int timeoutMS,
                                    bool ignoreNonNullExitCode,
                                    QString *output, QString *errorMessage)
{
    proc.start(binary, args);
    if (!proc.waitForStarted()) {
        *errorMessage = QCoreApplication::translate("ProjectExplorer::BuildableHelperLibrary",
                                                    "Cannot start process: %1").
                                                    arg(proc.errorString());
        return false;
    }
    // Read stdout/err and check for timeouts
    QByteArray stdOut;
    QByteArray stdErr;
    if (!SynchronousProcess::readDataFromProcess(proc, timeoutMS, &stdOut, &stdErr, false)) {
        *errorMessage = QCoreApplication::translate("ProjectExplorer::BuildableHelperLibrary",
                                                    "Timeout after %1s.").
                                                    arg(timeoutMS / 1000);
        SynchronousProcess::stopProcess(proc);
        return false;
    }
    if (proc.exitStatus() != QProcess::NormalExit) {
        *errorMessage = QCoreApplication::translate("ProjectExplorer::BuildableHelperLibrary",
                                                    "The process crashed.");
        return false;
    }
    const QString stdOutS = QString::fromLocal8Bit(stdOut);
    if (!ignoreNonNullExitCode && proc.exitCode() != 0) {
            *errorMessage = QCoreApplication::translate("ProjectExplorer::BuildableHelperLibrary",
                                                        "The process returned exit code %1:\n%2").
                                                         arg(proc.exitCode()).arg(stdOutS);
        return false;
    }
    output->append(stdOutS);
    return true;
}

// Run a build process with merged stdout/stderr and qWarn about errors.
static bool runBuildProcess(QProcess &proc,
                            const QString &binary,
                            const QStringList &args,
                            int timeoutMS,
                            bool ignoreNonNullExitCode,
                            QString *output, QString *errorMessage)
{
    const bool rc = runBuildProcessI(proc, binary, args, timeoutMS, ignoreNonNullExitCode, output, errorMessage);
    if (!rc) {
        // Fail - reformat error.
        QString cmd = binary;
        if (!args.isEmpty()) {
            cmd += QLatin1Char(' ');
            cmd += args.join(QString(QLatin1Char(' ')));
        }
        *errorMessage =
                QCoreApplication::translate("ProjectExplorer::BuildableHelperLibrary",
                                            "Error running '%1' in %2: %3").
                                            arg(cmd, proc.workingDirectory(), *errorMessage);
        qWarning("%s", qPrintable(*errorMessage));
    }
    return rc;
}


bool BuildableHelperLibrary::buildHelper(const QString &helperName, const QString &proFilename,
                                         const QString &directory, const QString &makeCommand,
                                         const QString &qmakeCommand, const QString &mkspec,
                                         const Utils::Environment &env, const QString &targetMode,
                                         QString *output, QString *errorMessage)
{
    const QChar newline = QLatin1Char('\n');
    // Setup process
    QProcess proc;
    proc.setEnvironment(env.toStringList());
    proc.setWorkingDirectory(directory);
    proc.setProcessChannelMode(QProcess::MergedChannels);

    output->append(QCoreApplication::translate("ProjectExplorer::BuildableHelperLibrary",
                                          "Building helper library '%1' in %2\n").arg(helperName, directory));
    output->append(newline);

    const QString makeFullPath = env.searchInPath(makeCommand);
    if (QFileInfo(directory + QLatin1String("/Makefile")).exists()) {
        if (makeFullPath.isEmpty()) {
            *errorMessage = QCoreApplication::translate("ProjectExplorer::DebuggingHelperLibrary",
                                                       "%1 not found in PATH\n").arg(makeCommand);
            return false;
        }
        const QString cleanTarget = QLatin1String("distclean");
        output->append(QCoreApplication::translate("ProjectExplorer::BuildableHelperLibrary",
                                                   "Running %1 %2...\n").arg(makeFullPath, cleanTarget));
        if (!runBuildProcess(proc, makeFullPath, QStringList(cleanTarget), 30000, true, output, errorMessage))
            return false;
    }
    output->append(newline);
    output->append(QCoreApplication::translate("ProjectExplorer::BuildableHelperLibrary", "Running %1 ...\n").arg(qmakeCommand));

    QStringList qMakeArgs;
    if (!targetMode.isEmpty())
        qMakeArgs << targetMode;
    if (!mkspec.isEmpty())
        qMakeArgs << QLatin1String("-spec") << mkspec;
    qMakeArgs << proFilename;
    if (!runBuildProcess(proc, qmakeCommand, qMakeArgs, 30000, false, output, errorMessage))
        return false;
    output->append(newline);
    if (makeFullPath.isEmpty()) {
        *errorMessage = QCoreApplication::translate("ProjectExplorer::BuildableHelperLibrary", "%1 not found in PATH\n").arg(makeCommand);
        return false;
    }
    output->append(QCoreApplication::translate("ProjectExplorer::BuildableHelperLibrary", "Running %1 ...\n").arg(makeFullPath));
    if (!runBuildProcess(proc, makeFullPath, QStringList(), 120000, false, output, errorMessage))
        return false;
    return true;
}

bool BuildableHelperLibrary::getHelperFileInfoFor(const QStringList &validBinaryFilenames,
                                                  const QString &directory, QFileInfo* info)
{
    if (!info)
        return false;

    foreach(const QString &binaryFilename, validBinaryFilenames) {
        info->setFile(directory + binaryFilename);
        if (info->exists())
            return true;
    }

    return false;
}

QString BuildableHelperLibrary::byInstallDataHelper(const QString &mainFilename,
                                                    const QStringList &installDirectories,
                                                    const QStringList &validBinaryFilenames)
{
    QDateTime sourcesModified = QFileInfo(mainFilename).lastModified();
    // We pretend that the lastmodified of gdbmacros.cpp is 5 minutes before what the file system says
    // Because afer a installation from the package the modified dates of gdbmacros.cpp
    // and the actual library are close to each other, but not deterministic in one direction
    sourcesModified = sourcesModified.addSecs(-300);

    // look for the newest helper library in the different locations
    QString newestHelper;
    QDateTime newestHelperModified = sourcesModified; // prevent using one that's older than the sources
    QFileInfo fileInfo;
    foreach(const QString &installDirectory, installDirectories) {
        if (getHelperFileInfoFor(validBinaryFilenames, installDirectory, &fileInfo)) {
            if (fileInfo.lastModified() > newestHelperModified) {
                newestHelper = fileInfo.filePath();
                newestHelperModified = fileInfo.lastModified();
            }
        }
    }
    return newestHelper;
}

} // namespace Utils