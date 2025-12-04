/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h> // read/write
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_eth.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "esp_vfs_l2tap.h"
#include "arpa/inet.h" // ntohs, etc.
#include "protocol_examples_common.h"
#include "esp_system.h"
#include "lifi_packet.h"
#include "lifi_config.h"

#if !defined(CONFIG_EXAMPLE_CONNECT_ETHERNET)
#error Ethernet interface is not configured to connect.
#endif

#define ETH_INTERFACE           "ETH_DEF"
#define ETH_TYPE_FILTER_NOBLOCK  0x2221
#define ETH_TYPE_FILTER_TX       0x2223

#define INVALID_FD              -1

static const char *TAG = "U-LiFi-Eth";

uint16_t eth_type_filter = ETH_TYPE_FILTER_TX;

// must initialize when a packet is recieved from the device otherwise packets are broadcast to nothing
u8_t compEthAddr = 0;

/** Opens and configures L2 TAP file descriptor */
static int init_l2tap_fd(int flags, uint16_t eth_type_filter)
{
    int fd = open("/dev/net/tap", flags);
    if (fd < 0) {
        ESP_LOGE(TAG, "Unable to open L2 TAP interface: errno %d", errno);
        goto error;
    }
    ESP_LOGI(TAG, "/dev/net/tap fd %d successfully opened", fd);

    // Check fd block status (just for demonstration purpose)
    flags = 0;
    flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        ESP_LOGE(TAG, "Unable to get L2 TAP fd %d status flag: errno %d", fd, errno);
        goto error;
    }
    if (flags & O_NONBLOCK) {
        ESP_LOGI(TAG, "L2 TAP fd %d configured in non-blocking mode", fd);
    } else {
        ESP_LOGI(TAG, "L2 TAP fd %d configured in blocking mode", fd);
    }

    // Configure Ethernet interface on which to get raw frames
    int ret;
    if ((ret = ioctl(fd, L2TAP_S_INTF_DEVICE, ETH_INTERFACE)) == -1) {
        ESP_LOGE(TAG, "Unable to bound L2 TAP fd %d with Ethernet device: errno %d", fd, errno);
        goto error;
    }
    ESP_LOGI(TAG, "L2 TAP fd %d successfully bound to `%s`", fd, ETH_INTERFACE);

    // Configure Ethernet frames we want to filter out
    if ((ret = ioctl(fd, L2TAP_S_RCV_FILTER, &eth_type_filter)) == -1) {
        ESP_LOGE(TAG, "Unable to configure fd %d Ethernet type receive filter: errno %d", fd, errno);
        goto error;
    }
    ESP_LOGI(TAG, "L2 TAP fd %d Ethernet type filter configured to 0x%x", fd, eth_type_filter);

    return fd;
error:
    if (fd != INVALID_FD) {
        close(fd);
    }
    return INVALID_FD;
}

static void copyFrame(eth_packet_t *in_frame, eth_packet_t *out_frame, int len) {
    memcpy(&out_frame->header.src.addr, &in_frame->header.src.addr, ETH_ADDR_LEN);
    //! TODO: if needed later set destination address same method as src
    // Set Ethernet type
    memcpy(&out_frame->header.type, &in_frame->header.type, sizeof(uint16_t));
    // Copy the payload
    memcpy(&out_frame->payload, in_frame->payload, len - ETH_HEADER_LEN); 

    memcpy(&out_frame->CRC, &in_frame->CRC, LIFI_CRC_LENGTH);
    
    printf("Packet Saved 🎊");
    printf("\n\n");
}

