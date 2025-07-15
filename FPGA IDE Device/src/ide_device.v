// ===================================================================
// File:         ide_device.v
// Version:      3.0 (LED State Debug)
// Mục đích:     Thêm cổng output để hiển thị FSM state ra đèn LED.
// ===================================================================

module ide_device(
    input clk,
    input reset_n,
    input dior_n,
    input diow_n,
    input cs0_n,
    input [2:0] addr,
    inout [15:0] data_bus,
    output reg intrq,
    output reg rd_req,
    input [7:0] rd_data,
    input busy,
    input done,
    output reg [31:0] lba,
    
    // Bỏ cổng uart_tx vì không dùng đến trong bài test này
    // output uart_tx,

    // Cổng output 6-bit mới để điều khiển 6 đèn LED
    output [5:0] led_fsm_state
);
    reg [7:0] command_reg;
    reg [2:0] state;
    localparam IDLE=3'd0, IDENTIFY_READY=3'd1, IDENTIFY_SEND=3'd2,
               READ_READY=3'd3, READ_WAIT=3'd4, READ_SEND=3'd5;

    // Gán trực tiếp giá trị nhị phân của state ra đèn LED
    // Mỗi state sẽ có một kiểu đèn sáng khác nhau
    assign led_fsm_state = 
               (state == IDLE)           ? 6'b000001 : // Đèn 0 sáng
               (state == IDENTIFY_READY) ? 6'b000010 : // Đèn 1 sáng
               (state == IDENTIFY_SEND)  ? 6'b000100 : // Đèn 2 sáng
               (state == READ_READY)     ? 6'b001000 : // Đèn 3 sáng
               (state == READ_WAIT)      ? 6'b010000 : // Đèn 4 sáng
               (state == READ_SEND)      ? 6'b100000 : // Đèn 5 sáng
                                         6'b111111;  // Tất cả đều sáng nếu là state lạ

    // Các phần khai báo và logic còn lại giữ nguyên
    reg [15:0] data_out;
    assign data_bus = (!cs0_n && !dior_n && (state == IDENTIFY_SEND || state == READ_SEND)) 
                        ? data_out : 16'hzzzz;
    reg [8:0] identify_index;
    reg [8:0] read_index;
    reg [15:0] identify_device[0:255];
    integer i;
    initial begin
        for(i=0; i<256; i=i+1) identify_device[i]=16'h0;
        identify_device[0]=16'h0040; identify_device[1]=16'd16383; identify_device[3]=16'd16;
        identify_device[6]=16'd63; identify_device[27]="M" << 8 | "Y"; identify_device[28]="F" << 8 | "P";
        identify_device[29]="G" << 8 | "A"; identify_device[30]="_" << 8 | "D";
        identify_device[31]="I" << 8 | "S"; identify_device[32]="K" << 8 | " ";
        identify_device[60]=16'd2048; identify_device[61]=16'd0;
    end
    
    always @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            state <= IDLE;
            // ... reset các reg khác ...
        end else begin
            case (state)
                IDLE: begin
                    intrq <= 1'b0;
                    rd_req <= 0;
                    if (!diow_n && !cs0_n && (addr == 3'b111)) begin
                        command_reg <= data_bus[7:0];
                        if (data_bus[7:0] == 8'hEC) begin
                            state <= IDENTIFY_READY;
                        end else if (data_bus[7:0] == 8'h20 || data_bus[7:0] == 8'hC8) begin
                            state <= READ_READY;
                        end
                    end
                end
                // ... các state khác giữ nguyên ...
                IDENTIFY_READY: begin
                    intrq <= 1'b1;
                    identify_index <= 0;
                    state <= IDENTIFY_SEND;
                end
                IDENTIFY_SEND: begin
                    intrq <= 1'b0;
                    if (!cs0_n && !dior_n) begin
                        data_out <= identify_device[identify_index];
                        identify_index <= identify_index + 1;
                        if (identify_index == 255) state <= IDLE;
                    end
                end
                READ_READY: begin
                    lba <= 32'd0; 
                    rd_req <= 1;
                    state <= READ_WAIT;
                end
                READ_WAIT: begin
                    rd_req <= 0;
                    if (~busy && done) begin
                        read_index <= 0;
                        state <= READ_SEND;
                    end
                end
                READ_SEND: begin
                    if (!cs0_n && !dior_n) begin
                        data_out <= {rd_data, rd_data};
                        read_index <= read_index + 1;
                        if (read_index == 255) state <= IDLE;
                    end
                end
                default: state <= IDLE;
            endcase
        end
    end
endmodule