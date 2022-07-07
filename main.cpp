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
#include <cstring>
#include <cstdlib>

int main(int argc, char *argv[])
{
    // Check for startup args that might cause us to bail early
    if (argc >= 2 && !std::strcmp(argv[1], "--apply")) {
        return CalibrationUtils::applyExistingCalibration() ? EXIT_SUCCESS : EXIT_FAILURE;
    } else if (argc >= 2 && !std::strcmp(argv[1], "--apply-or-calibrate")) {
        if (CalibrationUtils::applyExistingCalibration()) {
            return EXIT_SUCCESS;
        }
    }

    // If we're not applying existing calibration, reset the stored calibration to default, just to be safe.
    if (!CalibrationUtils::applyDefaultCalibration()) {
        qCritical("Unable to apply default calibration to touchscreen prior to calibration.");
        return EXIT_FAILURE;
    }

    // Now load up the screen to do the calibration process
    QApplication a(argc, argv);
    CalibrationWindow w;
    w.showFullScreen();
    return a.exec();
}
