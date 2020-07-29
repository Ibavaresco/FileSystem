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
#define	USE_MUTEX			0
#define	OPTIMIZE_BUSY		0
#define	OPTIMIZE_BUFFERS	0
//==============================================================================
#include <p18f87j60.h>
#include <genericpointer.h>
#include "SerialFlash.h"

#include "FreeRTOS.h"
#include "Task.h"

#if			USE_MUTEX == 1
#include "Mutex.h"
#endif	//	USE_MUTEX == 1
//==============================================================================
#define FLASH_nCS		LATFbits.LATF4
#define FLASH_nCS_DIR	TRISFbits.TRISF4

#define	SCK1			LATCbits.LATC3
#define	SCK1_DIR		TRISCbits.TRISC3

#define	SDI1			LATCbits.LATC4
#define	SDI1_DIR		TRISCbits.TRISC4

#define	SDO1			LATCbits.LATC5
#define	SDO1_DIR		TRISCbits.TRISC5
//==============================================================================
#if			USE_MUTEX == 1
xMutexHandle	xMutexSPI;
#endif	//	USE_MUTEX == 1

#if			OPTIMIZE_BUFFERS == 1
static WORD	PaginaCarregada[2]	= { 0xffff, 0xffff };
#endif	//	OPTIMIZE_BUFFERS == 1

#if			OPTIMIZE_BUSY == 1
static BYTE BusyBuffer			= 0xff;
#endif	//	OPTIMIZE_BUSY == 1
//==============================================================================
BYTE SerialFlashInit( void )
	{
	//--------------------------------------------------------------------------
	FLASH_nCS		= 1;	// CS da FLASH é desabilitado (nivel 1)
	FLASH_nCS_DIR	= 0;	// CS é uma saída

	SCK1			= 0;	// SPI modo 0: estado de repouso para SCK é 0
	SCK1_DIR		= 0;	// SCK é uma saída

	SDI1_DIR		= 1;	// SDI é uma entrada

	SDO1			= 0;	//
	SDO1_DIR		= 0;	// SDO é uma saída

	SSP1STAT		= 0x40;
	SSP1CON1		= 0x20;	// Ativa o módulo MSSP como SPI
	//--------------------------------------------------------------------------
#if			OPTIMIZE_BUFFERS == 1
	PaginaCarregada[0]	= 0xffff;
	PaginaCarregada[1]	= 0xffff;
#endif	//	OPTIMIZE_BUFFERS == 1

#if			OPTIMIZE_BUSY == 1
	BusyBuffer			= 0xff;
#endif	//	OPTIMIZE_BUSY == 1

#if			USE_MUTEX == 1
	if(( xMutexSPI = xMutexCreate() ) == NULL )
		return -1;
#endif	//	USE_MUTEX == 1

	return 0;
	}
//==============================================================================
static void SerialFlashReceive( BYTE RAM *Dados, WORD nBytes )
	{
	for( ; nBytes; nBytes--, Dados++ )
		{
		PIR1bits.SSP1IF	= 0;
		SSP1BUF			= 0;
		do { ClrWdt(); } while( !PIR1bits.SSP1IF );
		*Dados = SSP1BUF;
		}
	}
//==============================================================================
static void SerialFlashTransmit( const BYTE GENERIC *Dados, WORD nBytes )
	{
	for( ; nBytes; nBytes--, Dados++ )
		{
		PIR1bits.SSP1IF	= 0;
		SSP1BUF			= READ_GEN_PTR( Dados );
		do { ClrWdt(); } while( !PIR1bits.SSP1IF );
		}
	}
//==============================================================================
static BYTE SerialFlashIsBusy( void )
	{
	FLASH_nCS		= 0;

	PIR1bits.SSP1IF	= 0;
	SSP1BUF			= 0xd7;
	while( !PIR1bits.SSP1IF )
		ClrWdt();

	PIR1bits.SSP1IF	= 0;
	SSP1BUF			= 0x00;
	while( !PIR1bits.SSP1IF )
		ClrWdt();

	FLASH_nCS		= 1;

	return !( SSP1BUF & 0x80 );
	}
