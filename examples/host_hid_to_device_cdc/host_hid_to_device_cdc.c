/
 *Host Mouse -> Device Mouse bridge
 * RP2040 + Pico-PIO-USB + TinyUSB
 * Mouse buttons + X/Y + WHEEL FUNCIONANDO
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
  // USB necesita múltiplo de 12 MHz
  set_sys_clock_khz(120000, true);
  sleep_ms(10);

  multicore_launch_core1(core1_main);

  // USB DEVICE (mouse)
  tud_init(0);

  while (true)
  {
    tud_task();
  }
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
    // 🔥 CLAVE: FORZAR REPORT PROTOCOL PARA QUE LA RUEDA FUNCIONE
    tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_REPORT);

    tuh_hid_receive_report(dev_addr, instance);
  }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  (void) dev_addr;
  (void) instance;
}

// --------------------------------------------------------------------
// 🔑 PUENTE HOST → DEVICE (CON RUEDITA REAL)
// --------------------------------------------------------------------

void tuh_hid_report_received_cb(uint8_t dev_addr,
                                uint8_t instance,
                                uint8_t const* report,
                                uint16_t len)
{
  (void) dev_addr;

  uint8_t proto = tuh_hid_interface_protocol(dev_addr, instance);

  if (proto == HID_ITF_PROTOCOL_MOUSE && len >= 3)
  {
    uint8_t buttons = report[0];
    int8_t  x       = (int8_t) report[1];
    int8_t  y       = (int8_t) report[2];
    int8_t  wheel   = 0;

    // 🛞 RUEDITA
    if (len >= 4)
    {
      wheel = (int8_t) report[3];
    }

    if (tud_hid_ready())
    {
      tud_hid_mouse_report(
        0,        // report ID
        buttons,  // botones
        x,        // X
        y,        // Y
        wheel,    // WHEEL ✅
        0         // pan
      );
    }
  }

  // pedir siguiente reporte
  tuh_hid_receive_report(dev_addr, instance);
}

