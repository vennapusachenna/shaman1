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
 **************************************************************************/

#include "ShamanStatusBar.h"

#include "PackageProperties.h"

#include <QStatusBar>
#include <QTimer>

ShamanStatusBar::ShamanStatusBar(AlpmHandler *aH, MainWindow *parent)
 : QStatusBar(parent),
 aHandle(aH),
 mWin(parent)
{
	setUpStatusBar();
}

ShamanStatusBar::~ShamanStatusBar()
{
}

void ShamanStatusBar::setUpStatusBar()
{
	stBarImage = new QLabel();
	stBarText = new QLabel();
	stBarProg = new QProgressBar();
	
	stBarProg->setRange(0,100);
	stBarProg->setFormat("%p%");
	
	addWidget(stBarImage);
	addWidget(stBarText);
	
	QWidget *tmpwg = new QWidget();
	addPermanentWidget(tmpwg, 1024);
	addPermanentWidget(stBarProg);
	
	stBarProg->hide();
	
	setSizeGripEnabled(false);
	
	updateStatusBar();
}

void ShamanStatusBar::showStBarAction(const QString &text, const QPixmap &pixmap, int timeout)
{
	stBarText->setText(text);
	stBarImage->setPixmap(pixmap);
	
	QTimer::singleShot(timeout * 1000, this, SLOT(clearStBarAction()));
}

void ShamanStatusBar::clearStBarAction()
{
	stBarText->setText(QString());
	stBarImage->setPixmap(QPixmap());
	updateStatusBar();
}

void ShamanStatusBar::updateStatusBar()
{
	QString text = QString(tr("%1 Available Packages, %2 Installed Packages, %3 Upgradeable Packages").
			arg(aHandle->countPackages(Alpm::AllPackages)).arg(aHandle->countPackages(Alpm::InstalledPackages)).
			arg(aHandle->getUpgradeablePackages().count()));
	
	text.append(' ');
	
	// Now up to queue status
	
	int removeSize = 0;
	int addSize = 0;
	
	QList<QTreeWidgetItem *> addList = mWin->getInstallPackagesInWidgetQueue() + mWin->getUpgradePackagesInWidgetQueue();
	
	foreach(QTreeWidgetItem *itm, addList)
		addSize += aHandle->getPackageSize(itm->text(1), itm->text(5));
	
	QList<QTreeWidgetItem *> removeList = mWin->getRemovePackagesInWidgetQueue();
		
	foreach(QTreeWidgetItem *itm, removeList)
		removeSize += aHandle->getPackageSize(itm->text(1), itm->text(5));

	QString spaceToDo;

	if(addSize == removeSize) { }
	else if(addSize > removeSize)
	{
		QString sizeToShow(PackageProperties::formatSize(addSize - removeSize));
		spaceToDo = tr("%1 will be used").arg(sizeToShow);
		spaceToDo.append(']');
		spaceToDo.prepend(" [");
	}
	else
	{
		QString sizeToShow(PackageProperties::formatSize(removeSize - addSize));
		spaceToDo = tr("%1 will be freed").arg(sizeToShow);
		spaceToDo.append(']');
		spaceToDo.prepend(" [");
	}
	
	text.append('(' + tr("%1 to be Installed, %2 to be Removed").arg(addList.count()).arg(removeList.count())
			+ spaceToDo + ')');
	
	stBarText->setText(text);
}

void ShamanStatusBar::startProgressBar()
{
	stBarProg->setValue(0);
	stBarProg->show();
}

void ShamanStatusBar::stopProgressBar()
{
	stBarProg->hide();
}

void ShamanStatusBar::updateProgressBar(int percentage)
{
	stBarProg->setValue(percentage);
}
