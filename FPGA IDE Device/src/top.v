// ===================================================================
// File:         top.v
// Version:      6.0 (Final LED State Debug)
// Mục đích:     Module chính, tích hợp đầy đủ chức năng và đưa
//               trạng thái của FSM trong ide_device ra 6 đèn LED
//               để debug trực quan.
// ===================================================================

module top (
    // --- Cổng giao tiếp IDE (phía máy CNC/đầu đọc thẻ) ---
    input dior_n,
    input diow_n,
    input cs0_n,
    input [2:0] addr,
    inout [15:0] data_bus,
    output intrq,

    // --- Cổng giao tiếp thẻ nhớ microSD (SPI) ---
    output sd_clk,
    output sd_mosi,
    input sd_miso,
    output sd_cs,

    // --- Cổng Debug (Output 6-bit nối ra 6 chân LED) ---
    output [5:0] leds_ext
);
    // Dây để nhận clock từ IP OSC nội bộ
    wire clk_from_osc;

    // Khởi tạo IP OSC (27MHz)
    // Tên module "OSC" có thể khác tùy vào cách bạn đặt tên khi generate IP
    OSC osc_inst (
        .OSCOUT(clk_from_osc)
    );

    // --- System Reset Logic ---
    // Tạo một tín hiệu reset nội bộ ngắn để đảm bảo khởi động đúng
    reg [15:0] reset_counter = 0;
    wire sys_reset_n = (reset_counter < 16'hFFFF);

    always @(posedge clk_from_osc) begin
        if (reset_counter < 16'hFFFF) begin
            reset_counter <= reset_counter + 1;
        end
    end
    
    // --- Wires để kết nối các module ---
    wire [31:0] lba_addr;
    wire rd_req;
    wire [7:0] rd_data_from_sd;
    wire sd_busy;
    wire sd_done;
    wire [5:0] fsm_state_leds; // Dây trung gian để nhận trạng thái từ ide_device

    // --- Khởi tạo các module con (Instantiation) ---

    // 1. Module IDE Device: Giao diện với thế giới bên ngoài (CNC)
    ide_device ide_inst (
        .clk(clk_from_osc),
        .reset_n(sys_reset_n),
        .dior_n(dior_n),
        .diow_n(diow_n),
        .cs0_n(cs0_n),
        .addr(addr),
        .data_bus(data_bus),
        .intrq(intrq),
        
        // Kết nối đến SD controller
        .lba(lba_addr),
        .rd_req(rd_req),
        .rd_data(rd_data_from_sd),
        .busy(sd_busy),
        .done(sd_done),
        
        // Cổng output mới để lấy state ra ngoài
        .led_fsm_state(fsm_state_leds)
    );

    // 2. Module SD SPI Controller: Giao diện với thẻ nhớ microSD
    sd_spi_controller sd_inst (
        .clk(clk_from_osc),
        .reset_n(sys_reset_n),
        .start_read(rd_req),
        .start_write(1'b0),
        .lba(lba_addr),
        .data_in(8'h00),
        .data_addr(9'h00),
        .data_we(1'b0),
        .data_out(rd_data_from_sd),
        .busy(sd_busy),
        .done(sd_done),
        .sd_cs(sd_cs),
        .sd_sck(sd_clk),
        .sd_mosi(sd_mosi),
        .sd_miso(sd_miso)
    );
    
    // Nối dây trạng thái ra các chân LED vật lý
    // Đảo bit vì LED thường sáng ở mức thấp (active-low)
    assign leds_ext = ~fsm_state_leds;

endmodule