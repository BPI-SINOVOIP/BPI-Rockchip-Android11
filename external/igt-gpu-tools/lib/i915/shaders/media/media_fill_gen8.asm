        mov (4|M0)  r1.0<1>:ub    r1.0<0;1,0>:ub
        mov (8|M0)  r4.0<1>:ud    r0.0<8;8,1>:ud
        mov (2|M0)  r4.0<1>:ud    r2.0<2;2,1>:ud
        mov (1|M0)  r4.2<1>:ud    0xF000F:ud
        mov (16|M0) r5.0<1>:ud    r1.0<0;1,0>:ud
        mov (16|M0) r7.0<1>:ud    r1.0<0;1,0>:ud
        mov (16|M0) r9.0<1>:ud    r1.0<0;1,0>:ud
        mov (16|M0) r11.0<1>:ud   r1.0<0;1,0>:ud
        send (16|M0) acc0.0:uw  r4:d  0x1000000C  0x120A8000 //  DP_DC1  wr:9h, rd:0, fc: 0x28000
        mov (8|M0) r112.0<1>:ud  r0.0<8;8,1>:ud
        send (16|M0) null:uw  r112:d  0x10000027  0x02000010 {EOT} //  SPAWNER  wr:1, rd:0, fc: 0x10
