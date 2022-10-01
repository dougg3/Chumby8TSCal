# Chumby8TSCal
This is a utility for calibrating the Chumby 8 touchscreen on my modern custom Chumby 8 firmware. It is not designed to run on stock Chumby firmware. This program updates the libinput Xorg plugin's calibration matrix for the Chumby 8 touchscreen device. It live-updates the current calibration, and also saves the calibration matrix to a config file in /mnt/settings so it will automatically be applied on future boots. The calibration this utility performs is very simplistic; it only calculates a matrix that can do scaling and translation, but no rotation.

Background info: The Chumby 8 touchscreen reports X and Y coordinates in the range of 0 to 4095. However, only a portion of that range is actually used. On my Chumby for example, the X range is about 223 to 3774 and the Y range is about 370 to 3733. The *xf86-input-libinput* plugin scales the absolute minimum and maximum readings to the screen size by default. By changing the calibration matrix, we can ensure that only the usable range of the touchscreen is mapped to the display.

If you are still using a version of the custom firmware prior to the change to X11, use the [pre-x11](https://github.com/dougg3/Chumby8TSCal/tree/pre-x11) branch instead.

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

On hardware, run Chumby8TSCal with no arguments to calibrate the touchscreen.

`./Chumby8TSCal`

Follow the on-screen directions. The calibration will be saved to the file `/etc/X11/xorg.conf.d/touchscreen.conf` and applied immediately. No need to restart X. On future boots, X will automatically load the calibration from touchscreen.conf.