static void save_frame(eth_packet_t *in_frame, int len)
{
    if (strcmp(in_frame->payload, "RESET_ESP|RESET_ESP|RESET_ESP|RESET_ESP") == 0) {

        eth_packet_t* packet = &lifi_packets.ethToEspPacketsRecieveReserved;
        memset(packet->payload, 0, LIFI_PAYLOAD_LENGTH);
        strcpy(packet->payload, "REBOOTING[🌩️]");
        lifi_packets.ethToEspPacketsRecieveReserved.status = RECEIVED;
        set_receieve_packet(packet);
        xTaskNotifyGive(lifi_packets.recievedTaskHandler);
        vTaskDelay(1); // give time for other thread to send message
        esp_restart();
    }
    if (lifi_packets.ethToEspPacketSendReserved.status == EMPTY) {
        copyFrame(in_frame, &lifi_packets.ethToEspPacketSendReserved, len);
        lifi_packets.ethToEspPacketSendReserved.status = SEND;
    } else {
        for (int i = 0; i < PACKET_COUNT; i++) {
            xSemaphoreTake(lifi_packets.locks[i], portMAX_DELAY);
            if (lifi_packets.ethToEspPackets[i].status == EMPTY) {
                copyFrame(in_frame, &lifi_packets.ethToEspPackets[i], len);
                lifi_packets.ethToEspPackets[i].status = SEND;
                xSemaphoreGive(lifi_packets.locks[i]);
                break;
            }
            xSemaphoreGive(lifi_packets.locks[i]);
        }
        //! TODO: IF SPACE FOR PACKET NOT CURRENTLY FOUND IT IS DROPPED :( PLS FIX
        eth_packet_t* packet = &lifi_packets.errorBufferFullPacket;
        strcpy(packet->payload, "DROPPED[" + *in_frame->payload);
        while(!set_receieve_packet(packet));
        ESP_LOGI(TAG, "Dropped packet: ");
        print_packet(packet);
        xTaskNotifyGive(lifi_packets.recievedTaskHandler); 
        vTaskDelay(1); // give time for other thread to send message
    }
}

/** Demonstrates usage of L2 TAP non-blocking mode with select */
static void nonblock_l2tap_echo_task(void *pvParameters)
{
    uint8_t rx_buffer[128];
    int eth_tap_fd;

    // Open and configure L2 TAP File descriptor
    if ((eth_tap_fd = init_l2tap_fd(O_NONBLOCK, ETH_TYPE_FILTER_NOBLOCK)) == INVALID_FD) {
        goto error;
    }

    while (1) {
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(eth_tap_fd, &rfds);

        int ret_sel = select(eth_tap_fd + 1, &rfds, NULL, NULL, &tv);
        if (ret_sel > 0) {
            ssize_t len = read(eth_tap_fd, rx_buffer, sizeof(rx_buffer));
            if (len > 0) {
                eth_packet_t *recv_msg = (eth_packet_t *)rx_buffer;
                ESP_LOGI(TAG, "fd %d received %d bytes from %.2x:%.2x:%.2x:%.2x:%.2x:%.2x", eth_tap_fd,
                            len, recv_msg->header.src.addr[0], recv_msg->header.src.addr[1], recv_msg->header.src.addr[2],
                            recv_msg->header.src.addr[3], recv_msg->header.src.addr[4], recv_msg->header.src.addr[5]);

                save_frame(recv_msg, len);
                
            } else {
                ESP_LOGE(TAG, "L2 TAP fd %d read error: errno %d", eth_tap_fd, errno);
                break;
            }
        } else if (ret_sel == 0) {
            ESP_LOGD(TAG, "L2 TAP select timeout");
        } else {
            ESP_LOGE(TAG, "L2 TAP select error: errno %d", errno);
            break;
        }
    }
    close(eth_tap_fd);
error:
    vTaskDelete(NULL);
}

static ssize_t eth_transmit(int eth_tap_fd, eth_packet_t *packet) {
    eth_packet_t recieved_msg = {
            .header = {
                .dest.addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // broadcast address
                .type = htons(eth_type_filter)                     // convert to big endian (network) byte order
            }
        };

    memcpy(recieved_msg.payload, packet->payload, 44);

    // If no src address is given, then the esp assigns itself as the source
    if (packet->header.src.addr[0] || 
        packet->header.src.addr[1] || 
        packet->header.src.addr[2] ||
        packet->header.src.addr[3] ||
        packet->header.src.addr[4] || 
        packet->header.src.addr[5]) {
        memcpy(recieved_msg.header.src.addr, packet->header.src.addr, ETH_ADDR_LEN);
    } else {
        esp_eth_handle_t eth_hndl = get_example_eth_handle();
        esp_eth_ioctl(eth_hndl, ETH_CMD_G_MAC_ADDR, recieved_msg.header.src.addr);
    }

    // Send the Recieved frame
    return write(eth_tap_fd, &recieved_msg, ETH_HEADER_LEN + LIFI_PAYLOAD_LENGTH);
}

