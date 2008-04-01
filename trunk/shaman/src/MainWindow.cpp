/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi                                    *
 *   drf54321@yahoo.it                                                     *
 *   Copyright (C) 2008 by Lukas Appelhans				   				   *
 *   l.appelhans@gmx.de							   						   *
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
#include "MainWindow.h"
#include "callbacks.h"
#include "UpdateDbDialog.h"
#include "SysUpgradeDialog.h"
#include "QueueDialog.h"
#include "configDialog.h"
#include "BuildingDialog.h"
#include "EditPBuild.h"
#include "ABSHandler.h"
#include "BuildingHandler.h"
#include "ShamanTrayIcon.h"
#include "ui_aboutDialog.h"
#include "ui_authDialog.h"
#include "shamanadaptor.h"
#include "ReviewQueueDialog.h"
#include "ArchLinuxNewsReader.h"
#include "NewsViewer.h"
#include "LogViewer.h"
#include "Authenticator.h"
#include "ShamanDialog.h"

#include <iostream>
#include <alpm.h>
#include <sys/types.h>

#include <QMenu>
#include <QString>
#include <QListWidgetItem>
#include <QDebug>
#include <QMessageBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QTimer>
#include <QCloseEvent>
#include <QSettings>
#include <QWaitCondition>
#include <QMovie>
#include <QShortcut>
#include <QNetworkProxy>
#include <QMutex>

extern CallBacks CbackReference;
extern QMutex mutex;
extern QWaitCondition wCond;
extern Authenticator_Callback athCback;
extern struct pam_response *reply;

MainWindow::MainWindow(AlpmHandler *handler, QMainWindow *parent)
: QMainWindow(parent),
queueDl(),
currentpkgs(0),
aHandle(handler),
dbdialog(),
upDl(),
configDialog(),
qUi(),
bHandler(),
nView(),
lView(),
reviewQueue(),
upActive(false),
revActive(false),
dbActive(false),
quiActive(false),
turnOffSys(false)
{
	setupUi(this);
	addDockWidget(Qt::LeftDockWidgetArea, repoDockWidget);
	addDockWidget(Qt::BottomDockWidgetArea, pkgDockWidget);

	new ShamanAdaptor(this);

	QDBusConnection dbus = QDBusConnection::systemBus();

	dbus.registerObject("/Shaman", this);

	trayicon = new ShamanTrayIcon(this, aHandle);

	newsReader = new ArchLinuxNewsReader();

	newsReader->setUpdateInterval();

	qDebug() << "Shaman registered on the System Bus as" << dbus.baseService();

	if(!dbus.registerService("org.archlinux.shaman"))
		qWarning() << "Failed to register alias Service on DBus";
	else
		qDebug() << "Service org.archlinux.shaman successfully exported on the System Bus.";

	pkgsViewWG->setContextMenuPolicy(Qt::CustomContextMenu);
	repoList->setContextMenuPolicy(Qt::CustomContextMenu);
	pkgsViewWG->hideColumn(7);
	pkgsViewWG->hideColumn(8);
	resize(500, size().height());
	pkgsViewWG->headerItem()->setIcon(0, QIcon(":/Icons/icons/user-invisible.png"));
	pkgsViewWG->headerItem()->setText(0, QString()); //Set text to ""
	pkgsViewWG->setColumnWidth(0, 30);//TODO: Remember column-width
	pkgsViewWG->headerItem()->setIcon(2, QIcon(":/Icons/icons/applications-development.png"));
	pkgsViewWG->headerItem()->setText(2, QString());
	pkgsViewWG->setColumnWidth(2, 30);

	nameDescBox->addItem(tr("Name"));
	nameDescBox->addItem(tr("Description"));

	connect(&CbackReference, SIGNAL(questionStreamed(const QString&)), this, SLOT(streamTransQuestion(const QString&)));
	connect(actionUpdate_Database, SIGNAL(triggered()), SLOT(doDbUpdate()));
	connect(pkgsViewWG, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(showPkgsViewContextMenu()));
	connect(repoList, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(showRepoViewContextMenu()));
	connect(actionProcess_Queue, SIGNAL(triggered()), SLOT(widgetQueueToAlpmQueue()));
	connect(switchToRepo, SIGNAL(clicked()), SLOT(populateRepoColumn()));
	connect(switchToGrps, SIGNAL(clicked()), SLOT(populateGrpsColumn()));
	connect(removeButton, SIGNAL(clicked()), SLOT(removePackage()));
	connect(completeRemoveButton, SIGNAL(clicked()), SLOT(completeRemovePackage()));
	connect(cancelButton, SIGNAL(clicked()), SLOT(cancelAction()));
	connect(actionUpgrade_System, SIGNAL(triggered()), SLOT(fullSysUpgrade()));
	connect(packageSwitchCombo, SIGNAL(currentIndexChanged(int)), SLOT(refinePkgView()));
	connect(nameDescBox, SIGNAL(currentIndexChanged(int)), SLOT(refinePkgView()));
	connect(searchLine, SIGNAL(textChanged(const QString&)), SLOT(refinePkgView()));
	connect(actionPacman_Preferences, SIGNAL(triggered()), SLOT(showSettings()));
	connect(trayicon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), SLOT(systrayActivated(QSystemTrayIcon::ActivationReason)));
	connect(trayicon, SIGNAL(startDbUpdate()), SLOT(doDbUpdate()));
	connect(trayicon, SIGNAL(upgradePkgs()), SLOT(startUpgrading()));
	connect(this, SIGNAL(startTimer()), trayicon, SLOT(startTimer()));
	connect(this, SIGNAL(stopTimer()), trayicon, SLOT(stopTimer()));
	connect(actionQuit, SIGNAL(triggered()), SLOT(quitApp()));
	connect(actionAbout, SIGNAL(triggered()), SLOT(showAboutDialog()));
	connect(actionInstall_Package_From_File, SIGNAL(triggered()), SLOT(getPackageFromFile()));
	connect(actionUpdate_ABS_Tree, SIGNAL(triggered()), SLOT(updateABSTree()));
	connect(actionBuild_and_Install_Selected, SIGNAL(triggered()), SLOT(initSourceQueue()));
	connect(actionCancel_all_actions, SIGNAL(triggered()), SLOT(cancelAllActions()));
	connect(actionReadNews, SIGNAL(triggered()), SLOT(openNewsDialog()));
	connect(actionViewLog, SIGNAL(triggered()), SLOT(openLogViewer()));
	connect(aHandle, SIGNAL(streamDbUpdatingStatus(const QString&,int)), SIGNAL(streamDbUpdatingStatus(const QString&,int)));
	connect(&CbackReference, SIGNAL(streamTransDlProg(const QString&,int,int,int,int)), 
			SIGNAL(streamTransDlProg(const QString&,int,int,int,int)));
	connect(newsReader, SIGNAL(newItems()), trayicon, SLOT(newNewsAvailable()));
	connect(newsReader, SIGNAL(fetchingFailed()), trayicon, SLOT(newsFetchingFailed()));
	connect(&athCback, SIGNAL(passwordRequired(int)), SLOT(showAuthDialog(int)));

	QSettings *settings = new QSettings();

	if(settings->contains("gui/widgetstate"))
		restoreState(settings->value("gui/widgetstate").toByteArray());

	settings->deleteLater();

	setProxy();

	stBar = new QStatusBar(this);
	setStatusBar(stBar);

	QShortcut *clearLineEdit = new QShortcut(tr("Esc"), searchLine, SLOT(clear()));
	Q_UNUSED(clearLineEdit);

	return;
}

MainWindow::~MainWindow()
{
	return;
}

void MainWindow::streamReadySignal()
{
	emit shamanReady();
}

void MainWindow::quitApp()
{
	QSettings *settings = new QSettings();

	settings->setValue("gui/size", size());
	settings->setValue("gui/pos", pos());
	settings->setValue("gui/widgetstate", saveState());

	settings->deleteLater();

	emit aboutToQuit();
}

void MainWindow::closeEvent(QCloseEvent *evt)
{
	ShamanDialog::popupDialogDontShow(QString(tr("Shaman - Reducing To Tray")), 
			QString(tr("Shaman will keep running in the system tray.\nTo close it, click Quit in the file menu "
					"or in the tray icon context menu.\nWhile in the System Tray, Shaman will update your Databases\nat a regular"
					" interval and notify you about available upgrades.\nYou can change this behaviour in Settings.")), 
					"gui/confirmquit", this, ShamanProperties::InformationDialog);
	
	emit startTimer();

	evt->accept();
}

