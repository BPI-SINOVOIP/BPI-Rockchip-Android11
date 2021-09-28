

.text
.global neon_rgb888_to_gray16ARM_32




neon_rgb888_to_gray16ARM_32:

	@ r0 = dest
	@ r1 = src
	@ r2 = h
	@ r3 = w

		mov			  r12,r13
		push      {r4,r5,r6,r7,r8,r9,r10,r11,lr}
		ldmfd			r12,{r7}       @r7 = vir_width
		mov       r8,r3
		mul     	r2,r2,r7
		mov     	r3,#11
		mov     	r4,#16
		mov     	r5,#5
		mov       r9,#0x00
		mov       r11,r0
		vdup.8  	d4,r3
		vdup.8  	d5,r4
		vdup.8  	d6,r5
		mov       r3,#00
LOOP:
		vld4.8  	{d0-d3},[r1]!
		vld4.8  	{d7-d10},[r1]!
		vld4.8  	{d11-d14},[r1]!
		vld4.8  	{d15-d18},[r1]!

		vmull.u8  q10,d0,d4
		vmlal.u8  q10,d1,d5
		vmlal.u8  q10,d2,d6
		vshr.u16 	q11,q10,#9
		vmov.u16   r5,d22[0]
		and        r6,r5,#0x0000000f
		vmov.u16   r5,d22[1]
		orr        r6,r6,r5,lsl #4
		vmov.u16   r5,d22[2]
		orr      	 r6,r6,r5,lsl #8
		vmov.u16   r5,d22[3]
		orr        r6,r6,r5,lsl #12
		vmov.u16   r5,d23[0]
		orr        r6,r6,r5,lsl #16
		vmov.u16   r5,d23[1]
		orr        r6,r6,r5,lsl #20
		vmov.u16   r5,d23[2]
		orr      	 r6,r6,r5,lsl #24
		vmov.u16   r5,d23[3]
		orr        r6,r6,r5,lsl #28
		str        r6,[r0],#4

		vmull.u8  q10,d7,d4
		vmlal.u8  q10,d8,d5
		vmlal.u8  q10,d9,d6
		vshr.u16 	q11,q10,#9
		vmov.u16   r5,d22[0]
		and        r6,r5,#0x0000000f
		vmov.u16   r5,d22[1]
		orr        r6,r6,r5,lsl #4
		vmov.u16   r5,d22[2]
		orr      	 r6,r6,r5,lsl #8
		vmov.u16   r5,d22[3]
		orr        r6,r6,r5,lsl #12
		vmov.u16   r5,d23[0]
		orr        r6,r6,r5,lsl #16
		vmov.u16   r5,d23[1]
		orr        r6,r6,r5,lsl #20
		vmov.u16   r5,d23[2]
		orr      	 r6,r6,r5,lsl #24
		vmov.u16   r5,d23[3]
		orr        r6,r6,r5,lsl #28
		str        r6,[r0],#4



		vmull.u8  q10,d11,d4
		vmlal.u8  q10,d12,d5
		vmlal.u8  q10,d13,d6
		vshr.u16 	q11,q10,#9
		vmov.u16   r5,d22[0]
		and        r6,r5,#0x0000000f
		vmov.u16   r5,d22[1]
		orr        r6,r6,r5,lsl #4
		vmov.u16   r5,d22[2]
		orr      	 r6,r6,r5,lsl #8
		vmov.u16   r5,d22[3]
		orr        r6,r6,r5,lsl #12
		vmov.u16   r5,d23[0]
		orr        r6,r6,r5,lsl #16
		vmov.u16   r5,d23[1]
		orr        r6,r6,r5,lsl #20
		vmov.u16   r5,d23[2]
		orr      	 r6,r6,r5,lsl #24
		vmov.u16   r5,d23[3]
		orr        r6,r6,r5,lsl #28
		str        r6,[r0],#4

		vmull.u8  q10,d15,d4
		vmlal.u8  q10,d16,d5
		vmlal.u8  q10,d17,d6
		vshr.u16 	q11,q10,#9

		vmov.u16   r5,d22[0]
		and        r6,r5,#0x0000000f
		vmov.u16   r5,d22[1]
		orr        r6,r6,r5,lsl #4
		vmov.u16   r5,d22[2]
		orr      	 r6,r6,r5,lsl #8
		vmov.u16   r5,d22[3]
		orr        r6,r6,r5,lsl #12
		vmov.u16   r5,d23[0]
		orr        r6,r6,r5,lsl #16
		vmov.u16   r5,d23[1]
		orr        r6,r6,r5,lsl #20
		vmov.u16   r5,d23[2]
		orr      	 r6,r6,r5,lsl #24
		vmov.u16   r5,d23[3]
		orr        r6,r6,r5,lsl #28
		str        r6,[r0],#4


		add       r9,r9,#32
		add       r3,r3,#32
		cmp       r3,r7
	  bne       ADD_TO_LOOP
	  mov       r3,#00
	  add       r11,r11,r8,lsr #1
		mov       r0,r11
