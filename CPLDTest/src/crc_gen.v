//////////////////////////////////////////////////////////////////////
////                                                              ////
////  crc_gen.v                                                   ////
////                                                              ////
////  This file is part of the Ethernet IP core project           ////
////  http://www.opencores.org/projects.cgi/web/ethernet_tri_mode ////
////                                                              ////
////  Author(s):                                                  ////
////      - Jon Gao (gaojon@yahoo.com)                            ////
////                                                              ////
////                                                              ////
//////////////////////////////////////////////////////////////////////
////                                                              ////
//// Copyright (C) 2001 Authors                                   ////
////                                                              ////
//// This source file may be used and distributed without         ////
//// restriction provided that this copyright statement is not    ////
//// removed from the file and that any derivative work contains  ////
//// the original copyright notice and the associated disclaimer. ////
////                                                              ////
//// This source file is free software; you can redistribute it   ////
//// and/or modify it under the terms of the GNU Lesser General   ////
//// Public License as published by the Free Software Foundation; ////
//// either version 2.1 of the License, or (at your option) any   ////
//// later version.                                               ////
////                                                              ////
//// This source is distributed in the hope that it will be       ////
//// useful, but WITHOUT ANY WARRANTY; without even the implied   ////
//// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      ////
//// PURPOSE.  See the GNU Lesser General Public License for more ////
//// details.                                                     ////
////                                                              ////
//// You should have received a copy of the GNU Lesser General    ////
//// Public License along with this source; if not, download it   ////
//// from http://www.opencores.org/lgpl.shtml                     ////
////                                                              ////
//////////////////////////////////////////////////////////////////////
//                                                                ////                         
// CVS Revision History                                           ////
//                                                                ////
// $Log: not supported by cvs2svn $                               ////
// Revision 1.2  2005/12/16 06:44:17  Administrator               ////
// replaced tab with space.                                       ////
// passed 9.6k length frame test.                                 ////
//                                                                ////
// Revision 1.1.1.1  2005/12/13 01:51:45  Administrator           ////
// no message                                                     ////
//                                                                ////
//////////////////////////////////////////////////////////////////////
////                                                              ////
//// Copyright (C) 2016 Frozen Team                               ////
////                                                              ////
//// This module modified to conform with uniform code            ////
//// convention used in the project.                              ////
////                                                              ////
//// Perform 32 bit output at once without cycling each byte.     ////
////                                                              ////
//////////////////////////////////////////////////////////////////////

 
module crc_gen
(
    i_clk,
    i_rst_n,
    i_init,
    i_data,
    i_data_en,
    i_data_rd,
    o_crc
);
    input         i_rst_n;
    input         i_init;
    input         i_clk;
    input  [ 7:0] i_data;
    input         i_data_en;

    output [31:0] o_crc;
     
    reg    [ 7:0] crc_out;
    reg    [31:0] crc_reg;

    // Input data width is 8bit, and the first bit is bit[0]
    function [31:0] NextCRC;
        input [ 7:0] D;
        input [31:0] C;
        reg   [31:0] new_crc;
        begin
            new_crc[0] = C[24] ^ C[30] ^ D[1] ^ D[7];
            new_crc[1] = C[25] ^ C[31] ^ D[0] ^ D[6] ^ C[24] ^ C[30] ^ D[1] ^ D[7];
            new_crc[2] = C[26] ^ D[5] ^ C[25] ^ C[31] ^ D[0] ^ D[6] ^ C[24] ^ C[30] ^ D[1] ^ D[7];
            new_crc[3] = C[27] ^ D[4] ^ C[26] ^ D[5] ^ C[25] ^ C[31] ^ D[0] ^ D[6];
            new_crc[4] = C[28] ^ D[3] ^ C[27] ^ D[4] ^ C[26] ^ D[5] ^ C[24] ^ C[30] ^ D[1] ^ D[7];
            new_crc[5] = C[29] ^ D[2] ^ C[28] ^ D[3] ^ C[27] ^ D[4] ^ C[25] ^ C[31] ^ D[0] ^ D[6] ^ C[24] ^ C[30] ^ D[1] ^ D[7];
            new_crc[6] = C[30] ^ D[1] ^ C[29] ^ D[2] ^ C[28] ^ D[3] ^ C[26] ^ D[5] ^ C[25] ^ C[31] ^ D[0] ^ D[6];
            new_crc[7] = C[31] ^ D[0] ^ C[29] ^ D[2] ^ C[27] ^ D[4] ^ C[26] ^ D[5] ^ C[24] ^ D[7];
            new_crc[8] = C[0] ^ C[28] ^ D[3] ^ C[27] ^ D[4] ^ C[25] ^ D[6] ^ C[24] ^ D[7];
            new_crc[9] = C[1] ^ C[29] ^ D[2] ^ C[28] ^ D[3] ^ C[26] ^ D[5] ^ C[25] ^ D[6];
            new_crc[10] = C[2] ^ C[29] ^ D[2] ^ C[27] ^ D[4] ^ C[26] ^ D[5] ^ C[24] ^ D[7];
            new_crc[11] = C[3] ^ C[28] ^ D[3] ^ C[27] ^ D[4] ^ C[25] ^ D[6] ^ C[24] ^ D[7];
            new_crc[12] = C[4] ^ C[29] ^ D[2] ^ C[28] ^ D[3] ^ C[26] ^ D[5] ^ C[25] ^ D[6] ^ C[24] ^ C[30] ^ D[1] ^ D[7];
            new_crc[13] = C[5] ^ C[30] ^ D[1] ^ C[29] ^ D[2] ^ C[27] ^ D[4] ^ C[26] ^ D[5] ^ C[25] ^ C[31] ^ D[0] ^ D[6];
            new_crc[14] = C[6] ^ C[31] ^ D[0] ^ C[30] ^ D[1] ^ C[28] ^ D[3] ^ C[27] ^ D[4] ^ C[26] ^ D[5];
            new_crc[15] = C[7] ^ C[31] ^ D[0] ^ C[29] ^ D[2] ^ C[28] ^ D[3] ^ C[27] ^ D[4];
            new_crc[16] = C[8] ^ C[29] ^ D[2] ^ C[28] ^ D[3] ^ C[24] ^ D[7];
            new_crc[17] = C[9] ^ C[30] ^ D[1] ^ C[29] ^ D[2] ^ C[25] ^ D[6];
            new_crc[18] = C[10] ^ C[31] ^ D[0] ^ C[30] ^ D[1] ^ C[26] ^ D[5];
            new_crc[19] = C[11] ^ C[31] ^ D[0] ^ C[27] ^ D[4];
            new_crc[20] = C[12] ^ C[28] ^ D[3];
            new_crc[21] = C[13] ^ C[29] ^ D[2];
            new_crc[22] = C[14] ^ C[24] ^ D[7];
            new_crc[23] = C[15] ^ C[25] ^ D[6] ^ C[24] ^ C[30] ^ D[1] ^ D[7];
            new_crc[24] = C[16] ^ C[26] ^ D[5] ^ C[25] ^ C[31] ^ D[0] ^ D[6];
            new_crc[25] = C[17] ^ C[27] ^ D[4] ^ C[26] ^ D[5];
            new_crc[26] = C[18] ^ C[28] ^ D[3] ^ C[27] ^ D[4] ^ C[24] ^ C[30] ^ D[1] ^ D[7];
            new_crc[27] = C[19] ^ C[29] ^ D[2] ^ C[28] ^ D[3] ^ C[25] ^ C[31] ^ D[0] ^ D[6];
            new_crc[28] = C[20] ^ C[30] ^ D[1] ^ C[29] ^ D[2] ^ C[26] ^ D[5];
            new_crc[29] = C[21] ^ C[31] ^ D[0] ^ C[30] ^ D[1] ^ C[27] ^ D[4];
            new_crc[30] = C[22] ^ C[31] ^ D[0] ^ C[28] ^ D[3];
            new_crc[31] = C[23] ^ C[29] ^ D[2];
            NextCRC = new_crc;
        end
    endfunction
    //******************************************************************************

    wire [31:0] next_crc = NextCRC(i_data, crc_reg);

    always @(negedge i_clk or negedge i_rst_n)
        if (i_rst_n) begin
            crc_reg <= 32'hffffffff;
        end else if (i_init) begin
            crc_reg <= 32'hffffffff;
        end else if (i_data_en) begin
            crc_reg <= next_crc;
        end else if (i_data_rd) begin
            crc_reg <= {crc_reg[23:0], 8'b1};
        end

    wire [7:0] crc_out_inv;
    genvar i;
    generate
        for (i = 0; i < 8; i = i + 1) begin : GEN_CRC_OUT
            assign crc_out_inv[i] = next_crc[31 - i];
        end
    endgenerate
     
    always @(negedge i_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            crc_out <= 8'b0;
        end else begin
            if (i_data_en) begin
                crc_out <= ~crc_out_inv;
            end
        end
    end

    assign o_crc = crc_out;
 
endmodule