void MainWindow::removePackagesView()
{
	QTreeWidgetItem *itm;

	while((itm = pkgsViewWG->takeTopLevelItem(0)) != NULL)
		delete(itm);

}

void MainWindow::removeRepoColumn()
{
	QListWidgetItem *itm;

	while((itm = repoList->takeItem(0)) != 0)
		delete itm;

	disconnect(repoList, SIGNAL(itemPressed(QListWidgetItem*)), 0, 0);
}

bool MainWindow::populatePackagesView()
{
	alpm_list_t *databases;
	int count = 0;

	pkgsViewWG->setSortingEnabled(false);

	disconnect(pkgsViewWG, SIGNAL(itemSelectionChanged()), 0, 0);
	disconnect(PkgInfos, SIGNAL(currentChanged(int)), 0, 0);

	removePackagesView();

	databases = aHandle->getAvailableRepos();

	databases = alpm_list_first(databases);

	while(databases != NULL)
	{
		pmdb_t *dbcrnt = (pmdb_t *)alpm_list_getdata(databases);

		currentpkgs = alpm_db_getpkgcache(dbcrnt);
		count = 0;

		while(currentpkgs != NULL)
		{
			pmpkg_t *pkg = (pmpkg_t *)alpm_list_getdata(currentpkgs);
			QTreeWidgetItem *item = new QTreeWidgetItem(pkgsViewWG);
			alpm_list_t *grps = (alpm_list_t *)alpm_pkg_get_groups(pkg);
			QString grStr("");

			if(aHandle->isInstalled(pkg))
			{
				//item->setText(0, tr("Installed"));
				item->setIcon(0, QIcon(":/Icons/icons/user-online.png"));
				item->setText(3, aHandle->getPackageVersion(alpm_pkg_get_name(pkg), "local"));
			}
			else
			{
				//item->setText(0, tr("Not Installed"));
				item->setIcon(0, QIcon(":/Icons/icons/user-offline.png"));
				item->setText(3, alpm_pkg_get_version(pkg));
			}

			item->setText(1, alpm_pkg_get_name(pkg));
			item->setText(5, alpm_db_get_name(dbcrnt));
			item->setText(4, formatSize(aHandle->getPackageSize(item->text(1), item->text(5))));
			item->setText(7, alpm_pkg_get_desc(pkg));

			while(grps != NULL)
			{
				grStr.append(" ");

				grStr.append((char *)alpm_list_getdata(grps));
				grps = alpm_list_next(grps);
			}
			grStr.append(" ");

			item->setText(6, grStr);

			currentpkgs = alpm_list_next(currentpkgs);

			count++;
		}

		databases = alpm_list_next(databases);
	}

	databases = alpm_list_first(databases);

	alpm_list_t *locPkg = aHandle->getInstalledPackages();

	while(locPkg != NULL)
	{
		pmpkg_t *pkg = (pmpkg_t *)alpm_list_getdata(locPkg);
		QList<QTreeWidgetItem*> list = pkgsViewWG->findItems(alpm_pkg_get_name(pkg), Qt::MatchExactly, 1);
		if (list.isEmpty())
		{
			QTreeWidgetItem *item = new QTreeWidgetItem(pkgsViewWG);
			item->setIcon(0, QIcon(":/Icons/icons/user-online.png"));
			item->setText(1, alpm_pkg_get_name(pkg));
			item->setText(3, alpm_pkg_get_version(pkg));
			item->setText(7, alpm_pkg_get_desc(pkg));
			item->setText(5, "local");
			item->setText(4, formatSize(aHandle->getPackageSize(item->text(1), item->text(5))));
		}
		else
			list.first()->setText(3, alpm_pkg_get_version(aHandle->getPackageFromName(list.first()->text(1), "local")));

		locPkg = alpm_list_next(locPkg);
	}

	QStringList upgrds = aHandle->getUpgradeablePackages();
	foreach (QString pac, upgrds)
	{
		QTreeWidgetItem *item = pkgsViewWG->findItems(pac, Qt::MatchExactly, 1).first();
		if (item)
		{
			item->setText(8, tr("Upgrade"));
			item->setIcon(2, QIcon(":/Icons/icons/list-add.png"));
		}
	}

	pkgsViewWG->sortItems(1, Qt::AscendingOrder);
	pkgsViewWG->setSortingEnabled(true);//Enable sorting *after* inserting :D

	connect(pkgsViewWG, SIGNAL(itemSelectionChanged()), this,
			SLOT(itemChanged()));

	connect(PkgInfos, SIGNAL(currentChanged(int)), SLOT(showPkgInfo()));

	return true;
}

void MainWindow::populateRepoColumn()
{
	qDebug() << "Populating Repo column";

	switchToGrps->setChecked(false);
	repoDockWidget->setWindowTitle(tr("Repositories"));
	QStringList list = aHandle->getAvailableReposNames();

	removeRepoColumn();

	QListWidgetItem *item = new QListWidgetItem(repoList);

	item->setText(tr("All Repositories"));
	item->setSelected(true);

	foreach(QString dbname, list)
	{
		QListWidgetItem *item = new QListWidgetItem(repoList);

		item->setText(dbname);
	}


	QListWidgetItem *itm = new QListWidgetItem(repoList);

	itm->setText(tr("Local Packages"));

	connect(repoList, SIGNAL(itemPressed(QListWidgetItem*)), this,
			SLOT(refinePkgView()));
}

void MainWindow::populateGrpsColumn()
{
	switchToRepo->setChecked(false);
	repoDockWidget->setWindowTitle(tr("Package Groups"));
	QStringList grps = aHandle->getPackageGroups();

	removeRepoColumn();

	for(int i = 0; i < grps.size(); ++i)
	{
		QListWidgetItem *item = new QListWidgetItem(repoList);

		item->setText(grps.at(i));
	}

	repoList->sortItems(Qt::AscendingOrder);

	QListWidgetItem *item = new QListWidgetItem();

	item->setText(tr("All Groups"));
	item->setSelected(true);
	repoList->insertItem(0, item);

	connect(repoList, SIGNAL(itemPressed(QListWidgetItem*)), this,
			SLOT(refinePkgView()));
}

void MainWindow::refinePkgView()
{
	qDebug() << "refinePkgView";

	QList<QTreeWidgetItem*> list = pkgsViewWG->findItems(QString(), Qt::MatchRegExp | Qt::MatchWildcard);

	// Now first: What do we need to refine in the right column?
	if (list.isEmpty() && list.count() >= 0)
		return;

	if (repoList->selectedItems().count() > 0)
	{
		if((repoList->selectedItems().at(0)->text().compare(tr("All Repositories")) &&
				repoList->selectedItems().at(0)->text().compare(tr("All Groups"))) &&
				!repoList->selectedItems().isEmpty())
		{
			if(!repoList->findItems(QString(tr("All Repositories")),
					(Qt::MatchFlags)Qt::MatchExactly).isEmpty())
				// First time, so don't check anything.
				if(repoList->selectedItems().at(0)->text().compare(tr("Local Packages")))
					list = pkgsViewWG->findItems(repoList->selectedItems().at(0)->text(), 
							(Qt::MatchFlags)Qt::MatchExactly, 5);
				else
					list = pkgsViewWG->findItems("local", 
							(Qt::MatchFlags)Qt::MatchExactly, 5);
			else
			{
				list = pkgsViewWG->findItems(repoList->selectedItems().at(0)->text(), 
						(Qt::MatchFlags)Qt::MatchExactly, 6);
				QString tmp = repoList->selectedItems().at(0)->text();
				tmp.append(" ");
				tmp.prepend(" ");
				list += pkgsViewWG->findItems(tmp, 
						(Qt::MatchFlags)Qt::MatchContains, 6);
			}
		}
		else
		{
			for(int i = 0; i < pkgsViewWG->topLevelItemCount(); ++i)
			{
				list += pkgsViewWG->topLevelItem(i);
			}
		}
	}
	qDebug() << "The left TextBox is over, let's do the ComboBox";
	if(!packageSwitchCombo->currentIndex() == 0)
	{
		qDebug() << "Hehe, we should not show all Packages";
		if (packageSwitchCombo->currentText() == tr("Installed packages"))
		{
			//qDebug() << "We should only show the installed packages";
			foreach (QTreeWidgetItem *item, list)
			{
				//qDebug() << "Checking for installed packages" + item->text(2);
				if (!aHandle->isInstalled(item->text(1)))
					list.removeAt(list.indexOf(item));
			}
		}
		if (packageSwitchCombo->currentText() == tr("Not installed packages"))
		{
			foreach (QTreeWidgetItem *item, list)
			{
				//qDebug() << "Checking for installed packages" + item->text(2);
				if (aHandle->isInstalled(item->text(1)))
					list.removeAt(list.indexOf(item));
			}
		}
		if (packageSwitchCombo->currentText() == tr("Upgradeable packages"))
		{
			QStringList stringList = aHandle->getUpgradeablePackages();
			foreach (QTreeWidgetItem *item, list)
			{
				//qDebug() << "Checking for upgradeable packages" + item->text(2);
				if (!aHandle->isInstalled(item->text(1)) || !stringList.contains(item->text(1)))
					list.removeAt(list.indexOf(item));
			}
		}
		if (packageSwitchCombo->currentText() == tr("Packages in Queue"))
		{
			foreach (QTreeWidgetItem *item, list)
			{
				//qDebug() << "Checking for installed packages" + item->text(2);
				if (item->text(8).isEmpty())
					list.removeAt(list.indexOf(item));
			}
		}
	}
	else
		qDebug() << "Show all packages";

	if (!searchLine->text().isEmpty())
	{
		foreach (QTreeWidgetItem *item, list)
		{
			if(nameDescBox->currentText() == tr("Name"))
			{
				if (!item->text(1).contains(searchLine->text(), Qt::CaseInsensitive))
				{
					list.removeAt(list.indexOf(item));
				}
			}
			else if(nameDescBox->currentText() == tr("Description"))
			{
				if (!item->text(7).contains(searchLine->text(), Qt::CaseInsensitive))
				{
					list.removeAt(list.indexOf(item));
				}
			}
		}
	}


	/* When we are here, we have a list that contains the packages refined
	 * by all of our three components. And then we set that list to be
	 * visible. */

	for(int i = 0; i < pkgsViewWG->topLevelItemCount(); ++i)
	{
		QTreeWidgetItem *tmpitm = pkgsViewWG->topLevelItem(i);

		if(list.contains(tmpitm))
			tmpitm->setHidden(false);
		else
			tmpitm->setHidden(true);
	}
}

