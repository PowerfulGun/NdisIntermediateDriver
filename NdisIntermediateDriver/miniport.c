#include	"shared_head.h"


extern	NDIS_SPIN_LOCK	g_SpinLock;
extern	PADAPT	g_pAdaptList;
extern	NDIS_HANDLE	g_MiniportDriver;


//֧�ֵ�OID����
NDIS_OID IMSupportedOids[] =
{
	OID_GEN_SUPPORTED_LIST,
	OID_GEN_HARDWARE_STATUS,
	OID_GEN_MEDIA_SUPPORTED,
	OID_GEN_MEDIA_IN_USE,
	OID_GEN_MAXIMUM_LOOKAHEAD,
	OID_GEN_MAXIMUM_FRAME_SIZE,
	OID_GEN_LINK_SPEED,
	OID_GEN_TRANSMIT_BUFFER_SPACE,
	OID_GEN_RECEIVE_BUFFER_SPACE,
	OID_GEN_TRANSMIT_BLOCK_SIZE,
	OID_GEN_RECEIVE_BLOCK_SIZE,
	OID_GEN_VENDOR_ID,
	OID_GEN_VENDOR_DESCRIPTION,
	OID_GEN_VENDOR_DRIVER_VERSION,
	OID_GEN_CURRENT_PACKET_FILTER,
	OID_GEN_CURRENT_LOOKAHEAD,
	OID_GEN_DRIVER_VERSION,
	OID_GEN_MAXIMUM_TOTAL_SIZE,
	OID_GEN_PROTOCOL_OPTIONS,
	OID_GEN_MAC_OPTIONS,
	OID_GEN_MEDIA_CONNECT_STATUS,
	OID_GEN_MAXIMUM_SEND_PACKETS,
	OID_GEN_XMIT_OK,
	OID_GEN_RCV_OK,
	OID_GEN_XMIT_ERROR,
	OID_GEN_RCV_ERROR,
	OID_GEN_RCV_NO_BUFFER,
	OID_GEN_RCV_CRC_ERROR,
	OID_GEN_TRANSMIT_QUEUE_LENGTH,
	OID_GEN_STATISTICS,
	OID_802_3_PERMANENT_ADDRESS,
	OID_802_3_CURRENT_ADDRESS,
	OID_802_3_MULTICAST_LIST,
	OID_802_3_MAXIMUM_LIST_SIZE,
	OID_802_3_RCV_ERROR_ALIGNMENT,
	OID_802_3_XMIT_ONE_COLLISION,
	OID_802_3_XMIT_MORE_COLLISIONS,
	OID_802_3_XMIT_DEFERRED,
	OID_802_3_XMIT_MAX_COLLISIONS,
	OID_802_3_RCV_OVERRUN,
	OID_802_3_XMIT_UNDERRUN,
	OID_802_3_XMIT_HEARTBEAT_FAILURE,
	OID_802_3_XMIT_TIMES_CRS_LOST,
	OID_802_3_XMIT_LATE_COLLISIONS,
	OID_PNP_CAPABILITIES,
	OID_PNP_SET_POWER,
	OID_PNP_QUERY_POWER,
	OID_PNP_ADD_WAKE_UP_PATTERN,
	OID_PNP_REMOVE_WAKE_UP_PATTERN,
#if IEEE_VLAN_SUPPORT
	OID_GEN_VLAN_ID,
#endif    
	OID_PNP_ENABLE_WAKE_UP
};



MINIPORT_UNLOAD _MiniportUnload;
/*
�ú���ж��С�˿�����
*/
void	_MiniportUnload(
	IN	PDRIVER_OBJECT	_pDriverObject
)
{
	KdPrint( ("_MiniportUnload: Entered!\n") );

	_ProtocolUninstall();

	NdisMDeregisterMiniportDriver( g_MiniportDriver );

	NdisFreeSpinLock( &g_SpinLock );

	KdPrint( ("_MiniportUnload: Done!\n") );
}


