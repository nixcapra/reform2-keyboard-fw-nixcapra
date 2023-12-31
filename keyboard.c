/*
  MNT Reform 2.0 Keyboard Firmware
  Copyright 2019-2021  Lukas F. Hartmann / MNT Research GmbH, Berlin (lukas@mntre.com)
  And Contributors:
  Chartreuse
  V Körbes
  Robey Pointer (robey@lag.net)

  Based on code from LUFA Library (lufa-lib.org):
  Copyright 2018 Dean Camera (dean [at] fourwalledcubicle [dot] com)

  SPDX-License-Identifier: MIT
*/

#include <stdlib.h>
#include <avr/io.h>
#include "backlight.h"
// Important: version and variant info moved here
#include "constants.h"
#include "hid_report.h"
#include "keyboard.h"
#include "menu.h"
#include "oled.h"
#include "powersave.h"
#include "remote.h"
#include "scancodes.h"

#ifdef KBD_VARIANT_3
#include "matrix_3.h"
#else
#ifdef KBD_VARIANT_V
#include "matrix_v.h"
#else
#include "matrix.h"
#endif
#endif

/** Buffer to hold the previously generated Keyboard HID report, for comparison purposes inside the HID class driver. */
static uint8_t PrevKeyboardHIDReportBuffer[sizeof(USB_KeyboardReport_Data_t)];
static uint8_t PrevMediaControlHIDReportBuffer[sizeof(USB_MediaReport_Data_t)];

/** LUFA HID Class driver interface configuration and state information. This structure is
 *  passed to all HID Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_HID_Device_t Keyboard_HID_Interface =
  {
    .Config =
      {
        .InterfaceNumber              = INTERFACE_ID_Keyboard,
        .ReportINEndpoint             =
          {
            .Address              = KEYBOARD_EPADDR,
            .Size                 = HID_EPSIZE,
            .Banks                = 1,
          },
        .PrevReportINBuffer           = PrevKeyboardHIDReportBuffer,
        .PrevReportINBufferSize       = sizeof(PrevKeyboardHIDReportBuffer),
      },
  };

/** LUFA HID Class driver interface configuration and state information for the Media Controller */
USB_ClassInfo_HID_Device_t MediaControl_HID_Interface =
  {
    .Config =
      {
        .InterfaceNumber              = INTERFACE_ID_MediaControl,
        .ReportINEndpoint             =
          {
            .Address              = MEDIACONTROL_EPADDR,
            .Size                 = HID_EPSIZE,
            .Banks                = 1,
          },
        .PrevReportINBuffer           = PrevMediaControlHIDReportBuffer,
        .PrevReportINBufferSize       = sizeof(PrevMediaControlHIDReportBuffer),
      },
  };

uint8_t matrix_debounce[KBD_COLS*KBD_ROWS];
uint8_t matrix_state[KBD_COLS*KBD_ROWS];

int active_meta_mode = 0;
uint8_t last_meta_key = 0;

uint8_t* active_matrix = matrix;
bool media_toggle = 0;
bool fn_key = 0; // Am I holding FN?
bool circle = 0; // Am I holding circle?

// enter the menu
void enter_meta_mode(void) {
  active_meta_mode = 1;
  reset_and_render_menu();
}

void reset_keyboard_state(void) {
  for (int i = 0; i < KBD_COLS*KBD_ROWS; i++) {
    matrix_debounce[i] = 0;
    matrix_state[i] = 0;
  }
  last_meta_key = 0;
  reset_menu();
}

inline bool is_media_key(uint8_t keycode) {
  return (keycode>=HID_KEYBOARD_SC_MEDIA_PLAY);
}

