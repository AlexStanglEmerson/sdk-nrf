/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/net/buf.h>
#include <zephyr/net/ieee802154_radio.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/sys/util.h>
#include <nrf_802154.h>
#include <nrf_802154_const.h>
#include <stdlib.h>
#include <dk_buttons_and_leds.h>

#define RADIO_BASE_ADDR 0x40001000

#define CRCSTATUS_REG_OFFSET 0x400
#define CRCSTATUS_REG_ADDR (RADIO_BASE_ADDR + CRCSTATUS_REG_OFFSET)

#define CRC_VALUE_REG_OFFSET 0x40C
#define CRC_VALUE_REG_ADDR (RADIO_BASE_ADDR + CRC_VALUE_REG_OFFSET)

#define HEX_STRING_LENGTH (2 * MAX_PACKET_SIZE + 1)

static const struct device *radio_dev =
	DEVICE_DT_GET(DT_CHOSEN(zephyr_ieee802154));
static struct ieee802154_radio_api *radio_api;
static const struct shell *uart_shell;
static char hex_string[HEX_STRING_LENGTH];
static bool heartbeat_led_state;
static bool packet_led_state;
static k_timeout_t heartbeat_interval;

static void heartbeat(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(heartbeat_work, heartbeat);

uint8_t reverse_byte(uint8_t b)
{
	// Credit to the answer here: https://stackoverflow.com/a/2602885
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

uint16_t reverse_uint16(uint16_t i)
{
	uint16_t v =   reverse_byte(0x00FF & i) << 8
				| reverse_byte((0xFF00 & i) >> 8);
	return v;
}

uint32_t reverse_uint32(uint32_t i)
{
	uint32_t v =   reverse_byte(0x000000FF & i) << (8 * 3)
				| reverse_byte((0x0000FF00 & i) >> (8 * 1)) << (8 * 2)
				| reverse_byte((0x00FF0000 & i) >> (8 * 2)) << (8 * 1)
				| reverse_byte((0xFF000000 & i) >> (8 * 3));
	return v;
}

static void heartbeat(struct k_work *work)
{
	ARG_UNUSED(work);

	heartbeat_led_state = !heartbeat_led_state;
	dk_set_led(DK_LED1, heartbeat_led_state);
	k_work_reschedule(&heartbeat_work, heartbeat_interval);
}

enum net_verdict ieee802154_radio_handle_ack(struct net_if *iface,
					     struct net_pkt *pkt)
{
	ARG_UNUSED(iface);
	ARG_UNUSED(pkt);

	return NET_DROP;
}

int net_recv_data(struct net_if *iface, struct net_pkt *pkt)
{
	if (!pkt) {
		return -EINVAL;
	}

	if (net_pkt_is_empty(pkt)) {
		return -ENODATA;
	}

	uint32_t *p_crcstatus = (uint32_t *)CRCSTATUS_REG_ADDR;
	uint32_t crc_is_ok = sys_read32(p_crcstatus);

	uint32_t *p_rxcrc = (uint32_t *)CRC_VALUE_REG_ADDR;
	uint16_t crc = (uint16_t)sys_read32(p_rxcrc);
	uint16_t crc_reversed = reverse_uint16(crc);

	struct net_buf *frag = net_buf_frag_last(pkt->buffer);
	uint8_t *psdu = frag->data;
	size_t length = net_buf_frags_len(pkt->buffer);
	uint8_t lqi = net_pkt_ieee802154_lqi(pkt);
	int8_t rssi = net_pkt_ieee802154_rssi(pkt);

	struct net_ptp_time *pkt_time = net_pkt_timestamp(pkt);
	uint64_t timestamp =
		pkt_time->second * USEC_PER_SEC + pkt_time->nanosecond / NSEC_PER_USEC;

	packet_led_state = !packet_led_state;
	dk_set_led(DK_LED4, packet_led_state);
	bin2hex(psdu, length, hex_string, HEX_STRING_LENGTH);

	shell_print(uart_shell,
		    "received: %s power: %d lqi: %u time: %llu crc: 0x%04x crcreversed: 0x%04x crcstatus: %d",
		    hex_string,
		    rssi,
		    lqi,
		    timestamp,
			crc,
			crc_reversed,
			crc_is_ok);

	net_pkt_unref(pkt);

	return 0;
}

static int cmd_channel(const struct shell *shell, size_t argc, char **argv)
{
	uint32_t channel;

	switch (argc) {
	case 1:
		shell_print(shell, "%d", nrf_802154_channel_get());
		break;
	case 2:
		channel = atoi(argv[1]);
		radio_api->set_channel(radio_dev, channel);
		break;
	default:
		shell_print(shell, "invalid number of parameters: %d", argc);
		break;
	}

	return 0;
}
SHELL_CMD_ARG_REGISTER(channel, NULL, "Set radio channel", cmd_channel, 1, 1);

static int cmd_receive(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(shell);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	heartbeat_interval = K_MSEC(250);
	radio_api->start(radio_dev);

	return 0;
}
SHELL_CMD_ARG_REGISTER(receive, NULL, "Put radio in receive state", cmd_receive, 1, 0);

static int cmd_sleep(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(shell);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	heartbeat_interval = K_SECONDS(1);
	radio_api->stop(radio_dev);

	return 0;
}
SHELL_CMD_ARG_REGISTER(sleep, NULL, "Disable the radio", cmd_sleep, 1, 0);

int main(void)
{
	(void) dk_leds_init();

	uart_shell = shell_backend_uart_get_ptr();
	heartbeat_interval = K_SECONDS(1);
	k_work_reschedule(&heartbeat_work, heartbeat_interval);

	struct ieee802154_config config = {
		.promiscuous = true
	};

	radio_api = (struct ieee802154_radio_api *)radio_dev->api;
	__ASSERT_NO_MSG(radio_api);

#if !IS_ENABLED(CONFIG_NRF_802154_SERIALIZATION)
	/* The serialization API does not support disabling the auto-ack. */
	nrf_802154_auto_ack_set(false);
#endif

	radio_api->configure(radio_dev, IEEE802154_CONFIG_PROMISCUOUS, &config);

	return 0;
}
