         mov (4|M0)  r1.0<1>:ub    r1.0<0;1,0>:ub
         mul (1|M0)  r2.0<1>:ud    r0.1<0;1,0>:ud    0x10:ud
         mov (1|M0)  r2.1<1>:ud    r0.6<0;1,0>:ud
         mov (8|M0)  r4.0<1>:ud    r0.0<8;8,1>:ud
         mov (2|M0)  r4.0<1>:ud    r2.0<2;2,1>:ud
         mov (1|M0)  r4.2<1>:ud    0xF:ud
         mov (16|M0) r5.0<1>:ud    r1.0<0;1,0>:ud
         send (16|M0)  acc0.0:uw  r4:d  0xC       0x060A8000 //  DP_DC1  wr:3h, rd:0, fc: 0x28000
         mov (8|M0)  r112.0<1>:ud  r0.0<8;8,1>:ud
         send (16|M0)  null:uw  r112:d  0x27      0x02000010 {EOT} //  SPAWNER  wr:1, rd:0, fc: 0x10
