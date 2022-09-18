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
#include <QPainter>
#include <QScreen>
#include <linux/input.h>
#include <unistd.h>
#include <QTimer>

/// Offset (in pixels) from edges of screen to centers of calibration crosshairs
#define CROSSHAIR_OFFSET                20
/// Width/height of calibration crosshairs in pixels
#define CROSSHAIR_SIZE                  20
/// Number of points needed for calibration
#define NUM_CAL_POINTS                  4
/// Maximum number of samples to average for each calibration point
#define MAX_AVG_POINTS                  5

/**
 * @brief Constructor for CalibrationWindow
 * @param parent The parent widget, which should be nullptr because this is a full-screen window.
 */
CalibrationWindow::CalibrationWindow(QWidget *parent) :
    QMainWindow(parent),
    _instructionsLabel(this),
    _curCalPoint(0),
    _calibrationNotifier(nullptr),
    _tmpPressed(0),
    _touchIsPressed(false),
    _minXCal(0),
    _maxXCal(0),
    _minYCal(0),
    _maxYCal(0)
{
    // Fill out the four points
    QSize screenSize = qApp->screens().at(0)->size();
    _crosshairPoints << QPoint(CROSSHAIR_OFFSET, CROSSHAIR_OFFSET);
    _crosshairPoints << QPoint(screenSize.width() - CROSSHAIR_OFFSET, CROSSHAIR_OFFSET);
    _crosshairPoints << QPoint(screenSize.width() - CROSSHAIR_OFFSET, screenSize.height() - CROSSHAIR_OFFSET);
    _crosshairPoints << QPoint(CROSSHAIR_OFFSET, screenSize.height() - CROSSHAIR_OFFSET);

    // Add explanation text
    _instructionsLabel.setText("To calibrate the touchscreen, tap each crosshair point that appears.");
    _instructionsLabel.setStyleSheet("font-size: 20px;");
    _instructionsLabel.setAlignment(Qt::AlignCenter);

    // Find the touchscreen and listen for it
    _calibrationFd = CalibrationUtils::findTouchScreen();
    if (_calibrationFd < 0) {
        _instructionsLabel.setText("Unable to find touchscreen.");
        _curCalPoint = NUM_CAL_POINTS;
        // Since the touchscreen isn't working, bail after 5 seconds
        QTimer::singleShot(5000, qApp, &QApplication::quit);
    } else {
        // As long as we successfully opened up the touchscreen, listen for raw events
        _calibrationNotifier = new QSocketNotifier(_calibrationFd, QSocketNotifier::Read, this);
        connect(_calibrationNotifier, &QSocketNotifier::activated, this, &CalibrationWindow::readRawEvents);
        _calibrationNotifier->setEnabled(true);
    }
}

/**
 * @brief Destructor for CalibrationWindow
 */
CalibrationWindow::~CalibrationWindow()
{
    // Nothing to do; Qt's parenting system takes care of everything.
}

/**
 * @brief Handler called when this window needs to redraw itself
 */
void CalibrationWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    p.setPen(QPen(Qt::black, 1.0f));

    // Draw the crosshair at the current calibration point (if any)
    if (_curCalPoint < _crosshairPoints.length())
    {
        QPoint const &point = _crosshairPoints[_curCalPoint];
        p.drawLine(point.x() - CROSSHAIR_SIZE/2, point.y(),
                   point.x() + CROSSHAIR_SIZE/2, point.y());
        p.drawLine(point.x(), point.y() - CROSSHAIR_SIZE/2,
                   point.x(), point.y() + CROSSHAIR_SIZE/2);
    }
}

/**
 * @brief Handler called when the window is resized (mainly right after display)
 */
void CalibrationWindow::resizeEvent(QResizeEvent *)
{
    // Make sure the instructions label is correctly sized
    _instructionsLabel.setGeometry(rect());
}

/**
 * @brief Reads raw events from the touchscreen when available and parses them
 */
void CalibrationWindow::readRawEvents()
{
    // Read events as long as we can
    ssize_t result;
    do {
        input_event event;
        result = ::read(_calibrationFd, &event, sizeof(input_event));
        if (result == sizeof(input_event)) {
            switch (event.type) {
            case EV_KEY:
                // Look for touch state change, save in temporary variable
                if (event.code == BTN_TOUCH) {
                    _tmpPressed = event.value != 0;
                }
                break;
            case EV_ABS:
                // Look for X and Y changes, save in temporary variable
                if (event.code == ABS_X) {
                    _tmpXY.setX(event.value);
                } else if (event.code == ABS_Y) {
                    _tmpXY.setY(event.value);
                }
                break;
            case EV_SYN:
                // When a syn report occurs, we have a full sample to process from the touchscreen
                if (event.code == SYN_REPORT) {
                    handleTouchUpdate(_tmpXY, _tmpPressed);
                }
                break;
            }
        }
    } while (result == sizeof(input_event));
}

/**
 * @brief Called when a complete touch state update arrives
 * @param xy The last known touch location
 * @param pressed True if the screen is touched, false if not
 */