MINIPORT_INITIALIZE	_MiniportInitializeEx;
/*
�ú�����ʼ����������С�˿�,�� NdisIMInitializeDeviceInstanceEx
�оͻ����
����:
NdisMiniportHandle		NDISΪ����ָ����HANDLE
MiniportDriverContext	�ڵ���NdisMRegisterMiniportDriverʱ������Զ���Ĳ���
MiniportInitParameters	��ʼ���õ��Ĳ���
����ֵ:	����״̬
*/
NDIS_STATUS	_MiniportInitializeEx(
	IN	NDIS_HANDLE	_MiniportAdapterHandle ,
	IN	NDIS_HANDLE	_pMiniportDriverContext ,
	IN	PNDIS_MINIPORT_INIT_PARAMETERS _pMiniportInitParameters
)
{
	PADAPT	pAdapt;
	NDIS_STATUS	NdisStatus = NDIS_STATUS_SUCCESS;
	NDIS_MINIPORT_ADAPTER_ATTRIBUTES	MPAttri;

	do
	{
		//���PADAPT�ṹ��
		pAdapt = _pMiniportInitParameters->IMDeviceInstanceContext;
		pAdapt->MiniportIsHalted = FALSE;

		//������������registration attributes
		NdisZeroMemory(
			&MPAttri ,
			sizeof( NDIS_MINIPORT_ADAPTER_ATTRIBUTES ) );
		MPAttri.RegistrationAttributes.Header.Type =
			NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
		MPAttri.RegistrationAttributes.Header.Revision=
			NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
		MPAttri.RegistrationAttributes.Header.Size=
			sizeof( NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES );
		//������������
		MPAttri.RegistrationAttributes.MiniportAdapterContext = pAdapt;
		//����ϵͳ�������ߺ󲻻����MpHalt����
		MPAttri.RegistrationAttributes.AttributeFlags =
			NDIS_MINIPORT_ATTRIBUTES_NO_HALT_ON_SUSPEND;

		MPAttri.RegistrationAttributes.CheckForHangTimeInSeconds = 0;
		MPAttri.RegistrationAttributes.InterfaceType = 0;

		NdisStatus = NdisMSetMiniportAttributes(
			_MiniportAdapterHandle , &MPAttri );
		if (NdisStatus != NDIS_STATUS_SUCCESS)
		{
			KdPrint( ("_MiniportInitializeEx.NdisMSetMiniportAttributes: Fail to set register attributes,status=%x\n" ,
				NdisStatus) );
			break;
		}

		//������������GeneralAttribute,����ģ��С�˿��������Ĳ���
		NdisZeroMemory(
			&MPAttri ,
			sizeof( NDIS_MINIPORT_ADAPTER_ATTRIBUTES ) );
		MPAttri.GeneralAttributes.Header.Type=
			NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES;
		MPAttri.GeneralAttributes.Header.Revision=
			NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_1;
		MPAttri.GeneralAttributes.Header.Size=
			sizeof( NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES );

		MPAttri.GeneralAttributes.MediaType = NdisMedium802_3;
		MPAttri.GeneralAttributes.MtuSize = pAdapt->BindParameters.MtuSize;
		MPAttri.GeneralAttributes.MaxXmitLinkSpeed = pAdapt->BindParameters.MaxXmitLinkSpeed;
		MPAttri.GeneralAttributes.MaxRcvLinkSpeed = pAdapt->BindParameters.MaxRcvLinkSpeed;
		MPAttri.GeneralAttributes.XmitLinkSpeed = pAdapt->BindParameters.XmitLinkSpeed;
		MPAttri.GeneralAttributes.RcvLinkSpeed = pAdapt->BindParameters.RcvLinkSpeed;
		MPAttri.GeneralAttributes.LookaheadSize = pAdapt->BindParameters.LookaheadSize;
		MPAttri.GeneralAttributes.MaxMulticastListSize = pAdapt->BindParameters.MaxMulticastListSize;
		MPAttri.GeneralAttributes.MacAddressLength = pAdapt->BindParameters.MacAddressLength;
		MPAttri.GeneralAttributes.PhysicalMediumType = pAdapt->BindParameters.PhysicalMediumType;
		MPAttri.GeneralAttributes.AccessType = pAdapt->BindParameters.AccessType;
		MPAttri.GeneralAttributes.DirectionType = pAdapt->BindParameters.DirectionType;
		MPAttri.GeneralAttributes.ConnectionType = pAdapt->BindParameters.ConnectionType;
		MPAttri.GeneralAttributes.IfType = pAdapt->BindParameters.IfType;
		/*
		A Boolean value that indicates if a connector is present. 
		NDIS sets this value to TRUE if there is a physical adapter.
		����û������������,�������ø�ֵΪFALSE
		*/
		MPAttri.GeneralAttributes.IfConnectorPresent = FALSE;
		MPAttri.GeneralAttributes.RecvScaleCapabilities =
			(pAdapt->BindParameters.RcvScaleCapabilities) ?
			pAdapt->BindParameters.RcvScaleCapabilities : NULL;
		MPAttri.GeneralAttributes.MacOptions = NDIS_MAC_OPTION_NO_LOOPBACK;
		MPAttri.GeneralAttributes.SupportedPacketFilters = pAdapt->BindParameters.SupportedPacketFilters;
		MPAttri.GeneralAttributes.SupportedStatistics =
			NDIS_STATISTICS_XMIT_OK_SUPPORTED |
			NDIS_STATISTICS_RCV_OK_SUPPORTED |
			NDIS_STATISTICS_XMIT_ERROR_SUPPORTED |
			NDIS_STATISTICS_RCV_ERROR_SUPPORTED |
			NDIS_STATISTICS_RCV_CRC_ERROR_SUPPORTED |
			NDIS_STATISTICS_RCV_NO_BUFFER_SUPPORTED |
			NDIS_STATISTICS_TRANSMIT_QUEUE_LENGTH_SUPPORTED |
			NDIS_STATISTICS_GEN_STATISTICS_SUPPORTED;
		//����С�˿�����������������
		NdisMoveMemory(
			&MPAttri.GeneralAttributes.CurrentMacAddress ,
			&pAdapt->BindParameters.CurrentMacAddress ,
			ETH_LENGTH_OF_ADDRESS);
		NdisMoveMemory(
			&MPAttri.GeneralAttributes.PermanentMacAddress ,
			&pAdapt->BindParameters.CurrentMacAddress ,
			ETH_LENGTH_OF_ADDRESS );

		MPAttri.GeneralAttributes.PowerManagementCapabilities =
			pAdapt->BindParameters.PowerManagementCapabilities;
		MPAttri.GeneralAttributes.SupportedOidList = IMSupportedOids;
		MPAttri.GeneralAttributes.SupportedOidListLength = sizeof( IMSupportedOids );

		NdisStatus = NdisMSetMiniportAttributes(
			g_MiniportDriver , &MPAttri );
		if (NdisStatus != NDIS_STATUS_SUCCESS)
		{
			KdPrint( ("_MiniportInitializeEx.NdisMSetMiniportAttributes: Fail to set general attributes,status=%x\n" ,
				NdisStatus) );
			break;
		}

		pAdapt->MiniportAdapterHandle = _MiniportAdapterHandle;
		//ָ�����һ��״̬
		pAdapt->LastIndicatedMediaStatus = NDIS_STATUS_MEDIA_CONNECT;
		//�����Լ��ĺ���ʵ�󶨵�С�˿ڵ�Դ״̬��Ϊon
		pAdapt->MPDeviceState = NdisDeviceStateD0;
		pAdapt->PTDeviceState = NdisDeviceStateD0;

		//����������������ķ���ȫ��������������
		//����Ҫʹ��������ͬ��
		NdisAcquireSpinLock( &g_SpinLock );
		pAdapt->Next = g_pAdaptList;
		g_pAdaptList = pAdapt;
		NdisReleaseSpinLock( &g_SpinLock );

		//�����������Ŀ����豸
		if (
			_RegisterDevice() != NDIS_STATUS_SUCCESS
			)
			KdPrint( ("_MiniportInitializeEx: Fail to create CDO!\n") );
		
	} while (FALSE);

	ASSERT( pAdapt->MiniportInitPending == TRUE );
	pAdapt->MiniportInitPending = FALSE;//��ʼ�����
	NdisSetEvent( &pAdapt->MiniportInitEvent );

	//�������ɹ�����Ҫ���Ӷ������ĵ����ü���
	if (NdisStatus == NDIS_STATUS_SUCCESS)
		_ReferenceAdapt( pAdapt );

	return	NdisStatus;
}


