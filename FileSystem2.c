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
#define	USE_MUTEX		1
//==============================================================================
#include <p18f87j60.h>
#include <genericpointer.h>
#include <stdio.h>
#include <string.h>
#include "LCD.h"
#include "SerialFlash.h"
#include "FreeRTOS.h"
#include "Sistema.h"
#include "FileSystem.h"

#if			USE_MUTEX == 1
#include "Mutex.h"
#endif	//	USE_MUTEX == 1
//==============================================================================
#define	PAGE_TYPE_ROOTDIR	0x5af1u
#define	PAGE_TYPE_SUBDIR	0x5af2u
#define	PAGE_TYPE_FREELIST	0x5af4u
#define	PAGE_TYPE_INODE		0x5a1fu
#define	PAGE_TYPE_INODESEC	0x5a2fu
#define	PAGE_TYPE_FILEDATA	0x5a4fu
//==============================================================================
#define	FLASH_TOTAL_NUMBER_OF_PAGES	8192u
#define	FLASH_LAST_PAGE_NUMBER		( FLASH_TOTAL_NUMBER_OF_PAGES - 1 )
//==============================================================================
typedef struct
	{
	WORD	Bad;
	WORD	Type;
	WORD	Sequence;
	} TS_PageControl;
//==============================================================================
typedef struct
	{
	WORD	Name;
	WORD	iNode;
	BYTE	Attributes;
	TWORD	Length;
	} TS_DirEntry;
//==============================================================================
typedef struct
	{
	WORD	FirstEntry;
	WORD	LastEntry;
	WORD	Pages[17];
	} TS_FreeList;
//==============================================================================
typedef struct
	{
	BYTE		Proximo;
	BYTE		Referencias;
	BYTE		Mode;
	BYTE		Modes[2];
	BYTE		Access;
	BYTE		Accesses[2];
	WORD		iEntry;
	TS_DirEntry	DirEntry;
	} TS_OpenFile;
//==============================================================================
typedef struct
	{
	BYTE		Proximo;
	BYTE		OpenFile;
	BYTE		Mode;
	BYTE		Access;
	struct
		{
		BYTE	Error	: 7;
		BYTE	EoF		: 1;
		}		Status;
	TWORD		CurrentFilePos;
	} TS_FileLock;
//==============================================================================
#if			USE_MUTEX == 1
static xMutexHandle	xMutexFS = NULL;
#endif	//	USE_MUTEX == 1

static WORD			RootDirPage;
static WORD			Sequence;
static TS_FreeList	FreeList;
static BYTE			Buffer2Valid = 0;
static WORD			TempFreeListLastEntry;
//==============================================================================

static TS_OpenFile	OpenFiles[5]	= {	{ 0x01, 0x00 },
										{ 0x02, 0x00 },
										{ 0x03, 0x00 },
										{ 0x04, 0x00 },
										{ 0xff, 0x00 } };

static TS_FileLock	FileLocks[5]	= {	{ 0x01, 0xff },
										{ 0x02, 0xff },
										{ 0x03, 0xff },
										{ 0x04, 0xff },
										{ 0xff, 0xff } };
										
/*										{ 0x05, 0xff },
										{ 0x06, 0xff },
										{ 0x07, 0xff },
										{ 0x08, 0xff },
										{ 0x09, 0xff },
										{ 0xff, 0xff } };
*/
static BYTE			FreeOpenFiles	= 0x00;
static BYTE			UsedOpenFiles	= 0xff;
static BYTE			FreeFileLocks	= 0x00;
//==============================================================================
#if			0
static void Debug( void )
	{
	_asm nop _endasm;
	}
#else	//	0
#define Debug()
#endif	//	0
//==============================================================================
static BYTE ValidaPagina( WORD Page, WORD Type )
	{
	TS_PageControl	p;

	if( Page < 1u || Page > FLASH_LAST_PAGE_NUMBER )
		{
		Debug();
		return -1;
		}
	else
		{
		SerialFlashPageRead( Page, (BYTE *)&p, 1024, sizeof p );
		if(( p.Bad != 0x50fau ) || ( p.Type != Type ))
			{
			Debug();
			return -2;
			}
		}

	return 0;
	}
//==============================================================================
static BYTE ValidateFile( BYTE file )
	{
	BYTE	f;

	// Se 'file' é maior que o número de entradas na tabela...
	if( file >= sizeof FileLocks / sizeof FileLocks[0] )
		// ...retornamos falha.
		return -1;

	// A entrada não está usada, o arquivo não está aberto.
	if( FileLocks[file].Mode < O_READ || FileLocks[file].Mode > O_WRITE )
		// Retorna falha
		return -1;

	f	= FileLocks[file].OpenFile;

	if( f >= sizeof OpenFiles / sizeof OpenFiles[0] )
		return -1;

	if( OpenFiles[f].Mode < O_READ || OpenFiles[f].Mode > O_WRITE )
		return -1;

	if( OpenFiles[f].Referencias == 0u )
		return -1;

	return f;
	}
//==============================================================================
static BYTE IsFileOpen( WORD Name )
	{
	BYTE i;

	// Verifica se o arquivo já está aberto
	for( i = UsedOpenFiles; ( i < sizeof OpenFiles / sizeof OpenFiles[0] ) && ( OpenFiles[i].DirEntry.Name != Name ); i = OpenFiles[i].Proximo )
		; // Empty statement

	if( i < sizeof OpenFiles / sizeof OpenFiles[0] )
		return i;

	return 0xff;
	}
//==============================================================================
static BYTE FindFile( WORD Name, TS_DirEntry *e )
	{
	BYTE		i;

	if( ValidaPagina( RootDirPage, PAGE_TYPE_ROOTDIR ))
		return 0;

	for( i = 8; i < 128u; i++ )
		{
		SerialFlashPageRead( RootDirPage, (BYTE *)e, (WORD)i << 3, sizeof *e );
		if( e->Name == Name )
			return i;
		}
	return 0;
	}
//==============================================================================
char GetLastError( BYTE file )
	{
#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	// 'file' não faz referência a um arquivo aberto
	if( ValidateFile( file ) >= sizeof OpenFiles / sizeof OpenFiles[0] )
		{
#if			USE_MUTEX == 1
		xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
		// Retorna falha.
		return -1;
		}

#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return FileLocks[file].Status.Error;
	}
