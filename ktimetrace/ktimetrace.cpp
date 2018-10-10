/***************************************************************************
                          ktimetrace.cpp  -  description
                             -------------------
    begin                : Tue May 23 17:09:23 CDT 2000
    copyright            : (C) 2000 by Frank Mori Hess
    email                : fmhess@uiuc.edu
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ktimetrace.h"

#include <kapp.h>
#include <kicontheme.h>
#include <kiconloader.h>
#include <kmenubar.h>
#include <kglobal.h>

#include <qmessagebox.h>
#include <qtimer.h>

#include <iostream>

#include "dialog.h"
#include "bufferdialog.h"
#include "zoomdialog.h"
#include "ktracecolordialog.h"

KTraceApp::KTraceApp(QWidget *parent, const char *name) : KMainWindow(parent, name)
{
	resize(640, 480);

	// init tool bar
	myToolBar = new KToolBar(this, QMainWindow::Top);
	myToolBar->insertButton(KGlobal::iconLoader()->loadIcon("start", KIcon::Toolbar), ID_START, true, "Start a time trace");
	myToolBar->insertButton(KGlobal::iconLoader()->loadIcon("stop", KIcon::Toolbar), ID_STOP, true, "Stop time trace");
	connect(myToolBar, SIGNAL(clicked(int)), this, SLOT(commandCallback(int)));

	// init menu bar
	controlMenu = new QPopupMenu;
	controlMenu->insertItem(KGlobal::iconLoader()->loadIcon("start", KIcon::Small), "Start aquisition", ID_START);
	controlMenu->insertItem(KGlobal::iconLoader()->loadIcon("stop", KIcon::Small), "&Interrupt aquisition", ID_STOP);
	controlMenu->insertSeparator();
	controlMenu->insertItem(KGlobal::iconLoader()->loadIcon("exit", KIcon::Toolbar), "E&xit", ID_QUIT);
	connect(controlMenu, SIGNAL(activated(int)), this, SLOT(commandCallback(int)));

	deviceMenu = new QPopupMenu;
	deviceMenu->setCheckable(true);
	deviceMenu->insertItem("/dev/comedi0", ID_COMEDI_0);
	deviceMenu->insertItem("/dev/comedi1", ID_COMEDI_1);
	deviceMenu->insertItem("/dev/comedi2", ID_COMEDI_2);
	deviceMenu->insertItem("/dev/comedi3", ID_COMEDI_3);
	connect(deviceMenu, SIGNAL(activated(int)), this, SLOT(commandCallback(int)));

	settingsMenu = new QPopupMenu;
	settingsMenu->insertItem("&Device", deviceMenu);
	settingsMenu->insertItem("Comedi &Buffer...", this, SLOT(slotBuffer()));

	viewMenu = new QPopupMenu;
	viewMenu->insertItem("&Colors...", this, SLOT(slotColors()));
	viewMenu->insertItem("&Zoom...", this, SLOT(slotZoom()));

	myHelpMenu = new QPopupMenu;
	myHelpMenu->insertItem(KGlobal::iconLoader()->loadIcon("contexthelp.xpm", KIcon::Small),
		"What's &This", this, SLOT(whatsThis()), SHIFT + Key_F1);
	myHelpMenu->insertSeparator();
	myHelpMenu->insertItem("&About...", this, SLOT(slotAbout()));

	menuBar()->insertItem("&Control", controlMenu);
	menuBar()->insertItem("&Settings", settingsMenu, ID_DEVICE_MENU);
	menuBar()->insertItem("&View", viewMenu, ID_VIEW_MENU);
	menuBar()->insertItem("&Help", myHelpMenu, ID_HELP_MENU);

	view = new KTraceView(this);
	view->loadConfig(kapp->config());
	setCentralWidget(view);

	data.ADC(&dataCard);
	data.loadConfig(kapp->config());
	data.setView(view);

	// look for an analog input device
	unsigned int i;
	for(i = 0; i < maxDev; i++)
	{
		if(data.setDevice(i) == 0)
		{
			deviceMenu->setItemChecked(deviceMenu->idAt(i), true);
			view->fullScale(dataCard.maxData());
			break;
		}
	}
	// if we failed to find any good analog inputs
	if(i == maxDev)
	{
		static QMessageBox warning("Warning",
			"No available asynchronous analog input subdevices were found.\n"
			PACKAGE " will not function.", QMessageBox::Warning,
			QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton,
			this);
		QTimer::singleShot(0, &warning , SLOT( show() ) );
	}

	setCaption(PACKAGE " " VERSION);

	// initialize state of various menu options and toolbar buttons
	applyMainWindowSettings(kapp->config(), "window");
	myToolBar->show();	// make sure toolbar is not hidden
	setControlsEnabled(false);

	// set timer up to update status led once per second
	statusTimer = new QTimer(this);
	statusTimer->start(1000);	
	connect(statusTimer, SIGNAL(timeout()), SLOT(slotUpdateLED()));
}

KTraceApp::~KTraceApp()
{
	delete deviceMenu;
}

void KTraceApp::defaultCaption()
{
	const KTTSettings *settings = &data.settings;

	QString caption = "";
	caption += PACKAGE;
	caption += " ";
	caption += VERSION;
	caption += " : ";
	if(settings->saveData)
	{
		caption += settings->directory.absPath();
		caption += '/';
		QString fileName;
		fileName.sprintf("%s%.3i", (const char*)settings->fileStem, settings->fileNum);
		caption += fileName;
	} else
	{
		caption += "no file";
	}
	setCaption(caption);
}

bool KTraceApp::queryClose()
{
	if(data.writeDone() == false)
	{
		data.stop();
		QMessageBox::information(this, "Information",
			"Writing of data files is still in progress.\n"
			"This may cause a delay before the application closes.");
	}

	return true;
}

bool KTraceApp::queryExit()
{
	// save configuration
	saveConfig();
	
	// wait until all file writing threads have finished
	if(data.writeDone() == false)
	{
		std::cerr << "waiting for data file save to complete..." << std::endl;
		while(data.writeDone() == false)
		{
			usleep(100000);
		}
		std::cerr << "data save completed" << std::endl;
	}
	// work-around for qt bug
	exit(0);

	return true;
}

void KTraceApp::startTrace()
{
	if(dataCard.good() == false)
	{
		QMessageBox::warning(this, "Warning",
			"No analog input device is configured");
		return;
	}
	KTraceDialog *dialog = new KTraceDialog(data.settings, &dataCard, this);
	if(dialog->exec())
	{
		setControlsEnabled(true);
		data.settings = *dialog->settings;
		defaultCaption();
		data.collect();
	}
	delete dialog;
	return;
}

void KTraceApp::stopTrace()
{
	data.stop();
}

void KTraceApp::commandCallback(int id)
{
	switch(id)
	{
		case ID_START:
			startTrace();
			break;
		case ID_STOP:
			stopTrace();
			break;
		case ID_COMEDI_0:
		case ID_COMEDI_1:
		case ID_COMEDI_2:
		case ID_COMEDI_3:
			setDevice(deviceMenu->indexOf(id));
			break;
		case ID_QUIT:
			close();
			kapp->exit(0);
			break;
		default:
			break;
	}
}

void KTraceApp::slotBuffer()
{
	if(dataCard.good() == false)
	{
		QMessageBox::warning(this, "Warning",
			"No analog input device is configured");
		return;
	}

	BufferDialog *dialog = new BufferDialog(&dataCard, this);
	dialog->exec();
	delete dialog;
}

void KTraceApp::slotZoom()
{
	ZoomDialog *dialog = new ZoomDialog(this);

	// initialize dialog with current zoom values
	dialog->hZoom(view->hZoom());
	if(dialog->exec())
	{
		// write new zoom value
		view->hZoom(dialog->hZoom());
	}
	delete dialog;
}

void KTraceApp::slotColors()
{
	KTraceColorDialog *dialog = new KTraceColorDialog(this);
	dialog->fgColor(view->fgColor());
	dialog->bgColor(view->bgColor());
	if(dialog->exec())
	{
		view->fgColor(dialog->fgColor());
		view->bgColor(dialog->bgColor());
	}
	delete dialog;
}

void KTraceApp::slotAbout()
{
	QMessageBox::about(this, "About", PACKAGE " " VERSION "\n"
		"Home page: http://ktimetrace.sourceforge.net\n"
		"Author: Frank Mori Hess <fmhess@users.sourceforge.net>");
}

void KTraceApp::slotUpdateLED()
{
	pthread_mutex_lock(&aquisitionThreadCountLock);
	view->setWriteIndicator(aquisitionThreadCount);
	pthread_mutex_unlock(&aquisitionThreadCountLock);

	view->setDeviceIndicator(dataCard.good());
}

void KTraceApp::setControlsEnabled(bool go)
{
	toolBar()->setItemEnabled(ID_START, !go);
	toolBar()->setItemEnabled(ID_STOP, go);
	toolBar()->setEnableContextMenu(!go);
	controlMenu->setItemEnabled(ID_START, !go);
	controlMenu->setItemEnabled(ID_STOP, go);
	controlMenu->setItemEnabled(ID_QUIT, !go);
	menuBar()->setItemEnabled(ID_DEVICE_MENU, !go);	
	menuBar()->setItemEnabled(ID_VIEW_MENU, !go);	
	menuBar()->setItemEnabled(ID_HELP_MENU, !go);	
}

int KTraceApp::setDevice(unsigned int devNum)
{
	QString devName;

	if(devNum >= maxDev)
	{
		return -1;
	}
	// uncheck devices in devMenu
	for(unsigned int i = 0; i < maxDev; i++)
		deviceMenu->setItemChecked(deviceMenu->idAt(i), false);
	if(data.setDevice(devNum) == 0)
	{
		deviceMenu->setItemChecked(deviceMenu->idAt(devNum), true);
		view->fullScale(dataCard.maxData());
	}else
	{
		devName.setNum(devNum);
		devName = "/dev/comedi" + devName;
		QMessageBox::warning(this, "Warning",
			"No asynchronous analog input subdevice was found\n"
			"on " + devName + ".  Choose a different device.");
		return -1;
	}
	return devNum;
}

void KTraceApp::saveConfig()
{
	// save configuration
	saveMainWindowSettings(kapp->config(), "window");
	data.saveConfig(kapp->config());
	view->saveConfig(kapp->config());
	kapp->config()->sync();
}