/*
�ú�����������С�˿��豸��ֹͣ����
�ú���������������NdisIMDeInitializeDeviceInstance�󱻵���
����Ҫ��������
����:
MiniportAdapterContext	������������ָ��
HaltAction		�豸ֹͣ��ԭ��
����ֵ:��
*/
VOID	_MiniportHaltEx(
	IN	NDIS_HANDLE		_MiniportAdapterContext ,
	IN	NDIS_HALT_ACTION	_HaltAction
)
{
	PADAPT	pAdapt = _MiniportAdapterContext;
	PADAPT	pTemp = g_pAdaptList;
	NDIS_STATUS	NdisStatus;

	KdPrint( ("[_MiniportHaltEx]\n") );

	pAdapt->MiniportAdapterHandle = NULL;
	pAdapt->MiniportIsHalted = TRUE;

	//����������������Ľڵ��ȫ�ֵ������ж���
	if (pAdapt != pTemp)
	{//����ýڵ��ڵ�һ���Ͳ��ö�����
		NdisAcquireSpinLock( &g_SpinLock );
		do
		{
			if (pTemp->Next == pAdapt)
			{
				pTemp->Next = pAdapt->Next;
				break;
			}

			pTemp = pTemp->Next;

		} while (pTemp != NULL);

		NdisReleaseSpinLock( &g_SpinLock );
	}
	
	//ע�������豸
	_DeregisterDevice();

	//������ǵ������ɹ��󶨹�С�˿�,��Ҫ��������İ�
	if (pAdapt->BindingHandle != NULL)
	{
		NdisResetEvent( &pAdapt->Event );

		NdisStatus =
			NdisCloseAdapterEx( pAdapt->BindingHandle );
		if (NdisStatus == NDIS_STATUS_PENDING)
		{
			NdisWaitEvent( &pAdapt->Event , 0 );
			NdisStatus = pAdapt->Status;
		}
		ASSERT( NdisStatus == NDIS_STATUS_SUCCESS );
		pAdapt->BindingHandle = NULL;

		_DerefenceAdapt( pAdapt );
	}

	if (_DerefenceAdapt( pAdapt ))
		pAdapt = NULL;
}


/*
�ú�����ϵͳshutdownʱ������
����:
MiniportAdapterContext		������������
ShutdownAction		shutdownԭ��
����ֵ:wu
*/
VOID	_MiniportShutdownEx(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	NDIS_SHUTDOWN_ACTION	_ShutdownAction
)
{
	KdPrint( ("[_MiniportShutdownEx]\n") );

	return;
}


/*
��ndis֪ͨ���ǵ�С�˿��豸pnp�¼�ʱ����ô˺���
*/
VOID	_MiniportDevicePnPEventNotify(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	PNET_DEVICE_PNP_EVENT	_pNetDevicePnPEvent
)
{
	KdPrint( ("[_MiniportDevicePnPEventNotofy]\n") );

	return;
}