ADD_TO_LOOP:
		cmp       r9,r2
		blo       LOOP
		pop       {r4,r5,r6,r7,r8,r9,r10,r11,pc}




.text
.global neon_rgb888_to_gray16ARM_16

neon_rgb888_to_gray16ARM_16:

	@ r0 = dest
	@ r1 = src
	@ r2 = h
	@ r3 = w

		mov			  r12,r13
		push      {r4,r5,r6,r7,r8,r9,r10,r11,lr}
		ldmfd			r12,{r7}       @r7 = vir_width
		mov       r8,r3
		mul     	r2,r2,r7
		mov     	r3,#11
		mov     	r4,#16
		mov     	r5,#5
		mov       r9,#0x00
		mov       r11,r0
		vdup.8  	d4,r3
		vdup.8  	d5,r4
		vdup.8  	d6,r5
		mov       r3,#00
LOOP_16:
		vld4.8  	{d0-d3},[r1]!
		vld4.8  	{d7-d10},[r1]!

		vmull.u8  q10,d0,d4
		vmlal.u8  q10,d1,d5
		vmlal.u8  q10,d2,d6
		vshr.u16 	q11,q10,#9
		vmov.u16   r5,d22[0]
		and        r6,r5,#0x0000000f
		vmov.u16   r5,d22[1]
		orr        r6,r6,r5,lsl #4
		vmov.u16   r5,d22[2]
		orr      	 r6,r6,r5,lsl #8
		vmov.u16   r5,d22[3]
		orr        r6,r6,r5,lsl #12
		vmov.u16   r5,d23[0]
		orr        r6,r6,r5,lsl #16
		vmov.u16   r5,d23[1]
		orr        r6,r6,r5,lsl #20
		vmov.u16   r5,d23[2]
		orr      	 r6,r6,r5,lsl #24
		vmov.u16   r5,d23[3]
		orr        r6,r6,r5,lsl #28
		str        r6,[r0],#4

		vmull.u8  q10,d7,d4
		vmlal.u8  q10,d8,d5
		vmlal.u8  q10,d9,d6
		vshr.u16 	q11,q10,#9
		vmov.u16   r5,d22[0]
		and        r6,r5,#0x0000000f
		vmov.u16   r5,d22[1]
		orr        r6,r6,r5,lsl #4
		vmov.u16   r5,d22[2]
		orr      	 r6,r6,r5,lsl #8
		vmov.u16   r5,d22[3]
		orr        r6,r6,r5,lsl #12
		vmov.u16   r5,d23[0]
		orr        r6,r6,r5,lsl #16
		vmov.u16   r5,d23[1]
		orr        r6,r6,r5,lsl #20
		vmov.u16   r5,d23[2]
		orr      	 r6,r6,r5,lsl #24
		vmov.u16   r5,d23[3]
		orr        r6,r6,r5,lsl #28
		str        r6,[r0],#4


		add       r9,r9,#16
		add       r3,r3,#16
		cmp       r3,r7
	  bne       ADD_TO_LOOP_16
	  mov       r3,#00
	  add       r11,r11,r8,lsr #1
		mov       r0,r11
