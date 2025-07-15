// uart_rx.v
module uart_rx
#(
    parameter BAUD = 115200,
    parameter CLK_FREQ = 50000000
)
(
    input clk,
    input rx,
    output reg [7:0] data,
    output reg ready
);

    localparam CLKS_PER_BIT = CLK_FREQ / BAUD;
    localparam HALF_CLKS_PER_BIT = CLKS_PER_BIT / 2;

    reg [15:0] clk_count = 0;
    reg [2:0] bit_index = 0;
    reg [7:0] rx_shift = 0;

    reg rx_d1 = 1'b1;
    reg rx_d2 = 1'b1;

    reg busy = 0;

    always @(posedge clk) begin
        rx_d1 <= rx;
        rx_d2 <= rx_d1;
    end

    always @(posedge clk) begin
        ready <= 0;

        if (!busy) begin
            if (rx_d2 == 0) begin
                busy <= 1;
                clk_count <= HALF_CLKS_PER_BIT;
                bit_index <= 0;
            end
        end else begin
            if (clk_count == 0) begin
                if (bit_index < 8) begin
                    rx_shift[bit_index] <= rx_d2;
                    bit_index <= bit_index + 1;
                    clk_count <= CLKS_PER_BIT - 1;
                end else begin
                    busy <= 0;
                    data <= rx_shift;
                    ready <= 1;
                end
            end else begin
                clk_count <= clk_count - 1;
            end
        end
    end

endmodule