//==============================================================================
char open( const char GENERIC *name, unsigned char mode, unsigned char access )
	{
	WORD		Name;
	TS_DirEntry	e;
	BYTE		i, j, File;

	memcpy( &Name, name, sizeof Name );

	// O nome do arquivo é inválido.
	if( ((BYTE*)&Name)[0] == 0xff || ((BYTE*)&Name)[1] == 0xff )
		// Retorna falha.
		return -1;

	// Se o modo é inválido...
	if( mode < O_READ || mode > O_WRITE )
		// ...retorna falha.
		return -1;

	// Se o acesso é inválido...
	if( access < A_EXCLUSIVE || access > A_WRITE )
		// ...retorna falha.
		return -1;

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	// Não existe um arquivo com esse nome.
	if(( j = FindFile( Name, &e )) == 0u )
		// Retorna falha.
		goto Erro;

	// Se não tem 'file locks' disponíveis...
	if( FreeFileLocks >= sizeof FileLocks / sizeof FileLocks[0] )
		// ...retorna falha.
		goto Erro;

	// O arquivo já está aberto.
	if(( i = IsFileOpen( Name )) < sizeof OpenFiles / sizeof OpenFiles[0] )
		{
		// Se o modo em que queremos abrir o arquivo é maior que o nivel de acesso atual...
		if( mode > OpenFiles[i].Access )
			// ...retorna falha.
			goto Erro;
		// Se o modo atual é maior que o nivel de acesso que queremos dar ao arquivo...
		if( OpenFiles[i].Mode > access )
			// ...retorna falha.
			goto Erro;
		}
	// O aquivo não está aberto.
	else
		{
		// Não dá para abrir mais arquivos.
		if( FreeOpenFiles >= sizeof OpenFiles / sizeof OpenFiles[0] )
			// Retorna falha.
			goto Erro;

		// Obtem o índice de uma entrada vazia na tabela de arquivos abertos.
		i							= FreeOpenFiles;
		// Remove a entrada vazia da lista livre.
		FreeOpenFiles				= OpenFiles[i].Proximo;
		//----------------------------------------------------------------------
		// Inicializa os campos da estrutura
		//----------------------------------------------------------------------
		// Faz a entrada apontar para a cabeça da lista de arquivos abertos.
		OpenFiles[i].Proximo		= UsedOpenFiles;
		// Sinaliza que o arquivos não tem nenhum 'lock' ainda.
		OpenFiles[i].Referencias	= 0;
		OpenFiles[i].Mode			= 0;
		OpenFiles[i].Modes[0]		= 0;
		OpenFiles[i].Modes[1]		= 0;
		OpenFiles[i].Access			= A_WRITE;
		OpenFiles[i].Accesses[0]	= 0;
		OpenFiles[i].Accesses[1]	= 0;
		// Armazena o índice do arquivo no diretório.
		OpenFiles[i].iEntry			= j;
		// Copia os dados da entrada no diretório para a estrutura na lista de arquivos abertos
		memcpy( &OpenFiles[i].DirEntry, MAKE_GEN_PTR_RAM( &e ), sizeof OpenFiles[0].DirEntry );
		//----------------------------------------------------------------------
		// Faz a entrada ser a nova cabeça da lista de arquivos abertos.
		UsedOpenFiles				= i;
		}

	if( access < OpenFiles[i].Access )
		OpenFiles[i].Access = access;
	if( access >= A_READ && access <= A_APPEND )
		OpenFiles[i].Accesses[access-A_READ]++;

	if( mode > OpenFiles[i].Mode )
		OpenFiles[i].Mode	= mode;
	if( mode >= O_APPEND && mode <= O_WRITE )
		OpenFiles[i].Modes[mode-O_APPEND]++;

	// Incrementa o número de referências (locks) do arquivo aberto.
	OpenFiles[i].Referencias++;

	// Obtém o índice da primeira entrada não usada da tabela de locks.
	File							= FreeFileLocks;
	//--------------------------------------------------------------------------
	// Inicializa os campos da estrutura
	//--------------------------------------------------------------------------
	// Remove a entrada da lista de entradas não usadas.
	FreeFileLocks					= FileLocks[File].Proximo;
	// Desencadeia a entrada da lista livre para evitar acidentes.
	FileLocks[File].Proximo			= 0xff;
	// Sinaliza a qual arquivo aberto este lock se refere.
	FileLocks[File].OpenFile		= i;
	FileLocks[File].Mode			= mode;
	FileLocks[File].Access			= access;
	// O Arquivo inicia com o contador de locação no offset zero.
	FileLocks[File].CurrentFilePos	= 0;
	//--------------------------------------------------------------------------

#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1

	//--------------------------------------------------------------------------
	// Retorna o número do lock.
	return File;

Erro:
#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return -1;
	}
//==============================================================================
char close( BYTE file )
	{
	BYTE	i, j, f;

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	// 'file' não faz referência a um arquivo aberto
	if(( f = ValidateFile( file )) >= sizeof OpenFiles / sizeof OpenFiles[0] )
		// Retorna falha.
		goto Erro;

	// O arquivo não tem mais locks.
	if( --OpenFiles[f].Referencias == 0u )
		{
		if( UsedOpenFiles == f )
			UsedOpenFiles = OpenFiles[f].Proximo;
		else
			{
			for( i = UsedOpenFiles, j = 0xff; i != f && i < sizeof OpenFiles / sizeof OpenFiles[0]; j = i, i = OpenFiles[i].Proximo )
				; // Empty statement.
			if( i != f || j >= sizeof OpenFiles / sizeof OpenFiles[0] )
				goto Erro;
			OpenFiles[j].Proximo	= OpenFiles[f].Proximo;
			}

		OpenFiles[f].Mode			= 0;
		OpenFiles[f].Modes[0]		= 0;
		OpenFiles[f].Modes[1]		= 0;
		OpenFiles[f].Access			= 0;
		OpenFiles[f].Accesses[0]	= 0;
		OpenFiles[f].Accesses[1]	= 0;

		OpenFiles[f].Proximo		= FreeOpenFiles;
		FreeOpenFiles				= f;
		}
	// Ainda existem locks para o arquivo.
	else
		{
		j	= FileLocks[file].Access;
		if( j >= A_READ && j <= A_APPEND )
			{
			j -= A_READ;
			if( OpenFiles[f].Accesses[j] > 0u )
				OpenFiles[f].Accesses[j]--;
			}
		if( OpenFiles[f].Accesses[0] != 0u )
			OpenFiles[f].Access = A_READ;
		else if( OpenFiles[f].Accesses[1] != 0u )
			OpenFiles[f].Access = A_APPEND;
		else
			OpenFiles[f].Access = A_WRITE;

		j	= FileLocks[file].Mode;
		if( j >= O_APPEND && j <= O_WRITE )
			{
			j -= O_APPEND;
			if( OpenFiles[f].Modes[j] > 0u )
				OpenFiles[f].Modes[j]--;
			}
		if( OpenFiles[f].Modes[1] != 0u )
			OpenFiles[f].Mode = O_WRITE;
		else if( OpenFiles[f].Modes[0] != 0u )
			OpenFiles[f].Mode = O_APPEND;
		else
			OpenFiles[f].Mode = O_READ;
		}

	//--------------------------------------------------------------------------
	// Limpa os campos da estrutura por segurança
	//--------------------------------------------------------------------------
	FileLocks[file].OpenFile		= 0xff;
	FileLocks[file].Mode			= 0;
	FileLocks[file].Access			= 0;
	FileLocks[file].Status.Error	= 0;
	FileLocks[file].Status.EoF		= 0;
	FileLocks[file].CurrentFilePos	= 0;

	FileLocks[file].Proximo			= FreeFileLocks;
	//--------------------------------------------------------------------------
	FreeFileLocks					= file;

#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return 0;

Erro:
#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return -1;
	}
//==============================================================================
short long lseek( BYTE file, short long Offset, BYTE Origin )
	{
	BYTE	f;

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	// 'file' não faz referência a um arquivo aberto
	if(( f = ValidateFile( file )) >= sizeof OpenFiles / sizeof OpenFiles[0] )
		// Retorna falha.
		goto Erro;

	// Seek a partir da posição corrente
	if( Origin == SEEK_CUR )
		Offset = FileLocks[file].CurrentFilePos + Offset;
	else if( Origin == SEEK_END )
		Offset = OpenFiles[f].DirEntry.Length + Offset;

	if( Offset < 0 )
		{
		FileLocks[file].Status.EoF		= 1;
		goto Erro;
		}
	else if( (unsigned short long)Offset >= OpenFiles[f].DirEntry.Length )
		{
		FileLocks[file].CurrentFilePos	= OpenFiles[f].DirEntry.Length;
		FileLocks[file].Status.EoF		= 1;
		}
	else
		{
		FileLocks[file].CurrentFilePos = Offset;
		FileLocks[file].Status.EoF		= 0;
		}

#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1

	return FileLocks[file].CurrentFilePos;

Erro:
#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return -1;
	}
