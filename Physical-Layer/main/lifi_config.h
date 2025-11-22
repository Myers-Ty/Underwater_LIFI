//include Pin definitions and other config settings for LiFi
#include "driver/gpio.h"

//High is a ONE, Low is a ZERO
#define HIGH 1
#define LOW 0
//Our flash pin is pin 43
#define LED_PIN GPIO_NUM_2
//our input pin is pin 19
#define INPUT_PIN GPIO_NUM_19
#define NOTIFY_BIT 0b11011001

//Clock tick
#define CLOCK_TICK 1
