//  This file is part of ktimetrace.

//  ktimetrace is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.

//  ktimetrace is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.

//  You should have received a copy of the GNU General Public License
//  along with ktimetrace.  If not, see <https://www.gnu.org/licenses/>.

//  (C) 2001 by Frank Mori Hess <fmhess@uiuc.edu>
//  (C) 2018 by Helio Chissini de Castro <helio@kde.org>

#ifndef KTRACEAPP_H
#define KTRACEAPP_H

#include <QMainWindow>
#include <QList>

#include "data.h"
#include "resource.h"

class QAction;
class QPopupMenu;
class QTimer;
class QToolBar;

class KTraceView;

class KTraceApp : public QMainWindow
{
	Q_OBJECT

public:
	KTraceApp();

	/* creates a caption for the main window and displays it */
	void defaultCaption();
	/* our daq card object - this shouldn't need to be public! */
	adc dataCard;
	/* turns various gui controls on and off according to whether or not
	 * we are currently aquiring data*/
	void setControlsEnabled(bool go);
protected:
	virtual bool queryClose();
	void queryExit();
private:
	void createActions();
	void createToolbars();
	void createMenus();
	/* sets the /dev/comediX device we want to use */
	int setDevice(unsigned int);
	// save settings to confuration file
	void saveConfig();

	/* control menu on menubar */
	QMenu *controlMenu;
	/* settings menu on menubar */
	QMenu *settingsMenu;
	/* device submenu in settings menu */
	QMenu *deviceMenu;
	/* view menu on menubar */
	QMenu *viewMenu;
	/* help menu on menubar */
	QMenu *myHelpMenu;
	/* the main graphical display widget */
	KTraceView *view;
	/* our poorly thought-out catch-all class*/
	KTTEngine data;
	/* used to periodically update the status led */
	QTimer *statusTimer;
	/* total number of possible /dev/comediX files */
	static const unsigned int maxDev = 4;

	// Actions
	QAction *startTraceAction;
	QAction *stopTraceAction;
	QAction *aboutAction;
	QAction *whatsThisAction;
	QAction *colorsAction;
	QAction *zoomAction;
	QAction *bufferAction;
	QList<QAction *> devicesActionList;
	QAction *exitAction;

	// Toolbars
	QToolBar *mainToolBar;

private slots:
	void startTrace();
	void stopTrace();
	// handles 'comedi buffer' item on settings menu
	void slotBuffer();
	// handles 'zoom' item on view menu
	void slotZoom();
	// handles 'colors' item on view menu
	void slotColors();
	// handles 'about' item on help menu
	void slotAbout();
	// updates LED indicators
	void slotUpdateLED();
};

#endif
