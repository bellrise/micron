/* btscan.c
   Copyright (c) 2025 bellrise */

#ifndef ENABLE_BLE
# define ENABLE_BLE 1
#endif
#include "btstack.h"

#include <micron/mem.h>
#include <micron/micron.h>
#include <micron/net.h>
#include <micron/syslog.h>
#include <pico/cyw43_arch.h>
#include <pico/printf.h>

void start_scan() { }

void bt_handler(u8 packet_type, u16 channel, u8 *packet, u16 size)
{
    char namebuf[256];
    bd_addr_t addr;
    i32 etype;
    i32 rssi;
    u8 ev;

    if (packet_type != HCI_EVENT_PACKET)
        return;

    ev = hci_event_packet_get_type(packet);

    printf("ev 0x%02x\n", ev);

    switch (ev) {
    case BTSTACK_EVENT_STATE:
        etype = btstack_event_state_get_state(packet);
        printf("hci state %d\n", etype);

        if (etype == HCI_STATE_WORKING) {
            gap_set_scan_params(1, 4800, 500, 0);
            gap_start_scan();
        }

    case GAP_EVENT_ADVERTISING_REPORT:
        gap_event_advertising_report_get_address(packet, addr);
        rssi = gap_event_advertising_report_get_rssi(packet);

        ad_context_t ad;

        for (ad_iterator_init(
                 &ad, gap_event_advertising_report_get_data_length(packet),
                 gap_event_advertising_report_get_data(packet));
             ad_iterator_has_more(&ad); ad_iterator_next(&ad)) {

            const u8 *ad_data = ad_iterator_get_data(&ad);
            const i32 ad_size = ad_iterator_get_data_len(&ad);

            switch (ad_iterator_get_data_type(&ad)) {
            case 0x08:
            case 0x09:
                syslog_impl("/btscan.c", "", "BASIC Device %s rssi %d ",
                            bd_addr_to_str(addr), rssi - 255);
                for (i32 i = 0; i < ad_size; i++)
                    printf("%c", ad_data[i]);
                printf("\n");
                break;
            }
        }

        break;

    case GAP_EVENT_EXTENDED_ADVERTISING_REPORT:
        gap_event_extended_advertising_report_get_address(packet, addr);
        rssi = gap_event_extended_advertising_report_get_rssi(packet);
        syslog("EXTND Device %s rssi %d", bd_addr_to_str(addr), rssi - 255);
        break;
    }
}

static btstack_timer_source_t timer;

void heartbeat_handler(btstack_timer_source_t *source)
{
    syslog("heartbeat");

    /* Start another one. */

    btstack_run_loop_set_timer(&timer, 1000);
    btstack_run_loop_add_timer(&timer);
}

void user_main()
{
    l2cap_init();

    static btstack_packet_callback_registration_t bt_callbacks;

    bt_callbacks.callback = &bt_handler;
    hci_add_event_handler(&bt_callbacks);

    timer.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&timer, 1000);
    btstack_run_loop_add_timer(&timer);

    hci_set_inquiry_mode(INQUIRY_MODE_RSSI_AND_EIR);
    hci_power_control(HCI_POWER_ON);

    btstack_run_loop_execute();
}
