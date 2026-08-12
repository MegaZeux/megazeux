/* MegaZeux
 *
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2026 Alice Rowan <petrifiedrowan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* See interrupt.h */
#define TIMER_MS	$2		/* Number of ticks per timer IRQ */
#define TIMER_DIVIDER	$2386		/* ~500Hz, hardcoded timer rate */

.global _int_lock_start
_int_lock_start:

/* Common variables */
.balign 4
.global _int_ds				/* unsigned short int_ds; */
_int_ds:
	.word	0

/* Timer interrupt */
.balign 8
.global _timer_old_handler		/* __dpmi_paddr timer_old_handler; */
_timer_old_handler:
	.long	0, 0

.global _timer_ticks			/* volatile uint32_t timer_ticks; */
_timer_ticks:
	.long	0

.global _timer_pos			/* volatile int32_t timer_pos; */
_timer_pos:				/* Accumulator for timer (LPT only) */
	.long	0

.global _timer_prev_pos			/* volatile int32_t timer_prev_pos; */
_timer_prev_pos:			/* Accumulator for executing old IRQ0 */
	.long	0

/*.global _timer_prev_div		  uint32_t timer_prev_div; */
_timer_prev_div:			/* Number of clocks for old IRQ0 */
	.long	65536

.global _current_div			/* uint32_t current_div; */
_current_div:				/* Current active divider (<=2386) */
	.long	0

.balign 16
.global _timer_handler
_timer_handler:
	pushl	%eax
	pushl	%ds
	movw	%cs:_int_ds, %ds
_timer_handler_tick:
	/* FIXME: PC speaker */
	addl	TIMER_MS, _timer_ticks	/* timer_ticks += 2ms; */
_timer_handler_prev:
	movl	_timer_prev_pos, %eax
	subl	_current_div, %eax	/* timer_prev_pos -= current_div; */
	js	1f			/* if(timer_prev_pos < 0) goto chain_time; */
	movl	%eax, _timer_prev_pos
	movb	$0x20, %al
	outb	%al, $0x20		/* Send EOI to 8259 PIC */
	popl	%ds
	popl	%eax
	sti
	iret
1:
	addl	_timer_prev_div, %eax	/* timer_prev_pos += timer_prev_div; */
	movl	%eax, _timer_prev_pos
	popl	%ds
	popl	%eax
	ljmp	*%cs:_timer_old_handler

_timer_handler_var:			/* Arriving from LPT interrupts */
	movl	_timer_pos, %eax
	subl	_current_div, %eax	/* timer_pos -= current_div; */
	js	1f			/* if(timer_pos < 0) goto timer_handler_tick; */
	movl	%eax, _timer_pos
	jmp	_timer_handler_prev	/* goto timer_handler_prev; */
1:
	addl	TIMER_DIVIDER, %eax	/* timer_pos += TIMER_DIVIDER; */
	movl	%eax, _timer_pos
	jmp	_timer_handler_tick	/* goto timer_handler_tick; */


/* LPT audio interrupts */
.balign 4
.global _dos_audio_buffer		/* uint8_t *dos_audio_buffer; */
_dos_audio_buffer:
	.long	0

.global _dos_audio_buffer_pos		/* volatile uint8_t *dos_audio_buffer_pos; */
_dos_audio_buffer_pos:
	.long	0

.global _dos_audio_buffer_end		/* uint8_t *dos_audio_buffer_end; */
_dos_audio_buffer_end:
	.long	0

.global _dos_audio_buffer_size		/* uint32_t dos_audio_buffer_size; */
_dos_audio_buffer_size:
	.long	0

.global _dos_audio_buffer_next		/* uint32_t dos_audio_buffer_next; */
_dos_audio_buffer_next:
	.long	0

.global _dos_audio_buffer_frames	/* uint32_t dos_audio_buffer_frames; */
_dos_audio_buffer_frames:
	.long	0

.global _dos_audio_channels		/* uint32_t dos_audio_channels; */
_dos_audio_channels:
	.long	0

.global _lpt_left_port			/* int16_t lpt_left_port; */
_lpt_left_port:
	.word	0

.global _lpt_right_port			/* int16_t lpt_right_port; */
_lpt_right_port:
	.word	0

.balign 16
.global _lpt_mono_handler
_lpt_mono_handler:
	pushl	%eax
	pushl	%ds
	pushl	%esi
	push	%dx
	pushf
	movw	%cs:_int_ds, %ds
	movl	_dos_audio_buffer_pos, %esi
	mov	_lpt_left_port, %dx
	cld
	lodsb				/* al = *pos++; */
	outb	%al, %dx
	cmpl	_dos_audio_buffer_end, %esi
	jae	2f			/* if(pos >= end) goto 2; */
1:
	movl	%esi, _dos_audio_buffer_pos
	popf
	pop	%dx
	popl	%esi
	jmp	_timer_handler_var	/* goto timer_handler_var; */
2:					/* (early predictors assume no jump) */
	movl	_dos_audio_buffer, %esi	/* pos = dos_audio_buffer; */
	jmp	1b

.balign 16
.global _lpt_stereo1_handler
_lpt_stereo1_handler:
	pushl	%eax
	pushl	%ds
	pushl	%esi
	push	%dx
	pushf
	movw	%cs:_int_ds, %ds
	movl	_dos_audio_buffer_pos, %esi
	mov	_lpt_left_port, %dx
	cld
	lodsw				/* ax = *((uint16_t *)pos)++; */
	outb	%al, %dx		/* write left byte */
	add	$2, %dx			/* Init off (high), AutoLF on (low), Strobe off (high) */
	movb	$0x05, %al
	outb	%al, %dx
	sub	$2, %dx			/* write right byte */
	shr	$8, %ax
	outb	%al, %dx
	add	$2, %dx			/* Init off (high), AutoLF off (high), Strobe on (low) */
	movb	$0x06, %al
	outb	%al, %dx
	cmpl	_dos_audio_buffer_end, %esi
	jae	2b			/* if(pos >= end) goto 2; */
	jmp	1b

