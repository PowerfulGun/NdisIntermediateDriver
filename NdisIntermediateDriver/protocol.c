#include	"shared_head.h"


extern	NDIS_HANDLE	g_ProtocolDriver;
extern	NDIS_HANDLE	g_MiniportDriver;
extern	NDIS_HANDLE	g_hControlDevice;


PROTOCOL_UNINSTALL _ProtocolUninstall;
/*
�ú���ж��Э������
*/
VOID	_ProtocolUninstall()
{

	KdPrint( ("_ProtocolUninstall: Entered!\n") );

	if (g_ProtocolDriver)
	{
		NdisDeregisterProtocolDriver( g_ProtocolDriver );
		g_ProtocolDriver = NULL;
	}

	//ɾ�������豸
	if (g_hControlDevice)
	{
		IoDeleteDevice( g_hControlDevice );
		g_hControlDevice = NULL;
	}
	KdPrint( ("_ProtocolUninstall: Done!\n") );
}


PROTOCOL_BIND_ADAPTER_EX	_BindAdapterHandlerEx;
/*
�ú�����ndis�ص�,������һ��NIC�豸(�����豸)
�ú���������¹���:
1.�������ڷ��ͺͽ����������ݵ�NET_BUFFER_LIST��
2.���ú���OpenAdapterEx���²�NIC�豸
3.����NdisIMInitializeDeviceInstanceEx����
  ����ʼ��С�˿�����,����MpInitialize�ص�
����:
ProtocolDriverContext	������ע��ndisЭ��ʱ�����Զ��������Ĳ���,������û��ʹ��
BindContext				The handle that identifies the NDIS context area for this bind operation.
pBindParameters			ָ����Ҫ�󶨵��������Ĳ�����ָ��
����ֵ:	����״̬
*/
NDIS_STATUS	_BindAdapterHandlerEx(
	IN	NDIS_HANDLE	_ProtocolDriverContext ,
	IN	NDIS_HANDLE	_BindContext ,
	IN	PNDIS_BIND_PARAMETERS	_pBindParameters
)
{
	NDIS_STATUS	NdisStatus;
	PADAPT	pAdapt;
	BOOLEAN	bNoCleanUpNeeded = FALSE;

	do
	{
		KdPrint( ("_BindAdapterHandlerEx: BoundAdpterName:%wZ\n" ,
			_pBindParameters->BoundAdapterName) );
		KdPrint( ("\tMacAddress:%x-%x-%x\n" ,
			_pBindParameters->CurrentMacAddress[0] ,
			_pBindParameters->CurrentMacAddress[4] ,
			_pBindParameters->CurrentMacAddress[8]) );

		ULONG	TotalSize =
			sizeof( ADAPT ) + _pBindParameters->BoundAdapterName->MaximumLength;
		//�����ڴ�ʱԤ���������洢�Լ���miniport���ƵĿռ�
		pAdapt =
			NdisAllocateMemoryWithTagPriority(
			g_ProtocolDriver ,
			TotalSize,
			POOL_TAG ,
			NormalPoolPriority );
		if (pAdapt == NULL)
		{
			NdisStatus = NDIS_STATUS_RESOURCES;
			break;
		}

		//��ʼ��ADAPT�ṹ��,ͬʱ����IM-miniport������
		//Ϊ֮���NdisIMInitializeDeviceInstance����
		NdisZeroMemory( pAdapt , TotalSize );
		pAdapt->IMAdapterName.MaximumLength = _pBindParameters->BoundAdapterName->MaximumLength;
		pAdapt->IMAdapterName.Length = _pBindParameters->BoundAdapterName->Length;
		pAdapt->IMAdapterName.Buffer = (PWCHAR)(pAdapt + sizeof( ADAPT ));
		NdisMoveMemory(
			pAdapt->IMAdapterName.Buffer ,
			_pBindParameters->BoundAdapterName->Buffer ,
			_pBindParameters->BoundAdapterName->MaximumLength );

		//��ʼ���������е��¼�����,֮�����
		NdisInitializeEvent( &pAdapt->Event );	//��ʼ����¼��Ϳ��Ա��ȴ�
		NdisAllocateSpinLock( &pAdapt->Lock );

		//���NET_BUFFER_LIST_POOL_PARAMETERS�ṹ��
		NET_BUFFER_LIST_POOL_PARAMETERS	NetBufferListPoolParameters;
		NetBufferListPoolParameters.Header.Type =
			NDIS_OBJECT_TYPE_DEFAULT;
		NetBufferListPoolParameters.Header.Revision =
			NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
		NetBufferListPoolParameters.Header.Size =
			sizeof( NetBufferListPoolParameters );

		NetBufferListPoolParameters.ProtocolId = NDIS_PROTOCOL_ID_DEFAULT;
		NetBufferListPoolParameters.fAllocateNetBuffer = TRUE;	//ÿ��listĬ���Դ�һ��Buffer
		NetBufferListPoolParameters.ContextSize = 0;
		NetBufferListPoolParameters.PoolTag = POOL_TAG;
		NetBufferListPoolParameters.DataSize = 0;

		//���䷢��NET_BUFFER_LIST��
		pAdapt->SendNetBufferListPoolHandle =
			NdisAllocateNetBufferListPool(
			g_ProtocolDriver ,
			&NetBufferListPoolParameters );
		if (pAdapt->SendNetBufferListPoolHandle == NULL)
		{
			KdPrint( ("_BindAdapterHandlerEx: Fail to allocate SendNetBufferListPool!\n") );
			NdisStatus = NDIS_STATUS_FAILURE;
			break;
		}

		//�������NET_BUFFER_LIST��
		pAdapt->RecvNetBufferListPoolHandle =
			NdisAllocateNetBufferListPool(
			g_ProtocolDriver ,
			&NetBufferListPoolParameters );
		if (pAdapt->RecvNetBufferListPoolHandle == NULL)
		{
			KdPrint( ("_BindAdapterHandlerEx: Fail to allocate RecvNetBufferListPOol!\n") );
			NdisStatus = NDIS_STATUS_FAILURE;
			break;
		}

		//׼������������������,����дOpenParameters
		//��дOpenParameters����
		NDIS_OPEN_PARAMETERS	OpenParameters;
		NDIS_MEDIUM  MediumArray[1] = { NdisMedium802_3 };
		UINT                     SelectedMediumIndex;
		NET_FRAME_TYPE           FrameTypeArray[2] = 
		{ NDIS_ETH_TYPE_802_1X, NDIS_ETH_TYPE_802_1Q };

		OpenParameters.Header.Type = NDIS_OBJECT_TYPE_OPEN_PARAMETERS;
		OpenParameters.Header.Revision = NDIS_OPEN_PARAMETERS_REVISION_1;
		OpenParameters.Header.Size = NDIS_SIZEOF_OPEN_PARAMETERS_REVISION_1;
		OpenParameters.AdapterName =
			_pBindParameters->AdapterName;
		OpenParameters.MediumArray = MediumArray;
		OpenParameters.MediumArraySize =
			sizeof( MediumArray ) / sizeof( NDIS_MEDIUM );
		OpenParameters.SelectedMediumIndex = &SelectedMediumIndex;
		OpenParameters.FrameTypeArray = FrameTypeArray;
		OpenParameters.FrameTypeArraySize = 
			sizeof( FrameTypeArray ) / sizeof( NET_FRAME_TYPE );

		//��
		NdisStatus = NdisOpenAdapterEx(
			g_ProtocolDriver ,
			pAdapt ,	//�ᴫ��OpenComplete��������
			&OpenParameters ,
			_BindContext ,
			&pAdapt->BindingHandle );
		//�ȴ��������
		if (NdisStatus == NDIS_STATUS_PENDING)
		{
			NdisWaitEvent( &pAdapt->Event , 0 );
			NdisStatus = pAdapt->Status;
		}
		//��ʧ�ܾ��˳�
		if (NdisStatus != NDIS_STATUS_SUCCESS)
		{
			KdPrint( ("_BindAdapterHandlerEx.NdisOpenAdapterEx fail,status=%x\n" ,
				NdisStatus) );
			break;
		}
		//�󶨳ɹ�,��һ����ȡ������һЩ����
		pAdapt->BindParameters = *_pBindParameters;

		//���Ӷ�Adapt�����ü���
		_ReferenceAdapt( pAdapt );

		/*
		����׼����NDis��ʼ�����ǵ�С�˿ڱ�Ե
		*/
		//����һ��С�˿ڻ�δ��ʼ���õı�ʶ
		pAdapt->MiniportInitPending = TRUE;
		//���õȴ��¼�
		NdisInitializeEvent( &pAdapt->MiniportInitEvent );

		//��������
		_ReferenceAdapt( pAdapt );

		//���ú�����ʼ��,�ú���ִ�й����л����MpInitialize
		NdisStatus =
			NdisIMInitializeDeviceInstanceEx(
			g_MiniportDriver ,
			&pAdapt->IMAdapterName ,
			pAdapt );//�ò����ᴫ��MpInitializeEx��������
		if (NdisStatus != NDIS_STATUS_SUCCESS)
		{
			if (pAdapt->MiniportIsHalted == TRUE)
				bNoCleanUpNeeded = TRUE;

			KdPrint( ("_BindAdapterHandlerEx.NdisIMInitializeDeviceInstanceEx fail,status=%x\n" ,
				NdisStatus) );

			//������
			if (_DerefenceAdapt( pAdapt ))
				pAdapt = NULL;

			break;
		}
		_DerefenceAdapt( pAdapt );

	} while (FALSE);

	//������Դ
	if ((NdisStatus != NDIS_STATUS_SUCCESS)
		&& (bNoCleanUpNeeded == FALSE))
	{
		if (pAdapt != NULL)
		{
			if (pAdapt->BindingHandle != NULL)
			{
				NDIS_STATUS	CloseStatus;

				//���֮ǰ�İ�
				NdisResetEvent( &pAdapt->Event );

				CloseStatus
					= NdisCloseAdapterEx( pAdapt->BindingHandle );
				pAdapt->BindingHandle = NULL;
				if (CloseStatus == NDIS_STATUS_PENDING)
				{
					NdisWaitEvent( &pAdapt->Event , 0 );
					CloseStatus = pAdapt->Status;
					ASSERT( CloseStatus == NDIS_STATUS_SUCCESS );
				}

				if (_DerefenceAdapt( pAdapt ))
					pAdapt = NULL;

			}//if (pAdapt->BindingHandle != NULL)

		}//if (pAdapt != NULL)

	}//if ((NdisStatus != NDIS_STATUS_SUCCESS)
	 //&& (bNoCleanUpNeeded == FALSE))
	return	NdisStatus;
}


