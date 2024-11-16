.cpu cortex-m0
.thumb
.syntax unified

.global fill_alpha
fill_alpha:
	push {r4,r5,lr}
	movs r4,#0
fillloop:
	movs r5,'A' // load the character 'A' (integer value 65)
	adds r5,r4
	movs r0,r4
	movs r1,r5
	bl   show_char
	adds r4,#1
	movs r0,#7
	ands r4,r0
	ldr  r0,=1000000
	bl   nano_wait
	b    fillloop
	pop {r4,r5,pc} // not actually reached
