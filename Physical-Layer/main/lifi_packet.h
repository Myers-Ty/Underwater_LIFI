#ifndef LIFI_PACKET_H
#define LIFI_PACKET_H

#include <stdint.h>
#include "driver/gpio.h"
#include "lifi_config.h"
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lwip/prot/ethernet.h" // Ethernet header
//digital read/write functions
#define digitalWrite(pin, value) gpio_set_level(pin, value)
#define digitalRead(pin) gpio_get_level(pin)

//packet size
#define PACKET_COUNT 10
#define LIFI_PAYLOAD_LENGTH 44

typedef enum {
    EMPTY = 0,
    RECEIVED = 1,
    SEND = 2
} lifi_status_t;

typedef struct {
    struct eth_hdr header;
    char payload[LIFI_PAYLOAD_LENGTH];
    uint16_t CRC;

    lifi_status_t status;
} eth_packet_t;

typedef struct {
    // circular buffer of packets
    eth_packet_t ethToEspPackets[PACKET_COUNT];
    SemaphoreHandle_t locks[PACKET_COUNT];

    eth_packet_t ethToEspPacketSendReserved;
    eth_packet_t ethToEspPacketsRecieveReserved;

    eth_packet_t espToEspPacket;
    TaskHandle_t recievedTaskHandler;
} packet_handler_t;


void lifi_packet_init(void);

void send_packet_over_lifi(eth_packet_t *packet);

//dummy function for core 2 packet handler
void send_receiver_task(void *pvParameters);

//stored packet array (extern - defined in .c file)
extern packet_handler_t lifi_packets;

#endif // LIFI_PACKET_H