/*
�ú�����NdisOpenAdapterEx������֮��ص�
�����������¼�
*/
VOID	_OpenAdapterComplete(
	IN	NDIS_HANDLE	_ProtocolBindingContext ,
	IN	NDIS_STATUS	_NdisStatus
)
{
	PADAPT	pAdapt =
		_ProtocolBindingContext;

	KdPrint( ("[_OpenAdapterComplete]\n") );

	pAdapt->Status = _NdisStatus;
	NdisSetEvent( &pAdapt->Event );
}


/*
�ú�����NdisCloseAdapter֮��ص�,���������¼�
*/
VOID	_CloseAdapterComplete(
	IN	NDIS_HANDLE	_ProtocolBindingContext
)
{
	PADAPT	pAdapt =
		_ProtocolBindingContext;

	KdPrint( ("[_CloseAdapterComplete]\n") );

	NdisSetEvent( &pAdapt->Event );
}


/*
�ú�����ndis�ص�,����Ҫ������һ�������������İ�ʱ
����:
UnbindContext	The handle that identifies the NDIS context area for this unbind operation
ProtocolBindingContext	���ǵ���NdisOpenAdapterExʱָ���Ĵ�������
����ֵ:	����״̬
*/
NDIS_STATUS	_UnbindAdapterHandlerEx(
	IN	NDIS_HANDLE	_UnbindContext ,
	IN	NDIS_HANDLE	_ProtocolBindingContext
)
{
	PADAPT	pAdapt = _ProtocolBindingContext;
	NDIS_STATUS	NdisStatus;

	KdPrint( ("[_UnbindAdapterHandlerEx]\n") );

	//���ñ�־λ��������Unbinding,���֮����request����᷵��ʧ��
	NdisAcquireSpinLock( &pAdapt->Lock );
	pAdapt->UnbindingInProcess = TRUE;
	if (pAdapt->QueuedRequest == TRUE)
	{
		pAdapt->QueuedRequest = FALSE;
		NdisReleaseSpinLock( &pAdapt->Lock );
		//�ֹ�����������ɺ���
		_OidRequestComplete(
			pAdapt ,
			&pAdapt->OidRequest ,
			NDIS_STATUS_FAILURE );
	}
	NdisReleaseSpinLock( &pAdapt->Lock );

	//������ǵ��ù�NdisIMInitializeDeviceInstanceEx
	//����ʼ�������ǵ�С�˿�����,���ҳ�ʼ�����ڽ�����
	//�ͻ���Ҫ����NdisIMCancelInitializeDeviceInstance
	if (pAdapt->MiniportInitPending == TRUE)
	{
		//Cancel it
		NdisStatus =
			NdisIMCancelInitializeDeviceInstance(
			g_MiniportDriver ,
			&pAdapt->IMAdapterName );
		if (NdisStatus == NDIS_STATUS_SUCCESS)
		{
			//ȡ����ʼ���ɹ�
			pAdapt->MiniportInitPending = FALSE;
			ASSERT( pAdapt->MiniportAdapterHandle == NULL );
		}
		else
		{
			//ȡ����ʼ��ʧ��,�Ǿͱ����С�˿ڳ�ʼ�����
			NdisWaitEvent( &pAdapt->MiniportInitEvent , 0 );
			ASSERT( pAdapt->MiniportInitPending == FALSE );
		}
	}

	//��ʼ��С�˿ڳɹ��Ļ�Ҫ����NdisIMDeInitializeDeviceInstance
	//�������ǵ�С�˿�����ж��
	if (pAdapt->MiniportAdapterHandle != NULL)
	{
		NdisStatus =
			NdisIMDeInitializeDeviceInstance(
			pAdapt->MiniportAdapterHandle );
		if (NdisStatus != NDIS_STATUS_SUCCESS)
			NdisStatus = NDIS_STATUS_FAILURE;
	}
	
	//������������Ҫ��closeAdapter�������ʵС�˿������İ�
	//���ͷ�Щ��Դ����
	if (pAdapt->BindingHandle != NULL)
	{
		NdisResetEvent( &pAdapt->Event );
		
		NdisStatus =
			NdisCloseAdapterEx( pAdapt->BindingHandle );
		if (NdisStatus == NDIS_STATUS_PENDING)
		{
			NdisWaitEvent( &pAdapt->Event ,0);
			NdisStatus = pAdapt->Status;
		}
		pAdapt->BindingHandle = NULL;
	}

	//�ͷ���Դ
	ASSERT( _DerefenceAdapt( pAdapt ) == TRUE );

	return	NDIS_STATUS_SUCCESS;
}


