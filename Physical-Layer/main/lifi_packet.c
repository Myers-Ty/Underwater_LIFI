#include "lifi_packet.h"
#include "lifi_config.h"

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

void send_byte(char byte) {
    for (int i = 0; i < 8; i++) {
        digitalWrite(LED_PIN, 1 & byte);
        vTaskDelay(CLOCK_TICK);
        byte >>= 1;
    }
}

//send packet over lifi, in order of bytes 0 -> LIFI_PACKET_SIZE-1
void send_packet_data_over_lifi(eth_packets_t *packet)
{
    for(int i = 0; i < LIFI_PAYLOAD_LENGTH; i++) {
        send_byte(packet->data[i]);
    }
}

char receive_byte() {
    char byte = 0;
    for (int i = 0; i < 8; i++) {
        vTaskDelay(CLOCK_TICK);
        byte |= (digitalRead(INPUT_PIN) << i);
    }
    return byte;
}

void receieve_packet_over_lifi(eth_packet_t *packet)
{
    char byte = 0;
    while(1) {
        char byte = start_receive_sequence();
        if(byte == NOTIFY_BIT) {
            break;
        }
    }
    send_byte(NOTIFY_BIT); // send back notify bit to sender to indicate we are ready to receive the packet
    for(int i = 0; i < LIFI_PAYLOAD_LENGTH; i++) {
        packet->payload[i] = receive_byte(); //dummy data
    }
 
    packet->status = RECEIVED;
}

char start_receive_sequence() {
    //dummy function to start receive sequence
    char byte = 0;
    int bit = 0;
    while(bit < 8) {

        vTaskDelay(CLOCK_TICK);
        byte |= (digitalRead(INPUT_PIN) << bit);
        if(((byte >> bit) & 1) == ((NOTIFY_BIT >> bit) & 1)) {
            bit++;
        } else {
            if(bit = 0) {
            //check if we have a packet to send
                if(lifi_packets.espToEspPacket.status == SEND) {
                    return byte;
                }
            }
            bit = 0;
            byte = 0;
        }
    }
    return byte;
}

void start_send_sequence() {
    //dummy function to start send sequence
    while(1) {
        send_byte(NOTIFY_BIT);
        if(receive_byte() == NOTIFY_BIT) {
            break;
        }
    }
}

void send_lifi_packet() {

    if(lifi_packets.espToEspPacket.status == SEND) {
        start_send_sequence();
        send_packet_data_over_lifi(&lifi_packets.espToEspPacket);
        lifi_packets.espToEspPacket.status = EMPTY;
    }
    //move a circular buffer packet to the reserved send packet if it is marked as SEND
    for (int i = 0; i < PACKET_COUNT; i++) {
        if(xSemaphoreTake(lifi_packets.locks[i], portMAX_DELAY) == pdTRUE) {
            if(lifi_packets.ethToEspPackets[i].status == SEND) {
                memcpy(&lifi_packets.ethToEspPacketSendReserved, &lifi_packets.ethToEspPackets[i], sizeof(eth_packet_t));
                lifi_packets.ethToEspPacketSendReserved.status = SEND;
                lifi_packets.ethToEspPackets[i].status = EMPTY;
                xSemaphoreGive(lifi_packets.locks[i]);
                break;
            }
            xSemaphoreGive(lifi_packets.locks[i]);
        }
    }
}

eth_packet_t* get_avaliable_receive_packet() {
    //check if the reserved receive packet is empty
    if(lifi_packets.ethToEspPacketsRecieveReserved.status == EMPTY) {
        lifi_packets.ethToEspPacketsRecieveReserved.status = RECEIVED;
        return &lifi_packets.ethToEspPacketsRecieveReserved;
    }
    //check the circular buffer for an empty packet
    for(int i = 0; i < PACKET_COUNT; i++) {
        if(xSemaphoreTake(lifi_packets.locks[i], portMAX_DELAY) == pdTRUE) {
            if(lifi_packets.ethToEspPackets[i].status == EMPTY) {
                lifi_packets.ethToEspPackets[i].status = RECEIVED;
                xSemaphoreGive(lifi_packets.locks[i]);
                return &lifi_packets.ethToEspPackets[i];
            }
            xSemaphoreGive(lifi_packets.locks[i]);
        }
    }
    return NULL;
}

//dummy function for core 2 packet handler
void send_receiver_task(void *pvParameters)
{
    while (1) {
        char byte = start_receive_sequence();
        if(byte == NOTIFY_BIT) {
            eth_packet_t* packet = get_avaliable_receive_packet();
            receieve_packet_over_lifi(packet);
        } else if(lifi_packets.espToEspPacket.status == SEND) {
            send_lifi_packet();
        }

        //! TODO: When something is flagged as recieve you must call following
        xTaskNotifyGive(lifi_packets.recievedTaskHandler);
    }
}
