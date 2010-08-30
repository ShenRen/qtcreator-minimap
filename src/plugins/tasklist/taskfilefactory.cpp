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

#include "taskfilefactory.h"

#include "taskfile.h"

#include <projectexplorer/projectexplorer.h>
#include <coreplugin/icore.h>
#include <coreplugin/filemanager.h>

using namespace TaskList::Internal;

// --------------------------------------------------------------------------
// TaskFileFactory
// --------------------------------------------------------------------------

TaskFileFactory::TaskFileFactory(QObject * parent) :
    Core::IFileFactory(parent),
    m_mimeTypes(QStringList() << QLatin1String("text/x-tasklist"))
{ }

TaskFileFactory::~TaskFileFactory()
{ }

QStringList TaskFileFactory::mimeTypes() const
{
    return m_mimeTypes;
}

QString TaskFileFactory::id() const
{
    return QLatin1String("ProjectExplorer.TaskFileFactory");
}

QString TaskFileFactory::displayName() const
{
    return tr("Task file reader");
}

Core::IFile *TaskFileFactory::open(const QString &fileName)
{
    ProjectExplorer::Project * context =
        ProjectExplorer::ProjectExplorerPlugin::instance()->currentProject();
    return open(context, fileName);
}

Core::IFile *TaskFileFactory::open(ProjectExplorer::Project *context, const QString &fileName)
{
    TaskFile *file = new TaskFile(this);
    file->setContext(context);

    if (!file->open(fileName)) {
        delete file;
        return 0;
    }

    m_openFiles.append(file);

    // Register with filemanager:
    Core::ICore::instance()->fileManager()->addFile(file);

    return file;
}

void TaskFileFactory::closeAllFiles()
{
    foreach(Core::IFile *file, m_openFiles)
        file->deleteLater();
    m_openFiles.clear();
}