`timescale 1ns/1ns

module axi_lite_mem #(
    parameter integer ADDR_WIDTH = 32,
    parameter integer DATA_WIDTH = 32,
    parameter integer DEPTH_WORDS = 1024,          // number of 32-bit words
    parameter integer BASE_ADDR   = 32'h0000_0000  // base address decode
)(
    input  wire                     ACLK,
    input  wire                     ARESETn,

    // Write address channel
    input  wire [ADDR_WIDTH-1:0]    AWADDR,
    input  wire                     AWVALID,
    output reg                      AWREADY,

    // Write data channel
    input  wire [DATA_WIDTH-1:0]    WDATA,
    input  wire [(DATA_WIDTH/8)-1:0] WSTRB,
    input  wire                     WVALID,
    output reg                      WREADY,

    // Write response channel
    output reg  [1:0]               BRESP,
    output reg                      BVALID,
    input  wire                     BREADY,

    // Read address channel
    input  wire [ADDR_WIDTH-1:0]    ARADDR,
    input  wire                     ARVALID,
    output reg                      ARREADY,

    // Read data channel
    output reg  [DATA_WIDTH-1:0]    RDATA,
    output reg  [1:0]               RRESP,
    output reg                      RVALID,
    input  wire                     RREADY
);

    // Simple 32-bit word memory
    reg [DATA_WIDTH-1:0] mem [0:DEPTH_WORDS-1];

    // Latches for address when accepted
    reg [ADDR_WIDTH-1:0] awaddr_q;
    reg [ADDR_WIDTH-1:0] araddr_q;

    // Track whether we have accepted address/data for a write
    reg aw_seen;
    reg w_seen;

    // Address to word index helper
    function integer word_index(input [ADDR_WIDTH-1:0] addr);
        reg [ADDR_WIDTH-1:0] off;
        begin
            off = addr - BASE_ADDR;
            // word addressing: drop lowest 2 bits for 32-bit words
            /* verilator lint_off WIDTHEXPAND */
            word_index = off[ADDR_WIDTH-1:2];
        end
    endfunction

    integer i;

    always @(negedge ARESETn or posedge ACLK) begin
        if (!ARESETn) begin
            AWREADY <= 1'b0;
            WREADY  <= 1'b0;
            BVALID  <= 1'b0;
            BRESP   <= 2'b00;

            ARREADY <= 1'b0;
            RVALID  <= 1'b0;
            RRESP   <= 2'b00;
            RDATA   <= {DATA_WIDTH{1'b0}};

            awaddr_q <= {ADDR_WIDTH{1'b0}};
            araddr_q <= {ADDR_WIDTH{1'b0}};
            aw_seen  <= 1'b0;
            w_seen   <= 1'b0;

            // Optional: init memory to 0
            for (i = 0; i < DEPTH_WORDS; i = i + 1)
                mem[i] <= {DATA_WIDTH{1'b0}};
        end else begin
            // Defaults: ready when not holding a response
            // (simple design: only accept new write when no BVALID outstanding)
            AWREADY <= (!BVALID) && (!aw_seen);
            WREADY  <= (!BVALID) && (!w_seen);

            ARREADY <= (!RVALID); // accept new read only when no RVALID outstanding

            // -------------------------
            // WRITE ADDRESS HANDSHAKE
            // -------------------------
            if (AWREADY && AWVALID) begin
                awaddr_q <= AWADDR;
                aw_seen  <= 1'b1;
            end

            // -------------------------
            // WRITE DATA HANDSHAKE
            // -------------------------
            if (WREADY && WVALID) begin
                w_seen <= 1'b1;
            end

            // -------------------------
            // PERFORM WRITE when both seen
            // -------------------------
            if (!BVALID && aw_seen && w_seen) begin
                integer idx;
                reg [DATA_WIDTH-1:0] cur;
                idx = word_index(awaddr_q);

                // Bounds check: ignore writes out of range (still respond OKAY)
                if (idx >= 0 && idx < DEPTH_WORDS) begin
                    cur = mem[idx];
                    // Byte enables
                    if (WSTRB[0]) cur[7:0]   = WDATA[7:0];
                    if (WSTRB[1]) cur[15:8]  = WDATA[15:8];
                    if (WSTRB[2]) cur[23:16] = WDATA[23:16];
                    if (WSTRB[3]) cur[31:24] = WDATA[31:24];
                    mem[idx] <= cur;
                end

                BRESP  <= 2'b00; // OKAY
                BVALID <= 1'b1;

                // Clear for next write
                aw_seen <= 1'b0;
                w_seen  <= 1'b0;
            end

            // -------------------------
            // WRITE RESPONSE HANDSHAKE
            // -------------------------
            if (BVALID && BREADY) begin
                BVALID <= 1'b0;
            end

            // -------------------------
            // READ ADDRESS HANDSHAKE
            // -------------------------
            if (ARREADY && ARVALID) begin
                integer ridx;
                araddr_q <= ARADDR;
                ridx = word_index(ARADDR);

                if (ridx >= 0 && ridx < DEPTH_WORDS)
                    RDATA <= mem[ridx];
                else
                    RDATA <= {DATA_WIDTH{1'b0}};

                RRESP  <= 2'b00; // OKAY
                RVALID <= 1'b1;
            end

            // -------------------------
            // READ DATA HANDSHAKE
            // -------------------------
            if (RVALID && RREADY) begin
                RVALID <= 1'b0;
            end
        end
    end

endmodule

module top;
    logic ACLK = 0;
    logic ARESETn = 0;

    always #5 ACLK = ~ACLK;

    logic [31:0] AWADDR;
    logic AWVALID;
    logic AWREADY;
    logic [31:0] WDATA;
    logic [3:0] WSTRB;
    logic WVALID;
    logic WREADY;
    logic [31:0] ARADDR;
    logic ARVALID;
    logic ARREADY;
    logic [31:0] RDATA;
    logic [1:0] RRESP;
    logic RVALID;
    logic RREADY;
    logic [1:0] BRESP;
    logic BVALID;
    logic BREADY;

    axi_lite_mem u_mem (
        .ACLK(ACLK),
        .ARESETn(ARESETn),
        .AWADDR(AWADDR),
        .AWVALID(AWVALID),
        .AWREADY(AWREADY),
        .WDATA(WDATA),
        .WSTRB(WSTRB),
        .WVALID(WVALID),
        .WREADY(WREADY),
        .BRESP(BRESP),
        .BVALID(BVALID),
        .BREADY(BREADY),
        .ARADDR(ARADDR),
        .ARVALID(ARVALID),
        .ARREADY(ARREADY),
        .RDATA(RDATA),
        .RRESP(RRESP),
        .RVALID(RVALID),
        .RREADY(RREADY)
    );

    initial begin
        // reset pulse
        #50 ARESETn = 1;
    end

    // initial begin
    //     $dumpfile("axil.vcd");
    //     $dumpvars(0, top);
    // end


endmodule
