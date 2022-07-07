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

#ifndef CALIBRATIONWINDOW_H
#define CALIBRATIONWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QSocketNotifier>
#include <QQueue>

/**
 * @brief Window used for calibrating the Chumby 8's touchscreen
 */
class CalibrationWindow : public QMainWindow
{
    Q_OBJECT

public:
    CalibrationWindow(QWidget *parent = nullptr);
    ~CalibrationWindow();

protected:
    void paintEvent(QPaintEvent *);
    void resizeEvent(QResizeEvent *);

private:
    void readRawEvents();
    void handleTouchUpdate(QPoint xy, bool pressed);

    QLabel _instructionsLabel;
    QList<QPoint> _crosshairPoints;
    QList<QPoint> _calibrationPoints;
    int _curCalPoint;
    int _calibrationFd;
    QSocketNotifier *_calibrationNotifier;
    bool _tmpPressed;
    QPoint _tmpXY;
    bool _touchIsPressed;
    QQueue<QPoint> _points;

    int _minXCal;
    int _maxXCal;
    int _minYCal;
    int _maxYCal;
};
#endif // CALIBRATIONWINDOW_H
