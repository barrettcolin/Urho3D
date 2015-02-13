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
.extern d_viewbuffer /* not necessary, but for clarity */
.extern r_screenwidth
.global D_FlatFillSurface2
.func D_FlatFillSurface2
D_FlatFillSurface2:
    push {r4, lr}
    ldr r3, [r0, +#$surf$spans] /* r3 = surf->spans */
    ldr ip, =r_screenwidth /* ip = &r_screenwidth */
    ldr ip, [ip] /* ip = r_screenwidth */
    ldr r0, =d_viewbuffer /* r0 = &d_viewbuffer */
    ldr r0, [r0] /* r0 = d_viewbuffer */
D_FlatFillSurface_SpansLoop:
    ldr r4, [r3, +#$span$v] /* r4 = span->v */
    mla r4, r4, ip, r0 /* r4 = span->v * r_screenwidth + d_viewbuffer */
    ldr r2, [r3, +#$span$u] /* r2 = span->u */
    ldr lr, [r3, +#$span$count] /* lr = span->count */
    add lr, r2, lr /* lr = span->u + span->count */
D_FlatFillSurface_SpanLoop:
    cmp lr, r2
    strgtb r1, [r4, r2] /* r1 -> r4 + r2 */
    addgt r2, r2, #1
    bgt D_FlatFillSurface_SpanLoop
    ldr r3, [r3, +#$span$pnext] /* r3 = span->pnext */
    cmp r3, #0
    bne D_FlatFillSurface_SpansLoop
    pop {r4, pc}