//==============================================================================
char eof( BYTE file )
	{
	BYTE	f;

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	// 'file' não faz referência a um arquivo aberto
	if(( f = ValidateFile( file )) >= sizeof OpenFiles / sizeof OpenFiles[0] )
		{
#if			USE_MUTEX == 1
		xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
		// Retorna falha.
		return -1;
		}

	if( FileLocks[file].Status.EoF )
		{
#if			USE_MUTEX == 1
		xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
		return 1;
		}
	else
		{
#if			USE_MUTEX == 1
		xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
		return 0;
		}
	}
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
static unsigned short AllocPage( TS_PageControl *p )
	{
	WORD	OldFreeListFirstPage, OldFreeListLastPage, NewFreeListLastPage;
	WORD	NewPage, Aux;

	// Não existem suficientes páginas livres...
	if( FreeList.LastEntry < FreeList.FirstEntry + 3 )
		// ... retorna falha.
		return 0xffff;

	// Obtém o número da primeira página da lista livre.
	OldFreeListFirstPage	= FreeList.Pages[ ( FreeList.FirstEntry >> 9 ) & 31 ];

	if( ValidaPagina( OldFreeListFirstPage, PAGE_TYPE_FREELIST ))
		return 0xffff;

	// Obtém o número da página requisitada.
	SerialFlashPageRead( OldFreeListFirstPage, (BYTE *)&NewPage, ( FreeList.FirstEntry << 1 ) & 1023, sizeof NewPage );

	// Incrementa o índice da primeira entrada disponível na lista livre.
	FreeList.FirstEntry++;

	// A primeira página da lista livre esvaziou...
	if( FreeList.FirstEntry > 511u )
		{
		// A primeira página da lista livre está vazia, temos que compactar a lista livre.
	
		memmove( &FreeList.Pages[0], MAKE_GEN_PTR_RAM( &FreeList.Pages[1] ), sizeof FreeList.Pages - sizeof FreeList.Pages[0] );
		FreeList.FirstEntry		-= 512;
		FreeList.LastEntry		-= 512;
		TempFreeListLastEntry	-= 512;
	
		// Marca a última entrada da lista livre como inexistente.
		FreeList.Pages[sizeof FreeList.Pages / sizeof FreeList.Pages[0] - 1]	= 0xffff;
	
		OldFreeListLastPage	= FreeList.Pages[ ( TempFreeListLastEntry >> 9 ) & 31 ];
	
		if( !Buffer2Valid )
			{
			if( OldFreeListLastPage > FLASH_LAST_PAGE_NUMBER )
				{
				SerialFlashFillBuffer( 2, 0xff, 0, 1056 );
				}
			else
				{
				if( ValidaPagina( OldFreeListLastPage, PAGE_TYPE_FREELIST ))
					return 0xffff;
				SerialFlashLoadPage( 2, OldFreeListLastPage );
				}
			Buffer2Valid	= 1;
			}
	
		// Acrescenta a antiga primeira página da lista livre à própria lista livre (no buffer temporário).
		SerialFlashWriteBuffer( 2, MAKE_GEN_PTR_RAM_T( &OldFreeListFirstPage, BYTE ), ( TempFreeListLastEntry << 1 ) & 1023, sizeof OldFreeListFirstPage );
	
		// Incrementa o índice da última entrada na lista livre.
		TempFreeListLastEntry++;
	
		// Se o buffer temporário da última página da lista livre encheu...
		if(( TempFreeListLastEntry & 511 ) == 0u )
			{
			if( ValidaPagina( FreeList.Pages[ ( FreeList.FirstEntry >> 9 ) & 31 ], PAGE_TYPE_FREELIST ))
				return 0xffff;

			do
				{
				// ... obtém uma página para gravar o seu conteúdo.
				SerialFlashPageRead( FreeList.Pages[ ( FreeList.FirstEntry >> 9 ) & 31 ], (BYTE *)&NewFreeListLastPage, ( FreeList.FirstEntry << 1 ) & 1023, sizeof NewFreeListLastPage );
	
				FreeList.FirstEntry++;

				p->Bad		= 0x50fa;
				p->Type		= PAGE_TYPE_FREELIST;
				p->Sequence	= Sequence + 1;
				SerialFlashWriteBuffer( 2, MAKE_GEN_PTR_RAM_T( p, BYTE ), 1024, sizeof *p );

				//SerialFlashErasePage( NewFreeListLastPage );

				// Grava o buffer na página recém alocada.
				}
			while( SerialFlashProgramPage( 2, NewFreeListLastPage ));

			Aux	= (( TempFreeListLastEntry - 1 ) >> 9 ) & 31;

			// Atualiza o número da antiga última página na lista livre.
			FreeList.Pages[ Aux ] = NewFreeListLastPage;
	
			// Esvazia o buffer da imagem da última página da lista livre.
			SerialFlashFillBuffer( 2, 0xff, 0, 1056 );
	
			if( OldFreeListLastPage <= FLASH_LAST_PAGE_NUMBER )
				{
				// Acrescenta a antiga última página da lista livre à própria lista livre (no buffer temporário).
				SerialFlashWriteBuffer( 2, MAKE_GEN_PTR_RAM_T( &OldFreeListLastPage, BYTE ), ( TempFreeListLastEntry << 1 ) & 1023, sizeof OldFreeListLastPage );
				// Incrementa o índice da última entrada na lista livre.
				TempFreeListLastEntry++;
				}
	
			// Incrementa o índice da última entrada na lista livre.
			TempFreeListLastEntry++;
			}
		}

	// Retorna o número da página alocada.
	return NewPage;
	}
