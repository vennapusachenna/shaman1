/***************************************************************************
 *   Copyright (C) 2008 by Lukas Appelhans                                 *
 *   l.appelhans@gmx.de                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 ***************************************************************************/

#ifndef CONFIGDIALOG_H
#define CONFIGDIALOG_H

#include "ui_configDialog.h"
#include <QThread>
#include <QProcess>
#include <QPointer>

class ConfigDialog : public QDialog, public Ui::ConfigDialog
{

    Q_OBJECT

public:
    explicit ConfigDialog(QWidget *parent = 0);
    ~ConfigDialog();
    bool doDbUpdate();

signals:
    void setProxy();

private slots:
    void changeWidget(int position);
    void openAddDialog();
    void openEditDialog();
    void removeThirdParty();
    void performManteinanceAction();
    void mantProgress(const QString &progress);
    void maintenancePerformed(bool success);
    void saveConfiguration();
    void addMirror();
    void addKDEModMirror();
    void obfuscateSupfiles(bool state);
    void obfuscateDBUpdate(bool state);
    void obfuscateDBUpdateAt(bool state);
    void obfuscateRSSUpdate(bool state);

private:
    void setupRepos();
    void setupGeneral();
    void setupPacman();
    void setupABS();
    void setupAdvanced();
    void saveSettings();

private:
    QPointer<QDialog> addDialog;
    int m_currentMaint;
    bool upDb;
};

#endif /*CONFIGDIALOG_H*/