void get_media_keys(uint8_t keycode, USB_MediaReport_Data_t* mcr) {
  switch (keycode) {
  case HID_KEYBOARD_SC_MEDIA_BRIGHTNESS_DOWN: mcr->BrightnessDown = 1; break;
  case HID_KEYBOARD_SC_MEDIA_BRIGHTNESS_UP: mcr->BrightnessUp = 1; break;
  case HID_KEYBOARD_SC_MEDIA_PREVIOUS_TRACK: mcr->PreviousTrack = 1; break;
  case HID_KEYBOARD_SC_MEDIA_PLAY: mcr->PlayPause = 1; break;
  case HID_KEYBOARD_SC_MEDIA_NEXT_TRACK: mcr->NextTrack = 1; break;
  case HID_KEYBOARD_SC_MEDIA_MUTE: mcr->Mute = 1; break;
  case HID_KEYBOARD_SC_MEDIA_VOLUME_DOWN: mcr->VolumeDown = 1; break;
  case HID_KEYBOARD_SC_MEDIA_VOLUME_UP: mcr->VolumeUp = 1; break;
  }
}

#define MAX_SCANCODES 6
static uint8_t pressed_scancodes[MAX_SCANCODES] = {0,0,0,0,0,0};

// usb_report_mode: if you pass 0, you can leave KeyboardReport NULL
int process_keyboard(uint8_t* resulting_scancodes) {
  // how many keys are pressed this round
  uint8_t total_pressed = 0;
  uint8_t used_key_codes = 0;

  // pull ROWs low one after the other
  for (int y = 0; y < KBD_ROWS; y++) {
    switch (y) {
    case 0: output_low(PORTB, 6); break;
    case 1: output_low(PORTB, 5); break;
    case 2: output_low(PORTB, 4); break;
    case 3: output_low(PORTD, 7); break;
    case 4: output_low(PORTD, 6); break;
    case 5: output_low(PORTD, 4); break;
    }

    // wait for signal to stabilize
    // TODO maybe not necessary
    //_delay_us(10);

    // check input COLs
    for (int x=0; x<14; x++) {
      uint16_t keycode;
      uint16_t loc = y*KBD_COLS+x;
      keycode = active_matrix[loc];
      uint8_t  pressed = 0;
      uint8_t  debounced_pressed = 0;

      // column pins are all over the place
      switch (x) {
      case 0:  pressed = !(PIND&(1<<5)); break;
      case 1:  pressed = !(PINF&(1<<7)); break;
      case 2:  pressed = !(PINE&(1<<6)); break;
      case 3:  pressed = !(PINC&(1<<7)); break;
      case 4:  pressed = !(PINB&(1<<3)); break;
      case 5:  pressed = !(PINB&(1<<2)); break;
      case 6:  pressed = !(PINB&(1<<1)); break;
      case 7:  pressed = !(PINB&(1<<0)); break;
      case 8:  pressed = !(PINF&(1<<0)); break;
      case 9:  pressed = !(PINF&(1<<1)); break;
      case 10: pressed = !(PINF&(1<<4)); break;
      case 11: pressed = !(PINF&(1<<5)); break;
      case 12: pressed = !(PINF&(1<<6)); break;
      case 13: pressed = !(PINC&(1<<6)); break;
      }

      // shift new state as bit into debounce "register"
      matrix_debounce[loc] = (matrix_debounce[loc]<<1)|pressed;

      // if unclear state, we need to keep the last state of the key
      if (matrix_debounce[loc] == 0x00) {
        matrix_state[loc] = 0;
      } else if (matrix_debounce[loc] == 0x01) {
        matrix_state[loc] = 1;
      }
      debounced_pressed = matrix_state[loc];

      if (debounced_pressed) {
        total_pressed++;

        // Is circle key pressed?
        if (keycode == KEY_CIRCLE) {
          if (fn_key) {
            circle = 1;
          } else {
            if (!active_meta_mode && !last_meta_key) {
              enter_meta_mode();
            }
          }
        } else if (keycode == HID_KEYBOARD_SC_EXECUTE) {
          fn_key = 1;
          active_matrix = matrix_fn;
        } else {
          if (active_meta_mode) {
            // not holding the same key?
            if (last_meta_key != keycode) {
              // hyper/circle/menu functions
              int stay_meta = execute_meta_function(keycode);
              // don't repeat action while key is held down
              last_meta_key = keycode;

              // exit meta mode
              if (!stay_meta) {
                active_meta_mode = 0;
              }

              // after wake-up from sleep mode, skip further keymap processing
              if (stay_meta == 2) {
                reset_keyboard_state();
                enter_meta_mode();
                return 0;
              }
            }
          } else if (!last_meta_key) {
            // not meta mode, regular key: report keypress via USB
            // 6 keys is the limit in the HID descriptor
            if (used_key_codes < MAX_SCANCODES && resulting_scancodes) {
              resulting_scancodes[used_key_codes++] = keycode;
            }
          }
        }
      } else {
        // key not pressed
        if (keycode == HID_KEYBOARD_SC_EXECUTE) {
          fn_key = 0;
          if (media_toggle) {
            active_matrix = matrix_fn_toggled;
          } else {
            active_matrix = matrix;
          }
        } else if (keycode == KEY_CIRCLE) {
          if (fn_key && circle) {
            if (!media_toggle) {
              media_toggle = 1;
              active_matrix = matrix_fn_toggled;
            } else {
              media_toggle = 0;
              active_matrix = matrix_fn;
            }
          }
          circle = 0;
        }
      }
    }

    switch (y) {
    case 0: output_high(PORTB, 6); break;
    case 1: output_high(PORTB, 5); break;
    case 2: output_high(PORTB, 4); break;
    case 3: output_high(PORTD, 7); break;
    case 4: output_high(PORTD, 6); break;
    case 5: output_high(PORTD, 4); break;
    }
  }

  // if no more keys are held down, allow a new meta command
  if (total_pressed<1) last_meta_key = 0;

  return used_key_codes;
}

