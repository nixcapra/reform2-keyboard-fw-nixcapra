#!/bin/sh

set -eu

if [ ! -e ./keyboard.hex ]; then
	echo "keyboard.hex doesn't exist" >&2
	exit 1
fi

if [ "$(id -u)" -ne 0 ]; then
	echo "you need to run this as root (for example by using sudo)" >&2
	exit 1
fi

find_usb_device() {
	result=
	for p in /sys/bus/usb/devices/*; do
		[ -e "$p/idVendor" ] || continue
		[ "$(cat "$p/idVendor")" = "$1" ] || continue
		[ -e "$p/idProduct" ] || continue
		[ "$(cat "$p/idProduct")" = "$2" ] || continue
		[ "$(udevadm info --query=property --property=ID_MODEL --value "$p")" = "$3" ] || continue
		if [ -n "$result" ]; then
			echo "found more than one device matching $1 $2 $3" >&2
			exit 1
		fi
		result="$(realpath -e "$p")"
	done
	echo "$result"
}

remove_prefix_char() {
	result="$1"
	char="$2"
	while :; do
		case $result in
			"$char"*) result=${result#"$char"};;
			*) break;;
		esac
	done
	echo "$result"
}

path_keyboard=$(find_usb_device 03eb 2042 Reform_Keyboard)

busnum_keyboard=
devnum_keyboard=
if [ -n "$path_keyboard" ] && [ -e "$path_keyboard" ]; then
	busnum_keyboard="$(udevadm info --query=property --property=BUSNUM --value "$path_keyboard")"
	devnum_keyboard="$(udevadm info --query=property --property=DEVNUM --value "$path_keyboard")"
	echo "Toggle the programming DIP switch SW84 on the keyboard to “ON”." >&2
	echo "Then press the reset button SW83." >&2
	echo "Waiting for the keyboard to disappear..." >&2
	udevadm wait --removed "$path_keyboard"
	echo "Waiting for the Atmel DFU bootloader USB device to appear..." >&2
	udevadm wait --settle "$path_keyboard"
fi

path=$(find_usb_device 03eb 2ff4 ATm32U4DFU)
if [ -z "$path" ] || [ ! -e "$path" ]; then
	echo "cannot find Atmel DFU bootloader USB device" >&2
	exit 1
fi

busnum="$(udevadm info --query=property --property=BUSNUM --value "$path")"
devnum="$(udevadm info --query=property --property=DEVNUM --value "$path")"

# do some extra checks if we saw the usb device as a keyboard before
if [ -n "$path_keyboard" ] && [ -e "$path_keyboard" ]; then
	if [ "$path_keyboard" != "$path" ]; then
		echo "path of Atmel DFU bootloader USB device is different from the keyboard" >&2
		exit 1
	fi
	if [ "$busnum_keyboard" != "$busnum" ]; then
		echo "busnum of Atmel DFU bootloader USB device is different from the keyboard" >&2
		exit 1
	fi
	# the devnum of the atmel increments by 1 on every press of the reset
	# button (why?), so no sense comparing those
	#if [ "$((devnum_keyboard+1))" != "$devnum" ]; then
	#	echo "devnum of Atmel DFU bootloader USB device is different from the keyboard" >&2
	#	exit 1
	#fi
fi

device="atmega32u4:$(remove_prefix_char "$busnum" 0),$(remove_prefix_char "$devnum" 0)"

dfu-programmer "$device" erase --suppress-bootloader-mem

dfu-programmer "$device" flash ./keyboard.hex --suppress-bootloader-mem

dfu-programmer "$device" start

echo "Waiting for the Atmel DFU bootloader USB device to disappear..." >&2

udevadm wait --removed "$path"

echo "Waiting for the keyboard to re-appear..." >&2

udevadm wait --settle "$path"

echo "All done!" >&2
echo "Don't forget to toggle the programming DIP switch SW84 on the keyboard to “OFF” again" >&2