ADD_TO_LOOP_16:
		cmp       r9,r2
		blo       LOOP_16
		pop       {r4,r5,r6,r7,r8,r9,r10,r11,pc}




























.text
.global neon_bgr888_to_gray16ARM_32




neon_bgr888_to_gray16ARM_32:

	@ r0 = dest
	@ r1 = src
	@ r2 = h
	@ r3 = w

		mov			  r12,r13
		push      {r4,r5,r6,r7,r8,r9,r10,r11,lr}
		ldmfd			r12,{r7}       @r7 = vir_width
		mov       r8,r3
		mul     	r2,r2,r7
		mov     	r3,#11
		mov     	r4,#16
		mov     	r5,#5
		mov       r9,#0x00
		mov       r11,r0
		vdup.8  	d4,r3
		vdup.8  	d5,r4
		vdup.8  	d6,r5
		mov       r3,#00
LOOP_BGR:
		vld4.8  	{d0-d3},[r1]!
		vld4.8  	{d7-d10},[r1]!
		vld4.8  	{d11-d14},[r1]!
		vld4.8  	{d15-d18},[r1]!

		vmull.u8  q10,d0,d6
		vmlal.u8  q10,d1,d5
		vmlal.u8  q10,d2,d4
		vshr.u16 	q11,q10,#9
		vmov.u16   r5,d22[0]
		and        r6,r5,#0x0000000f
		vmov.u16   r5,d22[1]
		orr        r6,r6,r5,lsl #4
		vmov.u16   r5,d22[2]
		orr      	 r6,r6,r5,lsl #8
		vmov.u16   r5,d22[3]
		orr        r6,r6,r5,lsl #12
		vmov.u16   r5,d23[0]
		orr        r6,r6,r5,lsl #16
		vmov.u16   r5,d23[1]
		orr        r6,r6,r5,lsl #20
		vmov.u16   r5,d23[2]
		orr      	 r6,r6,r5,lsl #24
		vmov.u16   r5,d23[3]
		orr        r6,r6,r5,lsl #28
		str        r6,[r0],#4

		vmull.u8  q10,d7,d6
		vmlal.u8  q10,d8,d5
		vmlal.u8  q10,d9,d4
		vshr.u16 	q11,q10,#9
		vmov.u16   r5,d22[0]
		and        r6,r5,#0x0000000f
		vmov.u16   r5,d22[1]
		orr        r6,r6,r5,lsl #4
		vmov.u16   r5,d22[2]
		orr      	 r6,r6,r5,lsl #8
		vmov.u16   r5,d22[3]
		orr        r6,r6,r5,lsl #12
		vmov.u16   r5,d23[0]
		orr        r6,r6,r5,lsl #16
		vmov.u16   r5,d23[1]
		orr        r6,r6,r5,lsl #20
		vmov.u16   r5,d23[2]
		orr      	 r6,r6,r5,lsl #24
		vmov.u16   r5,d23[3]
		orr        r6,r6,r5,lsl #28
		str        r6,[r0],#4



		vmull.u8  q10,d11,d6
		vmlal.u8  q10,d12,d5
		vmlal.u8  q10,d13,d4
		vshr.u16 	q11,q10,#9
		vmov.u16   r5,d22[0]
		and        r6,r5,#0x0000000f
		vmov.u16   r5,d22[1]
		orr        r6,r6,r5,lsl #4
		vmov.u16   r5,d22[2]
		orr      	 r6,r6,r5,lsl #8
		vmov.u16   r5,d22[3]
		orr        r6,r6,r5,lsl #12
		vmov.u16   r5,d23[0]
		orr        r6,r6,r5,lsl #16
		vmov.u16   r5,d23[1]
		orr        r6,r6,r5,lsl #20
		vmov.u16   r5,d23[2]
		orr      	 r6,r6,r5,lsl #24
		vmov.u16   r5,d23[3]
		orr        r6,r6,r5,lsl #28
		str        r6,[r0],#4

		vmull.u8  q10,d15,d6
		vmlal.u8  q10,d16,d5
		vmlal.u8  q10,d17,d4
		vshr.u16 	q11,q10,#9

		vmov.u16   r5,d22[0]
		and        r6,r5,#0x0000000f
		vmov.u16   r5,d22[1]
		orr        r6,r6,r5,lsl #4
		vmov.u16   r5,d22[2]
		orr      	 r6,r6,r5,lsl #8
		vmov.u16   r5,d22[3]
		orr        r6,r6,r5,lsl #12
		vmov.u16   r5,d23[0]
		orr        r6,r6,r5,lsl #16
		vmov.u16   r5,d23[1]
		orr        r6,r6,r5,lsl #20
		vmov.u16   r5,d23[2]
		orr      	 r6,r6,r5,lsl #24
		vmov.u16   r5,d23[3]
		orr        r6,r6,r5,lsl #28
		str        r6,[r0],#4


		add       r9,r9,#32
		add       r3,r3,#32
		cmp       r3,r7
	  bne       ADD_TO_LOOP_BGR
	  mov       r3,#00
	  add       r11,r11,r8,lsr #1
		mov       r0,r11