int main(void)
{
#ifdef KBD_VARIANT_QWERTY_US
  matrix[KBD_COLS*4+1]=KEY_DELETE;
  matrix_fn[KBD_COLS*4+1]=KEY_DELETE;
  matrix_fn_toggled[KBD_COLS*4+1]=KEY_DELETE;
#endif
#ifdef KBD_VARIANT_NEO2
  matrix[KBD_COLS*3+0]=HID_KEYBOARD_SC_CAPS_LOCK; // M3
  matrix[KBD_COLS*2+13]=KEY_ENTER;
  matrix[KBD_COLS*3+13]=KEY_BACKSLASH_AND_PIPE; // M3
#endif

  setup_hardware();
  GlobalInterruptEnable();
  anim_hello();

  int counter = 0;

  for (;;)
  {
    process_keyboard(NULL);
    HID_Device_USBTask(&Keyboard_HID_Interface);
    HID_Device_USBTask(&MediaControl_HID_Interface);
    USB_USBTask();
    counter++;
#ifndef KBD_MODE_STANDALONE
      if (counter>=100000) {
        remote_check_for_low_battery();
        counter = 0;
      }
      if (counter%2048 == 0) {
        refresh_menu_page();
      }
      if (counter%750 == 0) {
        remote_process_alerts();
      }
#endif
  }
}

void setup_hardware(void)
{
  // Disable watchdog if enabled by bootloader/fuses
  MCUSR &= ~(1 << WDRF);
  wdt_disable();

  // Disable clock division
  clock_prescale_set(clock_div_1);

  // declare port pins as inputs (0) and outputs (1)
  DDRB  = 0b11110000;
  DDRC  = 0b00000000;
  DDRD  = 0b11011001;
  DDRE  = 0b00000000;
  DDRF  = 0b00000000;

  // initial pin states
  PORTB = 0b10001111;
  PORTC = 0b11000000;
  PORTD = 0b00100000;
  PORTE = 0b01000000;
  PORTF = 0b11111111;

  // disable JTAG
  MCUCR |=(1<<JTD);
  MCUCR |=(1<<JTD);

  kbd_brightness_init();
  gfx_init(false);
  remote_init();
  USB_Init();
}

