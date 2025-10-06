Begin:
	clr
	call ConfigTimer
	;call ConfigKeyboard
	
	std $0D
	ld r1
	
	std IntTable::8
	out p0
	std IntTable::0
	out p1
	ei
	
	pushs
	popb
.lock:
	std $41
	out p3
	jp .lock
	

IntTable:
	dw Keyboard
	dw Mouse
	dw Timer
	dw Unknown
	
define PID_1 0x0001
define PID_2 0x0002
define PID_3 0x0003
define PID_END 0x0204

ProcTable:
	dw Process1,  PID_1, 0, 0
	dw Process2,  PID_2, 0, 0
	dw Process3,  PID_3, 0, 0
	dw 0x0000,    PID_END, 0, 0
	
AddrCall:
	dw 0x0000
	
ProcIndex:
	db $FF
Status:
	db 0

; ISR 0 ------------
Keyboard:
	di
	std 0x02
	out p7
	call WaitACK
	std 0x00
	out p7
	call WaitACK
	in p7
	out p3
	ei
iret
; ------------------

; ISR 1 ------------
Mouse:

iret
; ------------------

; ISR 2 ------------
Timer:
	di
	pushs
	pushb
	pushd
	in p0
	pushd
	in p1
	pushd
	in p4
	pushd
	in p5
	pushd
	push r0
	push r1
	push r2
	push r3
	push r4
	push r5
	push r6
	push r7
	
	; Algoritmo pra implementar:
	;   Se ProcIndex != 0xFF:
	;		Decrementar ProcIndex
	;		Executar ReadProc
	;		Incrementar Ponteiro de ProcessN
	;		Subtrair DR = BP - SP
	;   	Somar DR = DR + 2
	; 		Salvar DR em ponteiro de ProcessN
	std 0x80
	idc
	std ProcIndex::8
	out p0
	std ProcIndex::0
	out p1
	in p2
	ld r2
	
	std $FF
	bt r2
	jz .skip_dynspace
	
	stl r2
	decr
	out p2
	
	pushs
	pop r6
	pop r5
	std 0x50
	ssp
	push r5
	push r6
	call ReadProc
	pops
	
	pushs
	pop r1
	popd
	pushb
	pop r3
	popd
	
	stl r3
	sub r1
	ld r1
	std 2
	add r1
	out p2
	
.skip_dynspace:
	cdr
	ld r3
	ld r2
	
.NextProc:
	pushs
	pop r6
	pop r5
	std 0x50
	ssp
	push r5
	push r6
	call ReadProc
	pops
	
	;   Se status == 1:
	;		Ler ProcessN para STD N, SSP (ao invés de STD 21, fazer stl r6)
	
	; R4 = PID, DR = Status
	; Verificar Status 0, 1 ou 2
	
	cdr
	bt r7
	jz .ProcessBegin	; Se status = 0, Inicia processo
	
	std 0x03
	bt r7
	jz .Next_Proc
	jp .Skip_Next
.Next_Proc:
	std $5B
	idc
	incr			; Incrementa R3
	jp .NextProc
	
.Skip_Next:
	std 0x41
	idc
	cdr
	std .ProcessCall::8	; Aqui retorna std 0
	ld r0
	std .ProcessCall::0
	ld r1
	incr
	push r0
	push r1
	popd
	popd

	std PID_END::0
	bt r4
	jz .StackReset		; Se status = 3, Fim da lista de Processos
	jp .SkipStackReset	; Se status = 1 | 2, Processo em execução
	
.StackReset:
	std ProcIndex::8
	out p0
	std ProcIndex::0
	out p1
	std $0F
	pushd
	std $FF
	pushd
	pops
	out p2
	
	std 19
	ssp
	jp .RestoreContext
	
.SkipStackReset:
	; 21 * N + X | N = R3
	std 21
	mul r3
	add r2
	ssp
	jp .RestoreContext
	
.ProcessBegin:
	decr
	std 0x01	; Altera status para 'exec'
	out p2
	
	pushs
	pop r6
	pop r5
	std 0x50
	ssp
	push r5
	push r6
	call CalcAddr
	pops
	clr
	
.ProcessCall:
	call AddrCall
	
	std 0x80
	idc
	std ProcIndex::8
	out p0
	std ProcIndex::0
	out p1
	in p2
	decr
	out p2
	
	call ReadProc
	
	decr
	std 0x03	; Altera status para 'end'
	out p2
	
	pushs
	pushs
	jp Timer
	