//==============================================================================
static BYTE SerialFlashReadStatus( void )
	{
	FLASH_nCS		= 0;

	PIR1bits.SSP1IF	= 0;
	SSP1BUF			= 0xd7;
	while( !PIR1bits.SSP1IF )
		ClrWdt();

	PIR1bits.SSP1IF	= 0;
	SSP1BUF			= 0x00;
	while( !PIR1bits.SSP1IF )
		ClrWdt();

	FLASH_nCS		= 1;

	return SSP1BUF;
	}
//==============================================================================
static TWORD RevertAddress( TWORD a )
	{
	TWORD Aux;

	((BYTE *)&Aux)[0] = ((BYTE *)&a)[2];
	((BYTE *)&Aux)[1] = ((BYTE *)&a)[1];
	((BYTE *)&Aux)[2] = ((BYTE *)&a)[0];

	return Aux;
	}
//==============================================================================
typedef struct
	{
	BYTE	Cmd;
	TWORD	Addr;
	} TS_Command;
//==============================================================================
BYTE SerialFlashProgramPage( BYTE Buffer, WORD Page )
	{
	TS_Command	Cmd;

	//--------------------------------------------------------------------------
	if( Buffer < 1u || Buffer > 2u )
		return -1;
	//--------------------------------------------------------------------------
	if( Page > 8191u )
		return -1;
	//--------------------------------------------------------------------------

	//--------------------------------------------------------------------------
	// ATENÇÃO: Aqui fazemos buffer ser 0-based
	//--------------------------------------------------------------------------
	Buffer -= 1;
	//--------------------------------------------------------------------------

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexSPI, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	while( SerialFlashIsBusy() )
		//vTaskDelay( 1 );
		vPortYield();

	Cmd.Cmd		= Buffer ? 0x86 : 0x83;		// Program with built-in erase
	//Cmd.Cmd		= Buffer ? 0x89 : 0x88;		// Program without built-in erase
	Cmd.Addr	= RevertAddress( (TWORD)Page << 11 );

	FLASH_nCS	= 0;
	SerialFlashTransmit( MAKE_GEN_PTR_RAM_T( &Cmd, BYTE ), sizeof Cmd );
	FLASH_nCS	= 1;

	while( SerialFlashIsBusy() )
		//vTaskDelay( 1 );
		vPortYield();

	Cmd.Cmd		= Buffer ? 0x61 : 0x60;		// Page to Buffer Compare
	Cmd.Addr	= RevertAddress( (TWORD)Page << 11 );

	FLASH_nCS	= 0;
	SerialFlashTransmit( MAKE_GEN_PTR_RAM_T( &Cmd, BYTE ), sizeof Cmd );
	FLASH_nCS	= 1;

	while( SerialFlashIsBusy() )
		//vTaskDelay( 1 );
		vPortYield();

	if( SerialFlashReadStatus() & 0x40 )
		Cmd.Cmd = -1;
	else
		Cmd.Cmd = 0;

#if			OPTIMIZE_BUSY == 1
	BusyBuffer	= Buffer;
#endif	//	OPTIMIZE_BUSY == 1

#if			OPTIMIZE_BUFFERS == 1
	PaginaCarregada[Buffer] = Page;
#endif	//	OPTIMIZE_BUFFERS == 1

#if			USE_MUTEX == 1
	xMutexGive( xMutexSPI, 0 );
#endif	//	USE_MUTEX == 1

	return Cmd.Cmd;
	}
//==============================================================================
BYTE SerialFlashErasePage( WORD Page )
	{
	TS_Command	Cmd;

	//--------------------------------------------------------------------------
	if( Page > 8191u )
		return -1;
	//--------------------------------------------------------------------------

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexSPI, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	while( SerialFlashIsBusy() )
		//vTaskDelay( 1 );
		vPortYield();

	Cmd.Cmd		= 0x81;
	Cmd.Addr	= RevertAddress( (TWORD)Page << 11 );

	FLASH_nCS	= 0;
	SerialFlashTransmit( MAKE_GEN_PTR_RAM_T( &Cmd, BYTE ), sizeof Cmd );
	FLASH_nCS	= 1;

#if			OPTIMIZE_BUFFERS == 1
	if( PaginaCarregada[0] == Page )
		PaginaCarregada[0] = 0xffff;
	if( PaginaCarregada[1] == Page )
		PaginaCarregada[1] = 0xffff;
#endif	//	OPTIMIZE_BUFFERS == 1

#if			OPTIMIZE_BUSY == 1
	BusyBuffer	= 0xff;
#endif	//	OPTIMIZE_BUSY == 1

#if			USE_MUTEX == 1
	xMutexGive( xMutexSPI, 0 );
#endif	//	USE_MUTEX == 1

	return 0;
	}
//==============================================================================
BYTE SerialFlashLoadPage( BYTE Buffer, WORD Page )
	{
	TS_Command	Cmd;

	//--------------------------------------------------------------------------
	if( Buffer < 1u || Buffer > 2u )
		return -1;
	//--------------------------------------------------------------------------
	if( Page > 8191u )
		return -1;
	//--------------------------------------------------------------------------

	//--------------------------------------------------------------------------
	// ATENÇÃO: Aqui fazemos buffer ser 0-based
	//--------------------------------------------------------------------------
	Buffer -= 1;
	//--------------------------------------------------------------------------

#if			OPTIMIZE_BUFFERS == 1
	if( PaginaCarregada[Buffer] == Page )
		return 0;
#endif	//	OPTIMIZE_BUFFERS == 1

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexSPI, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	while( SerialFlashIsBusy() )
		//vTaskDelay( 1 );
		vPortYield();

	Cmd.Cmd		= Buffer ? 0x55 : 0x53;
	Cmd.Addr	= RevertAddress( (TWORD)Page << 11 );

	FLASH_nCS	= 0;
	SerialFlashTransmit( MAKE_GEN_PTR_RAM_T( &Cmd, BYTE ), sizeof Cmd );
	FLASH_nCS	= 1;

#if			OPTIMIZE_BUFFERS == 1
	PaginaCarregada[Buffer] = Page;
#endif	//	OPTIMIZE_BUFFERS == 1

#if			OPTIMIZE_BUSY == 1
	BusyBuffer	= Buffer;
#endif	//	OPTIMIZE_BUSY == 1

#if			USE_MUTEX == 1
	xMutexGive( xMutexSPI, 0 );
#endif	//	USE_MUTEX == 1

	return 0;
	}
//==============================================================================
BYTE SerialFlashReadBuffer( BYTE Buffer, BYTE *Dst, WORD Src, WORD Len )
	{
	TS_Command	Cmd;

	//--------------------------------------------------------------------------
	if( Buffer < 1u || Buffer > 2u )
		return -1;
	//--------------------------------------------------------------------------
	if( Src > 1055u || Len == 0u || Src + Len > 1056u )
		return -1;
	//--------------------------------------------------------------------------

	//--------------------------------------------------------------------------
	// ATENÇÃO: Aqui fazemos buffer ser 0-based
	//--------------------------------------------------------------------------
	Buffer -= 1;
	//--------------------------------------------------------------------------

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexSPI, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

#if			OPTIMIZE_BUSY == 1
	if( BusyBuffer == Buffer )
#endif	//	OPTIMIZE_BUSY == 1
		while( SerialFlashIsBusy() )
			//vTaskDelay( 1 );
			vPortYield();

	Cmd.Cmd		= Buffer ? 0xd3 : 0xd1;
	Cmd.Addr	= RevertAddress( Src );

	FLASH_nCS	= 0;
	SerialFlashTransmit( MAKE_GEN_PTR_RAM_T( &Cmd, BYTE ), sizeof Cmd );
	SerialFlashReceive( Dst, Len );
	FLASH_nCS	= 1;

#if			USE_MUTEX == 1
	xMutexGive( xMutexSPI, 0 );
#endif	//	USE_MUTEX == 1

	return 0;
	}
//==============================================================================
BYTE SerialFlashWriteBuffer( BYTE Buffer, const BYTE GENERIC *Src, WORD Dst, WORD Len )
	{
	TS_Command	Cmd;

	//--------------------------------------------------------------------------
	if( Buffer < 1u || Buffer > 2u )
		return -1;
	//--------------------------------------------------------------------------
	if( Dst > 1055u || Len == 0u || Dst + Len > 1056u )
		return -1;
	//--------------------------------------------------------------------------

	//--------------------------------------------------------------------------
	// ATENÇÃO: Aqui fazemos buffer ser 0-based
	//--------------------------------------------------------------------------
	Buffer -= 1;
	//--------------------------------------------------------------------------

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexSPI, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

#if			OPTIMIZE_BUSY == 1
	if( BusyBuffer == Buffer )
#endif	//	OPTIMIZE_BUSY == 1
		while( SerialFlashIsBusy() )
			//vTaskDelay( 1 );
			vPortYield();

	Cmd.Cmd		= Buffer ? 0x87 : 0x84;
	Cmd.Addr	= RevertAddress( Dst );

	FLASH_nCS	= 0;
	SerialFlashTransmit( MAKE_GEN_PTR_RAM_T( &Cmd, BYTE ), sizeof Cmd );
	SerialFlashTransmit( Src, Len );
	FLASH_nCS	= 1;

#if			OPTIMIZE_BUFFERS == 1
	PaginaCarregada[Buffer] = 0xffff;
#endif	//	OPTIMIZE_BUFFERS == 1

#if			USE_MUTEX == 1
	xMutexGive( xMutexSPI, 0 );
#endif	//	USE_MUTEX == 1

	return 0;
	}
//==============================================================================
BYTE SerialFlashFillBuffer( BYTE Buffer, BYTE Value, WORD Dst, WORD Len )
	{
	TS_Command	Cmd;

	//--------------------------------------------------------------------------
	if( Buffer < 1u || Buffer > 2u )
		return -1;
	//--------------------------------------------------------------------------
	if( Dst > 1055u || Len == 0u || Dst + Len > 1056u )
		return -1;
	//--------------------------------------------------------------------------

	//--------------------------------------------------------------------------
	// ATENÇÃO: Aqui fazemos buffer ser 0-based
	//--------------------------------------------------------------------------
	Buffer -= 1;
	//--------------------------------------------------------------------------

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexSPI, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

#if			OPTIMIZE_BUSY == 1
	if( BusyBuffer == Buffer )
#endif	//	OPTIMIZE_BUSY == 1
		while( SerialFlashIsBusy() )
			//vTaskDelay( 1 );
			vPortYield();

	Cmd.Cmd		= Buffer ? 0x87 : 0x84;
	Cmd.Addr	= RevertAddress( Dst );

	FLASH_nCS	= 0;
	SerialFlashTransmit( MAKE_GEN_PTR_RAM_T( &Cmd, BYTE ), sizeof Cmd );
	for( ; Len; Len-- )
		{
		PIR1bits.SSP1IF	= 0;
		SSP1BUF			= Value;
		do { ClrWdt(); } while( !PIR1bits.SSP1IF );
		}
	FLASH_nCS	= 1;

#if			OPTIMIZE_BUFFERS == 1
	PaginaCarregada[Buffer] = 0xffff;
#endif	//	OPTIMIZE_BUFFERS == 1

#if			USE_MUTEX == 1
	xMutexGive( xMutexSPI, 0 );
#endif	//	USE_MUTEX == 1

	return 0;
	}
