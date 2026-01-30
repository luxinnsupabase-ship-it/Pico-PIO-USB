/*
 * Host Mouse -> Device Mouse bridge
 * RP2040 + Pico-PIO-USB + TinyUSB
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "pio_usb.h"
#include "tusb.h"

// --------------------------------------------------------------------
// CORE 1 = USB HOST (PIO USB)
// --------------------------------------------------------------------

void core1_main(void)
{
  sleep_ms(10);

  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

  tuh_init(1);

  while (true)
  {
    tuh_task();
  }
}

// --------------------------------------------------------------------
// CORE 0 = USB DEVICE (Native USB)
// --------------------------------------------------------------------

int main(void)
{
  set_sys_clock_khz(120000, true);
  sleep_ms(10);

  multicore_launch_core1(core1_main);

  // Device stack (mouse)
  tud_init(0);

  while (true)
  {
    tud_task();
  }
}

// --------------------------------------------------------------------
// USB DEVICE HID (MOUSE)
// --------------------------------------------------------------------

uint8_t const desc_hid_report[] =
{
  TUD_HID_REPORT_DESC_MOUSE()
};

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
  (void) instance;
  return desc_hid_report;
}

void tud_hid_set_report_cb(uint8_t instance,
                           uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const* buffer,
                           uint16_t bufsize)
{
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) bufsize;
}

// --------------------------------------------------------------------
// USB HOST HID
// --------------------------------------------------------------------

void tuh_hid_mount_cb(uint8_t dev_addr,
                      uint8_t instance,
                      uint8_t const* desc_report,
                      uint16_t desc_len)
{
  (void) desc_report;
  (void) desc_len;

  uint8_t proto = tuh_hid_interface_protocol(dev_addr, instance);

  if (proto == HID_ITF_PROTOCOL_MOUSE)
  {
    tuh_hid_receive_report(dev_addr, instance);
  }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  (void) dev_addr;
  (void) instance;
}

// --------------------------------------------------------------------
// 🔑 LA PIEZA FINAL: PUENTE HOST → DEVICE
// --------------------------------------------------------------------

void tuh_hid_report_received_cb(uint8_t dev_addr,
                                uint8_t instance,
                                uint8_t const* report,
                                uint16_t len)
{
  (void) dev_addr;
  (void) len;

  uint8_t proto = tuh_hid_interface_protocol(dev_addr, instance);

  if (proto == HID_ITF_PROTOCOL_MOUSE)
  {
    hid_mouse_report_t const* r =
      (hid_mouse_report_t const*) report;

    if (tud_hid_ready())
    {
      tud_hid_mouse_report(
        0,              // report ID
        r->buttons,
        r->x,
        r->y,
        r->wheel,
        0               // pan
      );
    }
  }

  tuh_hid_receive_report(dev_addr, instance);
}
