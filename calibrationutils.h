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

#ifndef CALIBRATIONUTILS_H
#define CALIBRATIONUTILS_H

#include <QVector>

/// The range of raw samples from the uncalibrated touchscreen
#define RAW_TOUCHSCREEN_RANGE             4095

/**
 * @brief Utility functions for calibrating the Chumby 8's touchscreen
 */
class CalibrationUtils
{
public:
    static int findTouchScreen();
    static bool applyCalibration(QVector<float> const &matrix);
    static bool saveNewCalibration(QVector<float> const &matrix);
};

#endif // CALIBRATIONUTILS_H