//==============================================================================
BYTE SerialFlashPageRead( WORD Page, BYTE RAM *Dst, WORD Src, WORD Len )
	{
	TS_Command	Cmd;

	//--------------------------------------------------------------------------
	if( Page > 8191u )
		return -1;
	//--------------------------------------------------------------------------
	if( Src > 1055u || Len == 0u || Src + Len > 1056u )
		return -1;
	//--------------------------------------------------------------------------

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexSPI, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	while( SerialFlashIsBusy() )
		//vTaskDelay( 1 );
		vPortYield();

	//--------------------------------------------------------------------------
	// Read buffer
	Cmd.Cmd		= 0x03;
	Cmd.Addr	= RevertAddress( Src | (TWORD)Page << 11 );

	FLASH_nCS	= 0;
	SerialFlashTransmit( MAKE_GEN_PTR_RAM_T( &Cmd, BYTE ), sizeof Cmd );
	SerialFlashReceive( Dst, Len );
	FLASH_nCS	= 1;

	//--------------------------------------------------------------------------
#if			USE_MUTEX == 1
	xMutexGive( xMutexSPI, 0 );
#endif	//	USE_MUTEX == 1

	return 0;
	}
//==============================================================================
/*
BYTE SerialFlashWriteToPage( BYTE Buffer, WORD Page, const BYTE GENERIC *Src, WORD Dst, WORD Len )
	{
	TS_Command	Cmd;

	//--------------------------------------------------------------------------
	if( Buffer < 1 || Buffer > 2 )
		return -1;
	//--------------------------------------------------------------------------
	if( Page > 8191 )
		return -1;
	//--------------------------------------------------------------------------
	if( Dst > 1055 || Len == 0 || Dst + Len > 1056 )
		return -1;
	//--------------------------------------------------------------------------

	//--------------------------------------------------------------------------
	// ATENÇÃO: Aqui fazemos buffer ser 0-based
	//--------------------------------------------------------------------------
	Buffer -= 1;
	//--------------------------------------------------------------------------

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexSPI, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

#if			OPTIMIZE_BUFFERS == 1
	if( PaginaCarregada[Buffer] != Page )
		{
#endif	//	OPTIMIZE_BUFFERS == 1
		while( SerialFlashIsBusy() )
			//vTaskDelay( 1 );
			vPortYield();
	
		//--------------------------------------------------------------------------
		// Load page
		Cmd.Cmd		= Buffer ? 0x55 : 0x53;
		Cmd.Addr	= RevertAddress( (TWORD)Page << 11 );
	
		FLASH_nCS	= 0;
		SerialFlashTransmit( MAKE_GEN_PTR_RAM_T( &Cmd, BYTE ), sizeof Cmd );
		FLASH_nCS	= 1;
		//--------------------------------------------------------------------------
#if			OPTIMIZE_BUSY == 1
		BusyBuffer	= Buffer;
#endif	//	OPTIMIZE_BUSY == 1
		//--------------------------------------------------------------------------
#if			OPTIMIZE_BUFFERS == 1
		}
#endif	//	OPTIMIZE_BUFFERS == 1

#if			OPTIMIZE_BUSY == 1
	if( BusyBuffer == Buffer )
#endif	//	OPTIMIZE_BUSY == 1
		while( SerialFlashIsBusy() )
			//vTaskDelay( 1 );
			vPortYield();

	//--------------------------------------------------------------------------
	// Write buffer
	Cmd.Cmd		= Buffer ? 0x87 : 0x84;
	Cmd.Addr	= RevertAddress( Dst );

	FLASH_nCS	= 0;
	SerialFlashTransmit( MAKE_GEN_PTR_RAM_T( &Cmd, BYTE ), sizeof Cmd );
	SerialFlashTransmit( Src, Len );
	FLASH_nCS	= 1;


	while( SerialFlashIsBusy() )
		//vTaskDelay( 1 );
		vPortYield();

	//--------------------------------------------------------------------------
	// Program page
	Cmd.Cmd		= Buffer ? 0x89 : 0x88;
	Cmd.Addr	= RevertAddress( (TWORD)Page << 11 );

	FLASH_nCS	= 0;
	SerialFlashTransmit( MAKE_GEN_PTR_RAM_T( &Cmd, BYTE ), sizeof Cmd );
	FLASH_nCS	= 1;

#if			OPTIMIZE_BUFFERS == 1
	PaginaCarregada[Buffer] = Page;
#endif	//	OPTIMIZE_BUFFERS == 1
	//--------------------------------------------------------------------------
#if			OPTIMIZE_BUSY == 1
	BusyBuffer	= Buffer;
#endif	//	OPTIMIZE_BUSY == 1
	//--------------------------------------------------------------------------
#if			USE_MUTEX == 1
	xMutexGive( xMutexSPI, 0 );
#endif	//	USE_MUTEX == 1

	return 0;
	}
//==============================================================================
BYTE SerialFlashReadFromPage( BYTE Buffer, WORD Page, BYTE RAM *Dst, WORD Src, WORD Len )
	{
	TS_Command	Cmd;

	//--------------------------------------------------------------------------
	if( Buffer < 1 || Buffer > 2 )
		return -1;
	//--------------------------------------------------------------------------
	if( Page > 8191 )
		return -1;
	//--------------------------------------------------------------------------
	if( Src > 1055 || Len == 0 || Src + Len > 1056 )
		return -1;
	//--------------------------------------------------------------------------

	//--------------------------------------------------------------------------
	// ATENÇÃO: Aqui fazemos buffer ser 0-based
	//--------------------------------------------------------------------------
	Buffer -= 1;
	//--------------------------------------------------------------------------

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexSPI, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

#if			OPTIMIZE_BUFFERS == 1
	if( PaginaCarregada[Buffer] != Page )
		{
#endif	//	OPTIMIZE_BUFFERS == 1
		while( SerialFlashIsBusy() )
			//vTaskDelay( 1 );
			vPortYield();
	
		//--------------------------------------------------------------------------
		// Load page
		Cmd.Cmd		= Buffer ? 0x55 : 0x53;
		Cmd.Addr	= RevertAddress( (TWORD)Page << 11 );
	
		FLASH_nCS	= 0;
		SerialFlashTransmit( MAKE_GEN_PTR_RAM_T( &Cmd, BYTE ), sizeof Cmd );
		FLASH_nCS	= 1;
		//--------------------------------------------------------------------------
#if			OPTIMIZE_BUFFERS == 1
		PaginaCarregada[Buffer] = Page;
#endif	//	OPTIMIZE_BUFFERS == 1
		//--------------------------------------------------------------------------
#if			OPTIMIZE_BUSY == 1
		BusyBuffer	= Buffer;
#endif	//	OPTIMIZE_BUSY == 1
		//--------------------------------------------------------------------------
		}

#if			OPTIMIZE_BUSY == 1
	if( BusyBuffer == Buffer )
#endif	//	OPTIMIZE_BUSY == 1
		while( SerialFlashIsBusy() )
			//vTaskDelay( 1 );
			vPortYield();

	//--------------------------------------------------------------------------
	// Read buffer
	Cmd.Cmd		= Buffer ? 0xd3 : 0xd1;
	Cmd.Addr	= RevertAddress( Src );

	FLASH_nCS	= 0;
	SerialFlashTransmit( MAKE_GEN_PTR_RAM_T( &Cmd, BYTE ), sizeof Cmd );
	SerialFlashReceive( Dst, Len );
	FLASH_nCS	= 1;

	//--------------------------------------------------------------------------
#if			USE_MUTEX == 1
	xMutexGive( xMutexSPI, 0 );
#endif	//	USE_MUTEX == 1

	return 0;
	}
*/
//==============================================================================
BYTE SerialFlashChipErase( void )
	{
#if			USE_MUTEX == 1
	if( xMutexTake( xMutexSPI, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1
	
	while( SerialFlashIsBusy() )
		//vTaskDelay( 1 );
		vPortYield();

	FLASH_nCS	= 0;
	SerialFlashTransmit( (BYTE GENERIC *)"\xc7\x94\x80\x9a", 4 );
	FLASH_nCS	= 1;

	while( SerialFlashIsBusy() )
		//vTaskDelay( 1 );
		vPortYield();

#if			USE_MUTEX == 1
	xMutexGive( xMutexSPI, 0 );
#endif	//	USE_MUTEX == 1

	return 0;
	}
//==============================================================================
