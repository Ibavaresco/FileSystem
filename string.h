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
#ifndef		__STRING_H__
#define		__STRING_H__
//==============================================================================
#include <stddef.h>
#include <genericpointer.h>
//==============================================================================

/* The ANSI specified versions */
/** @name memcpy
The \b memcpy function copies \b n characters from the object
pointed to by \b src into the object pointed to by \b dst. If
copying takes place between objects that overlap, the behaviour is
undefined.
Stack usage: 8 bytes. Re-entrant.
\param dst pointer to destination
\param src pointer to source
\param n count of bytes to copy
\return The \b memcpy function returns the value of \b dst.
*/
void RAM *memcpy( auto void RAM *dst, auto const void GENERIC *src, auto size_t n );

//==============================================================================
/** @name memmove
The \b memmove function copies \b n characters from the object
pointed to by \b s2 into the object pointed to by \b s1. Copying
takes place as if the \b n characters from the object pointed to
by \b s2 are first copied into a temporary array of \b n
characters that does not overlap the objects pointed to by \b s1
and \b s2, and then the \b n characters from the temporary array
are copied into the object pointed to by \b s1.
Stack usage: 8 bytes. Re-entrant.
\param s1 pointer to destination
\param s2 pointer to source
\param n count of bytes to copy
\return The \b memmove function returns the value of \b s1.
*/
void RAM *memmove( auto void RAM *dst, auto const void GENERIC *src, auto size_t n );

//==============================================================================
/** @name strcpy
The \b strcpy function copies the string pointed to by \b s2
( including the terminating null character) into the array pointed to
by \b s1. If copying takes place between objects that overlap,
the behaviour is undefined.
Stack usage: 6 bytes. Re-entrant.
\param s1 pointer to destination
\param s2 pointer to source
\return The \b strcpy function returns the value of \b s1.
*/
char RAM *strcpy( auto char RAM *dst, auto const char GENERIC *src );

//==============================================================================
/** @name strncpy
The \b strncpy function copies not more than \b n characters
( auto characters that follow a null character are not copied) from the
array pointed to by \b s2 to the array pointed to by \b s1.
If \b n characters are copies and no null character is found then
\b s1 will not be terminated.
If copying takes place between objects that overlap, the behaviour
is undefined.
Stack usage: 8 bytes. Re-entrant.
\param s1 pointer to destination
\param s2 pointer to source
\param n count of maximum characters to copy
\return The \b strncpy function returns the value of \b s1.
*/
char RAM *strncpy( auto char RAM *dst, auto const char GENERIC *src, auto size_t n );

//==============================================================================
/** @name strcat
The \b strcat function appends a copy of the string pointed to
by \b s2 (including the terminating null character) to the end
of the string pointed to by \b s1. The initial character of
\b s2 overwrites the null character at the end of \b s1. If
copying takes place between objects that overlap, the behaviour is
undefined.
Stack usage: 6 bytes. Re-entrant.
\param s1 pointer to destination
\param s2 pointer to source
\return The \b strcat function returns the value of \b s1.
*/
char RAM *strcat( auto char RAM *dst, auto const char GENERIC *src );

//==============================================================================
/** @name strncat
The \b strncat function appends not more than \b n characters
( a null character and characters that follow it are not appended)
from the array pointed to by \b s2 to the end of the string
pointed to by \b s1. The initial character of \b s2 overwrites
the null character at the end of \b s1. A terminating null 
character is always appended to the result. If copying takes place
between objects that overlap, the behaviour is undefined.
Stack usage: 8 bytes. Re-entrant.
\param s1 pointer to destination
\param s2 pointer to source
\param n count of maximum characters to copy
\return The \b strncat function returns the value of \b s1.
*/
char RAM *strncat( auto char RAM *dst, auto const char GENERIC *src, auto size_t n );

//==============================================================================
/** @name memcmp
The \b memcmp function compares the first \b n characters of the
object pointed to by \b s1 to the first \b n characters pointed
to by \b s2.
Stack usage: 6 bytes. Re-entrant.
\param s1 pointer to object one
\param s2 pointer to object two
\param n count of characters to compare
\return The \b memcmp function returns a signed char greater than,
equal to, or less than zero, accordingly as the object pointed to by
\b s1 is greater than, equal to, or less than the object pointed to by
\b s2.
*/
signed char memcmp( auto const void GENERIC *s1, auto const void GENERIC *s2, auto size_t n );