/*
�ú����������ǵ�С�˿��豸�յ���OID����
����:
MiniportAdapterContext		������������
pNdisRequest		����ָ��
����ֵ:	��
*/
NDIS_STATUS	_MiniportOidRequestHandler(
	IN	NDIS_HANDLE		_MiniportAdapterContext ,
	IN	PNDIS_OID_REQUEST	_pNdisOidRequest
)
{
	NDIS_STATUS	NdisStatus;
	PADAPT	pAdapt = _MiniportAdapterContext;
	NDIS_REQUEST_TYPE	RequestType;

	KdPrint( ("[_MiniportOidRequestHandler]\n") );

	RequestType = _pNdisOidRequest->RequestType;

	switch (RequestType)
	{
		case NdisRequestMethod:
			NdisStatus = NDIS_STATUS_NOT_SUPPORTED;
			break;

		case NdisRequestSetInformation:
			NdisStatus =
				_RequestSetInformation(
				pAdapt , _pNdisOidRequest );
				break;

		case NdisRequestQueryInformation:
		case NdisRequestQueryStatistics:
			NdisStatus =
				_RequestQueryInformation(
				pAdapt , _pNdisOidRequest );
				break;

		default:
			NdisStatus = NDIS_STATUS_NOT_SUPPORTED;
			break;
	}

	return	NdisStatus;
}


/*
�ú�������С�˿��յ���SetInformation���͵�����

*/
NDIS_STATUS	_RequestSetInformation(
	IN	PADAPT	_pAdapt ,
	IN	PNDIS_OID_REQUEST	_pNdisOidRequest
)
{
	NDIS_STATUS	NdisStatus = NDIS_STATUS_SUCCESS;
	BOOLEAN	bForwardRequest = FALSE;	//�Ƿ��͸���ʵС�˿��豸
	NDIS_OID	Oid = _pNdisOidRequest->DATA.SET_INFORMATION.Oid;
	PULONG	BytesRead =
		(PULONG)&(_pNdisOidRequest->DATA.SET_INFORMATION.BytesRead);
	PULONG	BytesNeeded =
		(PULONG)&(_pNdisOidRequest->DATA.SET_INFORMATION.BytesNeeded);
	PVOID	pInformationBuffer =
		_pNdisOidRequest->DATA.SET_INFORMATION.InformationBuffer;
	ULONG	InformationBufferLength =
		_pNdisOidRequest->DATA.SET_INFORMATION.InformationBufferLength;
	NDIS_DEVICE_POWER_STATE	NewDevicePowerState;
	NDIS_STATUS_INDICATION  StatusIndication;

	KdPrint( ("[_RequestSetInformation]\n") );

	*BytesRead = 0;
	*BytesNeeded = 0;

	switch (Oid)
	{
		//���漸��Oid��ת������ʵС�˿��豸
		case OID_PNP_ADD_WAKE_UP_PATTERN:
		case OID_PNP_REMOVE_WAKE_UP_PATTERN:
		case OID_PNP_ENABLE_WAKE_UP:
			bForwardRequest = TRUE;
			break;

		case OID_PNP_SET_POWER:
			*BytesNeeded = sizeof( NDIS_DEVICE_POWER_STATE );
			if (InformationBufferLength < *BytesNeeded)
			{
				NdisStatus = NDIS_STATUS_INVALID_LENGTH;
				break;
			}
			//�洢��Դ״̬
			NewDevicePowerState =
				*((PNDIS_DEVICE_POWER_STATE)pInformationBuffer);
			
			// If the miniport is transitioning from a low power state to ON (D0), then clear the StandingBy flag
			// All incoming requests will be pended until the physical miniport turns ON.
			//����豸�ӵ͵�Դ״̬��ָ�,
			//����Ҫָ��StandBy�׶�δָ����ý������״̬
			if (_pAdapt->MPDeviceState > NdisDeviceStateD0 &&  NewDevicePowerState == NdisDeviceStateD0)
			{
				_pAdapt->StandingBy = FALSE;

				if (_pAdapt->LastIndicatedMediaStatus
					!= _pAdapt->LatestUnIndicateMediaStatus)
				{
					if (_pAdapt->MiniportAdapterHandle != NULL)
					{
						StatusIndication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
						StatusIndication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
						StatusIndication.Header.Size = sizeof( NDIS_STATUS_INDICATION );

						StatusIndication.SourceHandle =
							_pAdapt->MiniportAdapterHandle;
						StatusIndication.StatusCode =
							_pAdapt->LatestUnIndicateMediaStatus;

						if (_pAdapt->LatestUnIndicateMediaStatus == NDIS_STATUS_LINK_STATE)
						{
							StatusIndication.StatusBuffer =
								&_pAdapt->LatestUnIndicateLinkState;
							StatusIndication.StatusBufferSize = 
								sizeof( NDIS_LINK_STATE );
						}
						else
						{
							StatusIndication.StatusBuffer = NULL;
							StatusIndication.StatusBufferSize = 0;
						}

						NdisMIndicateStatusEx(
							_pAdapt->MiniportAdapterHandle ,
							&StatusIndication );

						_pAdapt->LastIndicatedMediaStatus =
							_pAdapt->LatestUnIndicateMediaStatus;
						if (_pAdapt->LatestUnIndicateMediaStatus 
							== NDIS_STATUS_LINK_STATE)
						{
							_pAdapt->LastIndicatedLinkState =
								_pAdapt->LatestUnIndicateLinkState;
						}

					}// if (_pAdapt->MiniportAdapterHandle != NULL)

				}// if (_pAdapt->LastIndicatedMediaStatus
				//!= _pAdapt->LatestUnIndicateMediaStatus)
				else
				{
					if (_pAdapt->LastIndicatedMediaStatus == NDIS_STATUS_LINK_STATE)
					{
						if (!NdisEqualMemory(
							&_pAdapt->LatestUnIndicateLinkState ,
							&_pAdapt->LastIndicatedLinkState ,
							sizeof( NDIS_LINK_STATE )
							))
						{
							StatusIndication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
							StatusIndication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
							StatusIndication.Header.Size = sizeof( NDIS_STATUS_INDICATION );

							StatusIndication.SourceHandle = _pAdapt->MiniportAdapterHandle;
							StatusIndication.StatusCode = _pAdapt->LatestUnIndicateMediaStatus;
							StatusIndication.StatusBuffer = &_pAdapt->LatestUnIndicateLinkState;
							StatusIndication.StatusBufferSize = sizeof( NDIS_LINK_STATE );

							NdisMIndicateStatusEx(
								_pAdapt->MiniportAdapterHandle ,
								&StatusIndication );

							_pAdapt->LastIndicatedMediaStatus =
								_pAdapt->LatestUnIndicateMediaStatus;
							_pAdapt->LastIndicatedLinkState =
								_pAdapt->LatestUnIndicateLinkState;
						}
					}

				}// if (_pAdapt->LastIndicatedMediaStatus
				//!= _pAdapt->LatestUnIndicateMediaStatus)

			}//if (_pAdapt->MPDeviceState > NdisDeviceStateD0 
			//  &&  NewDevicePowerState == NdisDeviceStateD0)

			//����豸�Ƿ��D0�����˵͵�Դ״̬
			// Is the miniport transitioning from an On (D0) state to an Low Power State (>D0)
			// If so, then set the StandingBy Flag - (Block all incoming requests)
			if (_pAdapt->MPDeviceState == NdisDeviceStateD0 && NewDevicePowerState > NdisDeviceStateD0)
			{
				_pAdapt->StandingBy = TRUE;

				_pAdapt->LatestUnIndicateMediaStatus =
					_pAdapt->LastIndicatedMediaStatus;
				if (_pAdapt->LastIndicatedMediaStatus
					== NDIS_STATUS_LINK_STATE)
				{
					_pAdapt->LatestUnIndicateLinkState =
						_pAdapt->LastIndicatedLinkState;
				}

			}// if (_pAdapt->MPDeviceState == NdisDeviceStateD0 
			 // && NewDevicePowerState > NdisDeviceStateD0)
			
			 // Now update the state in the pAdapt structure;
			_pAdapt->MPDeviceState = NewDevicePowerState;

			break;

			default:
				NdisStatus = NDIS_STATUS_NOT_SUPPORTED;
				break;
	}

	if (bForwardRequest == FALSE)
	{
		if (NdisStatus == NDIS_STATUS_SUCCESS)
			*BytesRead = InformationBufferLength;
	}
	else
	{
		//ת������
		NdisStatus =
			_ForwardOidRequest(
			_pAdapt ,
			_pNdisOidRequest );
	}

	return	NdisStatus;
}