.balign 16
.global _lpt_stereo2_handler
_lpt_stereo2_handler:
	pushl	%eax
	pushl	%ds
	pushl	%esi
	push	%dx
	pushf
	movw	%cs:_int_ds, %ds
	movl	_dos_audio_buffer_pos, %esi
	cld
	lodsw				/* ax = *((uint16_t *)pos)++; */
	mov	_lpt_left_port, %dx	/* write left byte */
	outb	%al, %dx
	mov	_lpt_right_port, %dx	/* write right byte */
	shr	$8, %ax
	outb	%al, %dx
	cmpl	_dos_audio_buffer_end, %esi
	jae	2b			/* if(pos >= end) goto 2; */
	jmp	1b

.balign 16
.global _lpt_dss_handler
_lpt_dss_handler:
	pushl	%eax
	pushl	%ds
	pushl	%esi
	pushl	%edi
	push	%cx
	push	%dx
	pushf
	movw	%cs:_int_ds, %ds
	movb	$16, %cl
	movl	_dos_audio_buffer_pos, %esi
	movl	_dos_audio_buffer_end, %edi
	mov	_lpt_left_port, %dx
	inc	%dx			/* Status port */
	cld
1:
	in	%dx, %al		/* if(ACK) goto exit; */
	test	$0x40, %al
	jnz	3f
	dec	%dx			/* Data port */
	lodsb				/* outb(port, *pos++); */
	outb	%al, %dx
	cmpl	%edi, %esi
	jae	4f			/* if(pos >= end) goto 4; */
2:
	/* Note: not much explanation of this is given in the DSS manual. */
	add	$2, %dx			/* Control port */
	movb	$0x0c, %al		/* DSS: Strobe */
	outb	%al, %dx
	push	%ax
	pop	%ax
	movb	$0x04, %al		/* DSS: Power up */
	outb	%al, %dx
	push	%ax
	pop	%ax
	push	%ax
	pop	%ax
	dec	%dx			/* Status port */
	loop	1b			/* if((--cx) > 0) goto loop; */
3:
	movl	%esi, _dos_audio_buffer_pos
	popf
	pop	%dx
	pop	%cx
	popl	%edi
	popl	%esi
	jmp	_timer_handler_tick	/* goto timer_handler_tick; */
4:					/* (early predictors assume no jump) */
	movl	_dos_audio_buffer, %esi	/* pos = dos_audio_buffer; */
	jmp	2b


/* Keyboard interrupt */
.balign 8
.global _kbd_old_handler
_kbd_old_handler:
	.long 0, 0

.global _kbd_buffer
_kbd_buffer:
	.fill 256, 1, 0

.global _kbd_read
_kbd_read:
	.byte 0

.global _kbd_write
_kbd_write:
	.byte 0

.balign 16
.global _kbd_handler
_kbd_handler:
	pushl	%eax
	pushl	%ebx
	pushl	%ds
	movw	%cs:_int_ds, %ds
	inb	$0x60, %al		/* Get scan code */
	xorl	%ebx, %ebx
	movb	_kbd_write, %bl
	movb	_kbd_read, %bh
	incb	%bl
	cmpb	%bh, %bl
	je	2f			/* Buffer got full somehow */
	decb	%bl
	xorb	%bh, %bh
	movb	%al, _kbd_buffer(%ebx)
	incb	_kbd_write
2:
	popl	%ds
	popl	%ebx
	popl	%eax
	ljmp	*%cs:_kbd_old_handler

/* Mouse callback */
.balign 4
.global _mouse_regs			/* _go32_dpmi_registers mouse_regs; */
_mouse_regs:
	.skip 68

.global _mouse_buffer			/* struct mvt mouse_buffer[256] */
_mouse_buffer:
	.fill 256, 8, 0

.global _mouse_read			/* uint8_t mouse_read */
_mouse_read:
	.byte 0

.global _mouse_write			/* uint8_t mouse_write */
_mouse_write:
	.byte 0

.balign 16
.global _mouse_handler
_mouse_handler:			/* void mouse_handler(__dpmi_regs *reg) */
	pushl	%ebp
	movl	%esp, %ebp		/* Frame pointer shenanigans */
	movzxb	_mouse_write, %ecx
	movb	_mouse_read, %ch
	incb	%cl
	cmpb	%ch, %cl
	je	3f			/* Buffer got full somehow */
	decb	%cl
	xorb	%ch, %ch
	movl	8(%ebp), %eax		/* __dpmi_regs EAX = reg */
	movl	28(%eax), %edx		/* EDX = EAX->x.ax */
	movw	%dx, _mouse_buffer(,%ecx,8)
	movl	16(%eax), %edx		/* EDX = EAX->x.bx */
	movw	%dx, _mouse_buffer+2(,%ecx,8)
	movl	4(%eax), %edx		/* EDX = EAX->x.si */
	movw	%dx, _mouse_buffer+4(,%ecx,8)
	movl	(%eax), %edx		/* EDX = EAX->x.di */
	movw	%dx, _mouse_buffer+6(,%ecx,8)
	incb	_mouse_write
3:
	popl	%ebp
	ret

.global _int_lock_end
_int_lock_end:
	ret