//==============================================================================
/** @name strcmp
The \b strcmp function compares the string pointed to by \b s1 to
the string pointed to by \b s2.
Stack usage: 6 bytes. Re-entrant.
\param s1 pointer to string one
\param s2 pointer to string two
\return The \b strcmp function returns a signed char greater than,
equal to, or less than zero, accordingly as the string pointed to by
\b s1 is greater than, equal to, or less than the string pointed to
by \b s2.
*/
signed char strcmp( auto const char GENERIC *s1, auto const char GENERIC *s2 );

//==============================================================================
/** @name strcoll
Locale-aware string comparison. As MPLAB-C18 does not currently support
locales, the \b strcoll function is not required and is unimplemented.
*/

/** @name strncmp
The \b strncmp function compares not more than \b n characters
( auto characters that follow a null character are not compared) from the 
array pointed to by \b s1 to the array pointed to by \b s2.
Stack usage: 8 bytes. Re-entrant.
\param s1 pointer to string one
\param s2 pointer to string two
\param n count of characters to compare
\return The \b strncmp function returns a signed char greater than,
equal to, or less than zero, accordiongly as the possibly null-terminated
array pointed to by \b s1 is greater than, equal to, or less than the
possibly null-terminated array pointed to by \b s2.
*/
signed char strncmp( auto const char GENERIC *s1, auto const char GENERIC *s2, auto size_t n );

//==============================================================================
/** @name strxfrm
As MPLAB-C18 does not currently support locales, the \b strxfrm
function is not required and is unimplemented.
*/

/** @name memchr
The \b memchr function locates the first occurence of \b c [...]
in the initial \b n characters [...] of the object pointed to by
\b s.
 
The MPLAB-C18 version of the \b memchr function differs from the ANSI
specified function in that \b c is defined as an \b unsigned char
parameter rather than an \b int parameter.
Stack usage: 5 bytes. Re-entrant.
\param s pointer to object to search
\param c character to search for
\param n maximum number of chararacters to search
\return The \b memchr function returns a pointer to the located character,
or a null pointer if the character does not occur in the object.
*/
void GENERIC *memchr( auto const void GENERIC *s, auto unsigned char c, auto size_t n );

//==============================================================================
/** @name strchr
The \b strchr function locates the first occurence of \b c [...]
in the string pointed to by \b s. The terminating null character is
considered to be part of the string.

The MPLAB-C18 version of the \b strchr function differs from the ANSI
specified function in that \b c is defined as an \b char
parameter rather than an \b int parameter.
Stack usage: 3 bytes. Re-entrant.
\param s pointer to string to search
\param c character to search for
\return The \b strchr function returns a pointer to the located character,
or a null pointer if the character does not occur in the string.
*/
char GENERIC *strchr( auto const char GENERIC *s, auto char c );

//==============================================================================
/** @name strcspn
The \b strcspn function computes the length of the maximum initial
segment of the string pointed to by \b s1 which consists entirely
of characters \b not from the string pointed to by \b s2.
Stack usage: 6 bytes. Re-entrant.
\param s1 pointer to string to span
\param s2 pointer to set of characters
\return The \b strcspn function returns the length of the segment.
*/
size_t strcspn( auto const char RAM *s1, auto const char RAM *s2 );

//==============================================================================
/** @name strpbrk
The \b strpbrk function locates the first occurrence in the string
pointed to by \b s1 of any character from the string pointed to by
\b s2.
Stack usage: 6 bytes. Re-entrant.
\param s1 pointer to string to search
\param s2 pointer to set of characters
\return The \b strpbrk function returns a pointer to the character,
or a null-pointer if no character from \b s2 occurs in \b s1.
*/
char RAM *strpbrk( auto const char RAM *s1, auto const char RAM *s2 );

//==============================================================================
/** @name strrchr
The \b strrchr function locates the last occurrence of \b c [...]
in the string pointed to by \b s. The terminating null character is 
considered to be part of the string.

The MPLAB-C18 version of the \b strrchr function differs from the ANSI
specified function in that \b c is defined as an \b char
parameter rather than an \b int parameter.
Stack usage: 3 bytes. Re-entrant.
\param s pointer to string to search
\param c character to search for
\return The \b strrchr function returns a pointer to the character,
or a null pointer if \b c does not occur in the string.
*/
char GENERIC *strrchr( auto const char GENERIC *s, auto char c );

//==============================================================================
/** @name strspn
The \b strspn function computes the length of the maximum initial
segment of the string pointed to by \b s1 which consists entirely
of characters from the string pointed to by \b s2.
Stack usage: 6 bytes. Re-entrant.
\param s1 pointer to string to span
\param s2 pointer to set of characters
\return The \b strspn function returns the length of the segment.
*/
size_t strspn( auto const char RAM *s1, auto const char RAM *s2 );

