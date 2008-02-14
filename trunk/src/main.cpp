/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi                                    *
 *   drf54321@yahoo.it                                                     *
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

#include "AlpmHandler.h"
#include "alpm_list.h"
#include "MainWindow.h"

#include <iostream>
#include <QApplication>
#include <QString>
#include <QSettings>
#include <QMessageBox>
#include <QFile>
#include <QDate>
#include <signal.h>

static void cleanup(int signum)
{
	if(signum==SIGSEGV)
	{
		/* write a log message and write to stderr */
		printf("Segmentation Fault! We are sorry. Please report a bug, so we can fix that\n");
		exit(signum);
	} 
	else if((signum == SIGINT)) 
	{
		if(alpm_trans_interrupt() == 0)
			/* a transaction is being interrupted, don't exit qtPacman yet. */
			return;

		/* no committing transaction, we can release it now and then exit pacman */
		alpm_trans_release();
		/* output a newline to be sure we clear any line we may be on */
		printf("\n");
	}

	/* free alpm library resources */
	if(alpm_release() == -1) {
		printf("%s", alpm_strerrorlast());
	}

	exit(signum);
}

int main(int argc, char **argv)
{
	uid_t myuid = geteuid();

	QApplication app(argc, argv, QApplication::GuiClient);

	if(myuid > 0)
	{
		QMessageBox *message = new QMessageBox(QMessageBox::Information, QObject::tr("qtPacman", "Hey! "
				"If you're reading this, first of all thanks for helping us in making qtPacman better. "
				"There are not many comments unless where needed, since all the strings are pretty self-explanatory. "
				"You'll see a lot of HTML in some cases: don't let that scare you, but please edit text only. Editing "
				"HTML tags too may break our layout, so be careful. A good practice could be copying the whole string, "
				"and then translating just what's outside the tags, usually just a few words. "
				"If you have any doubts, or if you just want to drop us a line, there goes our email addresses:\n"
				"Dario: drf54321@gmail.com\nLukas: l.appelhans@gmx.de\n"
				"Thanks again, and enjoy your translation!"), 
				QObject::tr("You have to be root to run qtPacman.\nPlease restart it with root privileges."), QMessageBox::Ok);

		message->show();

		return app.exec();
	}

	AlpmHandler *aHandler = new AlpmHandler(true);

	if(!aHandler->testLibrary())
	{
		/* TODO: Give the dialog the 
		 * ability to clean up pacman cache?
		 */

		QMessageBox *message = new QMessageBox(QMessageBox::Information, QObject::tr("qtPacman"), QObject::tr("There was a problem while testing libalpm.\nMaybe another application has a lock on it."), QMessageBox::Ok);

		message->show();

		return app.exec();
	}
	
	app.setQuitOnLastWindowClosed(false);
	
	//TODO: Look if we should translate the program to another language and load it^^
	signal(SIGINT, cleanup);
	signal(SIGTERM, cleanup);
	signal(SIGSEGV, cleanup);
	
	QSettings *settings = new QSettings(QSettings::SystemScope, "qtPacman", "qtPacman");
	
	if(settings->value("gui/startupmode").toString() == 0)
	{
		/* Whoa! This probably means that this is either the first time we
		 * start qtPacman, or the config file has gone. In both cases,
		 * let's create some reasonable defaults. And let's backup
		 * pacman.conf too, our parser simply destroys all commented
		 * lines.
		 */
		
		QFile::copy("/etc/pacman.conf", QString("/etc/pacman.conf.bak.").append(QDate::currentDate().toString("ddMMyyyy")));
		
		settings->setValue("gui/startupmode", "window");
	}
	
	settings->deleteLater();

	MainWindow mainwin(aHandler);

	if(settings->value("gui/startupmode").toString() == "window")
	{
		/* case 1: we want to show Main Window
		 */
		mainwin.show();
		
	}
	else
	{
		/* case 2: we don't want to show Main Window,
		 * we want the program to start up in the systray
		 * only.
		 */
	}

	mainwin.populateRepoColumn();

	mainwin.populatePackagesView();
	QObject::connect(&mainwin, SIGNAL(aboutToQuit()), &app, SLOT(quit()));
	app.syncX();
	return app.exec();

}