//==============================================================================
static BYTE FreePage( unsigned short Page, TS_PageControl *p )
	{
	WORD	OldFreeListLastPage, NewFreeListLastPage, OldFreeListFirstPage;
	WORD	Aux;
	BYTE	Descartou;

	// O número da página é inválido...
	if( Page > FLASH_LAST_PAGE_NUMBER )
		// ... retorna sem fazer nada.
		return -1;

	// Não existem suficientes páginas livres...
	if( FreeList.LastEntry < FreeList.FirstEntry + 3 )
		// ... retorna sem fazer nada.
		return -1;

	Descartou			= 0;

	OldFreeListLastPage	= FreeList.Pages[ ( TempFreeListLastEntry >> 9 ) & 31 ];

	if( !Buffer2Valid )
		{
		if( OldFreeListLastPage > FLASH_LAST_PAGE_NUMBER )
			{
			SerialFlashFillBuffer( 2, 0xff, 0, 1056 );
			}
		else
			{
			if( ValidaPagina( OldFreeListLastPage, PAGE_TYPE_FREELIST ))
				return -1;
			SerialFlashLoadPage( 2, OldFreeListLastPage );
			}
		Buffer2Valid	= 1;
		}

	SerialFlashWriteBuffer( 2, MAKE_GEN_PTR_RAM_T( &Page, BYTE ), ( TempFreeListLastEntry << 1 ) & 1023, sizeof Page );

	TempFreeListLastEntry++;

	// O buffer temporário com a imagem da última página da lista livre encheu...
	if(( TempFreeListLastEntry & 511 ) == 0u )
		{
		// ... guarda o número da primeira página atual.
		OldFreeListFirstPage	= FreeList.Pages[ ( FreeList.FirstEntry >> 9 ) & 31 ];

		do
			{
			if( ValidaPagina( OldFreeListFirstPage, PAGE_TYPE_FREELIST ))
				return -1;

			// Obtém uma página para gravar o conteúdo do buffer temporário da última página da lista livre.
			SerialFlashPageRead( OldFreeListFirstPage, (BYTE *)&NewFreeListLastPage, ( FreeList.FirstEntry << 1 ) & 1023, sizeof NewFreeListLastPage );

			FreeList.FirstEntry++;

			if( FreeList.FirstEntry > 511u )
				{
				memmove( &FreeList.Pages[0], MAKE_GEN_PTR_RAM( &FreeList.Pages[1] ), sizeof FreeList.Pages - sizeof FreeList.Pages[0] );
				FreeList.FirstEntry		-= 512;
				FreeList.LastEntry		-= 512;
				TempFreeListLastEntry	-= 512;
				FreeList.Pages[sizeof FreeList.Pages / sizeof FreeList.Pages[0] - 1]	= 0xffff;
				Descartou				 = 1;
				}

			p->Bad					= 0x50fa;
			p->Type					= PAGE_TYPE_FREELIST;
			p->Sequence				= Sequence + 1;
			SerialFlashWriteBuffer( 2, MAKE_GEN_PTR_RAM_T( p, BYTE ), 1024, sizeof *p );

			//SerialFlashErasePage( NewFreeListLastPage );

			// Grava o buffer na página recém alocada.
			}
		while( SerialFlashProgramPage( 2, NewFreeListLastPage ));

		Aux	= (( TempFreeListLastEntry - 1 ) >> 9 ) & 31;

		// Atualiza o número da antiga última página na lista livre.
		FreeList.Pages[ Aux ] = NewFreeListLastPage;

		// Esvazia o buffer da imagem da última página da lista livre.
		SerialFlashFillBuffer( 2, 0xff, 0, 1056 );
	
		// A primeira página da lista livre esvaziou...
		if( Descartou )
			{
			// Acrescenta a antiga primeira página da lista livre à própria lista livre (no buffer temporário).
			SerialFlashWriteBuffer( 2, MAKE_GEN_PTR_RAM_T( &OldFreeListFirstPage, BYTE ), ( TempFreeListLastEntry << 1 ) & 1023, sizeof OldFreeListFirstPage );
			// Incrementa o índice da última entrada na lista livre.
			TempFreeListLastEntry++;
			}
	
		if( OldFreeListLastPage <= FLASH_LAST_PAGE_NUMBER )
			{
			// Acrescenta a antiga última página da lista livre à própria lista livre (no buffer temporário).
			SerialFlashWriteBuffer( 2, MAKE_GEN_PTR_RAM_T( &OldFreeListLastPage, BYTE ), ( TempFreeListLastEntry << 1 ) & 1023, sizeof OldFreeListLastPage );
			// Incrementa o índice da última entrada na lista livre.
			TempFreeListLastEntry++;
			}
		}

	return 0;
	}
//==============================================================================
static BYTE CommitFreeList( TS_PageControl *p )
	{
	WORD	OldFreeListFirstPages[2], NewFreeListLastPage;
	WORD	Aux;
	BYTE	Descartados, Encheu;

	// O buffer temporário com a imagem da última página da lista livre encheu...
	if((( TempFreeListLastEntry & 511 ) != 0u ) && ( TempFreeListLastEntry != FreeList.LastEntry ))
		{
		Descartados	= 0;
		Encheu		= 0;

		p->Type		= PAGE_TYPE_FREELIST;

		// Guarda o número da atual última página.
		Aux			= FreeList.Pages[ ( TempFreeListLastEntry >> 9 ) & 31 ];


		if( Aux <= FLASH_LAST_PAGE_NUMBER )
			{
			// Acrescenta a antiga última página da lista livre à própria lista livre (no buffer temporário).
			SerialFlashWriteBuffer( 2, MAKE_GEN_PTR_RAM_T( &Aux, BYTE ), ( TempFreeListLastEntry << 1 ) & 1023, sizeof Aux );
			// Incrementa o índice da última entrada na lista livre.
			TempFreeListLastEntry++;
			if(( TempFreeListLastEntry & 511 ) == 0u )
				Encheu	= 1;
			}

		do
			{
			do
				{
				// Não há mais páginas disponíveis...
				if( TempFreeListLastEntry < FreeList.FirstEntry + 1 )
					return -1;

				// Obtém o número da página requisitada.
				SerialFlashPageRead( FreeList.Pages[ ( FreeList.FirstEntry >> 9 ) & 31 ], (BYTE *)&NewFreeListLastPage, ( FreeList.FirstEntry << 1 ) & 1023, sizeof NewFreeListLastPage );

				FreeList.FirstEntry++;

				if( FreeList.FirstEntry > 511u )
					{
					if( Descartados >= sizeof OldFreeListFirstPages / sizeof OldFreeListFirstPages[0] )
						return -1;

					Aux						 = (( FreeList.FirstEntry - 1 ) >> 9 ) & 31;

					// ... guarda o número da atual primeira página.
					OldFreeListFirstPages[Descartados++]	 = FreeList.Pages[ Aux ];

					memmove( &FreeList.Pages[0], MAKE_GEN_PTR_RAM( &FreeList.Pages[1] ), sizeof FreeList.Pages - sizeof FreeList.Pages[0] );
					FreeList.FirstEntry		-= 512;
					FreeList.LastEntry		-= 512;
					TempFreeListLastEntry	-= 512;
					FreeList.Pages[sizeof FreeList.Pages / sizeof FreeList.Pages[0] - 1]	= 0xffff;
					}

				while( Descartados != 0u && !Encheu )
					{
					// Acrescenta a antiga primeira página da lista livre à própria lista livre (no buffer temporário).
					SerialFlashWriteBuffer( 2, MAKE_GEN_PTR_RAM_T( &OldFreeListFirstPages[--Descartados], BYTE ), ( TempFreeListLastEntry << 1 ) & 1023, sizeof OldFreeListFirstPages[0] );

					// Incrementa o índice da última entrada na lista livre.
					TempFreeListLastEntry++;
					if(( TempFreeListLastEntry & 511 ) == 0u )
						Encheu	= 1;
					}

				Aux	= (( TempFreeListLastEntry - 1 ) >> 9 ) & 31;

				// Atualiza o número da antiga última página na lista livre.
				FreeList.Pages[ Aux ] = NewFreeListLastPage;

				p->Bad			= 0x50fa;
				p->Type			= PAGE_TYPE_FREELIST;
				p->Sequence		= Sequence + 1;
				SerialFlashWriteBuffer( 2, MAKE_GEN_PTR_RAM_T( p, BYTE ), 1024, sizeof *p );
				}
			while( SerialFlashProgramPage( 2, NewFreeListLastPage ));

			// Esvazia o buffer da imagem da última página da lista livre.
			SerialFlashFillBuffer( 2, 0xff, 0, 1056 );
			Encheu	= 0;
			}
		while( Descartados != 0u );
		}

	Buffer2Valid	= 0;

	return 0;
	}