void CalibrationWindow::handleTouchUpdate(QPoint xy, bool pressed)
{
    bool touchJustPressed = false;
    bool touchJustReleased = false;

    // If the touchscreen was released and now it's pressed, mark as such
    if (pressed && !_touchIsPressed)
    {
        _touchIsPressed = true;
        touchJustPressed = true;
    }
    // Same idea if it was pressed and now it's released
    else if (!pressed && _touchIsPressed)
    {
        _touchIsPressed = false;
        touchJustReleased = true;
    }

    // As long as we are still calibrating, add the latest point to the queue
    if (_curCalPoint < NUM_CAL_POINTS)
    {
        _points.enqueue(xy);
        // Limit the queue size
        if (_points.count() > MAX_AVG_POINTS) {
            _points.dequeue();
        }

        // If the touchscreen was just released, save the calibration point we're on
        if (touchJustReleased)
        {
            int x = 0, y = 0, count = 0;
            // Dequeue all of the points and average them up
            while (!_points.isEmpty())
            {
                QPoint p = _points.dequeue();
                x += p.x();
                y += p.y();
                count++;
            }
            x = qRound(static_cast<float>(x) / static_cast<float>(count));
            y = qRound(static_cast<float>(y) / static_cast<float>(count));
            _calibrationPoints.append(QPoint(x, y));

            // Move onto the next phase
            _curCalPoint++;
            update();

            if (_curCalPoint == NUM_CAL_POINTS) {

                // Do some math to figure out the cal points. Cheesy, but we have two samples of each
                // X and Y point, so figure them out by averaging. This is not a super great way of
                // calibrating a touchscreen, but it works okay for the Chumby 8.
                float leftXCal = (_calibrationPoints[0].x() + _calibrationPoints[3].x()) / 2.0f;
                float rightXCal = (_calibrationPoints[1].x() + _calibrationPoints[2].x()) / 2.0f;
                float topYCal = (_calibrationPoints[0].y() + _calibrationPoints[1].y()) / 2.0f;
                float botYCal = (_calibrationPoints[2].y() + _calibrationPoints[3].y()) / 2.0f;

                // Here are their corresponding points in pixels
                float leftXPixels = _crosshairPoints[0].x();
                float rightXPixels = _crosshairPoints[1].x();
                float topYPixels = _crosshairPoints[0].y();
                float botYPixels = _crosshairPoints[2].y();

                // Calculate scale of original units to screen pixels
                float scaleX = (rightXCal - leftXCal) / (rightXPixels - leftXPixels);
                float scaleY = (botYCal - topYCal) / (botYPixels - topYPixels);

                // Now use the scale to extrapolate the min and max original points.
                _minXCal = leftXCal - (scaleX * CROSSHAIR_OFFSET);
                _maxXCal = rightXCal + (scaleX * CROSSHAIR_OFFSET);
                _minYCal = topYCal - (scaleY * CROSSHAIR_OFFSET);
                _maxYCal = botYCal + (scaleY * CROSSHAIR_OFFSET);

                // Make sure they're in range (if not, something's wrong...)
                if (_minXCal < 0 || _maxXCal > RAW_TOUCHSCREEN_RANGE ||
                    _minYCal < 0 || _maxYCal > RAW_TOUCHSCREEN_RANGE ||
                    _minXCal > _maxXCal ||
                    _minYCal > _maxYCal) {
                    _instructionsLabel.setText("Calibration error. Tap the screen to quit.");
                    _calibrationPoints.clear();
                } else {
                    _instructionsLabel.setText("Calibration complete. Tap the screen to apply and save.");
                }

            }
        }
    }
    // If they just pressed the screen for the first time after calibration finished
    // (or an error occurred), exit. Save as long as we have something to save and it
    // wasn't an error.
    else if (touchJustPressed)
    {
        // Save and apply the new calibration if we just finished
        if (_calibrationPoints.length() == NUM_CAL_POINTS) {
            // Assemble the calibration matrix. This is a 3x3 matrix that will
            // translate a vector [x, y, 1] from normalized [0...1] raw touchscreen x/y coordinates
            // to normalized [0...1] screen x/y coordinates. The coordinates we receive will be
            // the raw touchscreen 0-4095 coordinates, but normalized to 0.0-1.0 instead.
            //
            // Note: This is currently implemented as a very simplistic calibration. It doesn't
            // support any kind of rotation or skewing. So it assumes that all we have to do is
            // scale and translate the X and Y coordinates in order to calibrate.. This seems to
            // work decently enough for the Chumby 8's touchscreen. If rotation was needed, we
            // would need to use a more complicated method for creating the calibration matrix.
            float const rangeF = static_cast<float>(RAW_TOUCHSCREEN_RANGE);
            float a = rangeF / (_maxXCal - _minXCal);
            float c = _minXCal / (_minXCal - _maxXCal);
            float e = rangeF / (_maxYCal - _minYCal);
            float f = _minYCal / (_minYCal - _maxYCal);
            QVector<float> calibrationMatrix = QVector<float>(
                {a,    0.0f, c,
                 0.0f, e,    f,
                 0.0f, 0.0f, 1.0f});

            if (!CalibrationUtils::saveNewCalibration(calibrationMatrix)) {
                _instructionsLabel.setText("Error saving calibration. Tap the screen to quit.");
            } else if (!CalibrationUtils::applyCalibration(calibrationMatrix)) {
                _instructionsLabel.setText("Error applying final calibration. Tap the screen to quit.");
            } else {
                _instructionsLabel.setText("New calibration saved and applied successfully. Tap the screen to finish.");
            }
            // Clear calibration points so the next tap will quit
            _calibrationPoints.clear();
        } else {
            // They're tapping after a message was displayed that will quit. So quit.
            qApp->exit();
        }
    }
}
