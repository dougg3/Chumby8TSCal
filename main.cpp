/* Copyright (C) 2022 Doug Brown
 *
 * This file is part of Chumby8TSCal.
 *
 * Chumby8TSCal is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Chumby8TSCal is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "calibrationwindow.h"
#include "calibrationutils.h"

#include <QApplication>
#include <QDesktopWidget>
#include <cstring>
#include <cstdlib>

int main(int argc, char *argv[])
{
    // Now load up the screen to do the calibration process
    QApplication a(argc, argv);
    CalibrationWindow w;
    w.showFullScreen();
    // If there isn't a window manager running, showFullScreen() doesn't resize
    // the window to full-screen properly. So make sure we're the correct size,
    // even if there isn't a window manager running.
    w.setGeometry(0, 0, a.desktop()->size().width(), a.desktop()->size().height());
    return a.exec();
}