//==============================================================================
//==============================================================================
//==============================================================================
//extern char *BufferDebug;
//==============================================================================
static BYTE UpdateDirectory( TS_DirEntry *e, TS_PageControl *p, BYTE DirEntryIndex )
	{
	WORD NewRootDirPage;

	if( ValidaPagina( RootDirPage, PAGE_TYPE_ROOTDIR ))
		{
		Debug();
		return -1;
		}

	SerialFlashLoadPage		( 1, RootDirPage );

//	SerialFlashReadBuffer	( 1, BufferDebug, 0, 1056 );

	if( FreePage( RootDirPage, p ))
		{
		Debug();
		return -1;
		}

	SerialFlashWriteBuffer	( 1, MAKE_GEN_PTR_RAM_T( e, BYTE ), (WORD)DirEntryIndex << 3, sizeof *e );

	do
		{
		if(( NewRootDirPage = AllocPage( p )) > FLASH_LAST_PAGE_NUMBER )
			{
			Debug();
			return -1;
			}
		if( CommitFreeList( p ))
			{
			Debug();
			return -1;
			}

		SerialFlashWriteBuffer	( 1, MAKE_GEN_PTR_RAM_T( &FreeList, BYTE ), 0, sizeof FreeList );
		SerialFlashWriteBuffer	( 1, MAKE_GEN_PTR_RAM_T( &TempFreeListLastEntry, BYTE ), 2, sizeof TempFreeListLastEntry );

		p->Bad					= 0x50fa;
		p->Type					= PAGE_TYPE_ROOTDIR;
		p->Sequence				= Sequence + 1;
		SerialFlashWriteBuffer	( 1, MAKE_GEN_PTR_RAM_T( p, BYTE ), 1024, sizeof *p );

		//SerialFlashErasePage	( NewRootDirPage );
		}
	while( SerialFlashProgramPage( 1, NewRootDirPage ));

	FreeList.LastEntry		= TempFreeListLastEntry;
	RootDirPage				= NewRootDirPage;

	Sequence++;

	if( ValidaPagina( RootDirPage, PAGE_TYPE_ROOTDIR ))
		{
		Debug();
		return -1;
		}

	return 0;
	}
//==============================================================================
size_t write( BYTE file, const BYTE GENERIC *buf, size_t count )
	{
	TS_PageControl	p;

	BYTE			f;
	WORD			Parte, Gravados;
	
	WORD			DataPage,		DirectiNodePage,		IndirectiNodePage;
	WORD			AuxA,			NewDirectiNodePage,		NewIndirectiNodePage;

	WORD			OffsetInPage;
	WORD			DirectiNodeIndex;
	BYTE			IndirectiNodeIndex, Indirect;

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	// 'file' não faz referência a um arquivo aberto
	if(( f = ValidateFile( file )) >= sizeof OpenFiles / sizeof OpenFiles[0] )
		// Retorna falha.
		goto Erro;

	if( FileLocks[file].Mode == O_APPEND )
		FileLocks[file].CurrentFilePos = OpenFiles[f].DirEntry.Length;
	else if( FileLocks[file].Mode != O_WRITE )
		goto Erro;

	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	Buffer2Valid			= 0;
	TempFreeListLastEntry	= FreeList.LastEntry;
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	for( Gravados = 0; count ; )
		{
		//----------------------------------------------------------------------
		if( FreeList.LastEntry < FreeList.FirstEntry + 6 )
			goto Fim;
		//----------------------------------------------------------------------
		p.Bad		= 0x50fa;
		p.Sequence	= Sequence + 1;
		//----------------------------------------------------------------------
		OffsetInPage		= FileLocks[file].CurrentFilePos & 1023;
		DirectiNodeIndex	= FileLocks[file].CurrentFilePos >> 10;
		//----------------------------------------------------------------------
		if( count + OffsetInPage > 1024u )
			Parte	= 1024 - OffsetInPage;
		else
			Parte	= count;
		count	-= Parte;
		//----------------------------------------------------------------------
		DataPage			= 0xffff;

		if( DirectiNodeIndex < 496u )
			{
			Indirect				 = 0;
			//@@@@ IndirectiNodePage		 = 0xffff;
			DirectiNodePage			 = OpenFiles[f].DirEntry.iNode;
			}
		else
			{
			Indirect				 = 1;
			DirectiNodeIndex		-= 496;
			IndirectiNodeIndex		 = DirectiNodeIndex / 496;
			DirectiNodeIndex		%= 496;

			IndirectiNodePage		 = OpenFiles[f].DirEntry.iNode;
			if( IndirectiNodePage <= FLASH_LAST_PAGE_NUMBER )
				{
				if( ValidaPagina( IndirectiNodePage, PAGE_TYPE_INODE ))
					goto Erro;

				SerialFlashPageRead( IndirectiNodePage, (BYTE *)&DirectiNodePage, IndirectiNodeIndex << 1, sizeof DirectiNodePage );
				}
			else
				DirectiNodePage		 = 0xffff;
			}

		if( DirectiNodePage <= FLASH_LAST_PAGE_NUMBER )
			{
			if( ValidaPagina( DirectiNodePage, PAGE_TYPE_INODE ))
				goto Erro;
			SerialFlashPageRead( DirectiNodePage, (BYTE *)&DataPage, 32 + ( DirectiNodeIndex << 1 ), sizeof DataPage );
			}

		//----------------------------------------------------------------------
		// Atualiza os dados
		//----------------------------------------------------------------------
		if( DataPage > FLASH_LAST_PAGE_NUMBER )
			{
			SerialFlashFillBuffer( 1, 0xff, 0, 1056 );
			}
		else
			{
			if( ValidaPagina( DataPage, PAGE_TYPE_FILEDATA ))
				goto Erro;
			SerialFlashLoadPage( 1, DataPage );
			if( FreePage( DataPage, &p ))
				goto Erro;
			}

		SerialFlashWriteBuffer( 1, buf, OffsetInPage, Parte );
		SerialFlashFillBuffer( 1, 0xff, 1024, 32 );

		do
			{
			if(( AuxA = AllocPage( &p )) > FLASH_LAST_PAGE_NUMBER )
				goto Erro;

			p.Bad		= 0x50fa;
			p.Type		= PAGE_TYPE_FILEDATA;
			p.Sequence	= Sequence + 1;
			SerialFlashWriteBuffer( 1, MAKE_GEN_PTR_RAM_T( &p, BYTE ), 1024, sizeof p );

			//SerialFlashErasePage	( AuxA );
			}
		while( SerialFlashProgramPage( 1, AuxA ));
			

		//----------------------------------------------------------------------
		// Atualiza o iNode direto
		//----------------------------------------------------------------------

		if( DirectiNodePage > FLASH_LAST_PAGE_NUMBER )
			{
			SerialFlashFillBuffer	( 1, 0xff, 0, 1056 );
			}
		else
			{
			if( ValidaPagina( DirectiNodePage, PAGE_TYPE_INODE ))
				goto Erro;
			SerialFlashLoadPage( 1, DirectiNodePage );
			if( FreePage( DirectiNodePage, &p ))
				goto Erro;
			}

		SerialFlashWriteBuffer( 1, MAKE_GEN_PTR_RAM_T( &AuxA, BYTE ), 32 + ( DirectiNodeIndex << 1 ), sizeof AuxA );
		SerialFlashFillBuffer( 1, 0xff, 1024, 32 );

		do
			{
			if(( NewDirectiNodePage = AllocPage( &p )) > FLASH_LAST_PAGE_NUMBER )
				goto Erro;

			p.Bad		= 0x50fa;
			p.Type		= PAGE_TYPE_INODE;
			p.Sequence	= Sequence + 1;
			SerialFlashWriteBuffer( 1, MAKE_GEN_PTR_RAM_T( &p, BYTE ), 1024, sizeof p );

			//SerialFlashErasePage	( NewDirectiNodePage );
			}
		while( SerialFlashProgramPage( 1, NewDirectiNodePage ));

		//----------------------------------------------------------------------
		// Atualiza o iNode indireto
		//----------------------------------------------------------------------
		if( Indirect )
			{
			if( IndirectiNodePage > FLASH_LAST_PAGE_NUMBER )
				{
				SerialFlashFillBuffer( 1, 0xff, 0, 1056 );
				}
			else
				{
				if( ValidaPagina( IndirectiNodePage, PAGE_TYPE_INODE ))
					goto Erro;
				SerialFlashLoadPage( 1, IndirectiNodePage );
				if( FreePage( IndirectiNodePage, &p ))
					goto Erro;
				}

			SerialFlashWriteBuffer( 1, MAKE_GEN_PTR_RAM_T( &NewDirectiNodePage, BYTE ), IndirectiNodeIndex << 1, sizeof NewDirectiNodePage );
			SerialFlashFillBuffer( 1, 0xff, 1024, 32 );

			do
				{
				if(( NewIndirectiNodePage = AllocPage( &p )) > FLASH_LAST_PAGE_NUMBER )
					goto Erro;

				p.Bad		= 0x50fa;
				p.Type		= PAGE_TYPE_INODE;
				p.Sequence	= Sequence + 1;
				SerialFlashWriteBuffer( 1, MAKE_GEN_PTR_RAM_T( &p, BYTE ), 1024, sizeof p );

				//SerialFlashErasePage	( NewIndirectiNodePage );
				}
			while( SerialFlashProgramPage( 1, NewIndirectiNodePage ));
			}
 
		//----------------------------------------------------------------------
		buf								+= Parte;
		Gravados						+= Parte;
		FileLocks[file].CurrentFilePos	+= Parte;

		if( OpenFiles[f].DirEntry.Length < FileLocks[file].CurrentFilePos )
			{
			OpenFiles[f].DirEntry.Length = FileLocks[file].CurrentFilePos;
			FileLocks[file].Status.EoF		= 1;
			}
		else
			FileLocks[file].Status.EoF		= 0;

		if( Indirect )
			OpenFiles[f].DirEntry.iNode = NewIndirectiNodePage;
		else
			OpenFiles[f].DirEntry.iNode = NewDirectiNodePage;

		//----------------------------------------------------------------------
		// Atualização do diretório
		//----------------------------------------------------------------------
		if( UpdateDirectory( &OpenFiles[f].DirEntry, &p, OpenFiles[f].iEntry ))
			goto Erro;
		}

Fim:
#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return Gravados;

Erro:
#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return -1;
	}
