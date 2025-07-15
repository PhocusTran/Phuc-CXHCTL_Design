// sd_spi_controller.v
// Version 5.0 - Đã thêm reset, sửa lỗi timing và tối ưu hóa.

module sd_spi_controller (
    input clk,
    input reset_n, // Tín hiệu reset tích cực mức thấp
    input start_read,
    input start_write,
    input [31:0] lba,
    input [7:0] data_in,
    input [8:0] data_addr,
    input data_we,

    output reg [7:0] data_out,
    output reg busy,
    output reg done,

    output reg sd_cs,
    output reg sd_sck,
    output reg sd_mosi,
    input sd_miso
);

    // SPI clock divider (dựa trên clock 27MHz của board Tang Nano 9K)
    localparam CLK_FREQ = 27000000;
    localparam BAUD_INIT = 400000; // Tốc độ khởi tạo <= 400kHz
    localparam BAUD_DATA = 25000000; // Tốc độ tối đa khi truyền dữ liệu

    // Chia 2*N, nên CLK_DIV = CLK_FREQ / (2 * BAUD)
    localparam CLK_DIV_INIT = CLK_FREQ / (2 * BAUD_INIT);
    localparam CLK_DIV_DATA = (CLK_FREQ / (2 * BAUD_DATA) > 0) ? (CLK_FREQ / (2 * BAUD_DATA)) : 1;


    reg [7:0] spi_tx_data;
    reg [7:0] spi_rx_data;
    reg [3:0] bit_cnt;
    reg [15:0] clk_div; // Tăng kích thước để chứa CLK_DIV_INIT
    reg sck_rising;
    reg sck_falling;
    reg spi_clk_en;
    reg [7:0] byte_buffer [0:511];

    reg [3:0] state;
    parameter IDLE = 0,
              CMD_SEND = 1,
              CMD_LOAD = 2, // State mới để sửa lỗi timing
              CMD_WAIT = 3,
              READ_TOKEN = 4,
              READ_DATA = 5,
              FINISH = 9;

    reg [47:0] cmd_shift;
    reg [9:0] data_counter;
    reg op_read;

    // Gán data_out từ buffer khi có yêu cầu
    always @(posedge clk) begin
        if (~busy) begin
            data_out <= byte_buffer[data_addr];
        end
    end

    // =================================================================
    // KHỐI ALWAYS DUY NHẤT ĐIỀU KHIỂN TOÀN BỘ LOGIC
    // =================================================================
    always @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            // Trạng thái khởi tạo khi reset
            state <= IDLE;
            busy <= 0;
            done <= 0;
            sd_cs <= 1;
            sd_sck <= 1;
            sd_mosi <= 1;
            spi_clk_en <= 0;
            bit_cnt <= 0;
            data_counter <= 0;
            sck_rising <= 0;
            sck_falling <= 0;
        end else begin
            // ------------- Logic tạo xung nhịp và phát hiện sườn xung -------------
            sck_rising <= 1'b0;
            sck_falling <= 1'b0;
            
            if (spi_clk_en) begin
                if (clk_div == 0) begin
                    sd_sck <= ~sd_sck;
                    
                    if (state == READ_DATA) begin
                        clk_div <= CLK_DIV_DATA - 1;
                    end else begin
                        clk_div <= CLK_DIV_INIT - 1;
                    end
                    
                    if (~sd_sck) begin // Đây là sườn lên của sd_sck tiếp theo
                        sck_rising <= 1'b1;
                        sd_mosi <= spi_tx_data[7];
                        spi_tx_data <= spi_tx_data << 1;
                    end else begin
                        sck_falling <= 1'b1;
                    end
                end else begin
                    clk_div <= clk_div - 1;
                end
            end
            
            // ------------- Logic Máy Trạng Thái (FSM) -------------
            case (state)
                IDLE: begin
                    sd_cs <= 1;
                    sd_sck <= 1;
                    sd_mosi <= 1;
                    busy <= 0;
                    done <= 0;
                    spi_clk_en <= 0;

                    if (start_read) begin
                        busy <= 1;
                        done <= 0; // Quan trọng: reset cờ done
                        op_read <= 1;
                        state <= CMD_SEND;
                    end else if (start_write) begin
                        busy <= 1;
                        done <= 0; // Quan trọng: reset cờ done
                        op_read <= 0;
                        state <= CMD_SEND;
                    end
                end

                CMD_SEND: begin
                    // Chỉ nạp toàn bộ lệnh vào thanh ghi lớn cmd_shift
                    cmd_shift <= build_cmd(17, lba); // CMD17: READ_SINGLE_BLOCK
                    state <= CMD_LOAD;
                end

                CMD_LOAD: begin // State mới để nạp byte đầu tiên một cách an toàn
                    sd_cs <= 0;
                    bit_cnt <= 7;
                    spi_tx_data <= cmd_shift[47:40];
                    clk_div <= CLK_DIV_INIT - 1;
                    spi_clk_en <= 1;
                    state <= CMD_WAIT;
                end

                CMD_WAIT: begin
                    if (sck_falling) begin
                        spi_rx_data <= {spi_rx_data[6:0], sd_miso};

                        if (bit_cnt == 0) begin
                            bit_cnt <= 7;
                            if (cmd_shift[39:0] == 40'h0) begin
                                state <= READ_TOKEN;
                                spi_tx_data <= 8'hFF;
                            end else begin
                                cmd_shift <= cmd_shift << 8;
                                spi_tx_data <= cmd_shift[47:40];
                            end
                        end else begin
                            bit_cnt <= bit_cnt - 1;
                        end
                    end
                end

                READ_TOKEN: begin
                    if(sck_rising) begin
                        spi_tx_data <= 8'hFF;
                    end

                    if (sck_falling) begin
                        spi_rx_data <= {spi_rx_data[6:0], sd_miso};
                        if (spi_rx_data == 8'hFE) begin
                            data_counter <= 0;
                            bit_cnt <= 7;
                            state <= READ_DATA;
                        end
                    end
                end

                READ_DATA: begin
                    if(sck_rising) begin
                        spi_tx_data <= 8'hFF;
                    end
                
                    if (sck_falling) begin
                        spi_rx_data <= {spi_rx_data[6:0], sd_miso};

                        if (bit_cnt == 0) begin
                            byte_buffer[data_counter] <= spi_rx_data;
                            bit_cnt <= 7;

                            if (data_counter == 511) begin
                                state <= FINISH;
                            end else begin
                                data_counter <= data_counter + 1;
                            end
                        end else begin
                            bit_cnt <= bit_cnt - 1;
                        end
                    end
                end

                FINISH: begin
                    spi_clk_en <= 0;
                    sd_cs <= 1;
                    done <= 1;
                    busy <= 0;
                    state <= IDLE;
                end
                
                default: begin
                    state <= IDLE;
                end

            endcase
        end
    end

    function [47:0] build_cmd;
        input [7:0] cmd;
        input [31:0] arg;
        begin
            build_cmd = {8'h40 | cmd, arg, 7'h7F, 1'b1};
        end
    endfunction

endmodule