void MainWindow::itemChanged()
{
	if (pkgsViewWG->selectedItems().isEmpty())
		return;

	cancelButton->setDisabled(true);
	if (aHandle->isInstalled(pkgsViewWG->selectedItems().first()->text(1)))
	{
		removeButton->setEnabled(true);
		installButton->setEnabled(true);
		installButton->setText(tr("Mark for Reinstallation"));
		completeRemoveButton->setEnabled(true);
		disconnect(installButton, SIGNAL(clicked()), this, SLOT(installPackage()));
		connect(installButton, SIGNAL(clicked()), SLOT(reinstallPackage()));
	}
	if (!aHandle->isInstalled(pkgsViewWG->selectedItems().first()->text(1)))
	{
		removeButton->setDisabled(true);
		installButton->setEnabled(true);
		installButton->setText(tr("Mark for Installation"));
		completeRemoveButton->setDisabled(true);
		disconnect(installButton, SIGNAL(clicked()), this, SLOT(reinstallPackage()));
		connect(installButton, SIGNAL(clicked()), SLOT(installPackage()));
	}
	//if (pkgsViewWG->selectedItems().first()->text(1) == tr("Upgradeable"))

	if (pkgsViewWG->selectedItems().first()->text(8) == tr("Install"))
		installButton->setDisabled(true);
	if (pkgsViewWG->selectedItems().first()->text(8) == tr("Uninstall"))
		removeButton->setDisabled(true);
	if (pkgsViewWG->selectedItems().first()->text(8) == tr("Complete Uninstall"))
		completeRemoveButton->setDisabled(true);
	if (!pkgsViewWG->selectedItems().first()->text(8).isEmpty())
		cancelButton->setEnabled(true);

	showPkgInfo();
}

void MainWindow::showPkgInfo()
{
	if(pkgsViewWG->selectedItems().isEmpty() || pkgsViewWG->currentItem() == NULL)
		return;

	QString description;
	pmpkg_t *pkg = NULL;
	alpm_list_t *databases;
	pmdb_t *curdb = NULL;
	bool isLocal = false;

	databases = aHandle->getAvailableRepos();

	databases = alpm_list_first(databases);

	while(databases != NULL)
	{
		if(!strcmp(pkgsViewWG->currentItem()->text(5).toAscii().data(),
				alpm_db_get_name((pmdb_t *)alpm_list_getdata(databases))))
		{
			curdb = (pmdb_t *)alpm_list_getdata(databases);
			break;
		}
		databases = alpm_list_next(databases);
	}

	pkg = alpm_db_get_pkg(curdb, pkgsViewWG->currentItem()->text(1).toAscii().data());

	if(PkgInfos->currentIndex() == 0)
	{
		if(!pkg)
		{
			/* Well, if pkg is NULL we're probably searching for something coming
			 * from the local database. */
			pkg = aHandle->getPackageFromName(pkgsViewWG->currentItem()->text(1).toAscii().data(), "local");
			isLocal = true;
		}

		databases = alpm_list_first(databases);

		QTreeWidgetItem *item = pkgsViewWG->selectedItems().first();
		description.append("<b>");
		description.append(alpm_pkg_get_name(pkg));
		description.append(" (");
		description.append(alpm_pkg_get_version(pkg));
		description.append(")</b><br><br>");
		description.append(alpm_pkg_get_desc(pkg));
		description.append("<br><br>");
		description.append("<b>" + tr("Status: ") + "</b> ");
		if (aHandle->isInstalled(item->text(1)))
			description.append(tr("Installed")); //FIXME: Icon!!!!
		else
			description.append(tr("Not installed"));
		if (aHandle->isInstalled(item->text(1)))
		{
			description.append("<br><b>" + tr("Local Version: ") + "</b> ");
			description.append(aHandle->getPackageVersion(alpm_pkg_get_name(pkg), "local"));
		}
		if(!isLocal)
		{
			description.append("<br><b>" + tr("Version in the Repository: ") + "</b> ");
			description.append(alpm_pkg_get_version(pkg));
		}

		if (!item->text(8).isEmpty())
		{
			description.append("<br><b>" + tr("Action: ") + "</b>");
			description.append(item->text(8));//FIXME: Icon!!!
		}

		pkgInfo->setHtml(description);
	}

	else if(PkgInfos->currentIndex() == 2)
	{
		dependenciesWidget->clear();
		foreach (QString dep, aHandle->getPackageDependencies(alpm_pkg_get_name(pkg) , pkgsViewWG->currentItem()->text(5)))
		{
			if (!dep.isEmpty())
				dependenciesWidget->addItem(dep);
		}
	}

	else if(PkgInfos->currentIndex() == 1)
	{
		filesWidget->clear();
		filesWidget->header()->hide();
		QStringList files = aHandle->getPackageFiles(pkgsViewWG->selectedItems().first()->text(1));
		foreach (QString file, files)
		{
			QStringList splitted = file.split("/");
			QTreeWidgetItem *parentItem = 0;
			foreach (QString spl, splitted)
			{
				if (spl.isEmpty())
					continue;
				if (parentItem)
				{
					qDebug() << "Yay, we have a parentItem";
					bool there = false;
					int j = parentItem->childCount();
					for (int i = 0;i != j; i++)
					{
						if (parentItem->child(i)->text(0) == spl)
						{
							there = true;
							parentItem = parentItem->child(i);
							continue;
						}
					}
					if (!there)
						parentItem->addChild(new QTreeWidgetItem(parentItem, (QStringList) spl));
				}
				else
				{
					QList<QTreeWidgetItem*> list = filesWidget->findItems(spl, Qt::MatchExactly);
					if (!list.isEmpty())
					{
						qDebug() << "Hehe we have the same item already found";
						parentItem = list.first();
					}
					else
					{
						filesWidget->insertTopLevelItem(0, new QTreeWidgetItem(filesWidget, (QStringList) spl));
					}
				}
			}
		}
	}
}

void MainWindow::doDbUpdate()
{
	dbdialog = new UpdateDbDialog(aHandle, this);
	dbActive = true;

	emit actionStatusChanged("dbUpdateStarted");

	if(isVisible())
		dbdialog->show();

	connect(dbdialog, SIGNAL(killMe()), this, SLOT(finishDbUpdate()));

	dbdialog->doAction();
}

