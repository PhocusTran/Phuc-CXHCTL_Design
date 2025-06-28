#ifndef UART_HELPER_H
#define UART_HELPER_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"


#define BUF_SIZE 1024
static const char *TAG = "UART_IRQ";

void usart_init(uart_port_t uart_num, QueueHandle_t *uart_queue);

#endif /* end of include guard: UART_HELPER_H */
