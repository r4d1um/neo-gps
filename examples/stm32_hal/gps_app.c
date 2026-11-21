/*
 * STM32 HAL example (CubeMX: USARTx 9600 8N1, NVIC interrupt on).
 * Bytes arrive via interrupt in a ring buffer; parsing happens in the main loop.
 * (neo_gps_feed() is short enough to call directly in the ISR, too.)
 *
 * In main():  gps_app_init();  while (1) { gps_app_poll(); ... }
 */
#include "main.h"
#include "neo_gps.h"

extern UART_HandleTypeDef huart2;          /* UART connected to the module */
#define GPS_UART (&huart2)

static neo_gps_t gps;
static uint8_t rx_byte;
static volatile uint8_t ring[256];         /* 256 -> uint8_t indices wrap around automatically */
static volatile uint8_t head, tail;

static void gps_write(void *user, const uint8_t *data, uint16_t len)
{
    (void)user;
    HAL_UART_Transmit(GPS_UART, (uint8_t *)data, len, 100);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == GPS_UART) {
        ring[head++] = rx_byte;
        HAL_UART_Receive_IT(GPS_UART, &rx_byte, 1);
    }
}

void gps_app_init(void)
{
    neo_gps_init(&gps, gps_write, NULL);
    HAL_UART_Receive_IT(GPS_UART, &rx_byte, 1);

    neo_gps_cfg_output(&gps, NEO_OUT_NMEA_MINIMAL);
    neo_gps_cfg_dynmodel(&gps, NEO_DYN_AUTOMOTIVE);
    neo_gps_cfg_rate(&gps, 200);           /* 5 Hz - works on all three modules */
}

void gps_app_poll(void)
{
    while (tail != head)
        neo_gps_feed(&gps, ring[tail++]);

    if (neo_gps_updates(&gps) & NEO_UPD_POSITION) {
        const neo_gps_data_t *d = neo_gps_data(&gps);
        /* d->lat / d->lon in 1e-7 degrees, d->alt_msl in mm, d->speed in mm/s ... */
        (void)d;
    }
}
