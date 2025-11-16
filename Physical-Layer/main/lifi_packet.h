#ifndef LIFI_PACKET_H
#define LIFI_PACKET_H
void lifi_packet_init(void);

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

//packet size
#define LIFI_PACKET_SIZE 256

typedef enum {
    LIFI_EMPTY = 0,
    LIFI_RECEIVED = 1,
    LIFI_SEND = 2
} lifi_status_t;

typedef struct {
    SemaphoreHandle_t mutex;
    lifi_status_t status;
    uint8_t data[LIFI_PACKET_SIZE];
} lifi_packet_t;

//stored packet array (extern - defined in .c file)
extern lifi_packet_t lifi_packets[10];
//stored incoming packet (extern - defined in .c file)
extern lifi_packet_t incoming_lifi_packet;

#endif // LIFI_PACKET_H