/*
����ʵС�˿����������һ����ĵ�ʱ��,�ú����ᱻndis�ص�
����:
ProtocolBindingHandle	ָ���Զ���Ĵ�������(pAdapt)
pNdisRequest			ָ����ɵ�����
Status					��ɵ�״̬
����ֵ:	��
*/
VOID	_OidRequestComplete(
	IN	NDIS_HANDLE	_ProtocolBindingContext ,
	IN	PNDIS_OID_REQUEST	_pNdisOidRequest ,
	IN	NDIS_STATUS	_NdisStatus
)
{
	PADAPT	pAdapt =
		_ProtocolBindingContext;
	NDIS_OID	Oid =
		pAdapt->OidRequest.DATA.SET_INFORMATION.Oid;

	KdPrint( ("[_OidRequestComplete]\n") );

	ASSERT( pAdapt->OutstandingRequests > 0 );

	NdisInterlockedDecrement( &pAdapt->OutstandingRequests );

	//����Ҫ��һ�����Request,ʹ֮���ظ��ϲ��Э������(tcp/ip)
	switch (_pNdisOidRequest->RequestType)
	{
		case NdisRequestQueryInformation:
		case NdisRequestQueryStatistics:
			//������Ǹ���ѯ���͵�����

			//�������ǴӲ������·�OID_PNP_QUERY_POWER����
			//�����������������͵Ļص�,�Ǿ��Ǵ�����
			ASSERT( Oid != OID_PNP_QUERY_POWER );

			//�����OID_PNP_CAPABILITIES,ndis�������ѯ��С�˿�
			//�����Ƿ�֧���ٻ��ѹ���,������Ҫ�޸Ľ��Ϊ��֧��
			if ((Oid == OID_PNP_CAPABILITIES)
				&& (_NdisStatus == NDIS_STATUS_SUCCESS))
			{
				PNDIS_PNP_CAPABILITIES	pPnpCapabilities;
				PNDIS_PM_WAKE_UP_CAPABILITIES	pWakeUpCapabilities;

				KdPrint( ("_OidRequestComplete: Oid == OID_PNP_CAPABILITIES\n\
				\tReset WakeUpCapabilities") );

				if (_pNdisOidRequest->DATA.QUERY_INFORMATION.InformationBufferLength
					>= sizeof( NDIS_PNP_CAPABILITIES ))
				{
					pPnpCapabilities =
						(PNDIS_PNP_CAPABILITIES)
						_pNdisOidRequest->DATA.QUERY_INFORMATION.InformationBuffer;
					//��д���б��
					pWakeUpCapabilities = &pPnpCapabilities->WakeUpCapabilities;
					pWakeUpCapabilities->MinMagicPacketWakeUp = NdisDeviceStateUnspecified;
					pWakeUpCapabilities->MinLinkChangeWakeUp = NdisDeviceStateUnspecified;
					pWakeUpCapabilities->MinPatternWakeUp = NdisDeviceStateUnspecified;
					pAdapt->BytesReadOrWritten = sizeof( NDIS_PNP_CAPABILITIES );
					pAdapt->BytesNeeded = 0;

					//�������ǵ��豸״̬Ϊ����
					pAdapt->MPDeviceState = NdisDeviceStateD0;
					pAdapt->PTDeviceState = NdisDeviceStateD0;

					_NdisStatus = NDIS_STATUS_SUCCESS;
				}
				else
				{
					pAdapt->BytesNeeded = sizeof( NDIS_PNP_CAPABILITIES );
					_NdisStatus = NDIS_STATUS_RESOURCES;
				}
			} // if Oid == OID_PNP_CAPABILITIES

			pAdapt->BytesReadOrWritten =
				_pNdisOidRequest->DATA.QUERY_INFORMATION.BytesWritten;
			pAdapt->BytesNeeded =
				_pNdisOidRequest->DATA.QUERY_INFORMATION.BytesNeeded;

			break;

		case NdisRequestSetInformation:

			ASSERT( Oid != OID_PNP_SET_POWER );

			pAdapt->BytesReadOrWritten =
				_pNdisOidRequest->DATA.SET_INFORMATION.BytesRead;
			pAdapt->BytesNeeded =
				_pNdisOidRequest->DATA.SET_INFORMATION.BytesNeeded;

			break;

		default:
			ASSERT( FALSE );
	}
	//���������
	NdisMOidRequestComplete(
		pAdapt->MiniportAdapterHandle ,
		_pNdisOidRequest ,
		_NdisStatus );
}