ADD_TO_LOOP_BGR:
		cmp       r9,r2
		blo       LOOP_BGR
		pop       {r4,r5,r6,r7,r8,r9,r10,r11,pc}




.text
.global neon_bgr888_to_gray16ARM_16

neon_bgr888_to_gray16ARM_16:

	@ r0 = dest
	@ r1 = src
	@ r2 = h
	@ r3 = w

		mov			  r12,r13
		push      {r4,r5,r6,r7,r8,r9,r10,r11,lr}
		ldmfd			r12,{r7}       @r7 = vir_width
		mov       r8,r3
		mul     	r2,r2,r7
		mov     	r3,#11
		mov     	r4,#16
		mov     	r5,#5
		mov       r9,#0x00
		mov       r11,r0
		vdup.8  	d4,r3
		vdup.8  	d5,r4
		vdup.8  	d6,r5
		mov       r3,#00
LOOP_16_BGR:
		vld4.8  	{d0-d3},[r1]!
		vld4.8  	{d7-d10},[r1]!

		vmull.u8  q10,d0,d6
		vmlal.u8  q10,d1,d5
		vmlal.u8  q10,d2,d4
		vshr.u16 	q11,q10,#9
		vmov.u16   r5,d22[0]
		and        r6,r5,#0x0000000f
		vmov.u16   r5,d22[1]
		orr        r6,r6,r5,lsl #4
		vmov.u16   r5,d22[2]
		orr      	 r6,r6,r5,lsl #8
		vmov.u16   r5,d22[3]
		orr        r6,r6,r5,lsl #12
		vmov.u16   r5,d23[0]
		orr        r6,r6,r5,lsl #16
		vmov.u16   r5,d23[1]
		orr        r6,r6,r5,lsl #20
		vmov.u16   r5,d23[2]
		orr      	 r6,r6,r5,lsl #24
		vmov.u16   r5,d23[3]
		orr        r6,r6,r5,lsl #28
		str        r6,[r0],#4

		vmull.u8  q10,d7,d6
		vmlal.u8  q10,d8,d5
		vmlal.u8  q10,d9,d4
		vshr.u16 	q11,q10,#9
		vmov.u16   r5,d22[0]
		and        r6,r5,#0x0000000f
		vmov.u16   r5,d22[1]
		orr        r6,r6,r5,lsl #4
		vmov.u16   r5,d22[2]
		orr      	 r6,r6,r5,lsl #8
		vmov.u16   r5,d22[3]
		orr        r6,r6,r5,lsl #12
		vmov.u16   r5,d23[0]
		orr        r6,r6,r5,lsl #16
		vmov.u16   r5,d23[1]
		orr        r6,r6,r5,lsl #20
		vmov.u16   r5,d23[2]
		orr      	 r6,r6,r5,lsl #24
		vmov.u16   r5,d23[3]
		orr        r6,r6,r5,lsl #28
		str        r6,[r0],#4


		add       r9,r9,#16
		add       r3,r3,#16
		cmp       r3,r7
	  bne       ADD_TO_LOOP_16_BGR
	  mov       r3,#00
	  add       r11,r11,r8,lsr #1
		mov       r0,r11
