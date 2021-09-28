 pln (8|M0) r10.0<1>:f r6.0<0;1,0>:f r2.0<8;8,1>:f
 pln (8|M0) r11.0<1>:f r6.0<0;1,0>:f r4.0<8;8,1>:f
 pln (8|M0) r12.0<1>:f r6.4<0;1,0>:f r2.0<8;8,1>:f
 pln (8|M0) 13.0<1>:f r6.4<0;1,0>:f r4.0<8;8,1>:f
 send (16|M0) r112:f r10:ub 0x10000002 0x08840001 // SAMPLER wr:4, rd:8, fc: 0x40001
 send (16|M0) null:f r112:ub 0x10000025 0x10031000 {EOT} // DP_RC wr:8, rd:0, Render Target Write msc:16, to #0

