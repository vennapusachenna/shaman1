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
#include "AlpmHandler.h"

#include <iostream>
#include <string>
#include <alpm.h>
#include <alpm_list.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/utsname.h>
#include <QDir>
#include <QDebug>
#include <QMessageBox>
#include <QProcess>

#include "callbacks.h"

#define SHAMAN_VERSION "1.0Alpha2dev"

AlpmHandler::AlpmHandler(bool init)
: toRemove(NULL),
toSync(NULL)
{

	/* First, initialize Alpm. Then, make the whole class aware that no
	 * transaction with libalpm is in progress */
	if(!init)
		return;

	alpm_initialize();
	onTransaction = false;

	setUpAlpmSettings();

}

AlpmHandler::~AlpmHandler()
{

	if(isTransaction())
		releaseTransaction();

	sleep(2);

	alpm_release();
}

QStringList AlpmHandler::alpmListToStringList(alpm_list_t *list)
{
	QStringList retlist;
	
	list = alpm_list_first(list);
	
	while(list != NULL)
	{
		retlist.append((char *) alpm_list_getdata(list));
		list = alpm_list_next(list);
	}
	
	return retlist;
}

pmpkg_t *AlpmHandler::getPackageFromName(const QString &name, const QString &repo)
{	
	if(!repo.compare("local"))
		return alpm_db_get_pkg(db_local, name.toAscii().data());
	
	alpm_list_t *dbsync = alpm_list_first(sync_databases);
	
	while(dbsync != NULL)
	{
		pmdb_t *dbcrnt = (pmdb_t *)alpm_list_getdata(dbsync);

		if(!repo.compare(QString((char *)alpm_db_get_name(dbcrnt))))
		{
			dbsync = alpm_list_first(dbsync);
			return alpm_db_get_pkg(dbcrnt, name.toAscii().data());
		}

		dbsync = alpm_list_next(dbsync);
	}

	return NULL;
}

bool AlpmHandler::isTransaction()
{
	if(onTransaction)
		return true;
	else
		return false;
}

bool AlpmHandler::testLibrary()
{
	/* Simulate a transaction to make sure that libalpm is
	 * ready to rock. For example, if we don't have a lock
	 * on database this test will reveal it.
	 */

	if(!initTransaction(PM_TRANS_TYPE_SYNC, PM_TRANS_FLAG_ALLDEPS))
		return false;

	if(!releaseTransaction())
		return false;

	return true;
}

bool AlpmHandler::initTransaction(pmtranstype_t type, pmtransflag_t flags)
{
	if(isTransaction())
		return false;

	setuseragent();
	
	if(alpm_trans_init(type, flags, cb_trans_evt, cb_trans_conv,
			cb_trans_progress) == -1)
		return false;

	onTransaction = true;
	
	emit transactionStarted();
	
	return onTransaction;
}

bool AlpmHandler::releaseTransaction()
{
	if(!isTransaction())
		return false;

	if(alpm_trans_release() == -1)
		if(alpm_trans_interrupt() == -1)
			return false;

	onTransaction = false;
	emit transactionReleased();
	
	return true;
}

alpm_list_t *AlpmHandler::getAvailableRepos()
{
	return sync_databases;
}

alpm_list_t *AlpmHandler::getInstalledPackages()
{
	return alpm_db_getpkgcache(db_local);
}

QStringList AlpmHandler::getUpgradeablePackages()
{
	alpm_list_t *syncpkgs = NULL;
	QStringList retlist;
	alpm_list_t *syncdbs;
	retlist.clear();

	syncdbs = alpm_list_first(sync_databases);

	if(alpm_sync_sysupgrade(db_local, syncdbs, &syncpkgs) == -1) 
		return retlist;
	
	syncpkgs = alpm_list_first(syncpkgs);
	
	if(!syncpkgs)
		return retlist;
	else
	{
		qDebug() << "Upgradeable packages:";
		while(syncpkgs != NULL)
		{
			/* To Alpm Devs : LOL. Call three functions to get a fucking
			 * name of a package? Please.
			 */
			QString tmp(alpm_pkg_get_name(alpm_sync_get_pkg((pmsyncpkg_t *) alpm_list_getdata(syncpkgs))));
			qDebug() << tmp;
			retlist.append(tmp);
			syncpkgs = alpm_list_next(syncpkgs);
		}
		return retlist;
	}

}

