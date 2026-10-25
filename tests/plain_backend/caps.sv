(* blackbox *)
module BOX #(parameter W = 1) (input A, input B, output Y, inout P);
endmodule

module sub(inout y, output r);
  assign r = y;
endmodule

module top(input clk, input en, input [1:0] a, inout io, output y, output r, output reg l, output reg q, output [1:0] rd);
  reg [1:0] mem [0:3];
  assign io = en ? a[0] : 1'bz;
  BOX #(.W(2)) b(.A(a[0]), .Y(y));
  sub u(.y(io), .r(r));
  always_latch if (en) l = a[1];
  always_ff @(posedge clk) begin
    q <= a[0];
    mem[a] <= a;
    $display("q=%d", q);
  end
  assign rd = mem[a];
`ifndef NO_CHECKS
  always_comb assert (a != 2'b11 || en);
`endif
endmodule
