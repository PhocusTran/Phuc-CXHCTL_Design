// debug_uart.v - Gửi một số 32-bit ra UART dưới dạng 8 ký tự Hexa.
module debug_uart (
    input clk,
    input send_hex_start,
    input [31:0] hex_data,
    output reg busy,
    output reg uart_tx_start,
    output reg [7:0] uart_tx_data,
    input uart_tx_busy
);
    reg [1:0] state = 0;
    reg [2:0] nibble_index;

    function [7:0] to_hex_char;
        input [3:0] val;
        begin
            if (val < 10) to_hex_char = val + "0";
            else to_hex_char = val - 10 + "A";
        end
    endfunction

    always @(posedge clk) begin
        uart_tx_start <= 1'b0;
        case(state)
            0: begin // IDLE
                busy <= 1'b0;
                if (send_hex_start) begin
                    nibble_index <= 7;
                    busy <= 1'b1;
                    state <= 1;
                end
            end
            1: begin // CHUẨN BỊ GỬI
                if (!uart_tx_busy) begin
                    uart_tx_data <= to_hex_char(hex_data >> (nibble_index * 4));
                    uart_tx_start <= 1'b1;
                    state <= 2;
                end
            end
            2: begin // ĐANG CHỜ
                if (!uart_tx_start) begin
                    if (nibble_index == 0) state <= 0;
                    else begin
                        nibble_index <= nibble_index - 1;
                        state <= 1;
                    end
                end
            end
        endcase
    end
endmodule