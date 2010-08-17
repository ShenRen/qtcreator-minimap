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

#ifndef S60DEPLOYCONFIGURATION_H
#define S60DEPLOYCONFIGURATION_H

#include <projectexplorer/deployconfiguration.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/toolchain.h>

namespace Qt4ProjectManager {
class QtVersion;

namespace Internal {
class Qt4ProFileNode;
class Qt4Target;
class S60DeployConfigurationFactory;
class S60DeviceRunConfiguration;

class S60DeployConfiguration : public ProjectExplorer::DeployConfiguration
{
    Q_OBJECT
    friend class S60DeployConfigurationFactory;

public:
    S60DeployConfiguration(ProjectExplorer::Target *parent);
    virtual ~S60DeployConfiguration();

    bool isEnabled(ProjectExplorer::BuildConfiguration *configuration) const;

    ProjectExplorer::DeployConfigurationWidget *configurationWidget() const;

    QString targetName() const;
    const QtVersion *qtVersion() const;
    Qt4Target *qt4Target() const;
    ProjectExplorer::ToolChain::ToolChainType toolChainType() const;

    QString serialPortName() const;
    void setSerialPortName(const QString &name);

    char installationDrive() const;
    void setInstallationDrive(char drive);

    bool silentInstall() const;
    void setSilentInstall(bool silent);

    QStringList signedPackages() const;
    QString appSignedPackage() const;
    QStringList packageFileNamesWithTargetInfo() const;
    QStringList packageTemplateFileNames() const;
    QString appPackageTemplateFileName() const;
    QString localExecutableFileName() const;

    QVariantMap toMap() const;

signals:
    void targetInformationChanged();
    void serialPortNameChanged();

private slots:
    void updateActiveBuildConfiguration(ProjectExplorer::BuildConfiguration *buildConfiguration);
    void proFileUpdate(Qt4ProjectManager::Internal::Qt4ProFileNode *pro);

protected:
    S60DeployConfiguration(ProjectExplorer::Target *parent, S60DeployConfiguration *source);
    virtual bool fromMap(const QVariantMap &map);

private:
    void ctor();
    S60DeviceRunConfiguration* s60DeviceRunConf() const;
    bool runSmartInstaller() const;
    QString symbianPlatform() const;
    QString symbianTarget() const;
    bool isDebug() const;

private:
    ProjectExplorer::BuildConfiguration *m_activeBuildConfiguration;
    QString m_serialPortName;

    char m_installationDrive;
    bool m_silentInstall;
};

class S60DeployConfigurationFactory : public ProjectExplorer::DeployConfigurationFactory
{
    Q_OBJECT

public:
    explicit S60DeployConfigurationFactory(QObject *parent = 0);
    ~S60DeployConfigurationFactory();

    bool canCreate(ProjectExplorer::Target *parent, const QString &id) const;
    ProjectExplorer::DeployConfiguration *create(ProjectExplorer::Target *parent, const QString &id);
    bool canRestore(ProjectExplorer::Target *parent, const QVariantMap &map) const;
    ProjectExplorer::DeployConfiguration *restore(ProjectExplorer::Target *parent, const QVariantMap &map);
    bool canClone(ProjectExplorer::Target *parent, ProjectExplorer::DeployConfiguration *source) const;
    ProjectExplorer::DeployConfiguration *clone(ProjectExplorer::Target *parent, ProjectExplorer::DeployConfiguration *source);

    QStringList availableCreationIds(ProjectExplorer::Target *parent) const;
    // used to translate the ids to names to display to the user
    QString displayNameForId(const QString &id) const;
};

} // namespace Internal
} // namespace Qt4ProjectManager

#endif // S60DEPLOYCONFIGURATION_H