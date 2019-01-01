#include	"shared_head.h"


extern	NDIS_SPIN_LOCK	g_SpinLock;
extern	PADAPT	g_pAdaptList;
extern	NDIS_HANDLE	g_MiniportDriver;


//支持的OID请求
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
该函数卸载小端口驱动
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
该函数初始化本驱动的小端口,在 NdisIMInitializeDeviceInstanceEx
中就会调用
参数:
NdisMiniportHandle		NDIS为我们指定的HANDLE
MiniportDriverContext	在调用NdisMRegisterMiniportDriver时传入的自定义的参数
MiniportInitParameters	初始化用到的参数
返回值:	操作状态
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
		//获得PADAPT结构体
		pAdapt = _pMiniportInitParameters->IMDeviceInstanceContext;
		pAdapt->MiniportIsHalted = FALSE;

		//设置适配器的registration attributes
		NdisZeroMemory(
			&MPAttri ,
			sizeof( NDIS_MINIPORT_ADAPTER_ATTRIBUTES ) );
		MPAttri.RegistrationAttributes.Header.Type =
			NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
		MPAttri.RegistrationAttributes.Header.Revision=
			NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
		MPAttri.RegistrationAttributes.Header.Size=
			sizeof( NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES );
		//适配器上下文
		MPAttri.RegistrationAttributes.MiniportAdapterContext = pAdapt;
		//设置系统进入休眠后不会调用MpHalt函数
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

		//设置适配器的GeneralAttribute,尽量模仿小端口适配器的参数
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
		我们没有物理适配器,所以设置该值为FALSE
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
		//设置小端口适配器的网卡参数
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
		//指定最近一次状态
		pAdapt->LastIndicatedMediaStatus = NDIS_STATUS_MEDIA_CONNECT;
		//设置自己的和真实绑定的小端口电源状态都为on
		pAdapt->MPDeviceState = NdisDeviceStateD0;
		pAdapt->PTDeviceState = NdisDeviceStateD0;

		//将这个适配器上下文放入全局适配器单链表
		//操作要使用自旋锁同步
		NdisAcquireSpinLock( &g_SpinLock );
		pAdapt->Next = g_pAdaptList;
		g_pAdaptList = pAdapt;
		NdisReleaseSpinLock( &g_SpinLock );

		//创建本驱动的控制设备
		if (
			_RegisterDevice() != NDIS_STATUS_SUCCESS
			)
			KdPrint( ("_MiniportInitializeEx: Fail to create CDO!\n") );
		
	} while (FALSE);

	ASSERT( pAdapt->MiniportInitPending == TRUE );
	pAdapt->MiniportInitPending = FALSE;//初始化完成
	NdisSetEvent( &pAdapt->MiniportInitEvent );

	//操作都成功了需要增加对上下文的引用计数
	if (NdisStatus == NDIS_STATUS_SUCCESS)
		_ReferenceAdapt( pAdapt );

	return	NdisStatus;
}


/*
该函数处理虚拟小端口设备的停止工作
该函数会在驱动调用NdisIMDeInitializeDeviceInstance后被调用
其中要做清理工作
参数:
MiniportAdapterContext	适配器上下文指针
HaltAction		设备停止的原因
返回值:无
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

	//将这个适配器上下文节点从全局单链表中断链
	if (pAdapt != pTemp)
	{//如果该节点在第一个就不用断链了
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
	
	//注销控制设备
	_DeregisterDevice();

	//如过我们的驱动成功绑定过小端口,就要解除对它的绑定
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
该函数在系统shutdown时被调用
参数:
MiniportAdapterContext		适配器上下文
ShutdownAction		shutdown原因
返回值:wu
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
当ndis通知我们的小端口设备pnp事件时会调用此函数
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
该函数处理我们的小端口设备收到的OID请求
参数:
MiniportAdapterContext		适配器上下文
pNdisRequest		请求指针
返回值:	无
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
该函数处理小端口收到的SetInformation类型的请求

*/
NDIS_STATUS	_RequestSetInformation(
	IN	PADAPT	_pAdapt ,
	IN	PNDIS_OID_REQUEST	_pNdisOidRequest
)
{
	NDIS_STATUS	NdisStatus = NDIS_STATUS_SUCCESS;
	BOOLEAN	bForwardRequest = FALSE;	//是否发送给真实小端口设备
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
		//下面几个Oid都转发给真实小端口设备
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
			//存储电源状态
			NewDevicePowerState =
				*((PNDIS_DEVICE_POWER_STATE)pInformationBuffer);
			
			// If the miniport is transitioning from a low power state to ON (D0), then clear the StandingBy flag
			// All incoming requests will be pended until the physical miniport turns ON.
			//如果设备从低电源状态里恢复,
			//就需要指定StandBy阶段未指定的媒介连接状态
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

			//检查设备是否从D0进入了低电源状态
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
		//转发请求
		NdisStatus =
			_ForwardOidRequest(
			_pAdapt ,
			_pNdisOidRequest );
	}

	return	NdisStatus;
}


