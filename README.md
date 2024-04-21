# MNT Reform 2.0 Keyboard Firmware

This is my customized version of the MNT Reform Keyboard 2 Firmware, where I have made several tweaks to enhance the keyboard's comfort and usability specifically for my preferences.

The [Reform Keyboard Editor](https://mntre.com/media/reform_md/reform-keyboard-editor/index.html) might be useful.

The main repository for anything MNT Reform can be found [here](https://source.mnt.re/reform/reform/).

## Modifications I made:

- Eliminated the `HID_KEYBOARD_SC_APPLICATION` key from the matrix and replaced it with `KEY_CIRCLE`.
- Reassigned `KEY_CIRCLE` to function as `KEY_DELETE`.
- Swapped `HID_KEYBOARD_SC_EXECUTE` with `HID_KEYBOARD_SC_LEFT_ALT`.
- Swapped `HID_KEYBOARD_SC_RIGHT_ALT` with `HID_KEYBOARD_SC_EXECUTE`.
- Excluded the MNT Reform Logo from the font, located in the `gfx/` directory.
  - New `font.c` containing the new font.
- Removed the logo on boot.

## Code Structure

- `constants.h`: Define which keyboard variant you want to build for
- `keyboard.c`: Main entrypoint that includes the chosen matrix file (keyboard layout)
- `matrix.h`: Keyboard layout definition (default)
- `matrix_*.h`: Alternative layouts
- `backlight.c`: Keyboard backlight control
- `menu.c`: OLED Menu handling
- `oled.c`: OLED graphics control
- `remote.c`: Communication with MNT Reform motherboard LPC
- `hid_report.c`: USB HID raw report handler (commands sent by OS)
- `powersave.c`: Low power/sleep mode
- `descriptors.c`: USB HID descriptors
- `i2c.c`: Soft I2C master implementation (for OLED)
- `serial.c`: Soft UART implementation
- `font.c`: Bitmap data for OLED font and icons

## Dependencies

### Debian/Ubuntu

`apt install gcc-avr avr-libc dfu-programmer`

## Hacking

To change the keyboard layout, adjust the `matrix` arrays in `keyboard.c`.

## Building

> If you have a Reform Keyboard 2 Standalone you can simply run `build.sh`

Build the firmware by running `make`. The firmware can then be found in
keyboard.hex. To build for the different layouts of the MNT Reform keyboards 2
or 3, define the `KBD_VARIANT_2` or `KBD_VARIANT_3` preprocessor variables,
respectively, using the `REFORM_KBD_OPTIONS` argument to `make`.

    make REFORM_KBD_OPTIONS=-DKBD_VARIANT_2    # default for keyboard 2.0
    make REFORM_KBD_OPTIONS=-DKBD_VARIANT_3    # keyboard 3.0 layout

Without any options, `KBD_VARIANT_2` is the default. To build for the
standalone keyboard for, define `KBD_MODE_STANDALONE` using
`REFORM_KBD_OPTIONS` like this:

    make REFORM_KBD_OPTIONS="-DKBD_VARIANT_2 -DKBD_MODE_STANDALONE"
    make REFORM_KBD_OPTIONS="-DKBD_VARIANT_3 -DKBD_MODE_STANDALONE"

To flash, put your keyboard into [flashing mode](https://mntre.com/reform2/handbook/parts.html#keyboard-firmware) and run:
`sudo ./flash.sh`
