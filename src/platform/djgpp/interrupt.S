/* MegaZeux
 *
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
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

	.global _int_lock_start
_int_lock_start:

/* Common variables */
	.balign 4
	.global _int_ds
_int_ds:		.word	0

/* Timer interrupt */
	.balign 8
	.global _timer_old_handler
_timer_old_handler:	.long	0, 0
	.global _timer_ticks
_timer_ticks:		.long	0
	.global _timer_offset
_timer_offset:		.long	0
	.global _timer_length
_timer_length:		.long	0
	.global _timer_count
_timer_count:		.long	0
	.global _timer_normal
_timer_normal:		.long	0

	.balign 16
	.global _timer_handler
_timer_handler:
	pushl	%eax
	pushl	%ds
	movw	%cs:_int_ds, %ds
	movl	_timer_length, %eax
	addl	%eax, _timer_ticks	/* timer_ticks += timer_length; */
	movl	_timer_offset, %eax
	addl	_timer_count, %eax	/* timer_offset += timer_count; */
	cmpl	_timer_normal, %eax	/* if(timer_count >= timer_normal) */
	jae	1f			/*   goto chain_time; */
	movl	%eax, _timer_offset
	movb	$0x20, %al
	outb	%al, $0x20		/* Send EOI to 8259 PIC */
	popl	%ds
	popl	%eax
	sti
	iret
1:
	subl	_timer_normal, %eax	/* timer_offset -= timer_normal */
	movl	%eax, _timer_offset
	popl	%ds
	popl	%eax
	ljmp	*%cs:_timer_old_handler

/* Keyboard interrupt */
	.balign 8
	.global _kbd_old_handler
_kbd_old_handler:	.long 0, 0
	.global _kbd_buffer
_kbd_buffer:		.fill 256, 1, 0
	.global _kbd_read
_kbd_read:		.byte 0
	.global _kbd_write
_kbd_write:		.byte 0

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
	.global _mouse_regs
_mouse_regs:		.skip 68	/* _go32_dpmi_registers mouse_regs; */
	.global _mouse_buffer
_mouse_buffer:		.fill 256, 8, 0	/* struct mvt mouse_buffer[256] */
	.global _mouse_read
_mouse_read:		.byte 0		/* Uint8 mouse_read */
	.global _mouse_write
_mouse_write:		.byte 0		/* Uint8 mouse_write */

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