//==============================================================================
size_t read( BYTE file, void *buf, size_t count )
	{
	BYTE	f;
	WORD	Parte, Lidos;
	
	WORD	Offset, Page, iPage;

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	Lidos	= 0;

	// 'file' não faz referência a um arquivo aberto
	if(( f = ValidateFile( file )) >= sizeof OpenFiles / sizeof OpenFiles[0] )
		// Retorna falha.
		goto Fim;

	if( OpenFiles[f].DirEntry.iNode > FLASH_LAST_PAGE_NUMBER )
		{
		FileLocks[file].Status.EoF		= 1;
		goto Fim;
		}

	for( Lidos = 0; count && FileLocks[file].CurrentFilePos < OpenFiles[f].DirEntry.Length; )
		{
		//----------------------------------------------------------------------
		Offset	= FileLocks[file].CurrentFilePos & 1023;
		Page	= FileLocks[file].CurrentFilePos >> 10;
		//----------------------------------------------------------------------
		if( count + Offset > 1024u )
			Parte	= 1024 - Offset;
		else
			Parte	= count;
		count	-= Parte;

		if( Parte + FileLocks[file].CurrentFilePos > OpenFiles[f].DirEntry.Length )
			{
			Parte	= OpenFiles[f].DirEntry.Length - FileLocks[file].CurrentFilePos;
			count	= 0;
			FileLocks[file].Status.EoF		= 1;
			}
		else
			FileLocks[file].Status.EoF		= 0;
		//----------------------------------------------------------------------
		iPage	= OpenFiles[f].DirEntry.iNode;
		if( Page >= 496u )
			{
			Page	-= 496;
			if( ValidaPagina( iPage, PAGE_TYPE_INODE ))
				goto Erro;
			SerialFlashPageRead( iPage, (BYTE *)&iPage, ( Page / 496 ) << 1, sizeof iPage );
			if( iPage > FLASH_LAST_PAGE_NUMBER )
				{
				FileLocks[file].Status.EoF		= 1;
				goto Fim;
				}
			Page	 = Page % 496;
			}
		if( ValidaPagina( iPage, PAGE_TYPE_INODE ))
			goto Erro;
		SerialFlashPageRead( iPage, (BYTE *)&iPage, 32 + ( Page << 1 ), sizeof iPage );
		if( iPage > FLASH_LAST_PAGE_NUMBER )
			{
			FileLocks[file].Status.EoF		= 1;
			goto Fim;
			}
		if( ValidaPagina( iPage, PAGE_TYPE_FILEDATA ))
			goto Erro;
		SerialFlashPageRead( iPage, buf, Offset, Parte );
		//----------------------------------------------------------------------
		buf								 = Parte + (BYTE *)buf;
		Lidos							+= Parte;
		FileLocks[file].CurrentFilePos	+= Parte;
		//----------------------------------------------------------------------
		}

		if( FileLocks[file].CurrentFilePos >= OpenFiles[f].DirEntry.Length )
			FileLocks[file].Status.EoF		= 1;

Fim:
#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return Lidos;

Erro:
#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return -1;
	}
//==============================================================================
char rename( const char GENERIC *oldname, const char GENERIC *newname )
	{
	WORD		OldName, NewName;

	TS_PageControl	p;
	TS_DirEntry		e;
	BYTE			i;

	memcpy( &OldName, oldname, sizeof OldName );
	memcpy( &NewName, newname, sizeof NewName );

	// O nome original do arquivo é inválido.
	if( ((BYTE*)&OldName)[0] == 0xff || ((BYTE*)&OldName)[1] == 0xff )
		// Retorna falha.
		return -1;

	// O novo nome do arquivo é inválido.
	if( ((BYTE*)&NewName)[0] == 0xff || ((BYTE*)&NewName)[1] == 0xff )
		// Retorna falha.
		return -1;

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	// já existe um arquivo com o novo nome.
	if( FindFile( NewName, &e ) != 0u && NewName != OldName )
		// Retorna falha.
		goto Erro;

	// Não existe um arquivo com esse nome.
	if(( i = FindFile( OldName, &e )) == 0u )
		// Retorna falha.
		goto Erro;

	// O novo nome é igual ao antigo.
	if( NewName == OldName )
		goto Fim;

	// O arquivo está aberto.
	if( IsFileOpen( OldName ) < sizeof OpenFiles / sizeof OpenFiles[0] )
		// Retorna falha.
		goto Erro;

	//----------------------------------------------------------------------
	if( FreeList.LastEntry < FreeList.FirstEntry + 3 )
		goto Erro;
	//----------------------------------------------------------------------

	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	Buffer2Valid			= 0;
	TempFreeListLastEntry	= FreeList.LastEntry;
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	// Substitui o nome do arquivo no buffer.
	e.Name	= NewName;

	//--------------------------------------------------------------------------
	// Atualização do diretório
	//--------------------------------------------------------------------------
	if( UpdateDirectory( &e, &p, i ))
		goto Erro;

Fim:
#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return 0;

Erro:
#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return -1;
	}
