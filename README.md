# Chumby8TSCal
This is a utility for calibrating the Chumby 8 touchscreen on my modern custom Chumby 8 firmware. It is not designed to run on stock Chumby firmware. This program sets the absolute minimum and maximum X and Y coordinates on the Chumby 8 touchscreen input device. It's a very simplistic way of calibrating versus more flexible projects such as [tslib](https://github.com/libts/tslib), but it works fine with the Chumby's touchscreen.

Background info: The Chumby 8 touchscreen reports X and Y coordinates in the range of 0 to 4095. However, only a portion of that range is actually used. On my Chumby for example, the X range is about 223 to 3774 and the Y range is about 370 to 3733. Qt's *evdevtouch* plugin scales the absolute minimum and maximum readings to the screen size. By adjusting the absolute minimum and maximum to match the actual usable range prior to loading Qt, the *evdevtouch* plugin's scaling will work correctly.

## Building:

Use qmake created from buildroot (replace */path/to/buildroot* with your actual buildroot path):

```
cd Chumby8TSCal
mkdir build
cd build
/path/to/buildroot/output/host/bin/qmake ..
make -j $(nproc)
```

This should generate an application called Chumby8TSCal that can be run on the hardware.

## Usage:

As a general note, my custom firmware requires running Qt applications with the following environment variable set:

`export QT_QPA_FB_DRM=1`

### Calibrate the touchscreen

On hardware, run Chumby8TSCal with no arguments to calibrate the touchscreen.

`./Chumby8TSCal`

Follow the on-screen directions. The calibration will be saved to the file `/etc/touchscreencal`.

### Apply existing calibration

At every boot, the calibration needs to be applied to the touchscreen input device. This is handled by running the program with the following argument:

`./Chumby8TSCal --apply`

### Apply existing calibration or prompt for calibration

If you want to attempt to apply existing calibration if it's available, and otherwise run the calibration process, then you can run it with the following argument instead:

`./Chumby8TSCal --apply-or-calibrate`
