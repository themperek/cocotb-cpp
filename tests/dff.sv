// This file is public domain, it can be freely copied without restrictions.
// SPDX-License-Identifier: CC0-1.0

`timescale 1ns/1ns

module dff (
  input logic clk, d,
  output logic q
);

always @(posedge clk) begin
  q <= d;
end

// initial begin
//   $dumpfile("dff.vcd");
//   $dumpvars(0, dff);
// end

endmodule