//==============================================================================
/** @name strstr
The \b strstr function locates the first occurrence in the string 
pointed to by \b s1 of the sequence of characters( excluding the
null terminator) in the string pointed to by \b s2.
Stack usage: 8 bytes. Re-entrant.
\param s1 pointer to the string to search
\param s2 pointer to sequence to search for
\return The \b strstr function returns a pointer to the located
string, or a null pointer if the string is not found. If \b s2
points to a string with zero length, the function returns \b s1.
*/
char GENERIC *strstr( auto const char RAM *s1, auto const char RAM *s2 );

//==============================================================================
/** @name strtok
A sequence of calls to the \b strtok function breaks the
string pointed to by \b s1 into a sequence of tokens, each of
which is delimited by a character from the string pointed to by
\b s2. The first call in the sequence has \b s1 as its
first argument, and is followed by calls with a null pointer
as their first argument. The separator string pointed to by \b s2
may be different from call to call.

The first call in the sequence searches the string pointed to
by \b s1 for the first character that is \b not contained in
the current separator string \b s2. If no such character is found,
then there are no tokens in the string pointed to by \b s1 and the
\b strtok function returns a null pointer. If such a character is
found, it is the start of the first token.

The \b strtok function then searches from there for a character
that \b is contained in the current separator string. If no such
character is found, the current token extends to the end of the
string pointed to by \b s1, and subsequent searches for a token
will return a null pointer. If such a character is found, it is
overwritten by a null character, which terminates the current token.
The \b strtok function saves a pointer to the following character,
from which the next search for a token will start.

Each subsequent call, with a null pointer as the first argument, 
starts searching from the saved pointer and behaves as described
above.

The implementation shall behave as if no library function calls the
\b strtok function.

This function is implemented in C, is not re-entrant and calls the
functions: strspn, strpbrk, memchr.

\param s1 pointer to a string to begin searching, or null to continue
searching a prior string
\param s2 pointer to a string containing the set of separator characters
\return The \b strtok function returns a pointer to the first
character of a token, or a null pointer if there is no token. 
*/
char RAM *strtok( auto char RAM * RAM *s1, auto const char RAM *s2 );

//==============================================================================
/** @name memset
The \b memset function copies the value of \b c [...] into
each of the first \b n characters of the object pointed to by
\b s.

The MPLAB-C18 version of the \b memset function differs from the ANSI
specified function in that \b c is defined as an \b unsigned char
parameter rather than an \b int parameter.
Stack usage: 5 bytes. Re-entrant.
\param s pointer to object
\param c character to copy into object
\param n number of bytes of object to copy \b c into
\return The \b memset function returns the value of \b s.
*/
void RAM *memset( auto void RAM *s, auto unsigned char c, auto size_t n );

//==============================================================================
/** @name strerror
The \b strerror function maps the error number in \b errnum to
an error message string.

The implementation shall behave as if no library function calls the
\b strerror function.

MPLAB-C18 currently defines the \b strerror function to return
an empty string for all values of \b errnum. Future versions may
implement this function in a more meaningful manner.
\param errnum error number
\return The \b strerror function returns a pointer to the string,
the contents of which are implmentation defined. The array pointed to
shall not be modified by the program, but may be overwritten by a
subsequent call to the \b strerror function.
*/
#define strerror(n)((n),"")

//==============================================================================
/** @name strlen
The \b strlen function computes the length of the string pointed
to by \b s.
Stack usage: 2 bytes. Re-entrant.
\param s pointer to the string
\return The \b strlen function returns the number of characters
that precede the terminating null character.
*/
size_t strlen( auto const char GENERIC *s );

//==============================================================================
/** @name strupr
The \b strupr function converts each lower case character in the
string pointed to by \b s to the corresponding upper case character.
Stack usage: 2 bytes. Re-entrant.
\param s pointer to string
\return The \b strupr function returns the value of \b s.
*/
char RAM *strupr( auto char RAM *s );

//==============================================================================
/** @name strlwr
The \b strlwr function converts each upper case character in the
string pointed to by \b s to the corresponding lower case character.
Stack usage: 2 bytes. Re-entrant.
\param s pointer to string
\return The \b strlwr function returns the value of \b s.
*/
char RAM *strlwr( auto char RAM *s );

//==============================================================================
#endif	//	__STRING_H__
//==============================================================================