bool AlpmHandler::updateDatabase()
{
	int r;
	bool updated = false;
	alpm_list_t *syncdbs;
	
	syncdbs = alpm_list_first(sync_databases);
	
	QStringList list;
	
	while(syncdbs != NULL)
	{
		pmdb_t *dbcrnt = (pmdb_t *)alpm_list_getdata(syncdbs);
		
		list.append(QString((char *)alpm_db_get_name(dbcrnt)));
		syncdbs = alpm_list_next(syncdbs);
	}
	
	emit dbQty(list);
	
	syncdbs = alpm_list_first(sync_databases);

	if(!initTransaction(PM_TRANS_TYPE_SYNC, PM_TRANS_FLAG_NOSCRIPTLET))
		return false;

	while(syncdbs != NULL)
	{
		pmdb_t *dbcrnt = (pmdb_t *)alpm_list_getdata(syncdbs);
		QString curdbname((char *)alpm_db_get_name(dbcrnt));

		emit streamDbUpdatingStatus(curdbname, 0);
		
		r = alpm_db_update(0, dbcrnt);
		
		if(r == 1)
			emit streamDbUpdatingStatus(curdbname, 3);
		
		else if(r < 0)
			emit streamDbUpdatingStatus(curdbname, 4);
		
		else
		{
			emit dbUpdated();
			emit streamDbUpdatingStatus(curdbname, 3);
		}

		emit dbUpdatePerformed();

		syncdbs = alpm_list_next(syncdbs);
	}

	if(!releaseTransaction())
		qDebug() << "Failed to release transaction!!";
	
	qDebug() << "Database Update Performed";

	return updated;

}

bool AlpmHandler::reloadPacmanConfiguration()
{
	PacmanConf pdata;

	/* After reloading configuration, we immediately commit changes to Alpm,
	 * otherwise users would have to reboot Shaman. You know, we're not
	 * Windows, right?
	 */
	
	while(registered_db != NULL)
	{	
		alpm_db_unregister((pmdb_t *)alpm_list_getdata(registered_db));
		registered_db = alpm_list_next(registered_db);
	}
	
	alpm_db_unregister(db_local);


	pdata.HoldPkg = alpmListToStringList(alpm_option_get_holdpkgs());

	pdata.IgnorePkg = alpmListToStringList(alpm_option_get_ignorepkgs());

	pdata.IgnoreGrp = alpmListToStringList(alpm_option_get_ignoregrps());

	pdata.NoExtract = alpmListToStringList(alpm_option_get_noextracts());

	pdata.NoUpgrade = alpmListToStringList(alpm_option_get_noupgrades());

	foreach(QString str, pdata.HoldPkg)
		alpm_option_remove_holdpkg(str.toAscii().data());

	foreach(QString str, pdata.IgnorePkg)
		alpm_option_remove_ignorepkg(str.toAscii().data());

	foreach(QString str, pdata.IgnoreGrp)
		alpm_option_remove_ignoregrp(str.toAscii().data());

	foreach(QString str, pdata.NoExtract)
		alpm_option_remove_noextract(str.toAscii().data());

	foreach(QString str, pdata.NoUpgrade)
		alpm_option_remove_noupgrade(str.toAscii().data());
	
	alpm_option_remove_cachedir("/var/cache/pacman/pkg");
	
	pdata = getPacmanConf(true);

		if(!pdata.loaded)
			return false;

	setUpAlpmSettings();

	return true;
}

