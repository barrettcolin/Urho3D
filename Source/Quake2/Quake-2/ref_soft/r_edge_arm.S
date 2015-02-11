.struct 0
$surf$next: .long 0
$surf$prev: .long 0
$surf$spans: .long 0
$sz$surf: .byte 0
 
.struct 0
$span$u: .int 0
$span$v: .int 0
$span$count: .int 0
$span$pnext: .long 0
$sz$span: .byte 0

.section .text
.align 2
.arm
.global D_FlatFillSurface2
.func D_FlatFillSurface2
D_FlatFillSurface2:
    ldr r3, [r0, +#$surf$spans] /* r3 = surf->spans */

D_FlatFillSurface_SpanLoop:
    ldr r3, [r3, +#$span$pnext] /* r3 = span->pnext */
    cmp r3, #0
    bne D_FlatFillSurface_SpanLoop

    mov pc, lr
