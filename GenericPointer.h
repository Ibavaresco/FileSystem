//==============================================================================
// Copyright (c) 2005-2010, Isaac Marino Bavaresco
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Neither the name of the author nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//==============================================================================
#ifndef		__GENERICPOINTER_H__
#define		__GENERICPOINTER_H__
//==============================================================================
#define	RAM						far ram
#define	ROM						const far rom
#define	GENERIC					far rom

/** Returns the RAM pointer 'p' converted to a generic pointer to void pointing to the same address.*/
#define MAKE_GEN_PTR_RAM(p)		((void GENERIC*)((unsigned short long)(p)|0x800000))
/** Returns the RAM pointer 'p' converted to a generic pointer to type 't' pointing to the same address.*/
#define MAKE_GEN_PTR_RAM_T(p,t)	((t GENERIC*)((unsigned short long)(p)|0x800000))
/** Returns the ROM pointer 'p' converted to a generic pointer to void pointing to the same address.
NOTE: not necessary, supplied just for completeness. Will waste program words if used.*/
#define MAKE_GEN_PTR_ROM(p)		((void GENERIC*)((unsigned short long)(p)&0x7fffff))
/** Returns the ROM pointer 'p' converted to a generic pointer to type 't' pointing to the same address.
NOTE: not necessary, supplied just for completeness. Will waste program words if used.*/
#define MAKE_GEN_PTR_ROM_T(p,t)	((t GENERIC*)((unsigned short long)(p)&0x7fffff))

/** Reads one char from generic pointer 'p'*/
#define READ_GEN_PTR(p)			(((unsigned char RAM*)&(p))[2]&0x80?*(char RAM*)(p):*(char ROM*)(p))
/** Reads one elemet of type 't' from generic pointer 'p'*/
#define READ_GEN_PTR_T(p,t)		(((unsigned char RAM*)&(p))[2]&0x80?*(t RAM*)(p):*(t ROM*)(p))

/** Writes one char to the address pointed to by generic pointer 'p'*/
#define WRITE_GEN_PTR(p,c)		{if(((unsigned char RAM*)&(p))[2]&0x80)*(char RAM*)(p)=(c);}
/** Writes one element of type 't' to the address pointed to by generic pointer 'p'*/
#define WRITE_GEN_PTR_T(p,t,c)	{if(((unsigned char RAM*)&(p))[2]&0x80)*(t RAM*)(p)=(c);}

/** Returns non-zero if generic pointer 'p' points to RAM*/
#define IS_GEN_PTR_TO_RAM(p)	(((unsigned char RAM*)&(p))[2]&0x80)
/** Returns non-zero if generic pointer 'p' points to ROM*/
#define IS_GEN_PTR_TO_ROM(p)	(!(((unsigned char RAM*)&(p))[2]&0x80))
//==============================================================================
#endif	//	__GENERICPOINTER_H__
//==============================================================================