/*
�ú����ڵײ���ʵС�˿�״̬�����仯ʱ�ص�
�����������м������,���Ի�Ҫ��״̬�������ݸ��ϲ���ʵЭ������(tcp/ip.sys)
����:
ProtocolBindingContext		��������(����������������)
StatusIndication			����״̬��Ϣ��ָ��
����ֵ:��
*/
VOID	_StatusHandlerEx(
	IN	NDIS_HANDLE	_ProtocolBindingContext ,
	IN	PNDIS_STATUS_INDICATION	_pStatusIndication
)
{
	PADAPT	pAdapt = _ProtocolBindingContext;
	NDIS_STATUS	GeneralStatus = 
		_pStatusIndication->StatusCode;

	KdPrint( ("[_StatusHandlerEx]\n") );

	//������ǵ�С�˿��Ѿ���ʼ���ò���������״̬
	//�ͼ����ϴ�StatusIndication
	if ((pAdapt->MiniportAdapterHandle != NULL)
		&& (pAdapt->MPDeviceState == NdisDeviceStateD0)
		&& (pAdapt->PTDeviceState == NdisDeviceStateD0))
	{
		//��ý�������״̬�������ṹ��
		if ((GeneralStatus == NDIS_STATUS_MEDIA_CONNECT)
			|| (GeneralStatus == NDIS_STATUS_MEDIA_DISCONNECT))
		{
			pAdapt->LastIndicatedMediaStatus =
				GeneralStatus;
		}

		NdisMIndicateStatusEx(
			pAdapt->MiniportAdapterHandle ,
			_pStatusIndication );
	}
	else if((pAdapt->MiniportAdapterHandle!=NULL)
		&& ((GeneralStatus == NDIS_STATUS_MEDIA_CONNECT)
			|| (GeneralStatus == NDIS_STATUS_MEDIA_DISCONNECT)))
	{
		pAdapt->LatestUnIndicateMediaStatus = GeneralStatus;
	}
}