/*
�ú���ת��Oid������ʵС�˿��豸��
*/
NDIS_STATUS	_ForwardOidRequest(
	IN	PADAPT	_pAdapt ,
	IN	PNDIS_OID_REQUEST	_pOidRequest
)
{
	NDIS_STATUS	NdisStatus;

	KdPrint( ("[_ForwardOidRequest]\n") );

	do
	{
		//
		// If the miniport below is unbinding, fail the request
		//
		NdisAcquireSpinLock( &_pAdapt->Lock );
		if (_pAdapt->UnbindingInProcess == TRUE)
		{
			NdisReleaseSpinLock( &_pAdapt->Lock );
			NdisStatus = NDIS_STATUS_FAILURE;
			break;
		}
		NdisReleaseSpinLock( &_pAdapt->Lock );
		//
		// All other Set Information requests are failed, if the miniport is
		// not at D0 or is transitioning to a device state greater than D0.
		//
		if (_pAdapt->MPDeviceState > NdisDeviceStateD0)
		{
			NdisStatus = NDIS_STATUS_FAILURE;
			break;
		}

		NdisAcquireSpinLock( &_pAdapt->Lock );
		//����OidRequest
		_pAdapt->bRequestCanceled = FALSE;
		_pAdapt->pOldOidRequest = _pOidRequest;
		_pAdapt->OidRequest = *_pOidRequest;
		_pAdapt->RequestRefCount = 1;
		NdisReleaseSpinLock( &_pAdapt->Lock );
		//
		// If the miniport below is unbinding, fail the request
		//
		NdisAcquireSpinLock( &_pAdapt->Lock );
		if (_pAdapt->UnbindingInProcess == TRUE)
		{
			NdisReleaseSpinLock( &_pAdapt->Lock );
			NdisStatus = NDIS_STATUS_FAILURE;
			break;
		}
		//
		// If the device below is at a low power state, we cannot send it the
		// request now, and must pend it.
		//
		if ((_pAdapt->PTDeviceState > NdisDeviceStateD0)
			&& (_pAdapt->StandingBy == FALSE))
		{
			_pAdapt->QueuedRequest = TRUE;
			NdisReleaseSpinLock( &_pAdapt->Lock );
			NdisStatus = NDIS_STATUS_PENDING;
			break;
		}
		//
		// This is in the process of powering down the system, always fail the request
		// 
		if (_pAdapt->StandingBy == TRUE)
		{
			NdisReleaseSpinLock( &_pAdapt->Lock );
			NdisStatus = NDIS_STATUS_FAILURE;
			break;
		}

		_pAdapt->OutstandingRequests++;
		NdisReleaseSpinLock( &_pAdapt->Lock );

		//forward
		NdisStatus = NdisOidRequest(
			_pAdapt->BindingHandle ,
			_pOidRequest );
		if (NdisStatus != NDIS_STATUS_PENDING)
		{
			_pAdapt->OutstandingRequests--;
		}

	} while (FALSE);

	return	NdisStatus;
}