void MainWindow::finishDbUpdate()
{
	if(!dbActive)
	{
		Q_ASSERT_X(1 == 2, "finishDbUpdate", "dbdialog seems not to be valid");
		return;
	}

	disconnect(dbdialog, 0,0,0);

	if(dbdialog->dbHasBeenUpdated())
	{
		populatePackagesView();
		populateRepoColumn();
		populateGrpsColumn();
	}

	qDebug() << "DB Update Finished";

	QStringList list(aHandle->getUpgradeablePackages());

	if(dbdialog->anyErrors())
	{
		emit actionStatusChanged("dbUpdateFinished");

		if(dbdialog->isVisible())
			ShamanDialog::popupDialog(tr("Error"), QString(tr("One or more Databases could not be updated.\nLast error reported was:\n%1"))
					.arg(alpm_strerrorlast()), this, ShamanProperties::ErrorDialog);
		else
			trayicon->showMessage(QString(tr("Database Update")), QString(tr("One or more Databases could "
					"not be updated.\nLast error reported was:\n%1")).arg(alpm_strerrorlast()));
	}
	else
	{
		emit actionStatusChanged("dbUpdateFinished");
		QSettings *settings = new QSettings();
		if(dbdialog->isHidden() && list.isEmpty() && settings->value("scheduledUpdate/updateDbShowNotify").toBool())
			trayicon->showMessage(QString(tr("Database Update")), QString(tr("Databases Updated Successfully")));
	}

	dbdialog->deleteLater();
	dbActive = false;

	if(list.isEmpty())
		return;

	if (list.contains("pacman"))
	{
		switch (ShamanDialog::popupQuestionDialog(QString(tr("Pacman Update")), QString(tr("Pacman can be upgraded. "
				"It is advised to process it alone\nto avoid version conflicts.\n"
				"Do you want to Upgrade Pacman now?")), this))
				{
				case QMessageBox::Yes:

					/* Ok, let's set up a special queue for Shaman. */

					cancelAllActions();
					installPackage("pacman");
					widgetQueueToAlpmQueue();

					break;
				case QMessageBox::No:
					/* Well, just pass by */
					break;
				default:
					// should never be reached
					break;
				}


	}
	else if (list.contains("shaman") || list.contains("kdemod4-shaman-svn"))
	{
		switch (ShamanDialog::popupQuestionDialog(QString(tr("Shaman Update")), QString(tr("Shaman can be upgraded. "
				"It is advised to process it alone\nto avoid version conflicts.\n"
				"Do you want to Upgrade Shaman now?")), this))
				{
				case QMessageBox::Yes:

					/* Ok, let's set up a special queue for Shaman. */

					cancelAllActions();
					
					if(list.contains("kdemod4-shaman-svn"))
						installPackage("kdemod4-shaman-svn");
					else
						installPackage("shaman");
					
					widgetQueueToAlpmQueue();

					break;
				case QMessageBox::No:
					/* Well, just pass by */
					break;
				default:
					// should never be reached
					break;
				}

	}
	else if(!list.isEmpty())
	{
		QSettings *settings = new QSettings();

		if(settings->value("scheduledUpdate/addupgradestoqueue").toBool())
			addUpgradeableToQueue();

		settings->deleteLater();
	}
}

void MainWindow::showPkgsViewContextMenu()
{
	if(pkgsViewWG->selectedItems().isEmpty())
		return;

	qDebug() << "Let's show a context menu";
	QTreeWidgetItem *item = pkgsViewWG->selectedItems().first();
	QMenu *menu = new QMenu(this);
	QAction *installAction = menu->addAction(QIcon(":/Icons/icons/list-add.png"), tr("Mark for Installation"));
	connect(installAction, SIGNAL(triggered()), SLOT(installPackage()));
	QAction *removeAction = menu->addAction(QIcon(":/Icons/icons/list-remove.png"), tr("Mark for Removal"));
	connect(removeAction, SIGNAL(triggered()), SLOT(removePackage()));
	QAction *upgradeAction = menu->addAction(QIcon(":/Icons/icons/edit-redo.png"), tr("Mark for Upgrade"));
	connect(upgradeAction, SIGNAL(triggered()), SLOT(upgradePackage()));
	QAction *cancelAction = menu->addAction(QIcon(":/Icons/icons/dialog-cancel.png"), tr("Cancel Action"));
	connect(cancelAction, SIGNAL(triggered()), SLOT(cancelAction()));

	if (aHandle->isInstalled(item->text(1)) && aHandle->getUpgradeablePackages().contains(item->text(1)))
	{
		installAction->setText(tr("Mark for Reinstallation"));
		removeAction->setDisabled(true);
	}
	else if (!aHandle->isInstalled(item->text(1)))
	{
		removeAction->setDisabled(true);
		upgradeAction->setDisabled(true);
	}
	else//Package is marked as installed
	{
		installAction->setText(tr("Mark for Reinstallation"));
		upgradeAction->setDisabled(true);
	}//FIXME: Add completeRemove-action

	if (item->text(8).isEmpty())
		cancelAction->setDisabled(true);
	else if (item->text(8) == tr("Install"))
		installAction->setDisabled(true);
	else if (item->text(8) == tr("Uninstall"))
		removeAction->setDisabled(true);
	else //Upgrade
		upgradeAction->setDisabled(true);


	menu->popup(QCursor::pos());
}

void MainWindow::showRepoViewContextMenu()
{
	if (repoList->selectedItems().isEmpty())
		return;

	//QListWidgetItem *item = repoList->selectedItems().first();
	QMenu *menu = new QMenu(this);
	QAction *installAction = menu->addAction(QIcon(":/Icons/icons/list-add.png"), tr("Mark all for installation"));
	connect(installAction, SIGNAL(triggered()), SLOT(installAllRepoPackages()));
	QAction *reinstallAction = menu->addAction(QIcon(":/Icons/icons/list-add.png"), tr("Mark all for reinstallation"));
	connect(reinstallAction, SIGNAL(triggered()), SLOT(reinstallAllRepoPackages()));
	QAction *removeAction = menu->addAction(QIcon(":/Icons/icons/list-remove.png"), tr("Mark all for removal"));
	connect(removeAction, SIGNAL(triggered()), SLOT(removeAllRepoPackages()));
	QAction *cancelAction = menu->addAction(QIcon(":/Icons/icons/dialog-cancel.png"), tr("Cancel all actions"));
	connect(cancelAction, SIGNAL(triggered()), SLOT(cancelAllRepoActions()));

	menu->popup(QCursor::pos());
}

void MainWindow::cancelAllActions()
{
	qDebug() << "Hehe let's remove all actions";
	foreach(QTreeWidgetItem *item, pkgsViewWG->findItems(QString(), Qt::MatchRegExp | Qt::MatchWildcard))
	{
		if (!item->text(8).isEmpty())
			item->setText(8, QString());
		item->setIcon(2, QIcon());
	}
}

void MainWindow::installAllRepoPackages()
{
	qDebug() << "InstallAllRepoPackages";
	if (repoList->selectedItems().isEmpty())
		return;
	qDebug() << "InstallAllRepoPackages1";

	if (!repoList->findItems(tr("All Groups"), Qt::MatchExactly).isEmpty())
	{
		QString tmp = repoList->selectedItems().first()->text();
		tmp.append(" ");
		tmp.prepend(" ");
		qDebug() << "Hehe";
		foreach (QTreeWidgetItem *item, pkgsViewWG->findItems(tmp, (Qt::MatchFlags)Qt::MatchContains, 6))
		{
			installPackage(item->text(1));
		}
	}
	else
	{
		foreach (QTreeWidgetItem *item, pkgsViewWG->findItems(repoList->selectedItems().first()->text(), Qt::MatchExactly, 5))
		{
			installPackage(item->text(1));
		}
	}
}

void MainWindow::reinstallAllRepoPackages()
{
	qDebug() << "InstallAllRepoPackages";
	if (repoList->selectedItems().isEmpty())
		return;
	qDebug() << "InstallAllRepoPackages1";

	if (!repoList->findItems(tr("All Groups"), Qt::MatchExactly).isEmpty())
	{
		QString tmp = repoList->selectedItems().first()->text();
		tmp.append(" ");
		tmp.prepend(" ");
		qDebug() << "Hehe";
		foreach (QTreeWidgetItem *item, pkgsViewWG->findItems(tmp, (Qt::MatchFlags)Qt::MatchContains, 6))
		{
			if (aHandle->isInstalled(item->text(1)))
				reinstallPackage(item->text(1));
		}
	}
	else
	{
		foreach (QTreeWidgetItem *item, pkgsViewWG->findItems(repoList->selectedItems().first()->text(), Qt::MatchExactly, 5))
		{
			if (aHandle->isInstalled(item->text(1)))
				reinstallPackage(item->text(1));
		}
	}
}

void MainWindow::removeAllRepoPackages()
{
	if (repoList->selectedItems().isEmpty())
		return;

	if (!repoList->findItems(tr("All Groups"), Qt::MatchExactly).isEmpty())
	{
		QString tmp = repoList->selectedItems().first()->text();
		tmp.append(" ");
		tmp.prepend(" ");
		foreach (QTreeWidgetItem *item, pkgsViewWG->findItems(tmp, (Qt::MatchFlags)Qt::MatchContains, 6))
		{
			removePackage(item->text(1));
		}
	}
	else
	{
		foreach (QTreeWidgetItem *item, pkgsViewWG->findItems(repoList->selectedItems().first()->text(), (Qt::MatchFlags)Qt::MatchExactly, 5))
		{
			removePackage(item->text(1));
		}
	}
}

void MainWindow::cancelAllRepoActions()
{
	if (repoList->selectedItems().isEmpty())
		return;

	if (!repoList->findItems(tr("All Groups"), Qt::MatchExactly).isEmpty())
	{
		QString tmp = repoList->selectedItems().first()->text();
		tmp.append(" ");
		tmp.prepend(" ");
		foreach (QTreeWidgetItem *item, pkgsViewWG->findItems(tmp, (Qt::MatchFlags)Qt::MatchContains, 6))
		{
			cancelAction(item->text(1));
		}
	}
	else
	{
		foreach (QTreeWidgetItem *item, pkgsViewWG->findItems(repoList->selectedItems().first()->text(), (Qt::MatchFlags)Qt::MatchContains, 5))
		{
			cancelAction(item->text(1));
		}
	}
}

void MainWindow::installPackage()
{
	qDebug() << "Install Package";

	if(pkgsViewWG->selectedItems().isEmpty())
		return;

	foreach (QTreeWidgetItem *item, pkgsViewWG->selectedItems())
	installPackage(item->text(1));

	itemChanged();
}

void MainWindow::reinstallPackage()
{
	qDebug() << "Install Package";

	if(pkgsViewWG->selectedItems().isEmpty())
		return;

	foreach (QTreeWidgetItem *item, pkgsViewWG->selectedItems())
	{
		reinstallPackage(item->text(1));
	}

	itemChanged();
}

void MainWindow::reinstallPackage(const QString &package)
{
	if (pkgsViewWG->findItems(package, (Qt::MatchFlags)Qt::MatchExactly, 1).isEmpty())
	{
		qDebug() << "Can't find package: " + package;
		return;
	}
	QTreeWidgetItem *item = pkgsViewWG->findItems(package, (Qt::MatchFlags)Qt::MatchExactly, 1).first();

	qDebug() << item->text(1);
	if (aHandle->isProviderInstalled(package))
		return;

	if (item->text(8) == tr("Install"))
		return;
	else
	{
		item->setText(8, tr("Install"));
		item->setIcon(2, QIcon(":/Icons/icons/list-add.png"));
	}
}

void MainWindow::installPackage(const QString &package)
{
	qDebug() << "Install package: " + package;
	if (pkgsViewWG->findItems(package, (Qt::MatchFlags)Qt::MatchExactly, 1).isEmpty())
	{
		qDebug() << "Can't find package: " + package;
		return;
	}
	QTreeWidgetItem *item = pkgsViewWG->findItems(package, (Qt::MatchFlags)Qt::MatchExactly, 1).first();

	qDebug() << item->text(1);
	if(aHandle->isProviderInstalled(package))
		return;

	if (item->text(8) == tr("Install") || aHandle->isInstalled(item->text(1)))
		return;
	else
	{
		item->setText(8, tr("Install"));
		item->setIcon(2, QIcon(":/Icons/icons/list-add.png"));
	}
	qDebug() << item->text(5);


	foreach (QString dep, aHandle->getPackageDependencies(package, item->text(5)))
	{
		installPackage(dep);
	}
}

void MainWindow::removePackage()
{
	qDebug() << "Remove Package";

	if(pkgsViewWG->selectedItems().isEmpty())
		return;

	foreach (QTreeWidgetItem *item, pkgsViewWG->selectedItems())
	removePackage(item->text(1));

	itemChanged();
}

void MainWindow::removePackage(const QString &package)
{
	qDebug() << "Uninstall package: " + package;
	if (pkgsViewWG->findItems(package, (Qt::MatchFlags)Qt::MatchExactly, 1).isEmpty())
	{
		qDebug() << "Can't find package: " + package;
		return;
	}
	QTreeWidgetItem *item = pkgsViewWG->findItems(package, (Qt::MatchFlags)Qt::MatchExactly, 1).first();

	qDebug() << item->text(1);
	if (!aHandle->isInstalled(item->text(1)) || item->text(8) == tr("Uninstall"))
		return;
	else
	{
		item->setText(8, tr("Uninstall"));
		item->setIcon(2, QIcon(":/Icons/icons/list-remove.png"));
	}
	qDebug() << item->text(5);

	foreach (QString dep, aHandle->getDependenciesOnPackage(package, item->text(5)))
	{
		removePackage(dep);
	}
}

void MainWindow::completeRemovePackage()
{
	qDebug() << "Complete Remove Package";

	if(pkgsViewWG->selectedItems().isEmpty())
		return;

	QTreeWidgetItem *item = pkgsViewWG->selectedItems().first();

	qDebug() << item->text(1);
	if (!aHandle->isInstalled(item->text(1)))
		return;
	else
	{
		item->setText(8, tr("Complete Uninstall"));
		item->setIcon(2, QIcon(":/Icons/icons/edit-delete.png"));
	}

	//Now we remove the on-package-dependencies and the dependencies...

	foreach (QString onDep, aHandle->getDependenciesOnPackage(item->text(1), item->text(5)))
	{
		removePackage(onDep);
	}
	foreach (QString dep, aHandle->getPackageDependencies(item->text(1), item->text(5)))
	{
		removePackage(dep);
	}

	itemChanged();
}

void MainWindow::cancelAction()
{
	if(pkgsViewWG->selectedItems().isEmpty())
		return;

	foreach (QTreeWidgetItem *item, pkgsViewWG->selectedItems())
	cancelAction(item->text(1));

	itemChanged();
}

void MainWindow::cancelAction(const QString &package)
{
	qDebug() << "cancel action for: " + package;
	if (pkgsViewWG->findItems(package, (Qt::MatchFlags)Qt::MatchExactly, 1).isEmpty())
	{
		qDebug() << "Can't find package: " + package;
		return;
	}
	QTreeWidgetItem *item = pkgsViewWG->findItems(package, (Qt::MatchFlags)Qt::MatchExactly, 1).first();
	if (item->text(8).isEmpty())
		return;

	item->setText(8, QString());
	item->setIcon(2, QIcon());

	foreach (QString onDep, aHandle->getDependenciesOnPackage(item->text(1), item->text(5)))
	{
		cancelAction(onDep);
	}
	foreach (QString dep, aHandle->getPackageDependencies(item->text(1), item->text(5)))
	{
		cancelAction(dep);
	}

}

void MainWindow::startUpgrading()
{
	if(dbdialog)
	{
		disconnect(dbdialog, 0,0,0);

		if(dbdialog->dbHasBeenUpdated())
		{
			populatePackagesView();
			populateRepoColumn();
			populateGrpsColumn();
		}
	}

	QStringList list = aHandle->getUpgradeablePackages();

	if(list.isEmpty())
	{
		emit systemIsUpToDate();

		if (dbdialog)
		{
			if(dbdialog->isVisible())
				/* Display a simple ShamanDialog::popup saying the system is up-to-date. */
				ShamanDialog::popupDialog(tr("System Upgrade"), tr("Your system is up to date!"), this);
			else
				trayicon->showMessage(QString(tr("System Upgrade")), QString(tr("Your system is up to date!")));

		}

		qDebug() << "System is up to date";
	}
	else
		upgrade(list);

	if(dbdialog)
	{
		dbdialog->deleteLater();
		dbdialog = 0;
		dbActive = false;
	}
}

void MainWindow::upgrade(const QStringList &packages)
{
	if (packages.contains("pacman"))
	{
		switch (ShamanDialog::popupQuestionDialog(QString(tr("Pacman Update")), QString(tr("Pacman can be upgraded. "
				"It is advised to process it alone\nto avoid version conflicts.\n"
				"Do you want to Upgrade Pacman now?")), this))
				{
				case QMessageBox::Yes:

					/* Ok, let's set up a special queue for Shaman. */

					cancelAllActions();
					installPackage("pacman");
					widgetQueueToAlpmQueue();

					break;
				case QMessageBox::No:
					/* Well, just pass by */
					break;
				default:
					// should never be reached
					break;
				}


	}
	else if (packages.contains("shaman") || packages.contains("kdemod4-shaman-svn"))
	{
		switch (ShamanDialog::popupQuestionDialog(QString(tr("Shaman Update")), QString(tr("Shaman can be upgraded. "
				"It is advised to process it alone\nto avoid version conflicts.\n"
				"Do you want to Upgrade Shaman now?")), this))
				{
				case QMessageBox::Yes:

					/* Ok, let's set up a special queue for Shaman. */

					cancelAllActions();
					
					if(packages.contains("kdemod4-shaman-svn"))
						installPackage("kdemod4-shaman-svn");
					else
						installPackage("shaman");
					
					widgetQueueToAlpmQueue();

					break;
				case QMessageBox::No:
					/* Well, just pass by */
					break;
				default:
					// should never be reached
					break;
				}

	}
	else
	{
		/* Something to upgrade! Cool! Let's show our user the
		 * upgrade dialog */
		/* Preferences are handled in SysUpgradeDialog, there's
		 * a reason for that :)
		 */

		qDebug() << "Streaming Upgrades";

		emit upgradesAvailable();

		QSettings *settings = new QSettings();

		if(settings->value("newsreader/userss", true).toBool() && settings->value("newsreader/queuenotifier", true).toBool())
		{
			foreach(QString ent, packages)
			{
				if(newsReader->checkUnreadNewsOnPkg(ent))
				{
					switch (ShamanDialog::popupQuestionDialog(QString(tr("News Alert")), 
							QString(tr("There is an unread news about %1.\nDo you want to read it?")).arg(ent), this)) 
							{
							case QMessageBox::Yes:
								openNewsDialog();
								return;
								break;
							case QMessageBox::No:
								break;
							default:
								// should never be reached
								break;
							}
				}
			}
		}

		settings->deleteLater();

		upDl = new SysUpgradeDialog(aHandle, this);

		upDl->show();

		upActive = true;

		connect(upDl, SIGNAL(aborted()), SLOT(upgradeAborted()));
		connect(upDl, SIGNAL(upgradeNow()), SLOT(processQueue()));
		connect(upDl, SIGNAL(addToPkgQueue()), SLOT(addUpgradeableToQueue()));
	}

}

void MainWindow::upgradeAborted()
{
	upDl->deleteLater();
	upActive = false;
}

void MainWindow::addUpgradeableToQueue()
{
	qDebug() << "UpgradeableToQueue";
	if (aHandle->getUpgradeablePackages().isEmpty())
		return;

	foreach (QString package, aHandle->getUpgradeablePackages())
	{
		QTreeWidgetItem *item = pkgsViewWG->findItems(package, Qt::MatchExactly, 1).first();
		item->setIcon(0, QIcon(":/Icons/icons/user-invisible.png"));
		item->setText(8, tr("Upgrade"));
		item->setIcon(2, QIcon(":/Icons/icons/list-add.png"));
	}

	if(upDl)
	{
		upDl->deleteLater();
		upActive = false;
	}
	return;
}

void MainWindow::fullSysUpgrade()
{
	dbdialog = new UpdateDbDialog(aHandle, this);
	dbActive = true;

	if(isVisible())
		dbdialog->show();

	connect(dbdialog, SIGNAL(killMe()), this, SLOT(startUpgrading()));

	dbdialog->doAction();
}

void MainWindow::upgradePackage()
{
	qDebug() << "Upgrade Package";
	foreach (QTreeWidgetItem *item, pkgsViewWG->selectedItems())
	{
		item->setText(8, tr("Upgrade"));//Look if there are dependencies for the upgrade
		item->setIcon(2, QIcon(":/Icons/icons/list-add.png"));
	}
}

void MainWindow::processQueue()
{
	qDebug() << "Process Queue";

	/* This Function will only do the processing stuff. We'd better
	 * keep things modular as possible due to libalpm's design.
	 * So the "Process Queue" button does not point here, but
	 * to a function that reads the queue and prepares libalpm's
	 * transaction.
	 */


	/* Now, everything will be done inside our Queue Dialog.
	 * So, just create it and let him handle the job.
	 */

	qDebug() << "Queue Dialog started";

	queueDl = new QueueDialog(aHandle, this);

	connect(queueDl, SIGNAL(terminated(bool)), SLOT(queueProcessingEnded(bool)));

	queueDl->startProcessing();

	emit actionStatusChanged("queueProcessingStarted");

	queueDl->show();

	if(qUi)
	{
		if(qUi->isInTray())
		{
			trayicon->showMessage(QString(tr("Queue Processing")), QString(tr("Your Queue is being processed.\nPlease wait.")));
			hide();
			queueDl->hide();
		}

		if(qUi->isTurnOff())
			turnOffSys = true;

		qUi->deleteLater();
	}

	if(upActive)
		upDl->deleteLater();

	upActive = false;
	revActive = false;

}

void MainWindow::queueProcessingEnded(bool errors)
{
	emit actionStatusChanged("queueProcessingFinished");

	QSettings *settings = new QSettings();

	if(errors)
	{
		/* An error has occurred. Just notify the user, since we have already shown a
		 * ShamanDialog::popup with error details.
		 */

		qDebug() << "Errors occurred, Transaction was not completed";

		if(queueDl->isVisible())
			ShamanDialog::popupDialog(tr("Queue Processed"), tr("One or more errors occurred, your Queue\nwas not successfully processed"),
					this, ShamanProperties::ErrorDialog);
		else
			trayicon->showMessage(QString(tr("Queue Processed")), QString(tr("One or more errors occurred, your Queue\n"
					"was not successfully processed")));
	}
	else
	{
		/* Everything went fine! Cool then, just notify the user and refine the packages view.
		 */

		qDebug() << "Transaction Completed Successfully";

		if(!pkgsViewWG->findItems("pacman", Qt::MatchExactly, 1).first()->text(8).isEmpty())
		{
			ShamanDialog::popupDialog(tr("Restart required"), tr("Pacman or Shaman was updated. Shaman will now quit,\nplease restart it "
					"to use the new version"), this, ShamanProperties::WarningDialog);

			qApp->exit(0);
		}

		if(!pkgsViewWG->findItems("shaman", Qt::MatchExactly, 1).isEmpty())
		{
			if(!pkgsViewWG->findItems("shaman", Qt::MatchExactly, 1).first()->text(8).isEmpty())
			{
				ShamanDialog::popupDialog(tr("Restart required"), tr("Pacman or Shaman was updated. Shaman will now quit,\nplease restart it "
						"to use the new version"), this, ShamanProperties::WarningDialog);

				qApp->exit(0);
			}
		}

		if(!pkgsViewWG->findItems("kdemod4-shaman-svn", Qt::MatchExactly, 1).isEmpty())
		{
			if(!pkgsViewWG->findItems("kdemod4-shaman-svn", Qt::MatchExactly, 1).first()->text(8).isEmpty())
			{
				ShamanDialog::popupDialog(tr("Restart required"), tr("Pacman or Shaman was updated. Shaman will now quit,\nplease restart it "
						"to use the new version"), this, ShamanProperties::WarningDialog);

				qApp->exit(0);
			}
		}

		if(!pkgsViewWG->findItems("kernel26", Qt::MatchExactly, 1).first()->text(8).isEmpty())
			ShamanDialog::popupDialog(tr("Restart required"), tr("Your Kernel has been updated.\nPlease restart "
					"your PC soon to load the new Kernel."), this, ShamanProperties::WarningDialog);

		qApp->processEvents();

		populateQueuePackagesView();
		refinePkgView();

		if(queueDl->isVisible() && !settings->value("gui/keepqueuedialogopen", true).toBool())
			ShamanDialog::popupDialog(tr("Queue Processed"), tr("Your Queue was successfully processed!"), this);
		else
			trayicon->showMessage(QString(tr("Queue Processed")), QString(tr("Your Queue was successfully processed!!")));

		if(turnOffSys)
		{
			/* Ok, let's go. We need to stream a message through DBus to turn off stuff.
			 * KDE3 is not supported since it uses DCOP, sorry, GNOME will come soon.
			 */

			qDebug() << "Turning the system off";

			QDBusInterface iface("org.kde.ksmserver", "/KSMServer", "org.kde.KSMServerInterface");

			iface.call("logout", 0, 2, 0);

		}
	}

	turnOffSys = false;

	if(!settings->value("gui/keepqueuedialogopen", true).toBool())
		queueDl->deleteLater();

	settings->deleteLater();

}

void MainWindow::widgetQueueToAlpmQueue()
{
	/* This function takes the queue the user defined in the
	 * GUI and "translates" it to a libalpm transaction queue.
	 * So THIS is the function that the "Process Queue" buttons
	 * should call. processQueue() processes a libalpm queue,
	 * so you need to "translate" it first.
	 *
	 * First of all, we need to know what kind of actions we
	 * need.
	 */

	QSettings *settings = new QSettings();

	if(settings->value("newsreader/userss", true).toBool() && settings->value("newsreader/queuenotifier", true).toBool())
	{
		foreach(QTreeWidgetItem *ent, getInstallPackagesInWidgetQueue() + getUpgradePackagesInWidgetQueue() +getInstallPackagesInWidgetQueue())
		{
			if(newsReader->checkUnreadNewsOnPkg(ent->text(1)))
			{
				switch (ShamanDialog::popupQuestionDialog(QString(tr("News Alert")), 
						QString(tr("There is an unread news about %1.\nDo you want to read it?")).arg(ent->text(1)), this)) 
						{
						case QMessageBox::Yes:
							openNewsDialog();
							return;
							break;
						case QMessageBox::No:
							break;
						default:
							// should never be reached
							break;
						}
			}
		}

	}

	settings->deleteLater();

	if(pkgsViewWG->findItems(tr("Uninstall"), Qt::MatchExactly, 8).isEmpty() &&
			pkgsViewWG->findItems(tr("Complete Uninstall"), Qt::MatchExactly, 8).isEmpty() &&
			pkgsViewWG->findItems(tr("Install"), Qt::MatchExactly, 8).isEmpty() &&
			pkgsViewWG->findItems(tr("Upgrade"), Qt::MatchExactly, 8).isEmpty())
		return;

	else if(pkgsViewWG->findItems(tr("Uninstall"), Qt::MatchExactly, 8).isEmpty() &&
			pkgsViewWG->findItems(tr("Complete Uninstall"), Qt::MatchExactly, 8).isEmpty())
		aHandle->initQueue(false, true, false);

	else if(pkgsViewWG->findItems(tr("Install"), Qt::MatchExactly, 8).isEmpty() &&
			pkgsViewWG->findItems(tr("Upgrade"), Qt::MatchExactly, 8).isEmpty())
		aHandle->initQueue(true, false, false);

	else
		aHandle->initQueue(true, true, false);

	qUi = new ReviewQueueDialog(aHandle, this);

	revActive = true;

	qUi->show();

	connect(qUi, SIGNAL(goProcess()), SLOT(processQueue()));

}

void MainWindow::showSettings()
{
	configDialog = new ConfigDialog(aHandle, this);

	connect (configDialog, SIGNAL(setProxy()), this, SLOT(setProxy()));

	configDialog->show();
	
	connect(configDialog, SIGNAL(accepted()), SLOT(settingsClosed()));
	connect(configDialog, SIGNAL(rejected()), SLOT(settingsClosed()));
}

void MainWindow::settingsClosed()
{
	qDebug() << "Closing Settings";
	
	if(configDialog->result() == QDialog::Accepted)
	{
		if(configDialog->doDbUpdate())
			doDbUpdate();

		trayicon->changeTimerInterval();

		newsReader->setUpdateInterval();
	}

	configDialog->deleteLater();
}

void MainWindow::systrayActivated(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::Trigger)
	{
		QSettings *settings = new QSettings();

		if (isHidden())
		{
			/* Uh, we have to stop the Timer! */
			emit stopTimer();
			show();
			foreach(QObject *ent, children())
			{	
				QDialog *dlog = qobject_cast<QDialog *>(ent);
				if(dlog != 0)
					dlog->show();
			}

		}
		else
		{
			emit startTimer();
			hide();
			foreach(QObject *ent, children())
			{	
				QDialog *dlog = qobject_cast<QDialog *>(ent);
				if(dlog != 0)
					dlog->hide();
			}
		}

		settings->deleteLater();
	}
}

void MainWindow::getPackageFromFile()
{
	QString fileName = QFileDialog::getOpenFileName(this,
			tr("Install a Package"), qgetenv("HOME"), tr("Arch Linux Packages (*.pkg.tar.gz)"));
	pmpkg_t *pkg;

	if(fileName == NULL)
		return;

	// Sanity check
	if(alpm_pkg_load(fileName.toAscii().data(), 1, &pkg) == -1)
	{
		ShamanDialog::popupDialog(tr("Error"), QString(tr("%1 does not seem\na valid package")).
				arg(fileName), this, ShamanProperties::ErrorDialog);

		return;
	}

	qDebug() << "Selected" << alpm_pkg_get_name(pkg);

	alpm_pkg_free(pkg);

	aHandle->initQueue(false, false, true);

	aHandle->addFFToQueue(fileName);

	processQueue();

}

void MainWindow::showAboutDialog()
{
	QDialog *about = new QDialog(this);
	Ui::aboutDialog ui;

	ui.setupUi(about);

	ui.aboutInfoLabel->setText(QString(tr("<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\np, "
			"li { white-space: pre-wrap; }\n</style></head><body style=\" font-family:'Sans Serif'; font-size:10pt; font-weight:400;"
			" font-style:normal;\">\n<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; "
			"text-indent:0px;\"><span style=\" font-weight:600;\">A libalpm frontend in Qt</span></p>\n<p style=\"-qt-paragraph-type:empty; "
			"margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px; font-weight:600;\">"
			"</p>\n<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">"
			"License: GPLv2+</p>\n<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; "
			"text-indent:0px;\">Libalpm Version: v.%1</p>\n<p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px;"
			" margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"></p>\n<p style=\" margin-top:0px; margin-bottom:0px;"
			" margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">(C) 2008 Dario Freddi &lt;drf54321@yahoo.it&gt;</p>\n"
			"<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">"
			"(C) 2008 Lukas Appelhans &lt;l.appelhans@gmx.de&gt;</p></body></html>")).arg(aHandle->getAlpmVersion()));

	about->setWindowModality(Qt::ApplicationModal);

	about->exec();

	about->deleteLater();
}

QString MainWindow::formatSize(unsigned long size)
{
	QString s;
	if (size > 1024 * 1024 * 1024)
		s = tr("%1 GiB", "Size is in Gib").arg((double) size / (1024 *1024 * 1024), 0, 'f', 2);
	else if (size > 1024 * 1024)
		s = tr("%1 MiB", "Size is in MiB").arg((double) size / (1024 * 1024), 0, 'f', 2);
	else if (size > 1024)
		s = tr("%1 KiB", "Size is in KiB").arg(size / 1024);
	else
		s = tr("%1 Bytes", "Size is in Bytes").arg(size);

	return s;
}

void MainWindow::startTrayTimer()
{
	emit startTimer();
}

void MainWindow::streamTransQuestion(const QString &msg)
{
	switch (ShamanDialog::popupQuestionDialog(QString(tr("Library Question")), msg, this))
	{
	case QMessageBox::Yes:
		CbackReference.answer = 1;
		break;
	case QMessageBox::No:
		CbackReference.answer = 0;
		break;
	default:
		// should never be reached
		break;
	}

	qDebug() << "Waking Alpm Thread";

	wCond.wakeAll();
}

void MainWindow::updateABSTree()
{
	bHandler = new BuildingHandler(this, aHandle);

	bHandler->updateABSTree();

	connect(bHandler, SIGNAL(outOfScope()), SLOT(terminatedBuildingHandling()));
}

void MainWindow::initSourceQueue()
{
	qDebug() << "Starting Building Handler";

	bHandler = new BuildingHandler(this, aHandle);

	bHandler->validateSourceQueue();

	connect(bHandler, SIGNAL(outOfScope()), SLOT(terminatedBuildingHandling()));
	connect(bHandler, SIGNAL(buildingFinished()), SIGNAL(buildingFinished()));
	connect(bHandler, SIGNAL(buildingStarted()), SIGNAL(buildingStarted()));
}

