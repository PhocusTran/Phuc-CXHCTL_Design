// uart_tx.v - Hoạt động ổn định
module uart_tx #(
    parameter BAUD = 115200,
    parameter CLK_FREQ = 27000000 // Clock cho board Tang Nano 9K
) (
    input clk,
    input start,
    input [7:0] data,
    output reg tx,
    output reg busy
);

    localparam CLKS_PER_BIT = CLK_FREQ / BAUD;

    reg [3:0] bit_index = 0;
    reg [15:0] clk_count = 0;
    reg [9:0] shift = 10'b1111111111;

    always @(posedge clk) begin
        if (busy == 0) begin
            if (start) begin
                shift <= {1'b1, data, 1'b0};
                bit_index <= 0;
                busy <= 1;
                clk_count <= CLKS_PER_BIT - 1;
            end
        end else begin
            if (clk_count == 0) begin
                tx <= shift[0];
                shift <= {1'b1, shift[9:1]};
                bit_index <= bit_index + 1;

                if (bit_index == 9) begin
                    busy <= 0;
                end else begin
                    clk_count <= CLKS_PER_BIT - 1;
                end
            end else begin
                clk_count <= clk_count - 1;
            end
        end
    end

    initial begin
        tx = 1;
        busy = 0;
    end

endmodule       