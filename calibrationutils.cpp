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

#include "calibrationutils.h"
#include <QString>
#include <QDir>
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>

/// The name to look for in order to identify the touchscreen
#define CHUMBY_TOUCHSCREEN_NAME     "Chumby 8 touchscreen"
/// File used for storing touchscreen calibration
#define CALIBRATION_FILE            "/etc/touchscreencal"

/**
 * @brief Finds the touchscreen, and returns an opened file descriptor to it if found
 * @return The file descriptor if found, or -1 if it's not found
 */
int CalibrationUtils::findTouchScreen()
{
    // Search /dev/input and try to find the touchscreen
    QDir inputDir("/dev/input");
    QStringList inputs = inputDir.entryList(QDir::System);
    for (QString const &inputDevice : inputs)
    {
        QString fullPath = inputDir.absoluteFilePath(inputDevice);
        int fd = ::open(fullPath.toUtf8().constData(), O_RDONLY | O_NONBLOCK);
        if (fd > 0) {
            char name[32];
            if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0) {
                // Ensure it's null-terminated to be safe
                name[sizeof(name)-1] = 0;
            } else {
                name[0] = 0;
            }

            // If we found it, return it
            if (!::strncmp(name, CHUMBY_TOUCHSCREEN_NAME, sizeof(name))) {
                return fd;
            }

            // Otherwise, close it
            ::close(fd);
        }

    }

    qCritical("Unable to locate Chumby touchscreen");
    return -1;
}

/**
 * @brief Applies the supplied calibration system-wide
 * @param xmin Minimum valid raw X reading
 * @param xmax Maximum valid raw X reading
 * @param ymin Minimum valid raw Y reading
 * @param ymax Maximum valid raw Y reading
 * @return True on success, false on failure
 *
 * Note: This is a very simplistic calibration. It doesn't support any kind of rotation
 * or skewing. So it assumes that all we have to do is modify the X and Y min/max in order
 * to calibrate. This seems to work decently enough for the Chumby 8's touchscreen. For
 * anything more complicated, it would probably make more sense to use tslib instead.
 * This is the bare minimum required for Qt's evdevtouch plugin to work properly.
 */
bool CalibrationUtils::applyCalibration(int xmin, int xmax, int ymin, int ymax)
{
    // Find the touchscreen
    int tsfd = findTouchScreen();
    if (tsfd < 0) {
        return false;
    }

    // Apply the new calibration as absolute min/max, if possible
    bool anythingFailed = false;
    struct input_absinfo absval;

    // X axis calibration
    if (ioctl(tsfd, EVIOCGABS(ABS_X), &absval) >= 0) {
        absval.minimum = xmin;
        absval.maximum = xmax;
        if (ioctl(tsfd, EVIOCSABS(ABS_X), &absval) < 0) {
            qCritical("Unable to set new X axis calibration");
            anythingFailed = true;
        }
    } else {
        qCritical("Unable to get existing X axis calibration");
        anythingFailed = true;
    }

    // Y axis calibration
    if (ioctl(tsfd, EVIOCGABS(ABS_Y), &absval) >= 0) {
        absval.minimum = ymin;
        absval.maximum = ymax;
        if (ioctl(tsfd, EVIOCSABS(ABS_Y), &absval) < 0) {
            qCritical("Unable to set new Y axis calibration");
            anythingFailed = true;
        }
    } else {
        qCritical("Unable to get existing Y axis calibration");
        anythingFailed = true;
    }

    // Close the file descriptor
    ::close(tsfd);

    return !anythingFailed;
}

/**
 * @brief Reads the existing calibration and applies it
 * @return true on success, false on failure
 */
bool CalibrationUtils::applyExistingCalibration()
{
    if (!QFile::exists(CALIBRATION_FILE)) {
        qCritical("No calibration file exists");
        return false;
    }

    // Read the contents
    QFile calFile(CALIBRATION_FILE);
    if (!calFile.open(QFile::ReadOnly)) {
        qCritical("Unable to open calibration file");
        return false;
    }
    QByteArray calContent = calFile.readAll();
    calFile.close();

    // Parse the contents
    QList<QByteArray> components = calContent.split(' ');
    if (components.count() < 4) {
        qCritical("Not enough components in calibration file");
        return false;
    }
    QList<int> calibrations;
    for (int i = 0; i < 4; i++) {
        bool ok = false;
        int cal = components[i].toInt(&ok);
        if (!ok) {
            qCritical("Component %d in calibration file is not a number", i + 1);
            return false;
        }
        calibrations << cal;
    }

    // Attempt to apply them
    return applyCalibration(calibrations[0], calibrations[1],
                            calibrations[2], calibrations[3]);
}

/**
 * @brief Saves new calibration parameters to disk
 * @param xmin The minimum raw X coordinate
 * @param xmax The maximum raw X coordinate
 * @param ymin The minimum raw Y coordinate
 * @param ymax The maximum raw Y coordinate
 * @return True on success, false on failure
 */
bool CalibrationUtils::saveNewCalibration(int xmin, int xmax, int ymin, int ymax)
{
    // The filesystem is read-only, so we will have to briefly mount it read/write.
    // This is super ugly, but figuring out how to preserve the existing mount flags
    // using the mount() system call is kind of tricky...so it's easier just to call
    // out and let the "mount" executable handle it for us.
    if (::system("mount -oremount,rw /") != 0) {
        qCritical("Unable to remount rootfs read/write for calibration save");
        return false;
    }

    QByteArray outData = QString("%1 %2 %3 %4").arg(xmin).arg(xmax).arg(ymin).arg(ymax).toUtf8();
    QFile calFile(CALIBRATION_FILE);
    if (!calFile.open(QFile::WriteOnly | QFile::Truncate)) {
        qCritical("Unable to open calibration file for saving");
        return false;
    }
    qint64 written = calFile.write(outData);
    calFile.close();

    if (written != outData.length()) {
        qCritical("Unable to write calibration file");
        return false;
    }

    // Go back to read-only. Also ugly...same reasoning as above.
    if (::system("mount -oremount,ro /") != 0) {
        qWarning("Unable to remount rootfs read-only after calibration save, but save succeeded");
    }

    return true;
}
