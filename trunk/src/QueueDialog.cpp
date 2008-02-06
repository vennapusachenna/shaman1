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

#include <iostream>
#include "../ui_transactionDialog.h"
#include <alpm.h>
#include "callbacks.h"
#include <QtGui>
#include <AlpmHandler.h>

#include "QueueDialog.h"

extern CallBacks CbackReference;

using namespace std;

QueueDialog::QueueDialog(AlpmHandler *hnd, QWidget *parent)
: QDialog(parent),
  aHandle(hnd)
{
	setupUi(this);
	setWindowModality(Qt::ApplicationModal);
	
	qRegisterMetaType<pmtransevt_t>("pmtransevt_t");
	
	connect(&CbackReference, SIGNAL(streamTransEvent(pmtransevt_t, void*, void*)),
			SLOT(changeStatus(pmtransevt_t, void*, void*)));
	
	checkTransaction->setText(tr("<b>Check transaction validity</b>"));
	
	progressBar->setRange(0, 1);
	progressBar->setValue(0);
	
	status = 0;
}

QueueDialog::~QueueDialog()
{
}

TrCommitThread::TrCommitThread(AlpmHandler *aH)
{
	aHandle = aH;
}

void QueueDialog::startProcessing()
{
	cTh = new TrCommitThread(aHandle);
	
	cTh->start();
	
	connect(cTh, SIGNAL(finished()), SLOT(cleanup()));
}

void TrCommitThread::run()
{
	aHandle->performCurrentTransaction();
}

void QueueDialog::changeStatus(pmtransevt_t event, void *data1, void *data2)
{
		
	switch(event) {
		case PM_TRANS_EVT_CHECKDEPS_START:
		  actionDetail->setText(QString(tr("Validating Dependencies...")));
			break;
		case PM_TRANS_EVT_FILECONFLICTS_START:
			actionDetail->setText(QString(tr("Checking for Conflicts...")));
			break;
		case PM_TRANS_EVT_RESOLVEDEPS_START:
			actionDetail->setText(QString(tr("Resolving Dependencies...")));
			break;
		case PM_TRANS_EVT_INTERCONFLICTS_START:
			actionDetail->setText(QString(tr("Looking for Inter-Conflicts...")));
			break;
		case PM_TRANS_EVT_ADD_START:
			if(status == 1)
			{
				status = 2;
				startProcess();
			}
			actionDetail->setText(QString(tr("Installing %1...")).arg(alpm_pkg_get_name((pmpkg_t *)data1)));
			break;
		case PM_TRANS_EVT_ADD_DONE:
			actionDetail->setText(QString(tr("installed %1 (%2)")).arg(
					alpm_pkg_get_name((pmpkg_t *)data1)).arg(alpm_pkg_get_version((pmpkg_t *)data1)));
			//alpm_logaction(str);
			break;
		case PM_TRANS_EVT_REMOVE_START:
			if(status == 1)
			{
				status = 2;
				startProcess();
			}
			actionDetail->setText(QString(tr("Removing %1...")).arg(alpm_pkg_get_name((pmpkg_t *)data1)));
			break;
		case PM_TRANS_EVT_REMOVE_DONE:
			actionDetail->setText(QString(tr("Removed %1 (%2)")).
					arg(alpm_pkg_get_name((pmpkg_t *)data1)).arg(alpm_pkg_get_version((pmpkg_t *)data1)));
			//alpm_logaction(str);
			break;
		case PM_TRANS_EVT_UPGRADE_START:
			if(status == 1)
			{
				status = 2;
				startProcess();
			}
			actionDetail->setText(QString(tr("Upgrading %1...")).arg(alpm_pkg_get_name((pmpkg_t *)data1)));
			break;
		case PM_TRANS_EVT_UPGRADE_DONE:
			actionDetail->setText(QString(tr("Upgraded %1 (%2 -> %3)")).arg(
			         (char *)alpm_pkg_get_name((pmpkg_t *)data1)).arg((char *)alpm_pkg_get_version((pmpkg_t *)data2)).
			         arg((char *)alpm_pkg_get_version((pmpkg_t *)data1)));
			//alpm_logaction(str);
			break;
		case PM_TRANS_EVT_INTEGRITY_START:
			if(status == 1)
			{
				status = 2;
				startProcess();
			}
			actionDetail->setText(QString(tr("Checking package integrity...")));
			break;
		case PM_TRANS_EVT_DELTA_INTEGRITY_START:
			actionDetail->setText(QString(tr("Checking delta integrity...")));
			break;
		case PM_TRANS_EVT_DELTA_PATCHES_START:
			actionDetail->setText(QString(tr("Applying deltas...")));
			break;
		case PM_TRANS_EVT_DELTA_PATCH_START:
			actionDetail->setText(QString(tr("Generating %1 with %2... ")).arg((char *)data1).
					arg((char *)data2));
			break;
		case PM_TRANS_EVT_DELTA_PATCH_DONE:
			actionDetail->setText(QString(tr("Success!")));
			break;
		case PM_TRANS_EVT_DELTA_PATCH_FAILED:
			actionDetail->setText(QString(tr("Failed!")));
			break;
		case PM_TRANS_EVT_SCRIPTLET_INFO:
			actionDetail->setText(QString("%s").arg((char*)data1));
			break;
		case PM_TRANS_EVT_PRINTURI:
			actionDetail->setText(QString("%s/%s").arg((char*)data1).arg((char*)data2));
			break;
		case PM_TRANS_EVT_RETRIEVE_START:
			actionDetail->setText(QString(tr("Starting downloading packages from %1...")).arg((char*)data1));
			if(status == 0)
			{
				status = 1;
				startDownload();
			}
			break;
		/* all the simple done events, with fallthrough for each */
		case PM_TRANS_EVT_FILECONFLICTS_DONE:
		case PM_TRANS_EVT_EXTRACT_DONE:
		case PM_TRANS_EVT_CHECKDEPS_DONE:
		case PM_TRANS_EVT_RESOLVEDEPS_DONE:
		case PM_TRANS_EVT_INTERCONFLICTS_DONE:
		case PM_TRANS_EVT_INTEGRITY_DONE:
		case PM_TRANS_EVT_DELTA_INTEGRITY_DONE:
		case PM_TRANS_EVT_DELTA_PATCHES_DONE:
			break;
	}
}