/*
Ndisͨ���˺���֪ͨ����pnp�¼����ڵײ�С�˿ڵı仯
����:
ProtocolBindingContext	ָ���������
pNetPnPEventNotification	ָ��֪ͨ�¼�
����ֵ:	����״̬
*/
NDIS_STATUS	_PnpEventHandler(
	IN	NDIS_HANDLE	_ProtocolBindingContext ,
	IN	PNET_PNP_EVENT_NOTIFICATION	_pNetPnpEventNotification
)
{
	PADAPT	pAdapt = _ProtocolBindingContext;
	NDIS_STATUS	NdisStatus;
	NDIS_EVENT	PauseEvent;

	KdPrint( ("[_PnpEventHandler]\n") );

	switch (_pNetPnpEventNotification->NetPnPEvent.NetEvent)
	{
		case NetEventSetPower:
			NdisStatus =
				_PnPNetEventSetPower(
				pAdapt ,
				_pNetPnpEventNotification );
			break;

		case NetEventReconfigure:
		case NetEventIMReEnableDevice:
			NdisStatus = NDIS_STATUS_SUCCESS;
			break;

		case NetEventPause:
			NdisAcquireSpinLock( &pAdapt->Lock );
			pAdapt->BindingState = AdapterBindingPausing;

			ASSERT( pAdapt->pPauseEvent == NULL );

			if (pAdapt->OutstandingSends != 0)
			{
				NdisInitializeEvent( &PauseEvent );
				pAdapt->pPauseEvent = &PauseEvent;
				NdisReleaseSpinLock( &pAdapt->Lock );

				NdisWaitEvent( &PauseEvent , 0 );
				NdisAcquireSpinLock( &pAdapt->Lock );
			}

			pAdapt->BindingState = AdapterBindingPaused;
			NdisReleaseSpinLock( &pAdapt->Lock );

			NdisStatus = NDIS_STATUS_SUCCESS;
			break;

		case NetEventRestart:
			pAdapt->BindingState = AdapterBindingRunning;
			NdisStatus = NDIS_STATUS_SUCCESS;
			break;

		default:
			NdisStatus = NDIS_STATUS_SUCCESS;
			break;
	}

	return	NdisStatus;
}


