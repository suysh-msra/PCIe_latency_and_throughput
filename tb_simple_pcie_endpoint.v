`timescale 1ns/1ps

module tb_simple_pcie_endpoint;

    reg clk = 0;
    reg rst = 1;
    reg req_valid = 0;
    reg [31:0] req_data = 0;
    wire req_ready;
    wire rsp_valid;
    wire [31:0] rsp_data;
    wire [31:0] occupancy;

    simple_pcie_endpoint #(
        .DATA_W(32),
        .DEPTH(4),
        .SERVICE_CYCLES(3)
    ) dut (
        .clk(clk),
        .rst(rst),
        .req_valid(req_valid),
        .req_data(req_data),
        .req_ready(req_ready),
        .rsp_valid(rsp_valid),
        .rsp_data(rsp_data),
        .occupancy(occupancy)
    );

    always #5 clk = ~clk;

    task send_req(input [31:0] value);
        begin
            @(posedge clk);
            while (!req_ready) begin
                @(posedge clk);
            end
            req_valid <= 1'b1;
            req_data <= value;
            @(posedge clk);
            req_valid <= 1'b0;
            req_data <= 0;
        end
    endtask

    integer cycle;
    initial begin
        $display("time req_valid req_ready rsp_valid occupancy rsp_data");

        repeat (3) @(posedge clk);
        rst <= 0;

        fork
            begin
                send_req(32'h0000_0001);
                send_req(32'h0000_0002);
                send_req(32'h0000_0003);
                send_req(32'h0000_0004);
                send_req(32'h0000_0005);
                send_req(32'h0000_0006);
            end
            begin
                for (cycle = 0; cycle < 30; cycle = cycle + 1) begin
                    @(posedge clk);
                    $display("%0t %0b %0b %0b %0d 0x%08h",
                             $time, req_valid, req_ready, rsp_valid, occupancy, rsp_data);
                end
            end
        join

        $finish;
    end

endmodule
