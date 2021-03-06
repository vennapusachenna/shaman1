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

#include "UpdateDbDialog.h"

#include <aqpm/Backend.h>

#include <QDebug>

using namespace std;
using namespace Aqpm;

UpdateDbDialog::UpdateDbDialog(QWidget *parent)
        : QDialog(parent),
        actionDone(0),
        updated(false),
        errorsOccourred(false)
{
    setupUi(this);
    setWindowModality(Qt::WindowModal);
    setAttribute(Qt::WA_DeleteOnClose);

    connect(Backend::instance(), SIGNAL(dbStatusChanged(const QString&, int)),
            SLOT(updateLabel(const QString&, int)));
    connect(Backend::instance(), SIGNAL(dbQty(const QStringList&)), SLOT(createWidgets(const QStringList&)));
    connect(Backend::instance(), SIGNAL(streamDlProg(const QString&, int, int, int, int, int)),
            SLOT(updateDlBar()));
}

UpdateDbDialog::~UpdateDbDialog()
{
}

void UpdateDbDialog::updateLabel(const QString &repo, int action)
{
    qDebug() << "Signal received";

    /* Ok, you need to read createWidget first.
     * When we are here, first of all we obtain the current label,
     * that is at the index pointed by actionDone.
     */

    QLabel *toInsert = labelList.at(actionDone);

    /* This function is triggered by a slot from AlpmHandler. action
     * holds the action that the library is processing. Let's see.
     */

    switch (action) {
    case Globals::Checking:
        // The Database is being checked
        toInsert->setPixmap(QIcon(":/Icons/icons/edit-redo.png").pixmap(22));
        break;
    case Globals::Downloading:
        // The Database is being downloaded (Outdated, see updateDlBar instead)
        toInsert->setPixmap(QIcon(":/Icons/icons/view-refresh.png").pixmap(22));
        break;
    case Globals::Updating:
        // The Database is being installed (this action is usually so quick that this icon is not even shown)
        toInsert->setPixmap(QIcon(":/Icons/icons/edit-redo.png").pixmap(22));
        break;
    case Globals::Updated:
        toInsert->setPixmap(QIcon(":/Icons/icons/dialog-ok-apply.png").pixmap(22));
        setUpdated(repo);
        updateTotalProg();
        break;
    case Globals::Unchanged:
        // The Database has been processed successfully
        toInsert->setPixmap(QIcon(":/Icons/icons/dialog-ok-apply.png").pixmap(22));
        updateTotalProg();
        break;
    case Globals::DatabaseError:
        // There was an error updating the database
        toInsert->setPixmap(QIcon(":/Icons/icons/edit-delete.png").pixmap(22));
        updateTotalProg();
        errorsOccourred = true;
        break;
    default:
        break;
    }
}

void UpdateDbDialog::setUpdated(const QString &dbname)
{
    updated = true;
    emit updateRepo(dbname);
}

bool UpdateDbDialog::dbHasBeenUpdated()
{
    return updated;
}

bool UpdateDbDialog::anyErrors()
{
    return errorsOccourred;
}

void UpdateDbDialog::updateTotalProg()
{
    actionDone++;

    int actd = actionDone * 100;

    if (totalAction != 0)
        emit pBar(actd / totalAction);
}

void UpdateDbDialog::updateDlBar()
{
    QLabel *toInsert = labelList.at(actionDone);

    // The database is being downloaded.
    toInsert->setPixmap(QIcon(":/Icons/icons/view-refresh.png").pixmap(22));
}

void UpdateDbDialog::doAction()
{
    updatedRepos.clear();
    connect(Backend::instance(), SIGNAL(operationFinished(bool)), this, SLOT(scopeEnded()));
    Backend::instance()->updateDatabase();
    qDebug() << "Dialog released";
}

void UpdateDbDialog::scopeEnded()
{
    for (int i = 0; i < labelList.size(); ++i)
        delete labelList.at(i);

    emit killMe();

    close();
}

void UpdateDbDialog::createWidgets(const QStringList &list)
{
    qDebug() << "Creating Widgets:" << list;

    /* This is (cronologically) the first function called, and it is triggered
     * from a signal coming from AlpmHandler, that gives us the list of the
     * Databases that we are going to sync, already ordered.
     */

    totalAction = list.size();

    for (int i = 0; i < list.size(); ++i) {
        /* Each cycle dynamically creates a pair of labels, the first one contains
         * the name of the Database, so we simply show it. The second holds the icon.
         * Since we'll need to edit the icons during the operation, we will store the
         * pointers to the label in a QList, labelList. You'll see how this thing works in
         * the next function called, updateLabel
         */

        QLabel *labelDb = new QLabel(this);
        QLabel *labelStatus = new QLabel(this);
        labelStatus->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
        labelDb->setText(QString("<b>%1</b>").arg(list.at(i)));
        labelStatus->setPixmap(QIcon(":/Icons/icons/view-history.png").pixmap(22));

        gridLayout->addWidget(labelDb, i + 1, 0);
        gridLayout->addWidget(labelStatus, i + 1, 1);

        labelList.append(labelStatus);

    }
}

QStringList UpdateDbDialog::getUpdatedRepos()
{
    return updatedRepos;
}
