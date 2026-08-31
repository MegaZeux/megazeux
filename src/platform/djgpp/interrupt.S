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

/* See audio_struct.h, audio_sfx.h */
#define SFX_DIVIDEND_DX	$0x12		/* (PIT_BASE_CLOCK / 12) >> 16 */
#define SFX_DIVIDEND_AX	$0x34de		/* (PIT_BASE_CLOCK / 12) & 0xffff */
#define SFX_QUEUE_MASK	$4095		/* AUDIO_SFX_QUEUE_SIZE - 1 */
#define SFX_HEAD	16384		/* offsetof(struct audio_sfx_data, head_index) */
#define SFX_TAIL	16388		/* offsetof(struct audio_sfx_data, tail_index) */
#define SFX_CANCEL	16392		/* offsetof(struct audio_sfx_data, cancel_current_note) */
#define SFX_FREQ_REST	$1		/* Frequency value for nosound */

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

.global _sfx_ptr			/* struct audio_sfx_data *sfx_ptr; */
_sfx_ptr:				/* Pointer to audio.sfx */
	.long	0

.global _sfx_on_ptr			/* boolean *sfx_on_ptr; */
_sfx_on_ptr:				/* Pointer to audio.pcs_on */
	.long	0

.global _sfx_timer			/* uint16_t sfx_timer; */
_sfx_timer:				/* Countdown timer for current note */
	.word	0

.global _sfx_playing			/* uint8_t sfx_playing; */
_sfx_playing:				/* 1 if there is an active sound */
	.byte	0

.balign 16
.global _timer_handler
_timer_handler:
	pushl	%eax
	pushl	%ds
	movw	%cs:_int_ds, %ds
_timer_handler_tick:
	addl	TIMER_MS, _timer_ticks	/* timer_ticks += 2ms; */
	/* PC speaker sound effects
	 * This mirrors parts of audio_sfx.c, see that file for more info.
	 * Note the original implementation decremented sfx_timer from 1 to 0
	 * without updating; this is compensated for in sfx.c by adding 1 to
	 * every duration submitted to the sound system (see extra_duration).
	 */
	movl	_sfx_ptr, %eax
	testl	%eax, %eax		/* if(sfx_ptr == NULL) */
	je	_timer_handler_prev	/*   goto timer_handler_prev; */
	pushl	%ecx
	pushl	%edx
	movw	_sfx_timer, %dx
	decw	%dx			/* if((--sfx_timer) <= 0) */
	jle	4f			/*   goto pcs_update; */
	movw	%dx, _sfx_timer
	cmpb	$0, SFX_CANCEL(%eax)	/* if(sfx.cancel_current_note) */
	jnz	3f			/*   goto nosound; */
	jmp	2f			/* goto pcs_end */
4:
	movb	$0, SFX_CANCEL(%eax)	/* sfx.cancel_current_note = false; */
	movl	SFX_HEAD(%eax), %edx
	cmpl	SFX_TAIL(%eax), %edx	/* if(sfx.head_index == sfx.tail_index) */
	jz	3f			/*   goto nosound; */
	movl	(%eax, %edx, 4), %ecx	/* note = audio.sfx.queue[head]; */
	incl	%edx			/* head = (head + 1) & (AUDIO_SFX_QUEUE_SIZE - 1); */
	andl	SFX_QUEUE_MASK, %edx
	movl	%edx, SFX_HEAD(%eax)	/* sfx.head_index = head; */
	movw	%cx, %dx		/* dx = note.duration; */
	shrl	$16, %ecx		/* cx = note.freq; */
	movw	%dx, _sfx_timer		/* sfx_timer = note.duration; */
	cmpl	SFX_FREQ_REST, %ecx	/* if(note.freq <= AUDIO_SFX_FREQ_REST) */
	jle	3f			/*   goto nosound; */
	movl	_sfx_on_ptr, %eax
	cmpb	$0, (%eax)		/* if(audio.pcs_on == 0) */
	je	3f			/*   goto nosound; */
	/* sound(freq) */
	movb	$1, _sfx_playing	/* sfx_playing = true; */
	movb	$0xb6, %al		/* Channel 2, lo + hi, mode 2, binary */
	outb	%al, $0x43		/* PIT Mode/Command register */
	movw	SFX_DIVIDEND_DX, %dx
	movw	SFX_DIVIDEND_AX, %ax
	divw	%cx			/* ax = SFX_DIVIDEND / note.freq */
	outb	%al, $0x42		/* PIT Channel 2: write divider lo */
	movb	%ah, %al
	outb	%al, $0x42		/* PIT Channel 2: write divider hi */
	inb	$0x61, %al		/* I/O register: */
	orb	$0x03, %al		/*   enable PC speaker output */
	outb	%al, $0x61
	jmp	2f
