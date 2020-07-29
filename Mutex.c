/*============================================================================*/
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
/*============================================================================*/
#include "FreeRTOS.h"
#include "list.h"
#include "task.h"
/*============================================================================*/
typedef struct
{
	xList			xTasksWaitingToTake;
	xTaskHandle		pxOwner;
	size_t			uxCount;
} xMUTEX;
/*============================================================================*/
typedef xMUTEX *xMutexHandle;
/*============================================================================*/
xMutexHandle xMutexCreate( void )
{
xMUTEX *pxNewMutex;
	
	pxNewMutex = pvPortMalloc( sizeof( xMUTEX ));
	if( pxNewMutex != NULL )
	{
		pxNewMutex->pxOwner	= NULL;
		pxNewMutex->uxCount	= 0;
		vListInitialise( &( pxNewMutex->xTasksWaitingToTake ) );
	}

	return pxNewMutex;
}
/*============================================================================*/
signed portBASE_TYPE xMutexTake( xMutexHandle pxMutex, portTickType xTicksToWait )
	{
	portENTER_CRITICAL();
	if( pxMutex->pxOwner == xTaskGetCurrentTaskHandle() )
		{
		pxMutex->uxCount++;
		portEXIT_CRITICAL();
		return pdTRUE;
		}

	if(( xTicksToWait > ( portTickType ) 0 ) && ( pxMutex->pxOwner != NULL ))
		{
		vTaskPlaceOnEventList( &( pxMutex->xTasksWaitingToTake ), xTicksToWait );
		taskYIELD();

		if( pxMutex->pxOwner == xTaskGetCurrentTaskHandle() )
			{
			pxMutex->uxCount = 1;
			portEXIT_CRITICAL();
			return pdTRUE;
			}
		else
			{
			portEXIT_CRITICAL();
			return pdFALSE;
			}
		}

	if( pxMutex->pxOwner == NULL )
		{
		pxMutex->pxOwner = xTaskGetCurrentTaskHandle();
		pxMutex->uxCount = 1;
		portEXIT_CRITICAL();
		return pdTRUE;
		}

	portEXIT_CRITICAL();
	return pdFALSE;
	}
/*============================================================================*/
signed portBASE_TYPE xMutexGive( xMutexHandle pxMutex, portBASE_TYPE Release )
	{
	portENTER_CRITICAL();
	if( pxMutex->pxOwner != xTaskGetCurrentTaskHandle() )
		{
		portEXIT_CRITICAL();
		return pdFALSE;
		}

	if( Release )
		pxMutex->uxCount = 0;
	else
		{
		if( --pxMutex->uxCount != 0 )
			{
			portEXIT_CRITICAL();
			return pdFALSE;
			}
		}
	

	if( !listLIST_IS_EMPTY( &pxMutex->xTasksWaitingToTake ))
		{
		pxMutex->pxOwner = (xTaskHandle)listGET_OWNER_OF_HEAD_ENTRY( (&pxMutex->xTasksWaitingToTake) );
		pxMutex->uxCount = 1;

		if( xTaskRemoveFromEventList( &pxMutex->xTasksWaitingToTake ) == pdTRUE )
			taskYIELD();
		}
	else
		pxMutex->pxOwner = NULL;

	portEXIT_CRITICAL();
	return pdTRUE;
	}
/*============================================================================*/
signed portBASE_TYPE xDoIOwnTheMutex( xMutexHandle pxMutex )
	{
	portBASE_TYPE c;

	portENTER_CRITICAL();
	c = pxMutex->pxOwner == xTaskGetCurrentTaskHandle();
	portEXIT_CRITICAL();

	return c;
	}
/*============================================================================*/
