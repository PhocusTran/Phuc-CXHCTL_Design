#include "uart_helper.h"
#include "hal/uart_types.h"

void usart_init(uart_port_t uart_num, QueueHandle_t *uart_queue){
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        //.flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 122,
    };

    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_driver_install(uart_num, BUF_SIZE * 2, BUF_SIZE * 2, 20, uart_queue, 0));
    ESP_ERROR_CHECK(uart_set_pin(
                uart_num, 
                UART_PIN_NO_CHANGE, 
                UART_PIN_NO_CHANGE, 
                UART_PIN_NO_CHANGE, 
                UART_PIN_NO_CHANGE
                ));

    ESP_LOGI(TAG, "UART initialized with IRQ");
}
