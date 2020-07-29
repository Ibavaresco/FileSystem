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
#ifndef		__SERIALFLASH_H__
#define		__SERIALFLASH_H__
//==============================================================================
#include "Tipos.h"

/*=========================================================================*//**
\brief Inicializa a interface com a mem&oacute;ria SerialFlash.

Chamada apenas uma vez na inicializa&ccedil;&atilde;o pelo sistema operacional.

\return Nada.
*//*==========================================================================*/

BYTE SerialFlashInit		( void );

BYTE SerialFlashLoadPage	( BYTE Buffer, WORD Page );

BYTE SerialFlashProgramPage	( BYTE Buffer, WORD Page );

BYTE SerialFlashReadBuffer	( BYTE Buffer, BYTE *Dst, WORD Src, WORD Len );

BYTE SerialFlashWriteBuffer	( BYTE Buffer, const BYTE GENERIC *Src, WORD Dst, WORD Len );

BYTE SerialFlashFillBuffer	( BYTE Buffer, BYTE Value, WORD Dst, WORD Len );

BYTE SerialFlashPageRead	( WORD Page, BYTE RAM *Dst, WORD Src, WORD Len );

BYTE SerialFlashWriteToPage( BYTE Buffer, WORD Page, const BYTE GENERIC *Src, WORD Dst, WORD Len );

BYTE SerialFlashReadFromPage( BYTE Buffer, WORD Page, BYTE RAM *Dst, WORD Src, WORD Len );

BYTE SerialFlashChipErase( void );

/*=========================================================================*//**
\brief Apaga uma p&aacute;gina da mem&oacute;ria SerialFlash.

\param Page N&uacute;mero da p&aacute;gina a ser apagada.

\return Nada.
*//*==========================================================================*/

BYTE SerialFlashErasePage	( WORD Page );

//==============================================================================
#endif	//	__SERIALFLASH_H__
//==============================================================================