bool AlpmHandler::setUpAlpmSettings()
{
	PacmanConf pdata;
	int count = 0;
	registered_db = alpm_list_new();

	/* Let's prepare Alpm for the big showdown. First of all, let's set up
	 * some root paths, we will use pacman's default for now
	 */

	alpm_option_set_root("/");
	alpm_option_set_dbpath("/var/lib/pacman");
	alpm_option_add_cachedir("/var/cache/pacman/pkg");

	/* I love classes interaction */

	pdata = getPacmanConf(false);

	/* Register local database */

	db_local = alpm_db_register_local();

	/* Register our sync databases, kindly taken from pacdata */

	if(!pdata.loaded)
		qDebug() << "Error Parsing Pacman Configuration!!";

	for(int i = 0; i < pdata.syncdbs.size(); ++i)
	{
		if(pdata.serverAssoc.size() <= i)
		{
			qDebug() << "Could not find a matching repo for" << pdata.syncdbs.at(i);
			continue;
		}
			
		dbs_sync = alpm_db_register_sync(pdata.syncdbs.at(i).toAscii().data());

		if(alpm_db_setserver(dbs_sync, pdata.serverAssoc.at(i).toAscii().data()) == 0)
		{
			qDebug() << pdata.syncdbs.at(i) << "--->" << pdata.serverAssoc.at(i);

			registered_db = alpm_list_add(registered_db, dbs_sync);
		}
		else
			qDebug() << "Failed to add" << pdata.syncdbs.at(i) << "!!";
		
		count++;
	}

	if(pdata.xferCommand != NULL)
		alpm_option_set_xfercommand(pdata.xferCommand);
	
	alpm_option_set_dlcb(cb_dl_progress);
	alpm_option_set_nopassiveftp(pdata.noPassiveFTP);
	
	foreach(QString str, pdata.HoldPkg)
		alpm_option_add_holdpkg(str.toAscii().data());
	
	foreach(QString str, pdata.IgnorePkg)
		alpm_option_add_ignorepkg(str.toAscii().data());
		
	foreach(QString str, pdata.IgnoreGrp)
		alpm_option_add_ignoregrp(str.toAscii().data());

	foreach(QString str, pdata.NoExtract)
		alpm_option_add_noextract(str.toAscii().data());

	foreach(QString str, pdata.NoUpgrade)
		alpm_option_add_noupgrade(str.toAscii().data());

	//alpm_option_set_usedelta(pdata.useDelta); Until a proper implementation is there
	alpm_option_set_usesyslog(pdata.useSysLog);

	sync_databases = alpm_option_get_syncdbs();

	return true;
}

QStringList AlpmHandler::getPackageGroups()
{
	alpm_list_t *grps = NULL, *syncdbs;
	QStringList retlist;
	retlist.clear();

	syncdbs = alpm_list_first(sync_databases);

	while(syncdbs != NULL)
	{
		grps = alpm_db_getgrpcache((pmdb_t *)alpm_list_getdata(syncdbs));
		grps = alpm_list_first(grps);

		while(grps != NULL)
		{
			retlist.append((char *)alpm_list_getdata(grps));
			grps = alpm_list_next(grps);
		}

		syncdbs = alpm_list_next(syncdbs);
	}

	return retlist;
}

bool AlpmHandler::performCurrentTransaction()
{
	alpm_list_t *data = NULL;

	if(alpm_trans_prepare(&data) == -1)
	{
		releaseTransaction();
		return false;
	}

	if(alpm_trans_commit(&data) == -1)
	{
		releaseTransaction();
		return false;
	}

	if(data)
		FREELIST(data);

	releaseTransaction();

	return true;
}

bool AlpmHandler::fullSystemUpgrade()
{

	if(!initTransaction(PM_TRANS_TYPE_SYNC, PM_TRANS_FLAG_NOSCRIPTLET))
		return false;

	if(alpm_trans_sysupgrade() == -1)
	{
		releaseTransaction();
		return false;
	}

	removeAct = false;
	syncAct = false;
	upgradeAct = true;

	return true;

}

QStringList AlpmHandler::getPackageDependencies(pmpkg_t *package)
{
	alpm_list_t *deps;
	QStringList retlist;

	deps = alpm_pkg_get_depends(package);

	while(deps != NULL)
	{
		retlist.append(QString(alpm_dep_get_name((pmdepend_t *)alpm_list_getdata(deps))));
		deps = alpm_list_next(deps);
	}

	return retlist;
}

QStringList AlpmHandler::getPackageDependencies(const QString &name, const QString &repo)
{
	alpm_list_t *deps;
	QStringList retlist;

	deps = alpm_pkg_get_depends(getPackageFromName(name, repo));

	while(deps != NULL)
	{
		retlist.append(QString(alpm_dep_get_name((pmdepend_t *)alpm_list_getdata(deps))));
		deps = alpm_list_next(deps);
	}

	return retlist;
}

QStringList AlpmHandler::getDependenciesOnPackage(pmpkg_t *package)
{
	alpm_list_t *deps;
	QStringList retlist;

	deps = alpm_pkg_compute_requiredby(package);

	while(deps != NULL)
	{
		retlist.append(QString((char *)alpm_list_getdata(deps)));
		deps = alpm_list_next(deps);
	}

	return retlist;
}

