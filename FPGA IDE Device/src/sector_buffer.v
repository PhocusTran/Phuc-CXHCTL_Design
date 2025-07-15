// sector_buffer.v

module sector_buffer (
    input clk,
    input we,
    input [8:0] addr,       // 512 bytes = 9 bits
    input [7:0] data_in,
    output [7:0] data_out
);
    reg [7:0] mem [0:511];

    always @(posedge clk) begin
        if (we) begin
            mem[addr] <= data_in;
        end
    end

    assign data_out = mem[addr];

endmodule