3:
	/* nosound() */
	cmpb	$0, _sfx_playing	/* if(!sfx_playing) */
	je	2f			/*   goto pcs_end; avoid unnecessary port IO */
	movb	$0, _sfx_playing	/* sfx_playing = false; */
	inb	$0x61, %al		/* I/O register: */
	andb	$0xfc, %al		/*  disable PC speaker output */
	outb	%al, $0x61
2:
	popl	%edx
	popl	%ecx

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
	js	1f			/* if(timer_pos < 0) goto 1; */
	movl	%eax, _timer_pos
	jmp	_timer_handler_prev	/* goto timer_handler_prev; */
1:
	addl	TIMER_DIVIDER, %eax	/* timer_pos += TIMER_DIVIDER; */
	movl	%eax, _timer_pos
	jmp	_timer_handler_tick	/* goto timer_handler_tick; */


/* LPT audio interrupts
 *
 * TODO: the latching characteristics of Stereo-on-1s are very strange and vary
 * between designs. No emulator supports the details of them to an adequate
 * degree, so Stereo-on-1 support really needs hardware testing at some point,
 * particularly with Triton's '7528-based design.
 *
 * Notably, DOSBox latches when the control port strobe pins switch high, as
 * how designs based on the '374 are supposed to have worked (supported here).
 * Some other emulator(s) latch on the data port write only, which doesn't
 * match any real world design (resulting in flipped channels here).
 */
.balign 16
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

.global _lpt_left_port			/* uint16_t lpt_left_port; */
_lpt_left_port:
	.word	0

.global _lpt_right_port			/* uint16_t lpt_right_port; */
_lpt_right_port:
	.word	0

.balign 16
.global _lpt_mono_handler
_lpt_mono_handler:
	pushl	%eax
	pushl	%ds
	pushl	%esi
	pushl	%edx
	movw	%cs:_int_ds, %ds
	movl	_dos_audio_buffer_pos, %esi
	movw	_lpt_left_port, %dx
	movb	(%esi), %al		/* al = *pos++; */
	incl	%esi
	outb	%al, %dx		/* Write sample */
	cmpl	_dos_audio_buffer_end, %esi
	jae	2f			/* if(pos >= end) goto 2; */
1:
	movl	%esi, _dos_audio_buffer_pos
	popl	%edx
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
	pushl	%edx
	movw	%cs:_int_ds, %ds
	movl	_dos_audio_buffer_pos, %esi
	movw	_lpt_left_port, %dx	/* Data port */
	movw	(%esi), %ax		/* ax = *((uint16_t *)pos)++; */
	addl	$2, %esi
	outb	%al, %dx		/* Write left sample */
	addw	$2, %dx			/* Control port */
	movb	$0x01, %al		/* Latch left: AutoLF on (low), Strobe off (high) */
	outb	%al, %dx
	subw	$2, %dx			/* Data port */
	movb	%ah, %al
	outb	%al, %dx		/* Write right sample */
	addw	$2, %dx			/* Control port */
	movb	$0x02, %al		/* Latch right: AutoLF off (high), Strobe on (low) */
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
	pushl	%edx
	movw	%cs:_int_ds, %ds
	movl	_dos_audio_buffer_pos, %esi
	movw	_lpt_left_port, %dx
	movw	(%esi), %ax		/* ax = *((uint16_t *)pos)++; */
	addl	$2, %esi
	outb	%al, %dx		/* Write left sample */
	movw	_lpt_right_port, %dx
	movb	%ah, %al
	outb	%al, %dx		/* Write right sample */
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
	pushl	%ecx
	pushl	%edx
	movw	%cs:_int_ds, %ds
	movb	$16, %cl
	movl	_dos_audio_buffer_pos, %esi
	movl	_dos_audio_buffer_end, %edi
	mov	_lpt_left_port, %dx
	inc	%dx			/* Status port */
1:
	in	%dx, %al		/* if(ACK) goto exit; */
	test	$0x40, %al
	jnz	3f
	dec	%dx			/* Data port */
	movb	(%esi), %al		/* al = *pos++; */
	incl	%esi
	outb	%al, %dx		/* Write sample */
	cmpl	%edi, %esi
	jae	4f			/* if(pos >= end) goto 4; */
2:
	/* Note: not much explanation of this is given in the DSS manual. */
	add	$2, %dx			/* Control port */
	movb	$0x0c, %al		/* DSS: Strobe */
	outb	%al, %dx
	pushl	%eax
	popl	%eax
	movb	$0x04, %al		/* DSS: Power up */
	outb	%al, %dx
	pushl	%eax
	popl	%eax
	pushl	%eax
	popl	%eax
	dec	%dx			/* Status port */
	decb	%cl			/* if((--cl) > 0) */
	jnz	1b			/*    goto loop; */
3:
	movl	%esi, _dos_audio_buffer_pos
	popl	%edx
	popl	%ecx
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