//==============================================================================
/*

PAGINAS:

 0			- Descritor;
 1			- Diretório raiz
 2 ~   17	- ListaLivre
18 ~ 8191	- Área de dados

*/
//==============================================================================
void format( void )
	{
	TS_PageControl	p;
	BYTE			i;
	WORD			j, s;

	for( i = 0, s = 0, j = 1; j < FLASH_TOTAL_NUMBER_OF_PAGES; j++ )
		{
		SerialFlashPageRead	( j, (BYTE *)&p, 1024, sizeof p );
		if( p.Bad == 0x50fau && p.Type == PAGE_TYPE_ROOTDIR )
			{
			if( i == 0u || p.Sequence - s < FLASH_TOTAL_NUMBER_OF_PAGES )
				{
				s	= p.Sequence;
				i	= 1;
				}
			}
		}

	p.Bad		= 0x50fa;
	p.Sequence	= s + 1;
	Sequence	= s + 1;

	for( i = 0, j = 18; i < sizeof FreeList.Pages / sizeof FreeList.Pages[0] - 1; i++ )
		{
		FreeList.Pages[i] = i + 2;
		SerialFlashFillBuffer	( 1, 0xff, 0, 1056 );
		for( s = 0; s < 1024u && j < FLASH_TOTAL_NUMBER_OF_PAGES; s += 2, j++ )
			SerialFlashWriteBuffer( 1, MAKE_GEN_PTR_RAM_T( &j, BYTE ), s, sizeof j );

		p.Bad					= 0x50fa;
		p.Type					= PAGE_TYPE_FREELIST;
		p.Sequence				= Sequence;
		SerialFlashWriteBuffer	( 1, MAKE_GEN_PTR_RAM_T( &p, BYTE ), 1024, sizeof p );

		//SerialFlashErasePage	( i + 2 );
		SerialFlashProgramPage	( 1, i + 2 );
		}

	FreeList.Pages[i]	= 0xffff;

	FreeList.FirstEntry	= 0;
	FreeList.LastEntry	= FLASH_TOTAL_NUMBER_OF_PAGES - 18;

	//SerialFlashErasePage	( 1 );
	SerialFlashFillBuffer	( 1, 0xff, 0, 1056 );
	SerialFlashWriteBuffer	( 1, MAKE_GEN_PTR_RAM_T( &FreeList, BYTE ), 0, sizeof FreeList );

	p.Bad					= 0x50fa;
	p.Type					= PAGE_TYPE_ROOTDIR;
	p.Sequence				= Sequence;
	SerialFlashWriteBuffer	( 1, MAKE_GEN_PTR_RAM_T( &p, BYTE ), 1024, sizeof p );

	//SerialFlashErasePage	( 1 );
	SerialFlashProgramPage	( 1, 1 );

	RootDirPage	= 1;
	}
//==============================================================================
//extern BYTE *BufferAux;

char mount( void )
	{
	TS_PageControl	p;
	WORD			j, s, d;
	BYTE			i;

#if			USE_MUTEX == 1
	if( xMutexFS == NULL )
		if(( xMutexFS = xMutexCreate() ) == NULL )
			return -1;
#endif	//	USE_MUTEX == 1

	for( s = 0, d = 0, j = 1; j < FLASH_TOTAL_NUMBER_OF_PAGES; j++ )
		{
		SerialFlashPageRead	( j, (BYTE *)&p, 1024, sizeof p );
		if( p.Bad == 0x50fau && p.Type == PAGE_TYPE_ROOTDIR )
			{
			if( d == 0u || p.Sequence - s < FLASH_TOTAL_NUMBER_OF_PAGES )
				{
				s	= p.Sequence;
				d	= j;
				}
			}
		}

//	for( i = 1; i < 32; i++ )
//		SerialFlashPageRead( i, BufferAux, 0, 1056 );

	if( d != 0u )
		{
		RootDirPage	= d;
		Sequence	= s;
		SerialFlashPageRead	( RootDirPage, (BYTE *)&FreeList, 0, sizeof FreeList );

//		SerialFlashPageRead	( RootDirPage, BufferDebug, 0, 1056 );

		return 0;
		}

	return -1;
	}
//==============================================================================
//==============================================================================
char remove( const char GENERIC *name )
	{
	TS_PageControl	p;
	TS_DirEntry		e;
	WORD			Name;
	WORD			Aux;
	BYTE			i;
	WORD			Page, iPage, NumeroTotalPaginas;

	memcpy( &Name, name, sizeof Name );

	// O nome do arquivo é inválido.
	if( ((BYTE*)&Name)[0] == 0xff || ((BYTE*)&Name)[1] == 0xff )
		// Retorna falha.
		return -1;

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	// Não existe um arquivo com esse nome.
	if(( i = FindFile( Name, &e )) == 0u )
		// Retorna falha.
		goto Erro;;

	// O arquivo está aberto.
	if( IsFileOpen( Name ) < sizeof OpenFiles / sizeof OpenFiles[0] )
		// Retorna falha.
		goto Erro;;

	//----------------------------------------------------------------------
	if( FreeList.LastEntry < FreeList.FirstEntry + 3 )
		goto Erro;
	//----------------------------------------------------------------------

	Buffer2Valid			= 0;
	TempFreeListLastEntry	= FreeList.LastEntry;

	NumeroTotalPaginas		= ( e.Length + 1023 ) >> 10;

	while( NumeroTotalPaginas )
		{
		Page	= NumeroTotalPaginas - 1;
		iPage	= e.iNode;
		if( Page >= 496u )
			{
			Page	-= 496;
			if( ValidaPagina( iPage, PAGE_TYPE_INODE ))
				goto Erro;
			SerialFlashPageRead( iPage, (BYTE *)&iPage, ( Page / 496 ) << 1, sizeof iPage );
			Page	%= 496;
			}

		if( ValidaPagina( iPage, PAGE_TYPE_INODE ))
			goto Erro;

		for( Page++; Page--; )
			{
			if( FreeList.LastEntry < FreeList.FirstEntry + 2 )
				if( UpdateDirectory( &e, &p, i ))
					goto Erro;

			// Le o índice da última página do arquivo.
			SerialFlashPageRead( iPage, (BYTE *)&Aux, 32 + ( Page << 1 ), sizeof Aux );

			// Acrescenta o índice da página à lista livre.
			if( FreePage( Aux, &p ))
				goto Erro;
			
			// Decrementa o número total de páginas do arquivo.
			NumeroTotalPaginas--;

			// Calcula o novo tamanho do arquivo.
			e.Length = (unsigned short long)NumeroTotalPaginas << 10;
			}
		if( FreePage( iPage, &p ))
			goto Erro;
		}

	e.Name			= 0xffff;
	e.iNode			= 0xffff;
	e.Attributes	= 0xff;
	e.Length		= 0xffffff;

	if( UpdateDirectory( &e, &p, i ))
		goto Erro;

	Buffer2Valid	= 0;

#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return 0;

Erro:
#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return -1;
	}
//==============================================================================
char creat( const char GENERIC *name )
	{
	WORD			Name;

	TS_PageControl	p;
	TS_DirEntry		e;
	BYTE			i;

	memcpy( &Name, name, sizeof Name );

	// O nome do arquivo é inválido.
	if( ((BYTE*)&Name)[0] == 0xff || ((BYTE*)&Name)[1] == 0xff )
		// Retorna falha.
		return -1;

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	// Já existe um arquivo com esse nome.
	if( FindFile( Name, &e ) != 0u )
		// Retorna falha.
		goto Erro;

	// Procura por uma entrada livre no diretório raiz.
	if(( i = FindFile( 0xffff, &e )) == 0u )
		// Retorna falha.
		goto Erro;

	//----------------------------------------------------------------------
	if( FreeList.LastEntry < FreeList.FirstEntry + 3 )
		goto Erro;
	//----------------------------------------------------------------------

	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	Buffer2Valid			= 0;
	TempFreeListLastEntry	= FreeList.LastEntry;
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	e.Name			= Name;
	e.iNode			= 0xffff;
	e.Attributes	= 0xff;
	e.Length		= 0;

	//--------------------------------------------------------------------------
	// Atualização do diretório
	//--------------------------------------------------------------------------
	if( UpdateDirectory( &e, &p, i ))
		goto Erro;

#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return 0;

Erro:
#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return -1;
	}
