/*
 *   Copyright (C) 2008 by Lukas Appelhans				   				   
 *   l.appelhans@gmx.de		
 *   Copyright (C) 2008 by Dario Freddi                                    
 *   drf54321@yahoo.it                                                     					   						   
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2 as
 *   published by the Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "shamanRunner.h"

#include <KIcon>
#include <KLocale>
#include <KDebug>
#include <KRun>
#include <QDBusInterface>
#include <QDBusConnectionInterface>

ShamanRunner::ShamanRunner(QObject *parent, const QVariantList &args)
    : Plasma::AbstractRunner(parent, args),
    dbus(QDBusConnection::systemBus())
{
    Q_UNUSED(args)

    words << "install" << "remove" << "uninstall" << "upgrade-system" << "upgrade system" << "update database" 
                            << "update db" << "update-db" << "update-database" << "us" << "ud" << "i" << "r" 
                            << "complete-remove" << "complete-uninstall" << "package manager";
}

ShamanRunner::~ShamanRunner()
{
}

void ShamanRunner::match(Plasma::SearchContext *search)
{
    kDebug() << "Match search context?";
    QString term = search->searchTerm();

    foreach(QString word, words)
    {    	
    	if (term.startsWith(word, Qt::CaseInsensitive))
    	{
    		Plasma::SearchMatch* match = new Plasma::SearchMatch(this);
    		match->setType(Plasma::SearchMatch::ExactMatch);

    		/* Now throw some nice icons in here */

    		if (term.startsWith("install", Qt::CaseInsensitive) || 
    				term.startsWith("i", Qt::CaseInsensitive))
    		{
    			if (term.split(' ').count() <= 1)
    			{
    				match->setIcon(QIcon(":/Icons/icons/shaman/shaman-32.png"));
    				match->setText(i18n("Package Manager: Install Package"));
    			}
    			else
    			{    			
    				match->setIcon(QIcon(":/Icons/icons/shaman/shaman-32.png"));
    				match->setText(i18n("Package Manager: Install %1", term.split(' ').at(1)));
    			}
    		}

    		else if (term.startsWith("remove", Qt::CaseInsensitive) || 
    				term.startsWith("uninstall", Qt::CaseInsensitive) || 
    				term.startsWith("r", Qt::CaseInsensitive))
    		{
    			if (term.split(' ').count() <= 1)
    			{
    				match->setIcon(QIcon(":/Icons/icons/shaman/shaman-32.png"));
    				match->setText(i18n("Package Manager: Remove Package"));
    			}
    			else
    			{
    				match->setIcon(QIcon(":/Icons/icons/shaman/shaman-32.png"));
    				match->setText(i18n("Package Manager: Remove %1", term.split(' ').at(1)));
    			}
    		}
    		
    		else if (term.startsWith("upgrade-system", Qt::CaseInsensitive) || 
    				term.startsWith("upgrade system", Qt::CaseInsensitive) ||
    				term == "us")
    		{
    			match->setText(i18n("Package Manager: Upgrade System"));
    			match->setIcon(QIcon(":/Icons/icons/shaman/shaman-updates-available-32.png"));
    		}
    		
    		else if (term.startsWith("update database", Qt::CaseInsensitive) || 
    				term.startsWith("update-database", Qt::CaseInsensitive) || 
    				term.startsWith("update db", Qt::CaseInsensitive) || 
    				term.startsWith("update-db", Qt::CaseInsensitive) || 
    				term == "ud")
    		{
    			match->setText(i18n("Update Databases"));
    			match->setIcon(QIcon(":/Icons/icons/shaman/shaman-yellow-icon-32.png"));
    		}

    		match->setSubtext(i18n("Shaman Package Manager"));
    		match->setRelevance(1);
    	}
    }
}

void ShamanRunner::exec(const Plasma::SearchContext *search, const Plasma::SearchMatch *action)
{
	execTerm = search->searchTerm();

	/* First of all, let's check if Shaman has been already
	 * started.
     */
    
    if(!dbus.interface()->isServiceRegistered("org.archlinux.shaman"))
    	startShaman();
    else
    	executeAction();
}

void ShamanRunner::executeAction()
{
	if (execTerm.startsWith("install", Qt::CaseInsensitive) || 
			execTerm.startsWith("i ", Qt::CaseInsensitive))
	{
		if (execTerm.split(' ').count() <= 1)
			return;

		QDBusInterface iface("org.archlinux.shaman", "/Shaman", "org.archlinux.shaman", QDBusConnection::systemBus());

		iface.call("installPackage", execTerm.split(' ').at(1));

		iface.call("widgetQueueToAlpmQueue");
	}
	else if (execTerm.startsWith("remove", Qt::CaseInsensitive) || execTerm.startsWith("uninstall", Qt::CaseInsensitive) || 
			execTerm.startsWith("r ", Qt::CaseInsensitive))
	{
		if (execTerm.split(' ').count() <= 1)
			return;

		QDBusInterface iface("org.archlinux.shaman", "/Shaman", "org.archlinux.shaman", QDBusConnection::systemBus());

		iface.call("removePackage", execTerm.split(' ').at(1));

		iface.call("widgetQueueToAlpmQueue");
	}
	else if (execTerm.startsWith("upgrade-system", Qt::CaseInsensitive) || execTerm.startsWith("upgrade system", Qt::CaseInsensitive) ||
			execTerm == "us")
	{
		QDBusInterface iface("org.archlinux.shaman", "/Shaman", "org.archlinux.shaman", QDBusConnection::systemBus());

		iface.call("fullSysUpgrade");
	}
	else if (execTerm.startsWith("update database", Qt::CaseInsensitive) || execTerm.startsWith("update-database", Qt::CaseInsensitive) || 
			execTerm.startsWith("update db", Qt::CaseInsensitive) || execTerm.startsWith("update-db", Qt::CaseInsensitive) || 
			execTerm == "ud")
	{
		QDBusInterface iface("org.archlinux.shaman", "/Shaman", "org.archlinux.shaman", QDBusConnection::systemBus());

		iface.call("doDbUpdate");
	}
}

void ShamanRunner::startShaman()
{
	KRun::runCommand("shaman", "Shaman Package Manager", "shaman", 0);

	if (!dbus.interface()->isServiceRegistered("org.archlinux.shaman"))
		sleep(0.2);
	
	dbus.connect("org.archlinux.shaman", "/Shaman", "org.archlinux.shaman", 
				"shamanReady", this, SLOT(executeAction()));
}

#include "shamanRunner.moc"