NDIS_STATUS	_SetOptionsHandler(
	IN	NDIS_HANDLE	_NdisDriverHandle ,
	IN	NDIS_HANDLE	_DriverContext
)
{
	return	NDIS_STATUS_SUCCESS;
}


/*
�ú�����������С�˿��յ��Ĳ�ѯ����
*/
NDIS_STATUS	_RequestQueryInformation(
	IN	PADAPT	_pAdapt ,
	IN	PNDIS_OID_REQUEST	_pOidRequest
)
{
	NDIS_STATUS	NdisStatus = NDIS_STATUS_FAILURE;
	NDIS_OID	Oid =
		_pOidRequest->DATA.QUERY_INFORMATION.Oid;

	KdPrint( ("[_RequestQueryInformation]\n") );

	do
	{
		if (Oid == OID_PNP_QUERY_POWER)
		{
			//do not forward this
			NdisStatus = NDIS_STATUS_SUCCESS;
			break;
		}

		if (Oid == OID_GEN_SUPPORTED_GUIDS)
		{
			//do not forward this
			NdisStatus = NDIS_STATUS_NOT_SUPPORTED;
			break;
		}

		//���С�˿��豸���ڽ����,�����������ʧ��
		NdisAcquireSpinLock( &_pAdapt->Lock );
		if (_pAdapt->UnbindingInProcess == TRUE)
		{
			NdisReleaseSpinLock( &_pAdapt->Lock );
			NdisStatus = NDIS_STATUS_FAILURE;
			break;
		}
		NdisReleaseSpinLock( &_pAdapt->Lock );

		//С�˿��豸��Դû������Ҳʧ��
		if (_pAdapt->MPDeviceState > NdisDeviceStateD0)
		{
			NdisStatus = NDIS_STATUS_FAILURE;
			break;
		}

		//�洢���Oid����
		_pAdapt->OidRequest = *_pOidRequest;

		//���Э���豸��Դû������,����ʱ���������δ��
		if ((_pAdapt->PTDeviceState > NdisDeviceStateD0)
			&& (_pAdapt->StandingBy == FALSE))
		{
			_pAdapt->QueuedRequest = TRUE;
			NdisReleaseSpinLock( &_pAdapt->Lock );
			NdisStatus = NDIS_STATUS_PENDING;
			break;
		}
		//
		// This is in the process of powering down the system, always fail the request
		//
		if (_pAdapt->StandingBy == TRUE)
		{
			NdisReleaseSpinLock( &_pAdapt->Lock );
			NdisStatus = NDIS_STATUS_FAILURE;
			break;
		}

		_pAdapt->OutstandingRequests++;
		NdisReleaseSpinLock( &_pAdapt->Lock );

		//���󲿷�Oid����ױ�����󶨵���ʵС�˿��豸
		NdisStatus = _ForwardOidRequest(
			_pAdapt , _pOidRequest );
		if (NdisStatus != NDIS_STATUS_PENDING)
		{
			_OidRequestComplete(
				_pAdapt->BindingHandle ,
				_pOidRequest ,
				NdisStatus );
			NdisStatus = NDIS_STATUS_PENDING;
		}

	} while (FALSE);

	return	NdisStatus;
}