ADD_TO_LOOP_16_BGR:
		cmp       r9,r2
		blo       LOOP_16_BGR
		pop       {r4,r5,r6,r7,r8,r9,r10,r11,pc}





















































.text
.global neon_rgb888_to_gray256ARM_16
neon_rgb888_to_gray256ARM_16:

	@ r0 = dest
	@ r1 = src
	@ r2 = h
	@ r3 = w

		mov			  r12,r13
		push      {r4,r5,r6,r7,r8,r9,r10,r11,lr}

		ldmfd			r12,{r7}       @r7 = vir_width
		mov       r8,r3
		mul     	r2,r2,r7

		@mul     	r2,r2,r3
		@lsr    	 	r2,r2,#5

		mov     	r3,#77
		mov     	r4,#151
		mov     	r5,#28
		vdup.8  	d4,r3
		vdup.8  	d5,r4
		vdup.8  	d6,r5

		mov       r11,r0
		mov       r3,#00
		mov       r9,#0x00
LOOP256:
		vld4.8  	{d0-d3},[r1]!
		vld4.8  	{d7-d10},[r1]!

		vmull.u8  q10,d0,d4
		vmlal.u8  q10,d1,d5
		vmlal.u8  q10,d2,d6
		vshrn.u16 d19,q10,#8
		vst1.8    {d19},[r0]!

		vmull.u8  q10,d7,d4
		vmlal.u8  q10,d8,d5
		vmlal.u8  q10,d9,d6
		vshrn.u16 d19,q10,#8
		vst1.8    {d19},[r0]!






		add       r9,r9,#16
		add       r3,r3,#16
		cmp       r3,r7
	  bne       ADD_TO_256LOOP
	  mov       r3,#00
	  add       r11,r11,r8	@,lsr #1
		mov       r0,r11
ADD_TO_256LOOP:
		cmp       r9,r2
		blo       LOOP256
		pop       {r4,r5,r6,r7,r8,r9,r10,r11,pc}




.text
.global neon_rgb888_to_gray256ARM_32
neon_rgb888_to_gray256ARM_32:

	@ r0 = dest
	@ r1 = src
	@ r2 = h
	@ r3 = w

		mov			  r12,r13
		push      {r4,r5,r6,r7,r8,r9,r10,r11,lr}

		ldmfd			r12,{r7}       @r7 = vir_width
		mov       r8,r3
		mul     	r2,r2,r7

		@mul     	r2,r2,r3
		@lsr    	 	r2,r2,#5

		mov     	r3,#77
		mov     	r4,#151
		mov     	r5,#28
		vdup.8  	d4,r3
		vdup.8  	d5,r4
		vdup.8  	d6,r5

		mov       r11,r0
		mov       r3,#00
		mov       r9,#0x00