/*
�ú����������ǵ�Э�������յ����ڵ�Դ��pnp�¼�
��������˵͵�Դ״̬,��Ҫ�ڴ˵ȵ����еķ��ͺ��������
����:
pAdapt	������������
pNetPnpEventNotification	pnp�¼�
*/
NDIS_STATUS	_PnPNetEventSetPower(
	IN	PADAPT	_pAdapt ,
	IN	PNET_PNP_EVENT_NOTIFICATION	_pNetPnPEventNotification
)
{
	NDIS_STATUS	NdisStatus;
	NDIS_DEVICE_POWER_STATE	PrevDeviceState =
		_pAdapt->PTDeviceState;

	KdPrint( ("_PnPNetEventSetPower: Adapt:%p,set power to %d\n" ,
		_pAdapt , _pNetPnPEventNotification->NetPnPEvent.Buffer) );

	//�����Դ״̬
	NdisAcquireSpinLock( &_pAdapt->Lock );
	_pAdapt->PTDeviceState =
		*(PNDIS_DEVICE_POWER_STATE)
		_pNetPnPEventNotification->NetPnPEvent.Buffer;

	//����Ƿ�����˵͵�Դ״̬
	if (_pAdapt->PTDeviceState > NdisDeviceStateD0)
	{
		//���֮ǰ״̬��D0,�����StandBy�׶�,�⽫��ֹ����Ľ���,ֱ����Դ��������
		if (PrevDeviceState == NdisDeviceStateD0)
			_pAdapt->StandingBy = TRUE;

		NdisReleaseSpinLock( &_pAdapt->Lock );

		//�ȴ����ͺ������ȫ�����
		while (_pAdapt->OutstandingSends != 0)
		{
			NdisMSleep( 1000 );
		}

		while (_pAdapt->OutstandingRequests != 0)
		{
			NdisMSleep( 1000 );
		}
		
		//������ڵ�ǰ��������ʧ�ܵķ�ʽ���
		if (_pAdapt->QueuedRequest)
		{
			NdisAcquireSpinLock( &_pAdapt->Lock );
			_pAdapt->QueuedRequest = FALSE;
			NdisReleaseSpinLock( &_pAdapt->Lock );

			_OidRequestComplete(
				_pAdapt ,
				&_pAdapt->OidRequest ,
				NDIS_STATUS_FAILURE );
			ASSERT( _pAdapt->OutstandingRequests == FALSE );
		}

		ASSERT( _pAdapt->OutstandingRequests == 0 );
	}// if (_pAdapt->PTDeviceState > NdisDeviceStateD0)
	else
	{
		//�������С�˿ڵ�Դ������
		if (PrevDeviceState > NdisDeviceStateD0)
			_pAdapt->StandingBy = FALSE;

		//�²�����С�˿��豸�Ѿ�����ʹ����
		//���������Ϳ����·�
		if (_pAdapt->QueuedRequest == TRUE)
		{
			_pAdapt->QueuedRequest = FALSE;

			_pAdapt->OutstandingRequests++;
			NdisReleaseSpinLock( &_pAdapt->Lock );

			NdisStatus = NdisOidRequest(
				_pAdapt->BindingHandle ,
				&_pAdapt->OidRequest );
			if (NdisStatus != NDIS_STATUS_PENDING)
			{
				_OidRequestComplete(
					_pAdapt ,
					&_pAdapt->OidRequest ,
					NdisStatus );
			}
		}
		else
		{
			NdisReleaseSpinLock( &_pAdapt->Lock );
		}
	}
	
	return	NdisStatus;
}


