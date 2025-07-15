module uart_slave (
    input clk,
    input rx,
    output tx,

    // giao tiếp với SD controller
    output reg [31:0] lba,
    output reg rd_req,
    output reg wr_req,
    output reg [7:0] wr_data,
    output reg [8:0] wr_addr,
    input [7:0] rd_data,
    input busy,
    input done
);
    parameter IDLE = 0,
              CMD = 1,
              LBA = 2,
              DATA = 3,
              SEND_DATA = 4;

    reg [2:0] state = IDLE;
    reg [7:0] cmd;
    reg [2:0] lba_cnt;
    reg [8:0] byte_cnt;

    reg [7:0] rx_data;
    wire [7:0] uart_rx_data;
    wire uart_rx_ready;
    reg tx_start;
    reg [7:0] tx_data;

    // UART Receiver
    uart_rx #(.BAUD(115200)) RX (
        .clk(clk),
        .rx(rx),
        .data(uart_rx_data),
        .ready(uart_rx_ready)
    );

    // Gán vào reg rx_data
    always @(posedge clk) begin
        if (uart_rx_ready) begin
            rx_data <= uart_rx_data;
        end
    end

    // UART Transmitter
    uart_tx #(.BAUD(115200)) TX (
        .clk(clk),
        .start(tx_start),
        .data(tx_data),
        .tx(tx)
    );

    always @(posedge clk) begin
        case (state)
            IDLE: begin
                if (uart_rx_ready) begin
                    cmd <= uart_rx_data;
                    state <= CMD;
                end
            end

            CMD: begin
                lba_cnt <= 0;
                lba <= 0;
                byte_cnt <= 0;

                if (cmd == 8'h01) begin
                    state <= LBA;
                end else if (cmd == 8'h02) begin
                    state <= LBA;
                end else if (cmd == 8'h03) begin
                    tx_start <= 1;
                    tx_data <= "I"; // Dummy IDENTIFY
                    state <= IDLE;
                end
            end

            LBA: begin
                if (uart_rx_ready) begin
                    lba <= {lba[23:0], uart_rx_data};
                    lba_cnt <= lba_cnt + 1;

                    if (lba_cnt == 3) begin
                        if (cmd == 8'h01) begin
                            rd_req <= 1;
                            state <= SEND_DATA;
                        end else if (cmd == 8'h02) begin
                            state <= DATA;
                        end
                    end
                end
            end

            DATA: begin
                if (uart_rx_ready) begin
                    wr_data <= uart_rx_data;
                    wr_addr <= byte_cnt;
                    byte_cnt <= byte_cnt + 1;

                    wr_req <= 1;

                    if (byte_cnt == 9'd511) begin
                        wr_req <= 0;
                        state <= IDLE;
                    end
                end else begin
                    wr_req <= 0;
                end
            end

            SEND_DATA: begin
                rd_req <= 0;

                if (~busy && done) begin
                    if (byte_cnt < 512) begin
                        tx_start <= 1;
                        tx_data <= rd_data;
                        byte_cnt <= byte_cnt + 1;
                    end else begin
                        tx_start <= 0;
                        state <= IDLE;
                    end
                end
            end
        endcase
    end

endmodule