void MainWindow::terminatedBuildingHandling()
{
	bHandler->deleteLater();
}

QList<QTreeWidgetItem *> MainWindow::getInstallPackagesInWidgetQueue()
{
	return pkgsViewWG->findItems(tr("Install"), Qt::MatchExactly, 8);
}

QList<QTreeWidgetItem *> MainWindow::getUpgradePackagesInWidgetQueue()
{
	return pkgsViewWG->findItems(tr("Upgrade"), Qt::MatchExactly, 8);
}

QList<QTreeWidgetItem *> MainWindow::getRemovePackagesInWidgetQueue()
{
	return pkgsViewWG->findItems(tr("Uninstall"), Qt::MatchExactly, 8) +
	pkgsViewWG->findItems(tr("Complete Uninstall"), Qt::MatchExactly, 8);
}

bool MainWindow::packageExists(const QString &pkg)
{
	if(pkgsViewWG->findItems(pkg, Qt::MatchExactly | Qt::CaseInsensitive, 1).isEmpty())
		return false;
	else
		return true;
}

ShamanTrayIcon *MainWindow::getTrayIcon()
{
	return trayicon;
}

bool MainWindow::populateQueuePackagesView()
{
	pkgsViewWG->setSortingEnabled(false);

	disconnect(pkgsViewWG, SIGNAL(itemSelectionChanged()), this,
			SLOT(itemChanged()));

	/* Let's get Packages in the Queue first. */

	QList<QTreeWidgetItem *> list;
	list.clear();

	list = getInstallPackagesInWidgetQueue() + getRemovePackagesInWidgetQueue() + getUpgradePackagesInWidgetQueue();

	foreach(QTreeWidgetItem *itm, list)
	{
		if(itm->text(5) != "local" || (itm->text(8) != tr("Uninstall") && itm->text(8) != tr("Complete Uninstall")))
		{
			pmpkg_t *pkg = aHandle->getPackageFromName(itm->text(1), itm->text(5));
			QTreeWidgetItem *item = new QTreeWidgetItem(pkgsViewWG);
			alpm_list_t *grps = (alpm_list_t *)alpm_pkg_get_groups(pkg);
			QString grStr("");
			if(aHandle->isInstalled(pkg))
			{
				//item->setText(0, tr("Installed"));
				item->setIcon(0, QIcon(":/Icons/icons/user-online.png"));
				item->setText(3, aHandle->getPackageVersion(alpm_pkg_get_name(pkg), "local"));
			}
			else
			{
				//item->setText(0, tr("Not Installed"));
				item->setIcon(0, QIcon(":/Icons/icons/user-offline.png"));
				item->setText(3, alpm_pkg_get_version(pkg));
			}

			item->setText(1, alpm_pkg_get_name(pkg));
			item->setText(5, itm->text(5));
			item->setText(4, formatSize(aHandle->getPackageSize(item->text(1), item->text(5))));
			item->setText(7, alpm_pkg_get_desc(pkg));

			while(grps != NULL)
			{
				grStr.append(" ");

				grStr.append((char *)alpm_list_getdata(grps));
				grps = alpm_list_next(grps);
			}
			grStr.append(" ");

			item->setText(6, grStr);
		}

		removePackageFromView(itm);
	}

	alpm_list_t *locPkg = aHandle->getPackagesFromRepo("local");

	while(locPkg != NULL)
	{
		pmpkg_t *pkg = (pmpkg_t *)alpm_list_getdata(locPkg);
		QList<QTreeWidgetItem*> list = pkgsViewWG->findItems(alpm_pkg_get_name(pkg), Qt::MatchExactly, 1);
		if (list.isEmpty())
		{
			QTreeWidgetItem *item = new QTreeWidgetItem(pkgsViewWG);
			item->setIcon(0, QIcon(":/Icons/icons/user-online.png"));
			item->setText(1, alpm_pkg_get_name(pkg));
			item->setText(3, alpm_pkg_get_version(pkg));
			item->setText(7, alpm_pkg_get_desc(pkg));
			item->setText(5, "local");
			item->setText(4, formatSize(aHandle->getPackageSize(item->text(1), item->text(5))));
		}

		locPkg = alpm_list_next(locPkg);
	}

	QStringList upgrds = aHandle->getUpgradeablePackages();
	foreach (QString pac, upgrds)
	{
		QTreeWidgetItem *item = pkgsViewWG->findItems(pac, Qt::MatchExactly, 1).first();
		if (item)
		{
			item->setText(8, tr("Upgrade"));
			item->setIcon(2, QIcon(":/Icons/icons/list-add.png"));
		}
	}

	pkgsViewWG->sortItems(1, Qt::AscendingOrder);
	pkgsViewWG->setSortingEnabled(true);//Enable sorting *after* inserting :D

	connect(pkgsViewWG, SIGNAL(itemSelectionChanged()), this,
			SLOT(itemChanged()));

	return true;
}

void MainWindow::removePackageFromView(const QString &pkgname)
{
	int index = pkgsViewWG->indexOfTopLevelItem(pkgsViewWG->findItems(pkgname, Qt::MatchExactly
			| Qt::CaseInsensitive, 1).first());

	delete pkgsViewWG->takeTopLevelItem(index);
}

void MainWindow::removePackageFromView(QTreeWidgetItem *item)
{
	int index = pkgsViewWG->indexOfTopLevelItem(item);

	delete pkgsViewWG->takeTopLevelItem(index);
}

void MainWindow::setProxy()
{
	QSettings *settings = new QSettings();


	if (settings->value("proxy/enabled").toBool() && settings->value("proxy/httpProxy").toBool())
	{
		setenv("HTTP_PROXY", settings->value("proxy/proxyServer").toString().toLatin1() + ':' + settings->value("proxy/proxyPort").toString().toLatin1(), 1);
		qDebug() << QString("HTTP_PROXY: ") + settings->value("proxy/proxyServer").toString() + ':' + settings->value("proxy/proxyPort").toString();
		QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, settings->value("proxy/proxyServer").toString(),
				settings->value("proxy/proxyPort").toInt()));
	}
	else 
	{
		unsetenv("HTTP_PROXY");
		qDebug() << "--> UNSETENV HTTP_PROXY";
		QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::NoProxy));
	}


	if (settings->value("proxy/enabled").toBool() && settings->value("proxy/ftpProxy").toBool())
	{
		setenv("FTP_PROXY", settings->value("proxy/proxyServer").toString().toLatin1() + ':' + settings->value("proxy/proxyPort").toString().toLatin1(), 1);
		qDebug() << QString("FTP_PROXY: ") + settings->value("proxy/proxyServer").toString() + ':' + settings->value("proxy/proxyPort").toString();
	}
	else
	{
		unsetenv ("FTP_PROXY");
		qDebug() << "--> UNSETENV FTP_PROXY";
	}
}

void MainWindow::openNewsDialog()
{
	if(!nView)
	{
		nView = new NewsViewer(newsReader, this);

		nView->show();
	}
	else
		nView->show();
}

void MainWindow::openLogViewer()
{
	if(!lView)
	{
		lView = new LogViewer(this);

		lView->show();
	}
	else
		lView->show();
}

void MainWindow::showAuthDialog(int count)
{
	qDebug() << "Starting Auth Dialog";

	Ui::authDialog aUi;
	QDialog *dlog = new QDialog(this);

	dlog->setModal(true);

	aUi.setupUi(dlog);

	// Let's Hide out other dialogs

	foreach(QObject *ent, children())
	{	
		QDialog *edl = qobject_cast<QDialog *>(ent);
		if(edl != 0)
			edl->hide();
	}

	if(dlog->exec() == QDialog::Accepted)
	{
		pam_response tmp;
		tmp.resp_retcode = 0;

		char *str = (char *) malloc(strlen(aUi.lineEdit->text().toAscii().data()) * sizeof(char));
		strcpy(str, aUi.lineEdit->text().toAscii().data());

		tmp.resp = str;

		qDebug() << "Inserting Reply";
		reply[count] = tmp;
		qDebug() << "Reply inserted";
	}
	else
	{
		free(reply);
		qDebug() << "Reply freed, aborting...";
		reply = NULL;
	}

	dlog->deleteLater();

	// Ok, let's show back again hidden dialogs

	foreach(QObject *ent, children())
	{	
		QDialog *dlog = qobject_cast<QDialog *>(ent);
		if(dlog != 0)
			dlog->show();
	}

	wCond.wakeAll();
}