/*
该函数转发Oid请求到真实小端口设备上
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
		//保存OidRequest
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
该函数处理我们小端口收到的查询请求
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

		//如果小端口设备正在解除绑定,就让这个请求失败
		NdisAcquireSpinLock( &_pAdapt->Lock );
		if (_pAdapt->UnbindingInProcess == TRUE)
		{
			NdisReleaseSpinLock( &_pAdapt->Lock );
			NdisStatus = NDIS_STATUS_FAILURE;
			break;
		}
		NdisReleaseSpinLock( &_pAdapt->Lock );

		//小端口设备电源没有启动也失败
		if (_pAdapt->MPDeviceState > NdisDeviceStateD0)
		{
			NdisStatus = NDIS_STATUS_FAILURE;
			break;
		}

		//存储这个Oid请求
		_pAdapt->OidRequest = *_pOidRequest;

		//如果协议设备电源没有启动,就暂时让这个请求未决
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

		//将大部分Oid请求妆发给绑定的真实小端口设备
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
该函数处理接收数据包
发送NET_BUFFER_LIST给下层真实小端口驱动,并且调用Complete
参数:
MiniportAdapterContext	适配器上下文
NetBufferList	要处理的NetBufferList
PortNumber	端口号
SendFlags	标志位
返回值:无
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

	//要发送的NetBufferList可能不止一个所以要遍历
	while (_pNetBufferList != NULL)
	{
		//取出当前NetBufferList
		pCurrentNetBufferList = _pNetBufferList;
		NET_BUFFER_LIST_NEXT_NBL( pCurrentNetBufferList ) = NULL;//断链
		_pNetBufferList =
			NET_BUFFER_LIST_NEXT_NBL( _pNetBufferList );//指针下移

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
			//给当前NetBufferList分配一个自定义Context
			NdisStatus =
				NdisAllocateNetBufferListContext(
				pCurrentNetBufferList ,
				sizeof( IM_NBLC ) ,
				0 , POOL_TAG );
			if (NdisStatus != NDIS_STATUS_SUCCESS)
			{
				//不能成功分配就失败退出
				break;
			}

			pSendContext =
				(PIM_NBLC)NET_BUFFER_LIST_CONTEXT_DATA_START
				( pCurrentNetBufferList );
			//清零
			NdisZeroMemory( pSendContext , sizeof( IM_NBLC ) );

			//保存原sources handle
			pSendContext->PreviousSourceHandle =
				pCurrentNetBufferList->SourceHandle;
			pSendContext->pAdapt = pAdapt;
			//更换source handle
			pCurrentNetBufferList->SourceHandle =
				pAdapt->BindingHandle;

			//增加发送包计数
			NdisInterlockedIncrement( &pAdapt->OutstandingSends );

			//移除flags,ndis就不会环回这个NetBufferList
			_SendFlags &= NDIS_SEND_FLAGS_CHECK_FOR_LOOPBACK;

			//向下发送
			NdisSendNetBufferLists(
				pAdapt->BindingHandle ,
				pCurrentNetBufferList ,
				_PortNumber ,
				_SendFlags );

		} while (FALSE);

		//如果不成功
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

			//处理失败
			NET_BUFFER_LIST_STATUS( pCurrentNetBufferList ) = NdisStatus;

			if (NDIS_TEST_SEND_AT_DISPATCH_LEVEL( _SendFlags ))
			{
				NDIS_SET_SEND_COMPLETE_FLAG( SendCompleteFlags , NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL );
			}

			//调用自己虚拟小端口驱动的发送完成回调
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
该函数用来暂停小端口,暂停时不接受NetBufferList
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
该函数重启小端口
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