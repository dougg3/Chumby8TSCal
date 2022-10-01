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
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>

/// The name to look for in order to identify the touchscreen
#define CHUMBY_TOUCHSCREEN_NAME         "Chumby 8 touchscreen"
/// File used for storing touchscreen calibration
#define CALIBRATION_FILE                "/mnt/settings/touchscreen.conf"
/// Name of the X11 input device property containing the calibration matrix
#define LIBINPUT_CALIBRATION_PROPERTY   "libinput Calibration Matrix"

/// Keeps track of whether we received an error from X11
static volatile int xErrorOccurred = false;

/**
 * @brief Custom error handler for Xlib during touchscreen calibration
 * @param display The display the error occurred on
 * @param err Info about the error
 * @return Anything; ignored by Xlib
 */
static int touchScreenCalibrationErrorHandler(Display *display, XErrorEvent *err)
{
    Q_UNUSED(display);
    Q_UNUSED(err);

    // Mark that an error did indeed occur; applyCalibration will detect if this handler has run
    xErrorOccurred = true;

    return 0;
}

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
 * @param matrix The new 3x3 calibration matrix for libinput (top row, middle row, bottom row sequentially)
 * @return True on success, false on failure
 */
bool CalibrationUtils::applyCalibration(QVector<float> const &matrix)
{
    bool retval = false;
    bool found = false;
    XID deviceID;
    XDevice *device;
    Atom matrixAtom;
    Atom floatAtom;
    XErrorHandler prevErrorHandler;

    // Ensure there are exactly 9 entries in the calibration matrix
    if (matrix.length() != 9) {
        return false;
    }

    // Reset our error status
    xErrorOccurred = false;

    // We are going to talk to the X server to modify the touchscreen's
    // calibration matrix property live.
    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        // Couldn't open the display; something failed.
        return false;
    }

    // Install a special error handler because of how Xlib works. This feels icky,
    // but it's the only way that Xlib can tell us if setting the calibration
    // was successful. Save the old handler (likely installed by Qt) so we can
    // restore it when we're done.
    prevErrorHandler = XSetErrorHandler(touchScreenCalibrationErrorHandler);

    // We need to find the touchscreen's device in X. Get a list of all of them
    int deviceCount;
    XDeviceInfo *devices = XListInputDevices(display, &deviceCount);
    if (!devices) {
        // If we couldn't get a device list, something's wrong.
        goto exit_close_display;
    }

    // Now loop through them and try to find one that matches
    for (int i = 0; !found && i < deviceCount; i++) {
        QString deviceName(devices[i].name);
        if (deviceName == CHUMBY_TOUCHSCREEN_NAME) {
            // We found it!
            deviceID = devices[i].id;
            found = true;
        }
    }
    XFreeDeviceList(devices);
    if (!found) {
        goto exit_close_display;
    }

    // Now, we need to open the device
    device = XOpenDevice(display, deviceID);
    if (!device) {
        goto exit_close_display;
    }

    // Find the property
    matrixAtom = XInternAtom(display, LIBINPUT_CALIBRATION_PROPERTY, true);
    if (matrixAtom == None)
    {
        goto exit_close_device;
    }

    // We also need to be able to set "float" properties which doesn't seem
    // to be built into X11, so grab the atom representing it
    floatAtom = XInternAtom(display, "FLOAT", true);
    if (floatAtom == None)
    {
        goto exit_close_device;
    }

    // Now we can finally attempt to set the new property value of 9 floats
    XChangeDeviceProperty(display, device, matrixAtom, floatAtom, 32, PropModeReplace,
                          reinterpret_cast<const unsigned char *>(matrix.constData()), matrix.length());
    // So Xlib is kind of confusing for setting properties. There is no error
    // handling here. If setting this property fails, this function doesn't tell
    // me. Instead, an error handler will run sometime in the future. So we had
    // to register a custom error handler above to let us know if it failed. Crazy...

    // So assume we succeeded here, and we'll check for Xlib errors when we return.
    retval = true;

exit_close_device:
    XCloseDevice(display, device);

exit_close_display:
    // All done with the display
    XCloseDisplay(display);

    // Restore the original error handler; if an error happened, we've already been notified
    // by the time we reach this point.
    XSetErrorHandler(prevErrorHandler);

    // As long as we think we succeeded, and no X errors occurred while we were doing
    // our thing, we succeeded.
    return retval && !xErrorOccurred;
}

/**
 * @brief Saves new calibration parameters to disk
 * @param matrix The new 3x3 calibration matrix for libinput (top row, middle row, bottom row sequentially)
 * @return True on success, false on failure
 */
bool CalibrationUtils::saveNewCalibration(QVector<float> const &matrix)
{
    // Ensure there are exactly 9 entries in the calibration matrix
    if (matrix.length() != 9) {
        return false;
    }

    // To save it, we need to write a new file for configuring the touchscreen.
    QByteArray const calibrationFilePrefix =
        "Section \"InputClass\"\n"
        "\tIdentifier \"touchscreen\"\n"
        "\tMatchIsTouchscreen \"TRUE\"\n"
        "\tMatchDriver \"libinput\"\n"
        "\tOption \"CalibrationMatrix\" \"";

    QByteArray const calibrationFileSuffix =
        "\"\n"
        "EndSection\n";

    // Assemble the new file contents using the provided matrix
    QByteArray outData = calibrationFilePrefix;
    for (int i = 0; i < matrix.length(); i++) {
        if (i > 0) {
            outData += " ";
        }
        outData += QByteArray::number(matrix[i], 'f', 6);
    }
    outData += calibrationFileSuffix;

    // Save the new file
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

    return true;
}