LOOP256_32:
		vld4.8  	{d0-d3},[r1]!
		vld4.8  	{d7-d10},[r1]!
		vld4.8  	{d11-d14},[r1]!
		vld4.8  	{d15-d18},[r1]!

		vmull.u8  q10,d0,d4
		vmlal.u8  q10,d1,d5
		vmlal.u8  q10,d2,d6
		vshrn.u16 d19,q10,#8
		vst1.8    {d19},[r0]!

		vmull.u8  q10,d7,d4
		vmlal.u8  q10,d8,d5
		vmlal.u8  q10,d9,d6
		vshrn.u16 d19,q10,#8
		vst1.8    {d19},[r0]!

		vmull.u8  q10,d11,d4
		vmlal.u8  q10,d12,d5
		vmlal.u8  q10,d13,d6
		vshrn.u16 d19,q10,#8
		vst1.8    {d19},[r0]!

		vmull.u8  q10,d15,d4
		vmlal.u8  q10,d16,d5
		vmlal.u8  q10,d17,d6
		vshrn.u16 d19,q10,#8
		vst1.8    {d19},[r0]!




		add       r9,r9,#32
		add       r3,r3,#32
		cmp       r3,r7
	  bne       ADD_TO_256LOOP_32
	  mov       r3,#00
	  add       r11,r11,r8	@,lsr #1
		mov       r0,r11
ADD_TO_256LOOP_32:
		cmp       r9,r2
		blo       LOOP256_32
		pop       {r4,r5,r6,r7,r8,r9,r10,r11,pc}









.text
.global neon_gray16_to_gray2ARM
neon_gray16_to_gray2ARM:

	@ r0 = dest
	@ r1 = w
	@ r2 = h
		push      {r4,r5,r6,lr}
		mul     	r2,r1,r2
		lsr    	 	r2,r2,#3
LOOP2gray:

	  ldr        r4,[r0]
	  mov        r6,r4
	  and        r5,r4,#0x0f
	  cmp        r5,#0x0f
	  andne      r6,r6,#0xfffffff0

	  and        r5,r4,#0xf0
	  cmp        r5,#0xf0
	  andne      r6,r6,#0xffffff0f

	  and        r5,r4,#0x0f00
	  cmp        r5,#0x0f00
	  andne      r6,r6,#0xfffff0ff

	  and        r5,r4,#0xf000
	  cmp        r5,#0xf000
	  andne      r6,r6,#0xffff0fff

		and        r5,r4,#0xf0000
	  cmp        r5,#0xf0000
	  andne      r6,r6,#0xfff0ffff

	  and        r5,r4,#0xf00000
	  cmp        r5,#0xf00000
	  andne      r6,r6,#0xff0fffff

		and        r5,r4,#0xf000000
	  cmp        r5,#0xf000000
	  andne      r6,r6,#0xf0ffffff

	  and        r5,r4,#0xf0000000
	  cmp        r5,#0xf0000000
	  andne      r6,r6,#0x0fffffff

		str        r6,[r0],#4

		subs      r2,r2,#1
		bne       LOOP2gray
		pop       {r4,r5,r6,pc}



.text
.global neon_rgb256_to_gray16DITHER




neon_rgb256_to_gray16DITHER:

	@ r0 = src
	@ r1 = dst
	@ r2 = res0
	@ r3 = res1

		mov			  r12,r13
		@push      {r4,r5,r6,r7,r8,r9,r10,r11,lr}
		STMFD		r13!,{r4-r11,r14}
		ldmfd			r12,{r4}       @r4 = w
		mov       r5,#0x00
		mov       r14,#0x00
