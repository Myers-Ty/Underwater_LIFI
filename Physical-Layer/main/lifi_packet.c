#include "lifi_packet.h"

// Define the global packet arrays
lifi_packet_t lifi_packets[10];
lifi_packet_t incoming_lifi_packet;

// Initialize function to create mutexes
void lifi_packet_init(void) {
    // Initialize mutexes for packet array
    for (int i = 0; i < 10; i++) {
        lifi_packets[i].mutex = xSemaphoreCreateMutex();
        lifi_packets[i].status = LIFI_EMPTY;
    }
    
    // Initialize incoming packet mutex
    incoming_lifi_packet.mutex = xSemaphoreCreateMutex();
    incoming_lifi_packet.status = LIFI_EMPTY;
}