/*
�ú�������������ݰ�
����NET_BUFFER_LIST���²���ʵС�˿�����,���ҵ���Complete
����:
MiniportAdapterContext	������������
NetBufferList	Ҫ�����NetBufferList
PortNumber	�˿ں�
SendFlags	��־λ
����ֵ:��
*/
VOID	_MiniportSendNetBufferLists(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	PNET_BUFFER_LIST	_pNetBufferList ,
	IN	NDIS_PORT_NUMBER	_PortNumber ,
	IN	ULONG	_SendFlags
)
{
	NDIS_STATUS	NdisStatus = NDIS_STATUS_SUCCESS;
	PNET_BUFFER_LIST	pCurrentNetBufferList = NULL;
	PADAPT	pAdapt = _MiniportAdapterContext;
	PIM_NBLC	pSendContext;
	ULONG	SendCompleteFlags = 0;

	KdPrint( ("[_MiniportSendNetBufferLists]\n") );

	//Ҫ���͵�NetBufferList���ܲ�ֹһ������Ҫ����
	while (_pNetBufferList != NULL)
	{
		//ȡ����ǰNetBufferList
		pCurrentNetBufferList = _pNetBufferList;
		NET_BUFFER_LIST_NEXT_NBL( pCurrentNetBufferList ) = NULL;//����
		_pNetBufferList =
			NET_BUFFER_LIST_NEXT_NBL( _pNetBufferList );//ָ������

		NdisAcquireSpinLock( &pAdapt->Lock );
		if (pAdapt->BindingState != AdapterBindingRunning)
		{
			NdisStatus = NDIS_STATUS_REQUEST_ABORTED;
			NdisReleaseSpinLock( &pAdapt->Lock );
			break;
		}

		NdisReleaseSpinLock( &pAdapt->Lock );

		do
		{
			//����ǰNetBufferList����һ���Զ���Context
			NdisStatus =
				NdisAllocateNetBufferListContext(
				pCurrentNetBufferList ,
				sizeof( IM_NBLC ) ,
				0 , POOL_TAG );
			if (NdisStatus != NDIS_STATUS_SUCCESS)
			{
				//���ܳɹ������ʧ���˳�
				break;
			}

			pSendContext =
				(PIM_NBLC)NET_BUFFER_LIST_CONTEXT_DATA_START
				( pCurrentNetBufferList );
			//����
			NdisZeroMemory( pSendContext , sizeof( IM_NBLC ) );

			//����ԭsources handle
			pSendContext->PreviousSourceHandle =
				pCurrentNetBufferList->SourceHandle;
			pSendContext->pAdapt = pAdapt;
			//����source handle
			pCurrentNetBufferList->SourceHandle =
				pAdapt->BindingHandle;

			//���ӷ��Ͱ�����
			NdisInterlockedIncrement( &pAdapt->OutstandingSends );

			//�Ƴ�flags,ndis�Ͳ��ỷ�����NetBufferList
			_SendFlags &= NDIS_SEND_FLAGS_CHECK_FOR_LOOPBACK;

			//���·���
			NdisSendNetBufferLists(
				pAdapt->BindingHandle ,
				pCurrentNetBufferList ,
				_PortNumber ,
				_SendFlags );

		} while (FALSE);

		//������ɹ�
		if (NdisStatus != NDIS_STATUS_SUCCESS)
		{
			NdisAcquireSpinLock( &pAdapt->Lock );
			pAdapt->OutstandingSends--;

			if ((pAdapt->OutstandingSends == 0)
				&& (pAdapt->pPauseEvent != NULL))
			{
				NdisSetEvent( pAdapt->pPauseEvent );
				pAdapt->pPauseEvent = NULL;
			}
			NdisReleaseSpinLock( &pAdapt->Lock );

			//����ʧ��
			NET_BUFFER_LIST_STATUS( pCurrentNetBufferList ) = NdisStatus;

			if (NDIS_TEST_SEND_AT_DISPATCH_LEVEL( _SendFlags ))
			{
				NDIS_SET_SEND_COMPLETE_FLAG( SendCompleteFlags , NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL );
			}

			//�����Լ�����С�˿������ķ�����ɻص�
			NdisMSendNetBufferListsComplete(
				pAdapt->MiniportAdapterHandle ,
				pCurrentNetBufferList ,
				SendCompleteFlags );

			NdisStatus = NDIS_STATUS_SUCCESS;

		}// if (NdisStatus != NDIS_STATUS_SUCCESS)

	}// end while

	if (NdisStatus != NDIS_STATUS_SUCCESS)
	{
		PNET_BUFFER_LIST	pTempNetBufferList;

		for (pTempNetBufferList = pCurrentNetBufferList;
			pTempNetBufferList != NULL;
			pTempNetBufferList = NET_BUFFER_LIST_NEXT_NBL( pTempNetBufferList ))
		{
			NET_BUFFER_LIST_STATUS( pTempNetBufferList ) = NdisStatus;
		}
		if (NDIS_TEST_SEND_AT_DISPATCH_LEVEL( _SendFlags ))
		{
			NDIS_SET_SEND_COMPLETE_FLAG( SendCompleteFlags , NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL );
		}

		NdisMSendNetBufferListsComplete(
			pAdapt->MiniportAdapterHandle ,
			pCurrentNetBufferList ,
			SendCompleteFlags );

	}// end if (NdisStatus != NDIS_STATUS_SUCCESS)

}


