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
; @name memcmp
;The memcmp function compares the string pointed to by s1 to
;the string pointed to by s2.
;Stack usage: 6 bytes. Re-entrant.
;@param s1 pointer to string one
;@param s2 pointer to string two
;@return The strcmp function returns a signed char greater than, equal
;to, or less than zero, accordingly as the string pointed to by s1 is
;greater than, equal to, or less than the string pointed to by s2.''
;
; signed char memcmp( auto const char GENERIC *s1, auto const char GENERIC *s2 );

n		equ	0
s2		equ	2
s1		equ	5

		global	memcmp

; Procedure: Use FSR0 for 's1' and FSR2 for 's2'.

memcmp:		movff	FSR2L,POSTINC1		; *FSR1++	 = FSR2;
		movff	FSR2H,POSTINC1

		movff	FSR1L,FSR2L		; FSR2		 = FSR1
		movff	FSR1H,FSR2H

		subfsr	2,10			; FSR2		-= 7;
;-------------------------------------------------------------------------------
		rlcf	[s1+2],w
		bnc	s1InROM
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
s1InRAM:	movsf	[s1+0],FSR0L
		movsf	[s1+1],FSR0H

		rlcf	[s2+2],w
		bnc	RAM_ROM
;-------------------------------------------------------------------------------
RAM_RAM:	movsf	[n+0],TBLPTRL		;    TBLPTR	= n;
		movsf	[n+1],TBLPTRH

		movf	[s2+0],w		;    FSR2	 = s2;
		movsf	[s2+1],FSR2H
		movwf	FSR2L,ACCESS

RAM_RAMLoop:	movlw	0
		decf	TBLPTRL,f,ACCESS
		subwfb	TBLPTRH,f,ACCESS
		bnc	Epilog

		movf	POSTINC2,W,ACCESS
		subwf	POSTINC0,W,ACCESS
		bz	RAM_RAMLoop

		bra	Epilog
;-------------------------------------------------------------------------------
RAM_ROM:	movsf	[s2+0],TBLPTRL
		movsf	[s2+1],TBLPTRH
		movsf	[s2+2],TBLPTRU

RAM_ROMLoop:	movlw	0
		decf	[n+0],f
		subwfb	[n+1],f
		bnc	Epilog

		tblrd	*+
		movf	TABLAT,W,ACCESS
		subwf	POSTINC0,W,ACCESS
		bz	RAM_ROMLoop

		bra	Epilog
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
s1InROM:	rlcf	[s2+2],w
		bnc	ROM_ROM
;-------------------------------------------------------------------------------
ROM_RAM:	movsf	[s1+0],TBLPTRL
		movsf	[s1+1],TBLPTRH
		movsf	[s1+2],TBLPTRU

		movsf	[s2+0],FSR0L
		movsf	[s2+1],FSR0H
		
ROM_RAMLoop:	movlw	0
		decf	[n+0],f
		subwfb	[n+1],f
		bnc	Epilog

		tblrd	*+
		movf	POSTINC0,W,ACCESS
		subwf	TABLAT,W,ACCESS
		bz	ROM_RAMLoop

		bra	Epilog
;-------------------------------------------------------------------------------
ROM_ROMLoop:	movlw	0

		incf	[s1+0],f
		addwfc	[s1+1],f
		addwfc	[s1+2],f

		incf	[s2+0],f
		addwfc	[s2+1],f
		addwfc	[s2+2],f

ROM_ROM:	movlw	0
		decf	[n+0],f
		subwfb	[n+1],f
		bnc	Epilog

		movsf	[s2+0],TBLPTRL
		movsf	[s2+1],TBLPTRH
		movsf	[s2+2],TBLPTRU
		tblrd	*+
		movf	TABLAT,w,ACCESS

		movsf	[s1+0],TBLPTRL
		movsf	[s1+1],TBLPTRH
		movsf	[s1+2],TBLPTRU
		tblrd	*+
		subwf	TABLAT,w,ACCESS

		bz	ROM_ROMLoop
;-------------------------------------------------------------------------------
Epilog:		subfsr	1,1
		movff	POSTDEC1,FSR2H
		movff	INDF1,FSR2L

		return	0
;===============================================================================
		end
;===============================================================================