/*
�ú����ڵײ���ʵС�˿ڷ��������ݰ�֮��ص�
Ҫ�����лָ�NetBufferList��source handle
����:
ProtocolBindingContext	��������
NetBufferList	������ɵ����ݰ�
SendCompleteFlags	������ɱ�־λ
����ֵ:��
*/
VOID	_SendComplete(
	IN	NDIS_HANDLE	_ProtocolBindingContext ,
	IN	PNET_BUFFER_LIST	_pNetBufferLists ,
	IN	ULONG	_SendCompleteFlags
)
{
	PADAPT	pAdapt = _ProtocolBindingContext;
	NDIS_STATUS	NdisStatus;
	PIM_NBLC	pSendContext;
	PNET_BUFFER_LIST	pCurrentNetBufferList;

	KdPrint( ("[_SendComplete]\n") );

	while (_pNetBufferLists)
	{
		pCurrentNetBufferList = _pNetBufferLists;
		_pNetBufferLists =
			NET_BUFFER_LIST_NEXT_NBL
			( _pNetBufferLists );
		NET_BUFFER_LIST_NEXT_NBL( pCurrentNetBufferList ) = NULL;

		pSendContext =
			(PIM_NBLC)
			NET_BUFFER_LIST_CONTEXT_DATA_START( pCurrentNetBufferList );

		pAdapt = pSendContext->pAdapt;

		//�ָ�source handle
		pCurrentNetBufferList->SourceHandle =
			pSendContext->PreviousSourceHandle;

		NdisStatus =
			NET_BUFFER_LIST_STATUS( pCurrentNetBufferList );
		//�ͷ��Զ���Context
		NdisFreeNetBufferListContext(
			pCurrentNetBufferList , sizeof( IM_NBLC ) );

		//��ð��С�˿��������ϲ�Э�������ݽ����
		NdisMSendNetBufferListsComplete(
			pAdapt->MiniportAdapterHandle ,
			pCurrentNetBufferList ,
			_SendCompleteFlags );

		NdisAcquireSpinLock( &pAdapt->Lock );
		pAdapt->OutstandingSends--;

		if ((pAdapt->OutstandingSends == 0)
			&& (pAdapt->pPauseEvent != NULL))
		{
			NdisSetEvent( pAdapt->pPauseEvent );
			pAdapt->pPauseEvent = NULL;
		}
		NdisReleaseSpinLock( &pAdapt->Lock );
	}
}


/*
�ú�������������ݰ�
����:
ProtocolBindingContext     Pointer to our PADAPT structure
NetBufferLists             Net Buffer Lists received
PortNumber                 Port on which NBLS were received
NumberOfNetBufferLists     Number of Net Buffer Lists
ReceiveFlags               Flags associated with the receive
����ֵ:��
*/
VOID	_ReceiveNetBufferList(
	IN	NDIS_HANDLE	_ProtocolBindingContext ,
	IN	PNET_BUFFER_LIST	_pNetBufferLists ,
	IN	NDIS_PORT_NUMBER	_PortNumber ,
	IN	ULONG	_NumberOfNetBufferLists ,
	IN	ULONG	_ReceiveFlags
)
{
	PADAPT	pAdapt = _ProtocolBindingContext;
	PNET_BUFFER_LIST	pCurrentNetBufferList = NULL;
	PNET_BUFFER_LIST	pReturnNetBufferList = NULL;
	PNET_BUFFER_LIST	pLastReturnNetBufferList = NULL;
	ULONG	ReturnFlags = 0;
	BOOLEAN	bReturnNbl;

	KdPrint( ("[_ReceiveNetBufferList]\n") );

	if (NDIS_TEST_RECEIVE_AT_DISPATCH_LEVEL( _ReceiveFlags ))
	{
		NDIS_SET_RETURN_FLAG( ReturnFlags , NDIS_RETURN_FLAGS_DISPATCH_LEVEL );
	}

	ASSERT( _pNetBufferLists != NULL );

	//�������С�˿ڲ���û�г�ʼ����,�����豸�ǵ͵�Դ״̬
	if ((pAdapt->MiniportAdapterHandle = NULL)
		|| (pAdapt->MPDeviceState > NdisDeviceStateD0))
	{
		if (NDIS_TEST_RECEIVE_CAN_PEND
		( _ReceiveFlags ) == TRUE)
		{
			NdisReturnNetBufferLists(
				pAdapt->BindingHandle ,
				_pNetBufferLists ,
				ReturnFlags );
		}

		return;
	}
/*
	while (_pNetBufferLists != NULL)
	{
		pCurrentNetBufferList = _pNetBufferLists;
		_pNetBufferLists =
			NET_BUFFER_LIST_NEXT_NBL( _pNetBufferLists );
		NET_BUFFER_LIST_NEXT_NBL( pCurrentNetBufferList ) = NULL;

		bReturnNbl = TRUE;

		
	}*/

	//ð��С�˿����ϲ�Э�����������յ������ݰ�
	NdisMIndicateReceiveNetBufferLists(
		pAdapt->MiniportAdapterHandle ,
		_pNetBufferLists ,
		_PortNumber ,
		_NumberOfNetBufferLists ,
		_ReceiveFlags );
}


