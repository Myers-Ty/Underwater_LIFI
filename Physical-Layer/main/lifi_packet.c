#include "lifi_packet.h"
#include "lifi_config.h"
#include <unistd.h>

// Define the global packet handler
packet_handler_t lifi_packets;

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

void print_packet(eth_packet_t *packet) {
        // Print for debugging
    printf("Data (hex): ");
    for (uint32_t i = 0; i < sizeof(packet->payload) / sizeof(packet->payload[0]); i++) {
        printf("%02X ", packet->payload[i]);
    }
    printf("\n");
    
    // print as ASCII for readable messages
    printf("Data (ASCII): ");
    for (uint32_t i = 0; i < sizeof(packet->payload) / sizeof(packet->payload[0]); i++) {
        char c = (packet->payload[i] >= 32 && packet->payload[i] <= 126) ? packet->payload[i] : '.';
        printf("%c", c);
    }
    printf("\n\n");
}

u16_t calculate_crc(eth_packet_t *packet) {
    u16_t crc = 0x0000;
    uint8_t *data = (uint8_t *)packet;
    size_t length = ETH_HWADDR_LEN + LIFI_PAYLOAD_LENGTH + LIFI_CRC_LENGTH;

    for (size_t i = 0; i < length; i++) {
        crc += data[i];
    }
    return crc;
}

void send_byte(char byte) {
    // printf("Sending byte: %02X\n", byte);

    for (int i = 7; i >=0; i--) {
        int bit = (byte >> i) & 1;
        digitalWrite(LED_PIN, bit);
        lifi_sleep(CLOCK_TICK);
        // printf("%d", bit);
    }
    // printf("\n");
    digitalWrite(LED_PIN, 0);
}

char receive_byte() {
    char byte = 0;
    for (int i = 7; i >=0; i--) {
        int bit = digitalRead(INPUT_PIN);
        lifi_sleep(CLOCK_TICK);
        byte |= (bit << i);
    }
    return byte;
}

char receive_byte_no_final_sleep() {
    char byte = 0;
    for (int i = 7; i >=0; i--) {
        int bit = digitalRead(INPUT_PIN);
        if(i !=0){
            lifi_sleep(CLOCK_TICK);
        }
        byte |= (bit << i);
    }
    return byte;
}

//send packet over lifi, in order of bytes 0 -> LIFI_PACKET_SIZE-1
void send_packet_data_over_lifi(eth_packet_t *packet)
{
    
    for (int i = 0; i < ETH_HWADDR_LEN; i++) {
        send_byte(packet->header.src.addr[i]);
    }
    // for (int i = 0; i < ETH_HWADDR_LEN; i++) {
    //     packet->header.dest.addr[i] = receive_byte();
    // }
    // send_byte((packet->header.type >> 8) & 0xFF);
    // send_byte((packet->header.type & 0xFF));
    for(int i = 0; i < LIFI_PAYLOAD_LENGTH; i++) {
        send_byte(packet->payload[i]);
    }
    for (int i = 0; i < LIFI_CRC_LENGTH; i++) {
        send_byte(packet->CRC[i]);
    }
}

eth_packet_t* set_receieve_packet(eth_packet_t *packet) {

    //check if the reserved receive packet is empty
    if(lifi_packets.ethToEspPacketsRecieveReserved.status == EMPTY) {
        //set the reserved receive data to the packet data
        memcpy(&lifi_packets.ethToEspPacketsRecieveReserved, packet, sizeof(eth_packet_t));
        //mark the reserved receive packet as received (after memcpy to avoid overwriting)
        lifi_packets.ethToEspPacketsRecieveReserved.status = RECEIVED;
        
        return &lifi_packets.ethToEspPacketsRecieveReserved;
    }
    //check the circular buffer for an empty packet
    for(int i = 0; i < PACKET_COUNT; i++) {
        if(xSemaphoreTake(lifi_packets.locks[i], portMAX_DELAY) == pdTRUE) {
            if(lifi_packets.ethToEspPackets[i].status == EMPTY) {
                memcpy(&lifi_packets.ethToEspPackets[i], packet, sizeof(eth_packet_t));
                lifi_packets.ethToEspPackets[i].status = RECEIVED;
                xSemaphoreGive(lifi_packets.locks[i]);
                return &lifi_packets.ethToEspPackets[i];
            }
            xSemaphoreGive(lifi_packets.locks[i]);
        }
    }
    return NULL;
}

void send_sequence_start() {
    //dummy function to start send sequence
    while (1) {
        send_byte(LIFI_PREAMBLE);
        char response = receive_byte_no_final_sleep();
        if (response == LIFI_PREAMBLE) {
            printf("Received Notify Bit Ack\n");
            break;
        }
        if(response !=0){
            printf("Received unexpected byte: %02X\n", response);
        } 
    }
    

}