VOID	_MiniportReturnNetBufferLists(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	PNET_BUFFER_LIST	_pNetBufferLists ,
	IN	ULONG	_ReturnFlags
)
{
	PADAPT	pAdapt = _MiniportAdapterContext;
	NDIS_STATUS	NdisStatus;
	PNET_BUFFER_LIST	pCurrentNetBufferList =
		_pNetBufferLists;

	KdPrint( ("[_MiniportReturnNetBufferLists]\n") );

	NdisReturnNetBufferLists(
		pAdapt->BindingHandle ,
		_pNetBufferLists ,
		_ReturnFlags );
}


/*++

Routine Description:

The miniport entry point to hanadle cancellation of all send packets that
match the given CancelId. If we have queued any packets that match this,
then we should dequeue them and call NdisMSendCompleteNetBufferLists for
all such packets, with a status of NDIS_STATUS_REQUEST_ABORTED.

We should also call NdisCancelSendPackets in turn, on each lower binding
that this adapter corresponds to. This is to let miniports below cancel
any matching packets.

Arguments:

MiniportAdapterContext          Pointer to VELAN structure
CancelID                        ID of NetBufferLists to be cancelled

Return Value:
None

--*/
VOID	_MiniportCancelSendNetBufferLists(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	PVOID	_CancelId
)
{
	PADAPT	pAdapt = _MiniportAdapterContext;

	KdPrint( ("[_MiniportCancelSendNetBufferLists]\n") );

	NdisCancelSendNetBufferLists(
		pAdapt->BindingHandle , _CancelId );
}


/*
�ú���������ͣС�˿�,��ͣʱ������NetBufferList
NDIS calls a miniport driver's MiniportPause function to 
stop data flow before performing a Plug and Play operation, 
such as adding or removing a filter driver or binding or 
unbinding a protocol driver. The miniport adapter remains 
in the Pausing state until the pause operation has completed.
*/
NDIS_STATUS	_MiniportPauseHandler(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	PNDIS_MINIPORT_PAUSE_PARAMETERS pPauseParameters
)
{
	PADAPT	pAdapt = _MiniportAdapterContext;

	KdPrint( ("[_MiniportPauseHandler]\n") );

	NdisAcquireSpinLock( &pAdapt->Lock );
	pAdapt->bPaused = TRUE;
	NdisReleaseSpinLock( &pAdapt->Lock );

	return	NDIS_STATUS_SUCCESS;
}


/*
�ú�������С�˿�
*/
NDIS_STATUS	_MiniportRestartHandler(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	PNDIS_MINIPORT_RESTART_PARAMETERS pRestartParameters
)
{
	PADAPT	pAdapt = _MiniportAdapterContext;
	NDIS_STATUS	NdisStatus = NDIS_STATUS_SUCCESS;
	PNDIS_RESTART_ATTRIBUTES          NdisRestartAttributes;
	PNDIS_RESTART_GENERAL_ATTRIBUTES  NdisGeneralAttributes;

	KdPrint( ("[_MiniportRestartHandler]\n") );

	//
	// Here the driver can change its restart attributes 
	//
	NdisRestartAttributes = pRestartParameters->RestartAttributes;

	//
	// If NdisRestartAttributes is not NULL, then miniport can modify generic attributes and add
	// new media specific info attributes at the end. Otherwise, NDIS restarts the miniport because 
	// of other reason, miniport should not try to modify/add attributes
	//
	if (NdisRestartAttributes != NULL)
	{

		ASSERT( NdisRestartAttributes->Oid == OID_GEN_MINIPORT_RESTART_ATTRIBUTES );

		NdisGeneralAttributes = (PNDIS_RESTART_GENERAL_ATTRIBUTES)NdisRestartAttributes->Data;
		UNREFERENCED_PARAMETER( NdisGeneralAttributes );

		//
		// Check to see if we need to change any attributes, for example, the driver can change the current
		// MAC address here. Or the driver can add media specific info attributes.
		//
	}

	NdisAcquireSpinLock( &pAdapt->Lock );
	pAdapt->bPaused = FALSE;
	NdisReleaseSpinLock( &pAdapt->Lock );

	return	NdisStatus;
}


/*
The miniport entry point to hanadle cancellation of a request. This function
checks to see if the CancelRequest should be terminated at this level
or passed down to the next driver.
*/
VOID	_MiniportCancelOidRequest(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	PVOID	_RequestId
)
{
	PADAPT	pAdapt = _MiniportAdapterContext;
	BOOLEAN	bCancelRequest = FALSE;

	KdPrint( ("[_MiniportCancelOidRequest]\n") );

	NdisAcquireSpinLock( &pAdapt->Lock );
	if (pAdapt->pOldOidRequest != NULL)
	{
		if (pAdapt->pOldOidRequest->RequestId
			== _RequestId)
		{
			pAdapt->bRequestCanceled = TRUE;
			bCancelRequest = TRUE;
			pAdapt->RequestRefCount++;
		}
	}
	NdisReleaseSpinLock( &pAdapt->Lock );

	//
	// If we find the request, just send down the cancel, otherwise return because there is only 
	// one request pending from upper layer on the miniport
	//
	if (bCancelRequest)
	{
		NdisCancelOidRequest(
			pAdapt->BindingHandle ,
			&pAdapt->OidRequest );

		_CompleteForwardedRequest(
			pAdapt ,
			NDIS_STATUS_REQUEST_ABORTED );
	}
	
}