LOOP_DITHER:
		ldr       r9,[r0],#4
		mov       r12,#0x00
		and       r10,r9,#0xff
		ldrh      r11,[r2]
		add       r10,r10,r11
		add       r10,r10,r5

		cmp       r10,#0xff
		movhs     r10,#0xff

		and       r11,r10,#0xf0
		mov       r12,r11,lsr #4
		sub       r10,r10,r11   @ e

		mov       r11,#0x07
		mul       r5,r10,r11
		mov       r5,r5,lsr #4



		ldrh      r11,[r3]
		mov       r6,#0x05
		mul       r6,r10,r6
		mov       r6,r6,lsr #4
		add       r11,r11,r6
		strh      r11,[r3]



		ldrh      r11,[r3,#2]
		mov       r6,#0x01
		mul       r6,r10,r6
		mov       r6,r6,lsr #4
		add       r11,r11,r6
		strh      r11,[r3,#2]



		cmp       r14,#0x00
		beq       PIX2
		ldrh      r11,[r3,#-2]
		mov       r6,#0x03
		mul       r6,r10,r6
		mov       r6,r6,lsr #4
		add       r11,r11,r6
		strh      r11,[r3,#-2]
PIX2:
		and       r10,r9,#0x0000ff00
		mov       r10,r10,lsr #8
		ldrh      r11,[r2,#2]
		add       r10,r10,r11
		add       r10,r10,r5
		mov       r11,#00
		str       r11,[r2],#4

		cmp       r10,#0xff
		movhs     r10,#0xff

		and       r11,r10,#0xf0
		orr       r12,r12,r11
		sub       r10,r10,r11  @ e

		mov       r11,#0x07
		mul       r5,r10,r11
		mov       r5,r5,lsr #4


		ldrh      r11,[r3,#2]
		mov       r6,#0x05
		mul       r6,r10,r6
		mov       r6,r6,lsr #4
		add       r11,r11,r6
		strh      r11,[r3,#2]


		ldrh      r11,[r3,#4]
		mov       r6,#0x01
		mul       r6,r10,r6
		mov       r6,r6,lsr #4
		add       r11,r11,r6
		strh      r11,[r3,#4]


		ldrh      r11,[r3]
		mov       r6,#0x03
		mul       r6,r10,r6
		mov       r6,r6,lsr #4
		add       r11,r11,r6
		strh      r11,[r3]

PIX3:

		and       r10,r9,#0x00ff0000
		mov       r10,r10,lsr #16
		ldrh      r11,[r2]
		add       r10,r10,r11
		add       r10,r10,r5

		cmp       r10,#0xff
		movhs     r10,#0xff

		and       r11,r10,#0xf0
		orr       r12,r12,r11,lsl #4
		sub       r10,r10,r11  @ e

		mov       r11,#0x07
		mul       r5,r10,r11
		mov       r5,r5,lsr #4


		ldrh      r11,[r3,#4]
		mov       r6,#0x05
		mul       r6,r10,r6
		mov       r6,r6,lsr #4
		add       r11,r11,r6
		strh      r11,[r3,#4]

		ldrh      r11,[r3,#6]
		mov       r6,#0x01
		mul       r6,r10,r6
		mov       r6,r6,lsr #4
		add       r11,r11,r6
		strh      r11,[r3,#6]


		ldrh      r11,[r3,#2]
		mov       r6,#0x03
		mul       r6,r10,r6
		mov       r6,r6,lsr #4
		add       r11,r11,r6
		strh      r11,[r3,#2]

PIX4:

		and       r10,r9,#0xff000000
		mov       r10,r10,lsr #24
		ldrh      r11,[r2,#2]
		add       r10,r10,r11
		add       r10,r10,r5
		mov       r11,#00
		str       r11,[r2],#4
		cmp       r10,#0xff
		movhs     r10,#0xff

		and       r11,r10,#0xf0
		orr       r12,r12,r11,lsl #8
		sub       r10,r10,r11  @ e

		mov       r11,#0x07
		mul       r5,r10,r11
		mov       r5,r5,lsr #4


		ldrh      r11,[r3,#4]
		mov       r6,#0x03
		mul       r6,r10,r6
		mov       r6,r6,lsr #4
		add       r11,r11,r6
		strh      r11,[r3,#4]

		ldrh      r11,[r3,#6]
		mov       r6,#0x05
		mul       r6,r10,r6
		mov       r6,r6,lsr #4
		add       r11,r11,r6
		strh      r11,[r3,#6]

	  sub       r11,r4,#4
	  cmp       r14,r11
		beq       PIXEND

		ldrh      r11,[r3,#8]
		mov       r6,#0x01
		mul       r6,r10,r6
		mov       r6,r6,lsr #4
		add       r11,r11,r6
		strh      r11,[r3,#8]

PIXEND:
		strh      r12,[r1],#2
		add       r14,r14,#0x04
		add       r3,r3,#4
		cmp       r14,r4
		blo       LOOP_DITHER
		@pop       {r4,r5,r6,r7,r8,r9,r10,r11,pc}
		LDMFD		r13!, {r4, r5, r6, r7, r8, r9, r10, r11, pc}













.text
.global neon_gray256_to_gray16ARM_16

neon_gray256_to_gray16ARM_16:

	@ r0 = dest
	@ r1 = src
	@ r2 = h
	@ r3 = w

		mov			  r12,r13
		STMFD		r13!,{r4-r11,r14}

		ldmfd			r12,{r7}       @r7 = vir_width

		mov       r8,r3
	  mul     	r2,r2,r7
		mov       r9,#0x00
		mov       r11,r0
		mov       r3,#00
		mov       r10,#0xf0
		vdup.8  	d2,r10
LOOP_256_16:
		mov       r6,#0x00000000
		vld2.8  	{d0,d1},[r1]!


		vand      d0,d0,d2
		vand      d1,d1,d2
		vshr.u8   d0,d0,#4
		vorr      d1,d1,d0
		vst1.8    {d1},[r0]!



		add       r9,r9,#16
    add       r3,r3,#16
		cmp       r3,r7
	  bne       ADD_TO_LOOP_256_16


	  mov       r3,#00
	  add       r11,r11,r8,lsr #1
		mov       r0,r11
ADD_TO_LOOP_256_16:
		cmp       r9,r2
		blo       LOOP_256_16
		LDMFD		r13!, {r4, r5, r6, r7, r8, r9, r10, r11, pc}


.text
.global neon_gray256_to_gray16ARM_32

neon_gray256_to_gray16ARM_32:

	@ r0 = dest
	@ r1 = src
	@ r2 = h
	@ r3 = w

		mov			  r12,r13
		STMFD		r13!,{r4-r11,r14}

		ldmfd			r12,{r7}       @r7 = vir_width

		mov       r8,r3
	  mul     	r2,r2,r7
		mov       r9,#0x00
		mov       r11,r0
		mov       r3,#00
		mov       r10,#0xf0
		vdup.8  	d2,r10
LOOP_256_32:
		mov       r6,#0x00000000
		vld2.8  	{d0,d1},[r1]!
		vld2.8  	{d3,d4},[r1]!

		vand      d0,d0,d2
		vand      d1,d1,d2
		vand      d3,d3,d2
		vand      d4,d4,d2

		vshr.u8   d0,d0,#4
		vorr      d1,d1,d0
		vst1.8    {d1},[r0]!

		vshr.u8   d3,d3,#4
		vorr      d4,d4,d3
		vst1.8    {d4},[r0]!


		add       r9,r9,#32
    add       r3,r3,#32
		cmp       r3,r7
	  bne       ADD_TO_LOOP_256_32


	  mov       r3,#00
	  add       r11,r11,r8,lsr #1
		mov       r0,r11
ADD_TO_LOOP_256_32:
		cmp       r9,r2
		blo       LOOP_256_32
		LDMFD		r13!, {r4, r5, r6, r7, r8, r9, r10, r11, pc}





	.text
.global neon_gray256_to_gray256

neon_gray256_to_gray256:

	@ r0 = dest
	@ r1 = src
	@ r2 = h
	@ r3 = w

		STMFD		r13!,{r4-r11,r14}
		mul     r2,r2,r3
LOOP_256_TO_256:
		vld4.8  	{d0-d3},[r1]!
		vst4.8   {d0-d3},[r0]!
		add     	r9,r9,#32
		cmp       r9,r2
		blo       LOOP_256_TO_256
		LDMFD		r13!, {r4, r5, r6, r7, r8, r9, r10, r11, pc}