//==============================================================================
char findfile( const char GENERIC *mask, find_t RAM *findstruct, char *start )
	{
	WORD			Mask;

	TS_PageControl	p;
	TS_DirEntry		e;
	BYTE			i;
	
	memcpy( &Mask, mask, sizeof Mask );

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	if( ValidaPagina( RootDirPage, PAGE_TYPE_ROOTDIR ))
		goto Erro;

	if( *start < 8 )
		*start = 8;

	for( i = *start; i < 128u; i++ )
		{
		SerialFlashPageRead( RootDirPage, (BYTE*)&e, (WORD)i << 3, sizeof e );
		if( e.Name != 0xffff )
			{
			if(( (BYTE)Mask == 0xff || (BYTE)Mask == (BYTE)e.Name ) && ( Mask >> 8 == 0xff || Mask >> 8 == e.Name >> 8 ))
				{
				if( findstruct )
					{
					findstruct->attrib	= e.Attributes;
					findstruct->length	= e.Length;
					findstruct->name[0]	= (char)e.Name;
					findstruct->name[1]	= e.Name >> 8;
					findstruct->name[2]	= 0;
					}
				*start	= i + 1;

#if			USE_MUTEX == 1
				xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1

				return 0;
				}
			}
		}

Erro:
#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return -1;
	}
//==============================================================================
short long filesize( BYTE file )
	{
	short long	Aux;
	BYTE		f;

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	if(( f = ValidateFile( file )) >= sizeof OpenFiles / sizeof OpenFiles[0] )
		Aux	= -1;
	else
		Aux	= OpenFiles[f].DirEntry.Length;

#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1

	return Aux;
	}
//==============================================================================
short long filepos( BYTE file )
	{
	short long	Aux;

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	if( ValidateFile( file ) >= sizeof OpenFiles / sizeof OpenFiles[0] )
		Aux	= -1;
	else
		Aux	= FileLocks[file].CurrentFilePos;

#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1

	return Aux;
	}
//==============================================================================
char fsck( void )
	{
	TS_FreeList		TempFreeList;
	WORD			i;
	unsigned short	j;

	// Verifica se a página que contém o diretório raiz é válida.
	if( ValidaPagina( RootDirPage, PAGE_TYPE_ROOTDIR ))
		return -1;

	// Verifica se a cópia da lista livre em memória é igual a que esxtá em disco.
	SerialFlashPageRead( RootDirPage, (BYTE *)&TempFreeList, 0, sizeof TempFreeList );
	if( memcmp( MAKE_GEN_PTR_RAM( &FreeList ), MAKE_GEN_PTR_RAM( &TempFreeList ), sizeof TempFreeList ))
		return -2;

	// Verifica se a primeira página da lista livre está na primeira entrada da lista.
	if( FreeList.FirstEntry > 511u )
		return -3;

//	Verifica se o disco não está muito cheio.
//	if( FreeList.LastEntry < FreeList.FirstEntry + 20 )
//		return 1;

	// Verifica se a lista livre não tem entradas demais.
	if( FreeList.LastEntry - FreeList.FirstEntry > 8192u - 18u )
		return -4;

	// Verifica se a lista livre não tem entradas demais.
	if( FreeList.LastEntry > 8703u )
		return -5;

	// Verifica se todas as páginas da lista livre são válidas
	for( i = 0; i < (( FreeList.LastEntry + 511 ) >> 9 ); i++ )
		{
		if( ValidaPagina( FreeList.Pages[i], PAGE_TYPE_FREELIST ))
			return -6;
		}

	// Verifica se as entradas não usadas da lista livre estão invalidadas.
	for( ; i < sizeof FreeList.Pages / sizeof FreeList.Pages[0] ; i++ )
		{
		if( FreeList.Pages[i] != 0xffff )
			return -7;
		}

	// Verifica se todas os índices da lista livre são para páginas válidas.
	for( i = FreeList.FirstEntry; i < FreeList.LastEntry; i++ )
		{
		SerialFlashPageRead( FreeList.Pages[i >> 9], (BYTE *)&j, ( i << 1 ) & 1023, sizeof j );
		if( j > FLASH_LAST_PAGE_NUMBER )
			return -8;
		}

	return 0;
	}
//==============================================================================
short long df( void )
	{
	unsigned short long	Aux;

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	Aux		= 1024ul * ( FreeList.LastEntry - FreeList.FirstEntry );

#if			USE_MUTEX == 1
		xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
		// Retorna falha.
		return Aux;
	}
//==============================================================================
char chmod( const char GENERIC *name, unsigned char attribute )
	{
	WORD			Name;

	TS_PageControl	p;
	TS_DirEntry		e;
	BYTE			i;

	memcpy( &Name, name, sizeof Name );

	// O nome do arquivo é inválido.
	if( ((BYTE*)&Name)[0] == 0xff || ((BYTE*)&Name)[1] == 0xff )
		// Retorna falha.
		return -1;

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	// Não existe um arquivo com esse nome.
	if(( i = FindFile( Name, &e )) == 0u )
		// Retorna falha.
		goto Erro;

	// O arquivo está aberto.
	if( IsFileOpen( Name ) < sizeof OpenFiles / sizeof OpenFiles[0] )
		// Retorna falha.
		goto Erro;

	if( e.Attributes != attribute )
		{
		//----------------------------------------------------------------------
		if( FreeList.LastEntry < FreeList.FirstEntry + 3 )
			goto Erro;
		//----------------------------------------------------------------------
	
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		Buffer2Valid			= 0;
		TempFreeListLastEntry	= FreeList.LastEntry;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	
		// Substitui o nome do arquivo no buffer.
		e.Attributes			= attribute;
	
		//--------------------------------------------------------------------------
		// Atualização do diretório
		//--------------------------------------------------------------------------
		if( UpdateDirectory( &e, &p, i ))
			goto Erro;
		}

Fim:
#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return 0;

Erro:
#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return -1;
	}
//==============================================================================
char fileexist( const char GENERIC *name )
	{
	WORD			Name;

	TS_PageControl	p;
	TS_DirEntry		e;
	BYTE			i;

	memcpy( &Name, name, sizeof Name );

#if			USE_MUTEX == 1
	if( xMutexTake( xMutexFS, portMAX_DELAY ) != pdTRUE )
		return -1;
#endif	//	USE_MUTEX == 1

	if( ValidaPagina( RootDirPage, PAGE_TYPE_ROOTDIR ))
		goto Erro;

	for( i = 8; i < 128u; i++ )
		{
		SerialFlashPageRead( RootDirPage, (BYTE*)&e, (WORD)i << 3, sizeof e );
		if( e.Name != 0xffff )
			{
#if			USE_MUTEX == 1
			xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
			return 0;
			}
		}

Erro:
#if			USE_MUTEX == 1
	xMutexGive( xMutexFS, 0 );
#endif	//	USE_MUTEX == 1
	return -1;
	}
//==============================================================================
