#include "lifi_packet.h"
#include "lifi_config.h"
#include <unistd.h>

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

void send_sequence_start() {
    //dummy function to start send sequence
    while (1) {
        send_byte(LIFI_PREAMBLE);
        lifi_sleep(CLOCK_TICK); //wait a tick before receiving data
        char response = receive_byte();
        if (response == LIFI_PREAMBLE) {
            printf("Received Notify Bit Ack\n");
            break;
        }
        if(response !=0){
            printf("Received unexpected byte: %02X\n", response);
        } 
    }
    

}

char receive_sequence_start() {
    //dummy function to start receive sequence
    char byte = 0;
    int bit = 7;
    while (bit >= 0) {
        byte |= (digitalRead(INPUT_PIN) << bit);
        lifi_sleep(CLOCK_TICK);
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

//send packet over lifi, in order of bytes 0 -> LIFI_PACKET_SIZE-1
void send_packet(eth_packet_t *packet)
{
    digitalWrite(LED_PIN, 1); //set high to indicate start of packet transmission
    lifi_sleep(CLOCK_TICK); //wait a tick before sending data
    digitalWrite(LED_PIN, 0); //set low to indicate start of packet transmission
    lifi_sleep(CLOCK_TICK); //wait a tick before sending data
    send_byte(*packet->header.src.addr);
    send_byte((packet->header.type >> 8) & 0xFF);
    send_byte(packet->header.type & 0xFF);
    for(int i = 0; i < LIFI_PAYLOAD_LENGTH; i++) {
        send_byte(packet->payload[i]);
    }
}

<<<<<<< HEAD
void send() {
=======

void start_send_sequence() {
    //dummy function to start send sequence
    while (1) {
        send_byte(LIFI_PREAMBLE);
        lifi_sleep(CLOCK_TICK); //wait a tick before receiving data
        char response = receive_byte();
        if (response == LIFI_PREAMBLE) {
            printf("Received Notify Bit Ack\n");
            break;
        }
        if(response !=0){
            printf("Received unexpected byte: %02X\n", response);
        } 
    }
    

}


void send_lifi_packet() {

    
>>>>>>> 87b4fa19 (stopped trying dumb things)
    if(lifi_packets.ethToEspPacketSendReserved.status == SEND) {
        send_sequence_start();
        send_packet(&lifi_packets.ethToEspPacketSendReserved);
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

void receive()
{
    eth_packet_t* packet = &lifi_packets.espToEspPacket;
 
    //sleep one tick to switch from receieve to send mode

    // LIFI_PREAMBLE already received by receive_sequence_start() in caller
    // Send acknowledgment
    send_byte(LIFI_PREAMBLE);
    printf("Sent Notify Bit\n");
    while(digitalRead(INPUT_PIN) != HIGH) {
        //wait for line to go high before receiving data
    }
    while(digitalRead(INPUT_PIN) != LOW) {
        //wait for line to go low before receiving data
    }
    lifi_sleep(CLOCK_TICK + (CLOCK_TICK / 2)); //wait a tick before receiving data
    *packet->header.src.addr = receive_byte();
    packet->header.type = (receive_byte() << 8) | receive_byte();
    for (int i = 0; i < LIFI_PAYLOAD_LENGTH; i++) {
        packet->payload[i] = receive_byte();
        // printf("Received byte: %02X\n", packet->payload[i]);
    }
    packet->status = RECEIVED;

    while(!set_receieve_packet(packet));
    // debugging
    // print_packet(packet);
}

//dummy function for core 2 packet handler
void send_receive_task(void *pvParameters)
{
    eth_packet_t* packet = &lifi_packets.espToEspPacket;
    strcpy(packet->payload, "Im ready to receive your load");
    lifi_packets.espToEspPacket.status = RECEIVED;
    set_receieve_packet(packet);
    xTaskNotifyGive(lifi_packets.recievedTaskHandler);

    while (1) {
        printf("Waiting for packet...\n");
        char byte = receive_sequence_start();
        if(byte == LIFI_PREAMBLE) {
            receive();
            // if (lifi_packets.recievedTaskHandler) {
            xTaskNotifyGive(lifi_packets.recievedTaskHandler);
            // }
            

        } else if (lifi_packets.ethToEspPacketSendReserved.status == SEND) {
            printf("Attempting to send packet\n");
            send();
        }
    }
}