QStringList AlpmHandler::getDependenciesOnPackage(const QString &name, const QString &repo)
{
	alpm_list_t *deps;
	QStringList retlist;

	deps = alpm_pkg_compute_requiredby(getPackageFromName(name, repo));

	while(deps != NULL)
	{
		retlist.append(QString((char *)alpm_list_getdata(deps)));
		deps = alpm_list_next(deps);
	}

	return retlist;
}

bool AlpmHandler::isInstalled(pmpkg_t *pkg)
{
	pmpkg_t *localpackage = alpm_db_get_pkg(db_local, alpm_pkg_get_name(pkg));
	if(localpackage == NULL)
		return false;
	
	return true;
}

bool AlpmHandler::isInstalled(const QString &pkg)
{
	pmpkg_t *localpackage = alpm_db_get_pkg(db_local, pkg.toAscii().data());
	if(localpackage == NULL)
		return false;

	return true;
}

QStringList AlpmHandler::getPackageFiles(pmpkg_t *package)
{
	alpm_list_t *files;
	QStringList retlist;
	
	files = alpm_pkg_get_files(alpm_db_get_pkg(db_local, alpm_pkg_get_name(package)));
	
	while(files != NULL)
	{
		retlist.append(QString((char*)alpm_list_getdata(files)).prepend(alpm_option_get_root()));
		files = alpm_list_next(files);
	}
	
	return retlist;
}

QStringList AlpmHandler::getPackageFiles(const QString &name)
{
	alpm_list_t *files;
	QStringList retlist;
	
	files = alpm_pkg_get_files(alpm_db_get_pkg(db_local, name.toAscii().data()));
	
	while(files != NULL)
	{
		retlist.append(QString((char*)alpm_list_getdata(files)).prepend(alpm_option_get_root()));
		files = alpm_list_next(files);
	}
	
	return retlist;
}

void AlpmHandler::initQueue(bool rem, bool syncd, bool ff)
{	
	removeAct = rem;
	syncAct = syncd;
	upgradeAct = false;
	fromFileAct = ff;

	if(!toSync.isEmpty())
		toSync.clear();
	if(!toRemove.isEmpty())
		toRemove.clear();
	if(!toFromFile.isEmpty())
		toFromFile.clear();
}

void AlpmHandler::addSyncToQueue(const QString &toAdd)
{
	toSync.append(toAdd);
}

void AlpmHandler::addRemoveToQueue(const QString &toRm)
{
	toRemove.append(toRm);
}

void AlpmHandler::addFFToQueue(const QString &toFF)
{
	toFromFile.append(toFF);
}

int AlpmHandler::getNumberOfTargets(int action)
{
	if(action == 0)
		return toSync.size();
	else
		return toRemove.size();
}

void AlpmHandler::processQueue()
{
	
	if(removeAct)
	{
		/* Well, we need to remove packages first. Let's do this. */
		initTransaction(PM_TRANS_TYPE_REMOVE, PM_TRANS_FLAG_NOSCRIPTLET /*/PM_TRANS_FLAG_NOSCRIPTLET*/);
		
		for (int i = 0; i < toRemove.size(); ++i)
		{
			int res = alpm_trans_addtarget(toRemove.at(i).toAscii().data());
			if(res == -1)
				qDebug() << "Unable to add Target:" << toRemove.at(i);
		}
		
		performCurrentTransaction();
		
	}
	if(syncAct)
	{
		/* Time to install and upgrade packages, right? */
		initTransaction(PM_TRANS_TYPE_SYNC, PM_TRANS_FLAG_NOSCRIPTLET);
		
		for (int i = 0; i < toSync.size(); ++i)
		{
			int res = alpm_trans_addtarget(toSync.at(i).toAscii().data());
			if(res == -1)
				qDebug() << "Unable to add Target:" << toSync.at(i);
		}
		
		performCurrentTransaction();
		
		
	}
	if(upgradeAct)
	{
		/* We just have to start the transaction. */
		initTransaction(PM_TRANS_TYPE_SYNC, PM_TRANS_FLAG_NOSCRIPTLET);

		performCurrentTransaction();
	}
	if(fromFileAct)
	{
		initTransaction(PM_TRANS_TYPE_UPGRADE, PM_TRANS_FLAG_NOSCRIPTLET);

		for (int i = 0; i < toFromFile.size(); ++i)
		{
			int res = alpm_trans_addtarget(toFromFile.at(i).toAscii().data());
			if(res == -1)
				qDebug() << "Unable to add Target:" << toFromFile.at(i);
		}

		performCurrentTransaction();
	}


}

