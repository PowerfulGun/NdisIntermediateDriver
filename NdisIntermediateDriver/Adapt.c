#include	"shared_head.h"

//�ú�����������ADAPT��Ӧ�ü���
VOID	_ReferenceAdapt(
	IN	PADAPT	_pAdapt
)
{
	ASSERT( _pAdapt->RefCount >= 0 );
	NdisInterlockedIncrement( &_pAdapt->RefCount );
}


/*
�ú����������ٶ�ADAPT������
����ֵ:
TRUE	���ü�Ϊ0,��������Դ
FALSE	����δ��0
*/
BOOLEAN	_DerefenceAdapt(
	IN	PADAPT	_pAdapt
)
{
	ASSERT( _pAdapt->RefCount > 0 );

	NdisInterlockedDecrement( &_pAdapt->RefCount );

	if (_pAdapt->RefCount == 0)
	{
		//����ADAPT�ĳ���Դ
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

		//�ͷ�������
		NdisFreeSpinLock( &_pAdapt->Lock );

		//�ͷ�ADAPT
		NdisFreeMemory( _pAdapt , 0 , 0 );

		return	TRUE;
	}

	return	FALSE;
}