//! TODO: Reconstruct Ethernet frame from memory here
/** Demonstrates of how to construct Ethernet frame for transmit via L2 TAP */
static void eth_recieved_task(void *pvParameters)
{
    
    int eth_tap_fd;
    ssize_t ret = 0;
    // Open and configure L2 TAP File descriptor
    if ((eth_tap_fd = init_l2tap_fd(0, eth_type_filter)) == INVALID_FD) {
        goto error;
    }

    while (1) {

        // indefinitely wait until woken up
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        // Debug print
        ESP_LOGE(TAG, "Sending packet to Eth");

        // Construct frame
        if (lifi_packets.errorBufferFullPacket.status == RECEIVED) {
            ret = eth_transmit(eth_tap_fd, &lifi_packets.errorBufferFullPacket);
            lifi_packets.errorBufferFullPacket.status = EMPTY;
        }
        if (lifi_packets.ethToEspPacketsRecieveReserved.status == RECEIVED) {
            ret = eth_transmit(eth_tap_fd, &lifi_packets.ethToEspPacketsRecieveReserved);
            lifi_packets.ethToEspPacketsRecieveReserved.status = EMPTY;
        }
        for (int i = 0; i < PACKET_COUNT; i++) {
            xSemaphoreTake(lifi_packets.locks[i], portMAX_DELAY);
            if (lifi_packets.ethToEspPackets[i].status == RECEIVED) {
                ret = eth_transmit(eth_tap_fd, &lifi_packets.ethToEspPackets[i]);
                lifi_packets.ethToEspPackets[i].status = EMPTY;
            }
            xSemaphoreGive(lifi_packets.locks[i]);
        }

        if (ret == -1) {
            ESP_LOGE(TAG, "L2 TAP fd %d write error: errno: %d", eth_tap_fd, errno);
            break;
        }
    }
    close(eth_tap_fd);
error:
    vTaskDelete(NULL);
}

void initialize_gpio() {
    // Configure LED pin as output
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_PIN),
        .pull_down_en = 1,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);

    // Configure input pin as input with pull-up
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << INPUT_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "U-LiFi Initialized");
}

void app_main(void)
{
    initialize_gpio();

    // Initialize L2 TAP VFS interface
    ESP_ERROR_CHECK(esp_vfs_l2tap_intf_register(NULL));

    // Initialize NVS Flash
    ESP_ERROR_CHECK(nvs_flash_init());
    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    esp_eth_handle_t eth_hndl = get_example_eth_handle();
    uint8_t mac_addr[ETH_ADDR_LEN];
    esp_eth_ioctl(eth_hndl, ETH_CMD_G_MAC_ADDR, mac_addr);
    ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

    //init packets
    lifi_packet_init();

    
    // ROV connections on core 0
    xTaskCreatePinnedToCore(nonblock_l2tap_echo_task, "echo_no-block", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(eth_recieved_task, "EthRecievedMsgHandler", 4096, NULL, 5, &lifi_packets.recievedTaskHandler, 0);
    // Sender/Receiver task on core 1 (second core)
    xTaskCreatePinnedToCore(send_receive_task, "LifiSendReceiveTask", 4096, NULL, 4, NULL, 1);

    // Lets us send pause frames to stop transmission
    //! TODO: Fix flow control because currently enabling it fails :(
    // bool flow_ctrl_enable = true;
    // esp_eth_ioctl(eth_hndl, ETH_CMD_S_FLOW_CTRL, &flow_ctrl_enable);
}