char recieve_sequence_start() {
    //dummy function to start receive sequence
    char byte = 0;
    int bit = 7;
    while (bit >= 0) {
        byte |= (digitalRead(INPUT_PIN) << bit);
        if(bit != 0){
            lifi_sleep(CLOCK_TICK);
        }
        if (((byte >> bit) & 1) == ((LIFI_PREAMBLE >> bit) & 1)) {
            bit--;
        } else {
            if (bit == 7) {
                //check if we have a packet to send
                if (lifi_packets.ethToEspPacketSendReserved.status == SEND) {
                    return byte;
                }
            }
            // printf("byte is: %02X, expected bit: %d, received bit: %d\n", byte, (LIFI_PREAMBLE >> bit) & 1, (byte >> bit) & 1);
            bit = 7;
            byte = 0;
        }
    }

    return byte;
}

void send_lifi_packet() {
    
    if(lifi_packets.ethToEspPacketSendReserved.status == SEND) {
        bool ack_recieved = false;

        *lifi_packets.ethToEspPacketSendReserved.CRC = calculate_crc(&lifi_packets.ethToEspPacketSendReserved);
        send_sequence_start();
        send_packet_data_over_lifi(&lifi_packets.ethToEspPacketSendReserved);
        
        while (recieve_sequence_start() != LIFI_PREAMBLE); // wait indefinitely for the ack
    }
    int moved_packet = 0;
    //move a circular buffer packet to the reserved send packet if it is marked as SEND
    for (int i = 0; i < PACKET_COUNT; i++) {
        if(xSemaphoreTake(lifi_packets.locks[i], portMAX_DELAY) == pdTRUE) {
            if(lifi_packets.ethToEspPackets[i].status == SEND) {
                memcpy(&lifi_packets.ethToEspPacketSendReserved, &lifi_packets.ethToEspPackets[i], sizeof(eth_packet_t));
                lifi_packets.ethToEspPacketSendReserved.status = SEND;
                lifi_packets.ethToEspPackets[i].status = EMPTY;
                xSemaphoreGive(lifi_packets.locks[i]);
                    moved_packet = 1;
                break;
            }
            xSemaphoreGive(lifi_packets.locks[i]);
        }
    }
    if(!moved_packet) {
        lifi_packets.ethToEspPacketSendReserved.status = EMPTY;
    }
}

eth_packet_t receive_lifi_packet()
{
    eth_packet_t* packet = &lifi_packets.espToEspPacket;
    bool crc_match = true;

    send_byte(LIFI_PREAMBLE);
    printf("Sent Notify Bit\n");
    for (int i = 0; i < ETH_HWADDR_LEN; i++) {
        packet->header.src.addr[i] = receive_byte();
    }
    // for (int i = 0; i < ETH_HWADDR_LEN; i++) {
    //     packet->header.dest.addr[i] = receive_byte();
    // }
    // packet->header.type = (receive_byte() << 8) | receive_byte();
    for (int i = 0; i < LIFI_PAYLOAD_LENGTH; i++) {
        packet->payload[i] = receive_byte();
        // printf("Received byte: %02X\n", packet->payload[i]);
    }

    for (int i = 0; i < LIFI_CRC_LENGTH; i++) {
        packet->CRC[i] = receive_byte();
    }

    packet->status = RECEIVED;

    return *packet;
}

// packet handler on core 2
void send_receive_task(void *pvParameters)
{
    eth_packet_t* packet = &lifi_packets.espToEspPacket;
    strcpy(packet->payload, "Im ready to recieve your load");
    lifi_packets.espToEspPacket.status = RECEIVED;
    set_receieve_packet(packet);
    xTaskNotifyGive(lifi_packets.recievedTaskHandler);

    while (1) {
        printf("Waiting for packet...\n");
        char byte = recieve_sequence_start();
        if(byte == LIFI_PREAMBLE) {
            eth_packet_t packet_recv = receive_lifi_packet();
            bool crc_match = false;
            do {
                if (calculate_crc(packet) == packet->CRC) {
                    crc_match = true;
                    // send empty preamble as ack
                    send_sequence_start();
                } else {
                    printf("CRC Mismatch: calculated %04X, received %04X\n", calculate_crc(packet), packet->CRC);
                }
            } while(!crc_match);
            while(!set_receieve_packet(&packet_recv));
            print_packet(&packet_recv);
            xTaskNotifyGive(lifi_packets.recievedTaskHandler);           

        } else if (lifi_packets.ethToEspPacketSendReserved.status == SEND) {
            printf("Attempting to send packet\n");
            send_lifi_packet();
        }
    }
}