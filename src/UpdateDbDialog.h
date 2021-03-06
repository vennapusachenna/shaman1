/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi                                    *
 *   drf54321@yahoo.it                                                     *
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

#ifndef UPDATEDBDIALOG_H
#define UPDATEDBDIALOG_H

#include <iostream>
#include "ui_dbUpdateDialog.h"

#include <QObject>
#include <QDialog>

class UpdateDbDialog : public QDialog, private Ui::dbUpdateDialog
{
    Q_OBJECT

public:
    explicit UpdateDbDialog(QWidget *parent = 0);
    ~UpdateDbDialog();

    bool dbHasBeenUpdated();
    bool anyErrors();

public slots:
    void doAction();
    QStringList getUpdatedRepos();

private slots:
    void updateLabel(const QString &repo, int action);
    //void updateDlProg(float bytetotal, float bytedled, float speed);
    void createWidgets(const QStringList &list);
    void updateTotalProg();
    void setUpdated(const QString &dbname);
    void scopeEnded();
    void updateDlBar();

signals:
    void killMe();
    void updateRepo(const QString &dbname);
    void pBar(int val);

private:
    int currentAction;
    char *currentRepo;
    int actionDone;
    int totalAction;
    bool updated;
    bool errorsOccourred;
    QList<QLabel *> labelList;
    QStringList updatedRepos;

};

#endif /*UPDATEDBDIALOG_H*/
