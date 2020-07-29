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
#define	USE_MUTEX		0
//==============================================================================
#include <p18cxxx.h>
#include <stdio.h>
#include <string.h>
#include "SerialFlash.h"
#include "FileSystemRO.h"
#include "FileSystemConfigRO.h"

#if			USE_MUTEX == 1
#include "FreeRTOS.h"
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
	WORD		iEntry;
	TS_DirEntry	DirEntry;
	} TS_OpenFile;
//==============================================================================
typedef struct
	{
	BYTE		Proximo;
	BYTE		OpenFile;
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
static TS_FreeList	FreeList;
static BYTE			Buffer2Valid = 0;
static WORD			TempFreeListLastEntry;
//==============================================================================

static TS_OpenFile	OpenFiles[FS_MAXOPENFILES]	=
	{	
		{ 0xff, 0x00 }
	};

static TS_FileLock	FileLocks[FS_MAXSHARES]	=
	{
		{ 0xff, 0xff }
	};
										
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

	f	= FileLocks[file].OpenFile;

	if( f >= sizeof OpenFiles / sizeof OpenFiles[0] )
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
char open( const unsigned char GENERIC *name, unsigned char mode, unsigned char access )
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
	if( mode != O_READ )
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

	// O aquivo não está aberto.
	if(( i = IsFileOpen( Name )) >= sizeof OpenFiles / sizeof OpenFiles[0] )
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
		// Armazena o índice do arquivo no diretório.
		OpenFiles[i].iEntry			= j;
		// Copia os dados da entrada no diretório para a estrutura na lista de arquivos abertos
		memcpy( &OpenFiles[i].DirEntry, MAKE_GEN_PTR_RAM( &e ), sizeof OpenFiles[0].DirEntry );
		//----------------------------------------------------------------------
		// Faz a entrada ser a nova cabeça da lista de arquivos abertos.
		UsedOpenFiles				= i;
		}

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

		OpenFiles[f].Proximo		= FreeOpenFiles;
		FreeOpenFiles				= f;
		}

	//--------------------------------------------------------------------------
	// Limpa os campos da estrutura por segurança
	//--------------------------------------------------------------------------
	FileLocks[file].OpenFile		= 0xff;
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
/*

PAGINAS:

 0			- Descritor;
 1			- Diretório raiz
 2 ~   17	- ListaLivre
18 ~ 8191	- Área de dados

*/
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
		SerialFlashPageRead	( RootDirPage, (BYTE *)&FreeList, 0, sizeof FreeList );

//		SerialFlashPageRead	( RootDirPage, BufferDebug, 0, 1056 );

		return 0;
		}

	return -1;
	}
//==============================================================================
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
