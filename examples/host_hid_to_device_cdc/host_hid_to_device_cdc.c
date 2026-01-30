/*
 * Host Mouse -> Device Mouse bridge
 * RP2040 + Pico-PIO-USB + TinyUSB
 * + UART AIMBOT (GP0 RX / GP1 TX)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "pio_usb.h"
#include "tusb.h"

// --------------------------------------------------------------------
// Aimbot data (from second RP2040 via UART)
// --------------------------------------------------------------------

volatile int8_t aimbot_dx = 0;
volatile int8_t aimbot_dy = 0;
volatile int8_t aimbot_wheel = 0;

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
// UART AIMBOT (GP0 RX, GP1 TX)
// Protocol: "dx dy wheel\n"
// Example: 5 -3 1\n
// --------------------------------------------------------------------

void uart_aimbot_task(void)
{
  static char buf[32];
  static uint8_t idx = 0;

  while (uart_is_readable(uart0))
  {
    char c = uart_getc(uart0);

    if (c == '\n')
    {
      buf[idx] = 0;

      int dx = 0, dy = 0, wh = 0;
      int n = sscanf(buf, "%d %d %d", &dx, &dy, &wh);

      if (n >= 2)
      {
        if (dx > 127) dx = 127;
        if (dx < -127) dx = -127;
        if (dy > 127) dy = 127;
        if (dy < -127) dy = -127;
        if (wh > 127) wh = 127;
        if (wh < -127) wh = -127;

        aimbot_dx = dx;
        aimbot_dy = dy;
        aimbot_wheel = wh;
      }

      idx = 0;
    }
    else if (idx < sizeof(buf) - 1)
    {
      buf[idx++] = c;
    }
  }
}

// --------------------------------------------------------------------
// CORE 0 = USB DEVICE (Native USB)
// --------------------------------------------------------------------

int main(void)
{
  set_sys_clock_khz(120000, true);
  sleep_ms(10);

  // UART0 init (aimbot RP2040)
  uart_init(uart0, 115200);
  gpio_set_function(0, GPIO_FUNC_UART); // RX
  gpio_set_function(1, GPIO_FUNC_UART); // TX
  uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
  uart_set_fifo_enabled(uart0, false);

  multicore_launch_core1(core1_main);

  // USB DEVICE (mouse) – descriptors vienen de usb_descriptors.c
  tud_init(0);

  while (true)
  {
    tud_task();
    uart_aimbot_task();
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
    tuh_hid_receive_report(dev_addr, instance);
  }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  (void) dev_addr;
  (void) instance;
}

// --------------------------------------------------------------------
// 🔑 PUENTE HOST → DEVICE (MOUSE + AIMBOT + WHEEL FIX)
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
      int8_t mx = r->x + aimbot_dx;
      int8_t my = r->y + aimbot_dy;
      int8_t mw = r->wheel + aimbot_wheel;

      // clamp
      if (mx > 127) mx = 127;
      if (mx < -127) mx = -127;
      if (my > 127) my = 127;
      if (my < -127) my = -127;
      if (mw > 127) mw = 127;
      if (mw < -127) mw = -127;

      tud_hid_mouse_report(
        0,
        r->buttons,
        mx,
        my,
        mw, // ✅ RUEDITA FUNCIONAL
        0
      );
    }
  }

  tuh_hid_receive_report(dev_addr, instance);
}
