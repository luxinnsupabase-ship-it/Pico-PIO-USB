#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "pio_usb.h"
#include "tusb.h"

// --------------------------------------------------------------------
// CORE 1 = USB HOST (Donde conectas el Mouse físico)
// --------------------------------------------------------------------
void core1_main(void) {
    sleep_ms(10);
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
    tuh_init(1);

    while (true) {
        tuh_task();
    }
}

// --------------------------------------------------------------------
// CORE 0 = USB DEVICE (Donde el PC ve a la Pico como mouse)
// --------------------------------------------------------------------
int main(void) {
    set_sys_clock_khz(120000, true);
    sleep_ms(10);

    // Inicializa Standard I/O (para ver los printf en el PC)
    stdio_init_all();

    multicore_launch_core1(core1_main);

    tud_init(0);

    while (true) {
        tud_task();
    }
}

// CALLBACKS DEL HOST (Leyendo el mouse físico)
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    uint8_t proto = tuh_hid_interface_protocol(dev_addr, instance);
    if (proto == HID_ITF_PROTOCOL_MOUSE) {
        tuh_hid_receive_report(dev_addr, instance);
    }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    uint8_t proto = tuh_hid_interface_protocol(dev_addr, instance);

    if (proto == HID_ITF_PROTOCOL_MOUSE) {
        // --- SECCIÓN DE DEPURACIÓN ---
        // Esto se verá en tu monitor serial
        printf("Longitud: %u | Bytes: ", len);
        for (uint16_t i = 0; i < len; i++) {
            printf("[%d]:%02X ", i, report[i]);
        }
        printf("\n");
        // -----------------------------

        int8_t buttons = report[0];
        int8_t x = 0;
        int8_t y = 0;
        int8_t wheel = 0;

        if (len >= 3) {
            x = (int8_t) report[1];
            y = (int8_t) report[2];
        }

        // PRUEBA: Si tu rueda no funciona con el índice 3, 
        // mira el monitor serial y cambia el [3] por el índice que cambie al mover la rueda.
        if (len >= 4) {
            wheel = (int8_t) report[3]; 
        }

        if (tud_hid_ready()) {
            tud_hid_mouse_report(0, buttons, x, y, wheel, 0);
        }
    }
    tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {}