ISR(WDT_vect)
{
  // WDT interrupt enable and flag cleared on entry
  Delay_MS(1);
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
  bool ConfigSuccess = true;

  ConfigSuccess &= HID_Device_ConfigureEndpoints(&Keyboard_HID_Interface);
	ConfigSuccess &= HID_Device_ConfigureEndpoints(&MediaControl_HID_Interface);

  USB_Device_EnableSOFEvents();
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
  HID_Device_ProcessControlRequest(&Keyboard_HID_Interface);
  HID_Device_ProcessControlRequest(&MediaControl_HID_Interface);
}

/** Event handler for the USB device Start Of Frame event. */
void EVENT_USB_Device_StartOfFrame(void)
{
  HID_Device_MillisecondElapsed(&Keyboard_HID_Interface);
  HID_Device_MillisecondElapsed(&MediaControl_HID_Interface);
}

/** HID class driver callback function for the creation of HID reports to the host.
 *
 *  \param[in]     HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in,out] ReportID    Report ID requested by the host if non-zero, otherwise callback should set to the generated report ID
 *  \param[in]     ReportType  Type of the report to create, either HID_REPORT_ITEM_In or HID_REPORT_ITEM_Feature
 *  \param[out]    ReportData  Pointer to a buffer where the created report should be stored
 *  \param[out]    ReportSize  Number of bytes written in the report (or zero if no report is to be sent)
 *
 *  \return Boolean \c true to force the sending of the report, \c false to let the library determine if it needs to be sent
 */

bool CALLBACK_HID_Device_CreateHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                         uint8_t* const ReportID,
                                         const uint8_t ReportType,
                                         void* ReportData,
                                         uint16_t* const ReportSize)
{
  int num_keys = process_keyboard(pressed_scancodes);
  if (num_keys > MAX_SCANCODES) num_keys = MAX_SCANCODES;

  if (HIDInterfaceInfo == &Keyboard_HID_Interface) {
    // host asks for a keyboard report
    USB_KeyboardReport_Data_t* KeyboardReport = (USB_KeyboardReport_Data_t*)ReportData;
    *ReportSize = sizeof(USB_KeyboardReport_Data_t);
    for (int i=0; i<num_keys; i++) {
      uint8_t sc = pressed_scancodes[i];
      if (!is_media_key(sc)) {
        KeyboardReport->KeyCode[i] = sc;
      }
    }
  } else if (HIDInterfaceInfo == &MediaControl_HID_Interface) {
    // host asks for a media control report
    USB_MediaReport_Data_t* MediaControlReport = (USB_MediaReport_Data_t*)ReportData;
    *ReportSize = sizeof(USB_MediaReport_Data_t);
    for (int i=0; i<num_keys; i++) {
      uint8_t sc = pressed_scancodes[i];
      if (is_media_key(sc)) {
        get_media_keys(sc, MediaControlReport);
      }
    }
  }
  return false;
}

/** HID class driver callback function for the processing of HID reports from the host.
 *
 *  \param[in] HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in] ReportID    Report ID of the received report from the host
 *  \param[in] ReportType  The type of report that the host has sent, either HID_REPORT_ITEM_Out or HID_REPORT_ITEM_Feature
 *  \param[in] ReportData  Pointer to a buffer where the received report has been stored
 *  \param[in] ReportSize  Size in bytes of the received HID report
 */
void CALLBACK_HID_Device_ProcessHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                          const uint8_t ReportID,
                                          const uint8_t ReportType,
                                          const void* ReportData,
                                          const uint16_t ReportSize)
{
  uint8_t* data = (uint8_t*)ReportData;
  if (ReportSize<4) return;

  hid_report_cmd(data);
}
