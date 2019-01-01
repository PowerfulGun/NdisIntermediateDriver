#include	"shared_head.h"

//该函数用来增加ADAPT的应用计数
VOID	_ReferenceAdapt(
	IN	PADAPT	_pAdapt
)
{
	ASSERT( _pAdapt->RefCount >= 0 );
	NdisInterlockedIncrement( &_pAdapt->RefCount );
}


/*
该函数用来减少对ADAPT的引用
返回值:
TRUE	引用减为0,并清理资源
FALSE	引用未到0
*/
BOOLEAN	_DerefenceAdapt(
	IN	PADAPT	_pAdapt
)
{
	ASSERT( _pAdapt->RefCount > 0 );

	NdisInterlockedDecrement( &_pAdapt->RefCount );

	if (_pAdapt->RefCount == 0)
	{
		//清理ADAPT的池资源
		if (_pAdapt->SendNetBufferListPoolHandle != NULL)
		{
			NdisFreeNetBufferListPool(
				_pAdapt->SendNetBufferListPoolHandle );
			_pAdapt->SendNetBufferListPoolHandle = NULL;
		}
		if (_pAdapt->RecvNetBufferListPoolHandle != NULL)
		{
			NdisFreeNetBufferListPool(
				_pAdapt->RecvNetBufferListPoolHandle );
			_pAdapt->RecvNetBufferListPoolHandle = NULL;
		}

		//释放自旋锁
		NdisFreeSpinLock( &_pAdapt->Lock );

		//释放ADAPT
		NdisFreeMemory( _pAdapt , 0 , 0 );

		return	TRUE;
	}

	return	FALSE;
}