/*
�ú����������Ҫ�����Oid����,��Ϊ�Ӻ�����RequestComplete����
*/
VOID	_CompleteForwardedRequest(
	IN	PADAPT	_pAdapt ,
	IN	NDIS_STATUS	_NdisStatus
)
{
	PNDIS_OID_REQUEST	pOidRequest =
		&_pAdapt->OidRequest;
	NDIS_OID	Oid;
	PNDIS_OID_REQUEST	pOldOidRequest = NULL;
	BOOLEAN	bCompleteRequest = FALSE;

	KdPrint( ("[_CompleteForwardedRequest]\n") );

	NdisAcquireSpinLock( &_pAdapt->Lock );
	_pAdapt->RequestRefCount--;
	if (_pAdapt->RequestRefCount == 0)
	{
		bCompleteRequest = TRUE;
		pOldOidRequest = _pAdapt->pOldOidRequest;
		_pAdapt->pOldOidRequest = NULL;
	}
	NdisReleaseSpinLock( &_pAdapt->Lock );

	if (bCompleteRequest == FALSE)
		return;

	//
	//	��ɾɵ�����
	//
	Oid = pOidRequest->DATA.QUERY_INFORMATION.Oid;
	switch (pOidRequest->RequestType)
	{
		case NdisRequestQueryInformation:
		case NdisRequestQueryStatistics:
			//������Ǹ���ѯ���͵�����

			pOldOidRequest->DATA.QUERY_INFORMATION.BytesWritten =
				pOidRequest->DATA.QUERY_INFORMATION.BytesWritten;
			pOldOidRequest->DATA.QUERY_INFORMATION.BytesNeeded =
				pOidRequest->DATA.QUERY_INFORMATION.BytesNeeded;

			//�����OID_PNP_CAPABILITIES,ndis�������ѯ��С�˿�
			//�����Ƿ�֧���ٻ��ѹ���,������Ҫ�޸Ľ��Ϊ��֧��
			if ((Oid == OID_PNP_CAPABILITIES)
				&& (_NdisStatus == NDIS_STATUS_SUCCESS))
			{
				PNDIS_PNP_CAPABILITIES	pPnpCapabilities;
				PNDIS_PM_WAKE_UP_CAPABILITIES	pWakeUpCapabilities;

				KdPrint( ("_OidRequestComplete: Oid == OID_PNP_CAPABILITIES\n\
				\tReset WakeUpCapabilities") );

				if (pOidRequest->DATA.QUERY_INFORMATION.InformationBufferLength
					>= sizeof( NDIS_PNP_CAPABILITIES ))
				{
					pPnpCapabilities =
						(PNDIS_PNP_CAPABILITIES)
						pOidRequest->DATA.QUERY_INFORMATION.InformationBuffer;
					//��д���б��
					pWakeUpCapabilities = &pPnpCapabilities->WakeUpCapabilities;
					pWakeUpCapabilities->MinMagicPacketWakeUp = NdisDeviceStateUnspecified;
					pWakeUpCapabilities->MinLinkChangeWakeUp = NdisDeviceStateUnspecified;
					pWakeUpCapabilities->MinPatternWakeUp = NdisDeviceStateUnspecified;
					_pAdapt->BytesReadOrWritten = sizeof( NDIS_PNP_CAPABILITIES );
					_pAdapt->BytesNeeded = 0;

					//�������ǵ��豸״̬Ϊ����
					_pAdapt->MPDeviceState = NdisDeviceStateD0;
					_pAdapt->PTDeviceState = NdisDeviceStateD0;

					_NdisStatus = NDIS_STATUS_SUCCESS;
				}
				else
				{
					_pAdapt->BytesNeeded = sizeof( NDIS_PNP_CAPABILITIES );
					_NdisStatus = NDIS_STATUS_RESOURCES;
				}
			} // if Oid == OID_PNP_CAPABILITIES

			_pAdapt->BytesReadOrWritten =
				pOidRequest->DATA.QUERY_INFORMATION.BytesWritten;
			_pAdapt->BytesNeeded =
				pOidRequest->DATA.QUERY_INFORMATION.BytesNeeded;

			break;

		case NdisRequestSetInformation:

			ASSERT( Oid != OID_PNP_SET_POWER );

			pOldOidRequest->DATA.SET_INFORMATION.BytesRead =
				pOidRequest->DATA.SET_INFORMATION.BytesRead;
			pOldOidRequest->DATA.QUERY_INFORMATION.BytesNeeded =
				pOidRequest->DATA.SET_INFORMATION.BytesNeeded;

			break;

		default:
			ASSERT( FALSE );
	}
	//���������
	NdisMOidRequestComplete(
		_pAdapt->MiniportAdapterHandle ,
		pOldOidRequest ,
		_NdisStatus );
}


NDIS_STATUS	_SetOptionHandler(
	IN	NDIS_HANDLE	_DriverHandler ,
	IN	NDIS_HANDLE	_DriverContext
)
{
	return	NDIS_STATUS_SUCCESS;
}