bool AlpmHandler::cleanUnusedDb()
{
	DIR *dir;
	struct dirent *ent;

	dir = opendir(alpm_option_get_dbpath());
	if(dir == NULL)
		return false;

	rewinddir(dir);
	/* step through the directory one file at a time */
	while((ent = readdir(dir)) != NULL) 
	{
		char path[4096];
		int found = 0;
		char *dname = ent->d_name;

		if(!strcmp(dname, ".") || !strcmp(dname, ".."))
			continue;

		/* skip the local and sync directories */
		if(!strcmp(dname, "sync") || !strcmp(dname, "local")) 
			continue;

		/* We have a directory that doesn't match any syncdb.
		 * Ask the user if he wants to remove it. */
		if(!found) 
			if(rmrf(path)) 
				return false;
	}
	
	char newpath[4096];
	
	sprintf(newpath, "%s%s", alpm_option_get_dbpath(), "sync/");
	
	closedir(dir);
	dir = opendir(newpath);
	if(dir == NULL)
		return false;
	
	while((ent = readdir(dir)) != NULL) 
	{
		char path[4096];
		alpm_list_t *syncdbs = NULL, *i;
		int found = 0;
		char *dname = ent->d_name;

		if(!strcmp(dname, ".") || !strcmp(dname, ".."))
			continue;

		/* skip the local and sync directories */
		if(!strcmp(dname, "sync") || !strcmp(dname, "local")) 
			continue;

		syncdbs = alpm_option_get_syncdbs();
		for(i = syncdbs; i && !found; i = alpm_list_next(i)) 
		{
			pmdb_t *db = (pmdb_t *)alpm_list_getdata(i);
			found = !strcmp(dname, alpm_db_get_name(db));
		}
		
		/* We have a directory that doesn't match any syncdb.
		 * Ask the user if he wants to remove it. */
		if(!found) 
			if(rmrf(path)) 
				return false;
	}
	
	return true;
}

int AlpmHandler::rmrf(const char *path)
{
	int errflag = 0;
	struct dirent *dp;
	DIR *dirp;

	if(!unlink(path))
		return(0);
	else 
	{
		if(errno == ENOENT) 
			return(0);
		else if(errno == EPERM) { }
			/* fallthrough */
		else if(errno == EISDIR) { }
			/* fallthrough */
		else if(errno == ENOTDIR)
			return(1);
		else
			/* not a directory */
			return(1);

		if((dirp = opendir(path)) == (DIR *)-1)
			return(1);
		for(dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) 
		{
			if(dp->d_ino) 
			{
				char name[4096];
				sprintf(name, "%s/%s", path, dp->d_name);
				if(strcmp(dp->d_name, "..") && strcmp(dp->d_name, "."))
					errflag += rmrf(name);
			}
		}
		
		closedir(dirp);
		if(rmdir(path))
			errflag++;
		
		return(errflag);
	}
}

bool AlpmHandler::cleanCache(bool empty)
{
	alpm_list_t* cachedirs = alpm_option_get_cachedirs();
	QString cachedir((char *)alpm_list_getdata(cachedirs));

	if(!empty) 
	{
		qDebug() << "Incomplete Cleanup";
		/* incomplete cleanup */
		QDir dir(cachedir); 

		if(!dir.exists()) 
			return false;

		dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
		QFileInfoList list = dir.entryInfoList();

		for (int i = 0; i < list.size(); ++i)
		{

			pmpkg_t *localpkg = NULL, *dbpkg = NULL;
			QFileInfo fileInfo = list.at(i);

			/* attempt to load the package, skip file on failures as we may have
			 * files here that aren't valid packages. we also don't need a full
			 * load of the package, just the metadata. */
			if(alpm_pkg_load(fileInfo.absolutePath().toAscii().data(), 0, &localpkg) != 0 || localpkg == NULL)
				continue;

			/* check if this package is in the local DB */
			dbpkg = alpm_db_get_pkg(db_local, alpm_pkg_get_name(localpkg));
			if(dbpkg == NULL)
				/* delete package, not present in local DB */
				unlink(fileInfo.absolutePath().toAscii().data());
			else if(alpm_pkg_vercmp(alpm_pkg_get_version(localpkg),
					alpm_pkg_get_version(dbpkg)) != 0) 
				/* delete package, it was found but version differs */
				unlink(fileInfo.absolutePath().toAscii().data());

			/* else version was the same, so keep the package */
			/* free the local file package */
			alpm_pkg_free(localpkg);
		}
	} 
	else 
	{
		/* full cleanup */

		if(rmrf(cachedir.toAscii().data())) 
			return false;

		if(makepath(cachedir.toAscii().data())) 
			return false;
	}

	return true;
}