.RestoreContext:
	pop r7
	pop r6
	pop r5
	pop r4
	pop r3
	pop r2
	pop r1
	pop r0
	popd
	out p5
	popd
	out p4
	popd
	out p1
	popd
	out p0
	stl r7
	idc
	popd
	popb
	pops
	ei
iret
; ------------------

; ISR 3 ------------
Unknown:

iret
; ------------------

Process1:
	ei
	pushs
	popb
	std $42
	ld r2
	cdr
	ld r0
	std $31
	ld r1
	push r1
	cdr
	ld r1
	.loop_proc1:
		stl r2
		out p3
		in p3
		bt r0
		jz .loop_proc1
		pop r1
		bt r1
		jz .ret_proc1
		push r1
		jp .loop_proc1
.ret_proc1:
	di
ret

Process2:
	ei
	pushs
	popb
	std $43
	ld r2
	cdr
	ld r0
	std $32
	ld r1
	.loop_proc2:
		stl r2
		out p3
		in p3
		bt r0
		jz .loop_proc2
		bt r1
		jz .ret_proc2
		jp .loop_proc2
.ret_proc2:
	di
ret

Process3:
	ei
	pushs
	popb
	std $44
	ld r2
	cdr
	ld r0
	std $33
	ld r1
	.loop_proc3:
		stl r2
		out p3
		in p3
		bt r0
		jz .loop_proc3
		bt r1
		jz .ret_proc3
		jp .loop_proc3
.ret_proc3:
	di
ret


ConfigTimer:
	std $FA			; ACKnowledge Response
	ld r0
	std 0x01		; Abrir dispositivo no controller
	out p7
	call WaitACK
	std 0x02		; Escolher Dispositivo Timer -> Command: 0x01, Data: 0x02
	out p7
	call WaitACK
	call WriteTimerLimit
ret

WriteTimerLimit:
	std 0x02		; Ler dispositivo no controller
	out p7
	call WaitACK
	std 0x02		; Escolher Dispositivo Timer -> Command: 0x02, Data: 0x02
	out p7
	call WaitACK
	
	; Configurar contador de 32 bits -> Valor: MAX
	; Velocidade mínima = ~125ms (0xFFFFFFFF)
	; Velocidade máxima = ? (0x2000FF)
	std $00
	out p7
	call WaitACK
	std $00
	out p7
	call WaitACK
	std $FF
	out p7
	call WaitACK
	std $FF
	out p7
	call WaitACK
ret

WaitACK:
	in p7
	bt r0
	jz ACK.Done
	jp WaitACK
ACK.Done:
	ret
	
ConfigKeyboard:
	std $FA			; ACKnowledge Response
	ld r0
	std 0x01
	out p7
	call WaitACK
	std 0x00
	out p7
	call WaitACK
ret

; Input  -> DR : Vector Index
; Output -> R0:R1 : Absolute Address
; 		 -> DR : Program Status = 0,1 or 2
;		 -> R4 : Program PID
ReadProc:
	std 0x80
	idc
	std ProcIndex::8
	out p0
	std ProcIndex::0
	out p1
	in p2
	incr
	out p2
	
	pushd
	std ProcTable::8
	ld r0
	std ProcTable::0
	ld r1
	popd
	
	shl 3		; Multiplicar x 8 pra skipar 8 bytes
	add r1
	ld r1
	jc .incr0
	jp .defports
.incr0:
	std 0x40
	idc
	incr
.defports:
	stl r0
	out p0
	stl r1
	out p1
	
	std 0x01
	idc
	in p2
	ld r1
	incr
	in p2
	ld r0
	incr
	in p2
	ld r4
	incr
	in p2
	ld r7
	incr
	in p2
	ld r2
ret

; Input  -> R0:R1 : Absolute Address
;        -> R2:R3 : Jump Address
; Output -> R0:R1 : Jump + Relative Address
CalcAddr:
	pushd
	std AddrCall::8
	ld r2
	out p0
	std AddrCall::0
	ld r3
	out p1
	
	std 2
	add r3
	ld r3
	jc .incr2
	jp .calcrel
.incr2:
	std 0x52
	idc
	incr

.calcrel:
	stl r0
	sub r2
	ld r0
	stl r1
	sub r3
	ld r3
	jc .skipinc
	std 0x40
	idc
	decr
.skipinc:
	std $F0
	or r0
	ld r0
	stl r3
	ld r1
	
	std 0x01
	idc
	stl r0
	out p2
	incr
	stl r1
	out p2
	popd
ret

.End:

