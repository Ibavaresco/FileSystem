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
#ifndef		__FILESYSTEM_H__
#define		__FILESYSTEM_H__
//==============================================================================
#include "Tipos.h"

/*=========================================================================*//**
\name Modos de abertura do arquivo:
*//*====================================================================*//*@{*/
/*! \brief Somente leitura. */
#define	O_READ		1u
/*! \brief Leitura e 'append'. */
#define O_APPEND	2u
/*! \brief Leitura, 'append' e escrita. */
#define	O_WRITE		3u
/*@}*//*======================================================================*/

/*=========================================================================*//**
\name Modos que outros processos podem usar para abrir o arquivo para compartilhamento:
*//*====================================================================*//*@{*/
/*! \brief Exclusivo, o arquivo n&atilde;o pode ser aberto por mais ningu&eacute;m. */
#define	A_EXCLUSIVE	0u
/*! \brief Outros processos podem abrir o arquivo para leitura. */
#define	A_READ		1u
/*! \brief Outros processos podem abrir o arquivo para 'append'. */
#define	A_APPEND	2u
/*! \brief Outros processos podem abrir o arquivo para escrita. */
#define	A_WRITE		3u
/*@}*//*======================================================================*/

/*=========================================================================*//**
\name Posi&ccedil;&atilde;o que 'seek' toma como refer&ecirc;ncia:
*//*====================================================================*//*@{*/
/*! \brief Seek a partir do in&iacute;cio do arquivo. */
#define SEEK_SET	0u
/*! \brief Seek a partir da posi&ccedil;&atilde;o corrente. */
#define SEEK_CUR	1u
/*! \brief Seek a partir do fim do arquivo. */
#define SEEK_END	2u
/*@}*//*======================================================================*/

/*=========================================================================*//**
\brief Formata o sistema de arquivos.

Ap&oacute;s o format, o sistema de arquivos fica vazio, quaisquer dados que
porventura houvessem s&atilde;o apagados;
*//*==========================================================================*/
void		format	( void );

/*=========================================================================*//**
\brief Monta o sistema de arquivos.

\return Caso mount tenha sucesso, retorna 0, sen&atilde;o retorna -1.
*//*==========================================================================*/
char		mount	( void );

/*=========================================================================*//**
\brief Cria um novo arquivo.

\param name Nome do arquivo a ser criado. N&atildeo pode existir outro arquivo com o
mesmo nome. Tem comprimento fixo de dois caracteres, não necessita de `\\0'. Se forem
passados mais caracteres, ser&aacute; trucada para dois caracteres. Se for passado
apenas um caractere, o '\\0' final far&aacute; parte do nome do arquivo.
Podem ser usados todos os caracteres na faixa de 0x00 a 0xfe.

\return Caso tenha sucesso, retorna 0, sen&atilde;o retorna -1.
*//*==========================================================================*/
char		creat	( const char GENERIC *name );

/*=========================================================================*//**
\brief Renomeia um arquivo.

O arquivo n&atilde;o pode estar aberto.

\return Caso tenha sucesso, retorna 0, sen&atilde;o retorna -1.
*//*==========================================================================*/
char		rename	( const char GENERIC *oldname, const char GENERIC *newname );

/*=========================================================================*//**
\brief Apaga um arquivo.

O arquivo n&atilde;o pode estar aberto.

\return Caso tenha sucesso, retorna 0, sen&atilde;o retorna -1.
*//*==========================================================================*/

char		remove	( const char GENERIC *name );

size_t		read	( BYTE file, void *buf, size_t count );
size_t		write	( BYTE file, const BYTE GENERIC *buf, size_t count );
char		eof		( BYTE file );
short long	lseek	( BYTE file, short long Offset, BYTE Origin );

char		open	( const char GENERIC *name, unsigned char mode, unsigned char access );
char		close	( BYTE file );
short long	filesize( BYTE file );
short long	filepos	( BYTE file );

/*============================================================================*/

typedef struct
	{
	char		name[3];
	short long	length;
	char		attrib;
	} find_t;

/*=========================================================================*//**
\brief Busca no sistema de arquivos por arquivos cujos nomes coincidam com a m&aacute;scara ::mask.

\param mask M&aacute;scara de pesquisa.
\param findstruct Ponteiro para um objeto tipo ::find_t que receber&aacute; os dados dos arquivos.
\param start Ponteiro para uma variável que será usada para iterar pela lista de arquivos.

\return Quando encontrar um arquivo que coincida com a m&aacute;scara, preenche a estrutura apontada
por ::findstruct com os meta-dados do arquivo e retorna ZERO, sen&atilde;o retorna -1.

A vari&aacute;vel apontada por ::start deve ser inicializada com ZERO antes da primeira chamada
&agrave; fun&ccedil;&atilde;o findfile.
<br>
A m&aacute;scara &eacute; um string de dois caracteres. O valor 0xFF em qualquer posi&ccedil;&atilde;o
&eacute; um "wildcard" (corresponde a '?' em linux e windows), qualquer outro caractere &eacute;
interpretado literalmente.

*//*==========================================================================*/
char		findfile( const char GENERIC *mask, find_t RAM *findstruct, char RAM *start );

/*=========================================================================*//**
\brief Verifica a integridade do sistema de arquivos.

\return Em caso de sucesso retorna ZERO, sen&atilde;o retorna um c&oacute;digo de erro.
*//*==========================================================================*/
char		fsck	( void );

/*=========================================================================*//**
\brief Retorna o espa&ccedil;o livre em bytes dispon&iacute;vel no sistema de arquivos.

\return Em caso de sucesso retorna o n&uacute;mero de bytes livres, sen&atilde;o retorna -1.
*//*==========================================================================*/
short long	df		( void );

/*=========================================================================*//**
\brief Altera os atributos de um arquivo.

\param name String com o nome do arquivo.
\param attribute Novo valor para os atributos do arquivo.

\return Em caso de sucesso retorna ZERO, sen&atilde;o retorna -1.

O arquivo não pode estar aberto.
*//*==========================================================================*/
char		chmod	( const char GENERIC *name, unsigned char attribute );

//==============================================================================
#endif	//	__FILESYSTEM_H__
//==============================================================================