int AlpmHandler::makepath(const char *path)
{
	char *orig, *str, *ptr;
	char full[PATH_MAX+1] = "";
	mode_t oldmask;

	oldmask = umask(0000);

	orig = strdup(path);
	str = orig;
	while((ptr = strsep(&str, "/"))) 
	{
		if(strlen(ptr)) 
		{
			struct stat buf;

			strcat(full, "/");
			strcat(full, ptr);
			if(stat(full, &buf)) 
			{
				if(mkdir(full, 0755)) 
				{
					free(orig);
					umask(oldmask);
					return(1);
				}
			}
		}
	}
	free(orig);
	umask(oldmask);
	return(0);
}

QStringList AlpmHandler::getProviders(const QString &name, const QString &repo)
{
	alpm_list_t *provides;
	QStringList retlist;

	provides = alpm_pkg_get_provides(getPackageFromName(name, repo));

	while(provides != NULL)
	{
		retlist.append(QString((char *)alpm_list_getdata(provides)));
		provides = alpm_list_next(provides);
	}

	return retlist;
}

bool AlpmHandler::isProviderInstalled(const QString &provider)
{
	/* Here's what we need to do: iterate the installed
	 * packages, and find if something between them provides
	 * &provider
	 */
	
	alpm_list_t *localpack = alpm_db_getpkgcache(db_local);
	
	while(localpack != NULL)
	{
		QStringList prv(getProviders(QString(alpm_pkg_get_name(
				(pmpkg_t *)alpm_list_getdata(localpack))), QString("local")));
		
		for(int i = 0; i < prv.size(); ++i)
		{
			QStringList tmp(prv.at(i).split("="));
			if(!tmp.at(0).compare(provider))
			{
				qDebug() << "Provider is installed and it is" << alpm_pkg_get_name(
						(pmpkg_t *)alpm_list_getdata(localpack));
				return true;
			}
		}
		
		localpack = alpm_list_next(localpack);
	}
	
	return false;
}

unsigned long AlpmHandler::getPackageSize(pmpkg_t *package)
{
	return alpm_pkg_get_size(package);
}

unsigned long AlpmHandler::getPackageSize(const QString &name, const QString &repo)
{
	return getPackageSize(getPackageFromName(name, repo));
}

QString AlpmHandler::getAlpmVersion()
{
	QProcess *proc = new QProcess();
	proc->start("pacman --version");
	
	proc->waitForFinished();
	
	QByteArray btAr(proc->readAllStandardOutput());
	btAr.remove(0, btAr.indexOf("libalpm"));
	btAr.remove(0, 9);
	btAr.remove(5, btAr.length());
	
	QString version(btAr);
	
	return version;
}

void AlpmHandler::setuseragent()
{
	char agent[101];
	struct utsname un;

	uname(&un);
	snprintf(agent, 100, "shaman/" SHAMAN_VERSION " (%s %s) libalpm/%s",
			un.sysname, un.machine, getAlpmVersion().toAscii().data());
	setenv("HTTP_USER_AGENT", agent, 0);
}

QString AlpmHandler::getPackageVersion(pmpkg_t *package)
{
	return alpm_pkg_get_version(package);
}

QString AlpmHandler::getPackageVersion(const QString &name, const QString &repo)
{
	return getPackageVersion(getPackageFromName(name, repo));
}

QString AlpmHandler::getPackageRepo(const QString &name, bool checkver)
{
	alpm_list_t *syncdbs = alpm_option_get_syncdbs();
	for(alpm_list_t *i = syncdbs; i; i = alpm_list_next(i)) 
	{
		pmdb_t *db = (pmdb_t *)alpm_list_getdata(i);
		pmpkg_t *pkg;
		pkg = alpm_db_get_pkg(db, name.toAscii().data());
		if(pkg == NULL)
			continue;
		
		if(checkver && (alpm_pkg_get_version(pkg) == getPackageVersion(alpm_pkg_get_name(pkg), "local")))
			continue;
			
		return alpm_db_get_name(db);
	}
	
	return QString();
}