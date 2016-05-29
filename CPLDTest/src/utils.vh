`ifndef _UTILS_VH_
`define _UTILS_VH_

function integer u_log2;
    input [31:0] depth;
    integer i;
    begin
        i = depth;        
        for(u_log2 = 0; i > 0; u_log2 = u_log2 + 1) begin
            i = i >> 1;
        end
    end
endfunction

`endif // _UTILS_VH_