void QueueDialog::updateProgressBar(char *c, int bytedone, int bytetotal, int speed,
		int listdone, int listtotal, int speedtotal)
{
	Q_UNUSED(speedtotal);

	double bt = bytetotal/1024;
	double bd = bytedone/1024;

	unsigned int eta_h = 0, eta_m = 0, eta_s = 0;

	eta_s = (listtotal - listdone) / (speedtotal * 1024.0);
	eta_h = eta_s / 3600;
	eta_s -= eta_h * 3600;
	eta_m = eta_s / 60;
	eta_s -= eta_m * 60;

	progressBar->setFormat(QString("%p% (%1 KB/s, %4:%5:%6 remaining)").
			arg(speed).arg((int)eta_h,2,10,QChar('0')).arg((int)eta_m,2,10,QChar('0')).
			arg((int)eta_s,2,10,QChar('0')));
	progressBar->setRange(0, listtotal);
	progressBar->setValue(listdone);


	if(bytetotal > 2048)
		actionDetail->setText(QString(tr("Downloading %1... (%2 MB of %3 MB)")).
				arg(c).arg(bd/1024, 0, 'f', 2).arg(bt/1024, 0, 'f', 2));
	else
		actionDetail->setText(QString(tr("Downloading %1... (%2 KB of %3 KB)")).
				arg(c).arg(bd, 0, 'f', 0).arg(bt, 0, 'f', 0));
}

void QueueDialog::updateProgressBar(pmtransprog_t event, char *pkgname, int percent,
        int howmany, int remain)
{
	Q_UNUSED(pkgname);
	Q_UNUSED(event);
	Q_UNUSED(percent);
	
	progressBar->setFormat("%p");
	progressBar->setRange(0,howmany);
	progressBar->setValue(remain);
}

void QueueDialog::startDownload()
{
	checkTransaction->setText(tr("Check transaction validity"));
	downloadPackages->setText(tr("<b>Download Packages</b>"));
	
	connect(&CbackReference, SIGNAL(streamTransDlProg(char*,int,int,int,int,int,int)), 
			SLOT(updateProgressBar(char*,int,int,int,int,int,int)));
}

void QueueDialog::startProcess()
{
	downloadPackages->setText(tr("Download Packages"));
	processingQueue->setText(tr("<b>Process queue</b>"));
	
	disconnect(&CbackReference, SIGNAL(streamTransDlProg(char*,int,int,int,int,int,int)), 0, 0);
	qRegisterMetaType<pmtransprog_t>("pmtransprog_t");
	connect(&CbackReference, SIGNAL(streamTransProgress(pmtransprog_t,char*,int,int,int)),
			SLOT(updateProgressBar(pmtransprog_t,char*,int,int,int)));
}

void QueueDialog::cleanup()
{
	disconnect(&CbackReference, SIGNAL(streamTransProgress(pmtransprog_t,char*,int,int,int)), 0, 0);
	processingQueue->setText(tr("Process queue"));
	cleaningUp->setText(tr("<b>Cleanup</b>"));
	
	emit terminated(false);
}
