#include "lifi_config.h"

void lifi_sleep(int microseconds) {
    int start = esp_timer_get_time();
    while (esp_timer_get_time() - start < microseconds) {
        //busy wait
    }
}