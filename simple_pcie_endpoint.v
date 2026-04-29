module simple_pcie_endpoint #(
    parameter integer DATA_W = 32,
    parameter integer DEPTH = 4,
    parameter integer SERVICE_CYCLES = 3
) (
    input  wire                  clk,
    input  wire                  rst,
    input  wire                  req_valid,
    input  wire [DATA_W-1:0]     req_data,
    output wire                  req_ready,
    output reg                   rsp_valid,
    output reg  [DATA_W-1:0]     rsp_data,
    output reg  [31:0]           occupancy
);

    localparam PTR_W = (DEPTH <= 2) ? 1 : $clog2(DEPTH);

    reg [DATA_W-1:0] fifo [0:DEPTH-1];
    reg [PTR_W-1:0] wr_ptr;
    reg [PTR_W-1:0] rd_ptr;
    reg [31:0] count;
    reg [31:0] service_ctr;
    reg busy;

    assign req_ready = (count < DEPTH);

    integer i;
    always @(posedge clk) begin
        if (rst) begin
            wr_ptr <= 0;
            rd_ptr <= 0;
            count <= 0;
            service_ctr <= 0;
            busy <= 0;
            rsp_valid <= 0;
            rsp_data <= 0;
            occupancy <= 0;
            for (i = 0; i < DEPTH; i = i + 1) begin
                fifo[i] <= 0;
            end
        end else begin
            rsp_valid <= 0;

            if (req_valid && req_ready) begin
                fifo[wr_ptr] <= req_data;
                wr_ptr <= (wr_ptr == DEPTH - 1) ? 0 : wr_ptr + 1'b1;
                count <= count + 1'b1;
            end

            if (!busy && (count != 0)) begin
                busy <= 1'b1;
                service_ctr <= SERVICE_CYCLES - 1;
            end else if (busy) begin
                if (service_ctr == 0) begin
                    busy <= 1'b0;
                    rsp_valid <= 1'b1;
                    rsp_data <= fifo[rd_ptr] + 32'h1000_0000;
                    rd_ptr <= (rd_ptr == DEPTH - 1) ? 0 : rd_ptr + 1'b1;
                    count <= count - 1'b1;
                end else begin
                    service_ctr <= service_ctr - 1'b1;
                end
            end

            occupancy <= count;
        end
    end

endmodule
