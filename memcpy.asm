;===============================================================================
;// Copyright (c) 2005-2010, Isaac Marino Bavaresco
;// All rights reserved.
;//
;// Redistribution and use in source and binary forms, with or without
;// modification, are permitted provided that the following conditions are met:
;//     * Redistributions of source code must retain the above copyright
;//       notice, this list of conditions and the following disclaimer.
;//     * Neither the name of the author nor the
;//       names of its contributors may be used to endorse or promote products
;//       derived from this software without specific prior written permission.
;//
;// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY
;// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
;// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
;// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
;// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
;// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
;// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
;// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
;// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
;// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;===============================================================================
#include "P18CXXX.INC"
;===============================================================================
		radix	decimal
;===============================================================================
STRING		CODE
;===============================================================================
n		equ	0
src		equ	2
dst		equ	5

; void *memcpy (void ram *dst, const void GENERIC *src, size_t n);

		global	memcpy

memcpy:		movff	FSR2L,POSTINC1		; *FSR1++	 = FSR2;
		movff	FSR2H,POSTINC1

		movff	FSR1L,FSR2L		; FSR2		 = FSR1
		movff	FSR1H,FSR2H
		
		subfsr	2,9			; FSR2		-= 9;

		movf	[dst+0],w		; PROD = FSR0	 = dst;
		movwf	FSR0L,ACCESS
		movwf	PRODL,ACCESS
		movf	[dst+1],w
		movwf	FSR0H,ACCESS
		movwf	PRODH,ACCESS
;-------------------------------------------------------------------------------
		rlcf	[src+2],w		; if( (unsigned short long)src & 0x800000UL )
		bnc	InROM			;    {
;-------------------------------------------------------------------------------
InRAM:		movsf	[n+0],TBLPTRL		;    TBLPTR	 = n;
		movsf	[n+1],TBLPTRH
		
		movf	[src+0],w		;    FSR2	 = src;
		movsf	[src+1],FSR2H
		movwf	FSR2L,ACCESS

		movlw	0
		bra	InRAMStart		;    while( TBLPTR-- )

InRAMLoop:	movff	POSTINC2,POSTINC0	;       *FSR0++	 = *FSR2++;

InRAMStart:	decf	TBLPTRL,f,ACCESS
		subwfb	TBLPTRH,f,ACCESS
		bc	InRAMLoop

		bra	Epilog			;    }
;-------------------------------------------------------------------------------
						; else
						;    {
InROM:		movsf	[src+0],TBLPTRL		;    TBLPTR = src;
		movsf	[src+1],TBLPTRH
		movsf	[src+2],TBLPTRU

		movlw	0
		bra	InROMStart

InROMLoop:	tblrd	*+
		movff	TABLAT,POSTINC0

InROMStart:	decf	[n+0],f
		subwfb	[n+1],f
		bc	InROMLoop
;-------------------------------------------------------------------------------
Epilog:		subfsr	1,1
		movff	POSTDEC1,FSR2H
		movff	INDF1,FSR2L

		return	0
;===============================================================================
		end
;===============================================================================
