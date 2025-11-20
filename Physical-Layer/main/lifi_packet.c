#include "lifi_packet.h"

// Define the global packet handler
packet_handler_t lifi_packets;

// Initialize function to create mutexes
void lifi_packet_init(void) {
    // Initialize mutexes for packet array
    for (int i = 0; i < PACKET_COUNT; i++) {
        lifi_packets.locks[i] = xSemaphoreCreateMutex();
        lifi_packets.ethToEspPackets[i].status = EMPTY;
    }
    
    lifi_packets.ethToEspPacketSendReserved.status = EMPTY;
    lifi_packets.ethToEspPacketsRecieveReserved.status = EMPTY;
    lifi_packets.espToEspPacket.status = EMPTY;

    lifi_packets.recievedTaskHandler = NULL;
}

//send packet over lifi, in order of bytes 0 -> LIFI_PACKET_SIZE-1
/*
void send_packet_over_lifi(eth_packets_t *packet)
{
    for(int i = 0; i < LIFI_PACKET_SIZE; i++) {
        send_byte(packet->data[i]);
    }
}
*/

//dummy function for core 2 packet handler
void send_receiver_task(void *pvParameters)
{
    //dummy function
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        //! TODO: When something is flagged as recieve you must call following
        xTaskNotifyGive(lifi_packets.recievedTaskHandler);
    }
}
