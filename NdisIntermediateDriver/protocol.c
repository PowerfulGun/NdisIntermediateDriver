#include	"shared_head.h"


extern	NDIS_HANDLE	g_ProtocolDriver;
extern	NDIS_HANDLE	g_MiniportDriver;
extern	NDIS_HANDLE	g_hControlDevice;


PROTOCOL_UNINSTALL _ProtocolUninstall;
/*
该函数卸载协议驱动
*/
VOID	_ProtocolUninstall()
{

	KdPrint( ("_ProtocolUninstall: Entered!\n") );

	if (g_ProtocolDriver)
	{
		NdisDeregisterProtocolDriver( g_ProtocolDriver );
		g_ProtocolDriver = NULL;
	}

	//删除控制设备
	if (g_hControlDevice)
	{
		IoDeleteDevice( g_hControlDevice );
		g_hControlDevice = NULL;
	}
	KdPrint( ("_ProtocolUninstall: Done!\n") );
}


PROTOCOL_BIND_ADAPTER_EX	_BindAdapterHandlerEx;
/*
该函数被ndis回调,用来绑定一个NIC设备(网卡设备)
该函数完成以下功能:
1.分配用于发送和接收网络数据的NET_BUFFER_LIST池
2.调用函数OpenAdapterEx绑定下层NIC设备
3.调用NdisIMInitializeDeviceInstanceEx函数
  来初始化小端口驱动,激活MpInitialize回调
参数:
ProtocolDriverContext	驱动在注册ndis协议时传的自定义上下文参数,本驱动没有使用
BindContext				The handle that identifies the NDIS context area for this bind operation.
pBindParameters			指向需要绑定的适配器的参数的指针
返回值:	操作状态
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
		//分配内存时预留出用来存储自己的miniport名称的空间
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

		//初始化ADAPT结构体,同时拷贝IM-miniport的名称
		//为之后的NdisIMInitializeDeviceInstance调用
		NdisZeroMemory( pAdapt , TotalSize );
		pAdapt->IMAdapterName.MaximumLength = _pBindParameters->BoundAdapterName->MaximumLength;
		pAdapt->IMAdapterName.Length = _pBindParameters->BoundAdapterName->Length;
		pAdapt->IMAdapterName.Buffer = (PWCHAR)(pAdapt + sizeof( ADAPT ));
		NdisMoveMemory(
			pAdapt->IMAdapterName.Buffer ,
			_pBindParameters->BoundAdapterName->Buffer ,
			_pBindParameters->BoundAdapterName->MaximumLength );

		//初始化上下文中的事件和锁,之后会用
		NdisInitializeEvent( &pAdapt->Event );	//初始后该事件就可以被等待
		NdisAllocateSpinLock( &pAdapt->Lock );

		//填充NET_BUFFER_LIST_POOL_PARAMETERS结构体
		NET_BUFFER_LIST_POOL_PARAMETERS	NetBufferListPoolParameters;
		NetBufferListPoolParameters.Header.Type =
			NDIS_OBJECT_TYPE_DEFAULT;
		NetBufferListPoolParameters.Header.Revision =
			NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
		NetBufferListPoolParameters.Header.Size =
			sizeof( NetBufferListPoolParameters );

		NetBufferListPoolParameters.ProtocolId = NDIS_PROTOCOL_ID_DEFAULT;
		NetBufferListPoolParameters.fAllocateNetBuffer = TRUE;	//每个list默认自带一个Buffer
		NetBufferListPoolParameters.ContextSize = 0;
		NetBufferListPoolParameters.PoolTag = POOL_TAG;
		NetBufferListPoolParameters.DataSize = 0;

		//分配发送NET_BUFFER_LIST池
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

		//分配接收NET_BUFFER_LIST池
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

		//准备绑定真是网卡适配器,先填写OpenParameters
		//填写OpenParameters参数
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

		//绑定
		NdisStatus = NdisOpenAdapterEx(
			g_ProtocolDriver ,
			pAdapt ,	//会传入OpenComplete中做参数
			&OpenParameters ,
			_BindContext ,
			&pAdapt->BindingHandle );
		//等待请求完成
		if (NdisStatus == NDIS_STATUS_PENDING)
		{
			NdisWaitEvent( &pAdapt->Event , 0 );
			NdisStatus = pAdapt->Status;
		}
		//绑定失败就退出
		if (NdisStatus != NDIS_STATUS_SUCCESS)
		{
			KdPrint( ("_BindAdapterHandlerEx.NdisOpenAdapterEx fail,status=%x\n" ,
				NdisStatus) );
			break;
		}
		//绑定成功,进一步获取网卡的一些参数
		pAdapt->BindParameters = *_pBindParameters;

		//增加对Adapt的引用计数
		_ReferenceAdapt( pAdapt );

		/*
		现在准备让NDis初始化我们的小端口边缘
		*/
		//设置一个小端口还未初始化好的标识
		pAdapt->MiniportInitPending = TRUE;
		//设置等待事件
		NdisInitializeEvent( &pAdapt->MiniportInitEvent );

		//增加引用
		_ReferenceAdapt( pAdapt );

		//调用函数初始化,该函数执行过程中会调用MpInitialize
		NdisStatus =
			NdisIMInitializeDeviceInstanceEx(
			g_MiniportDriver ,
			&pAdapt->IMAdapterName ,
			pAdapt );//该参数会传入MpInitializeEx中做参数
		if (NdisStatus != NDIS_STATUS_SUCCESS)
		{
			if (pAdapt->MiniportIsHalted == TRUE)
				bNoCleanUpNeeded = TRUE;

			KdPrint( ("_BindAdapterHandlerEx.NdisIMInitializeDeviceInstanceEx fail,status=%x\n" ,
				NdisStatus) );

			//减引用
			if (_DerefenceAdapt( pAdapt ))
				pAdapt = NULL;

			break;
		}
		_DerefenceAdapt( pAdapt );

	} while (FALSE);

	//清理资源
	if ((NdisStatus != NDIS_STATUS_SUCCESS)
		&& (bNoCleanUpNeeded == FALSE))
	{
		if (pAdapt != NULL)
		{
			if (pAdapt->BindingHandle != NULL)
			{
				NDIS_STATUS	CloseStatus;

				//解除之前的绑定
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
该函数在NdisOpenAdapterEx调用完之后回调
在其中设置事件
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
该函数在NdisCloseAdapter之后回调,其中设置事件
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
该函数被ndis回调,当被要求解除对一个网卡适配器的绑定时
参数:
UnbindContext	The handle that identifies the NDIS context area for this unbind operation
ProtocolBindingContext	我们调用NdisOpenAdapterEx时指定的打开上下文
返回值:	操作状态
*/
NDIS_STATUS	_UnbindAdapterHandlerEx(
	IN	NDIS_HANDLE	_UnbindContext ,
	IN	NDIS_HANDLE	_ProtocolBindingContext
)
{
	PADAPT	pAdapt = _ProtocolBindingContext;
	NDIS_STATUS	NdisStatus;

	KdPrint( ("[_UnbindAdapterHandlerEx]\n") );

	//设置标志位表明正在Unbinding,如果之后有request请求会返回失败
	NdisAcquireSpinLock( &pAdapt->Lock );
	pAdapt->UnbindingInProcess = TRUE;
	if (pAdapt->QueuedRequest == TRUE)
	{
		pAdapt->QueuedRequest = FALSE;
		NdisReleaseSpinLock( &pAdapt->Lock );
		//手工调用请求完成函数
		_OidRequestComplete(
			pAdapt ,
			&pAdapt->OidRequest ,
			NDIS_STATUS_FAILURE );
	}
	NdisReleaseSpinLock( &pAdapt->Lock );

	//如果我们调用过NdisIMInitializeDeviceInstanceEx
	//来初始化过我们的小端口驱动,并且初始化正在进行中
	//就还需要调用NdisIMCancelInitializeDeviceInstance
	if (pAdapt->MiniportInitPending == TRUE)
	{
		//Cancel it
		NdisStatus =
			NdisIMCancelInitializeDeviceInstance(
			g_MiniportDriver ,
			&pAdapt->IMAdapterName );
		if (NdisStatus == NDIS_STATUS_SUCCESS)
		{
			//取消初始化成功
			pAdapt->MiniportInitPending = FALSE;
			ASSERT( pAdapt->MiniportAdapterHandle == NULL );
		}
		else
		{
			//取消初始化失败,那就必须等小端口初始化完成
			NdisWaitEvent( &pAdapt->MiniportInitEvent , 0 );
			ASSERT( pAdapt->MiniportInitPending == FALSE );
		}
	}

	//初始化小端口成功的话要调用NdisIMDeInitializeDeviceInstance
	//来让我们的小端口驱动卸载
	if (pAdapt->MiniportAdapterHandle != NULL)
	{
		NdisStatus =
			NdisIMDeInitializeDeviceInstance(
			pAdapt->MiniportAdapterHandle );
		if (NdisStatus != NDIS_STATUS_SUCCESS)
			NdisStatus = NDIS_STATUS_FAILURE;
	}
	
	//我们在这里需要先closeAdapter解除对真实小端口驱动的绑定
	//再释放些资源调用
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

	//释放资源
	ASSERT( _DerefenceAdapt( pAdapt ) == TRUE );

	return	NDIS_STATUS_SUCCESS;
}


/*
当真实小端口驱动完成了一个请的的时候,该函数会被ndis回调
参数:
ProtocolBindingHandle	指向自定义的打开上下文(pAdapt)
pNdisRequest			指向完成的请求
Status					完成的状态
返回值:	无
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

	//我们要进一步完成Request,使之返回给上层的协议驱动(tcp/ip)
	switch (_pNdisOidRequest->RequestType)
	{
		case NdisRequestQueryInformation:
		case NdisRequestQueryStatistics:
			//如果这是个查询类型的请求

			//由于我们从不传递下发OID_PNP_QUERY_POWER请求
			//如果遇到这个请求类型的回调,那就是错误了
			ASSERT( Oid != OID_PNP_QUERY_POWER );

			//如果是OID_PNP_CAPABILITIES,ndis用这个来询问小端口
			//驱动是否支持再唤醒功能,我们需要修改结果为不支持
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
					//重写下列标记
					pWakeUpCapabilities = &pPnpCapabilities->WakeUpCapabilities;
					pWakeUpCapabilities->MinMagicPacketWakeUp = NdisDeviceStateUnspecified;
					pWakeUpCapabilities->MinLinkChangeWakeUp = NdisDeviceStateUnspecified;
					pWakeUpCapabilities->MinPatternWakeUp = NdisDeviceStateUnspecified;
					pAdapt->BytesReadOrWritten = sizeof( NDIS_PNP_CAPABILITIES );
					pAdapt->BytesNeeded = 0;

					//设置我们的设备状态为启动
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
	//再完成请求
	NdisMOidRequestComplete(
		pAdapt->MiniportAdapterHandle ,
		_pNdisOidRequest ,
		_NdisStatus );
}


/*
该函数在底层真实小端口状态发生变化时回调
由于我们是中间层驱动,所以还要将状态继续传递给上层真实协议驱动(tcp/ip.sys)
参数:
ProtocolBindingContext		打开上下文(亦做适配器上下文)
StatusIndication			包含状态信息的指针
返回值:无
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

	//如果我们的小端口已经初始化好并且是启动状态
	//就继续上传StatusIndication
	if ((pAdapt->MiniportAdapterHandle != NULL)
		&& (pAdapt->MPDeviceState == NdisDeviceStateD0)
		&& (pAdapt->PTDeviceState == NdisDeviceStateD0))
	{
		//把媒介的连接状态保留进结构体
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
Ndis通过此函数通知我们pnp事件关于底层小端口的变化
参数:
ProtocolBindingContext	指向打开上下文
pNetPnPEventNotification	指向通知事件
返回值:	操作状态
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
该函数处理我们的协议驱动收到关于电源的pnp事件
如果进入了低电源状态,就要在此等等所有的发送和请求完成
参数:
pAdapt	适配器上下文
pNetPnpEventNotification	pnp事件
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

	//保存电源状态
	NdisAcquireSpinLock( &_pAdapt->Lock );
	_pAdapt->PTDeviceState =
		*(PNDIS_DEVICE_POWER_STATE)
		_pNetPnPEventNotification->NetPnPEvent.Buffer;

	//检查是否进入了低电源状态
	if (_pAdapt->PTDeviceState > NdisDeviceStateD0)
	{
		//如果之前状态是D0,便进入StandBy阶段,这将阻止请求的接收,直到电源重新启动
		if (PrevDeviceState == NdisDeviceStateD0)
			_pAdapt->StandingBy = TRUE;

		NdisReleaseSpinLock( &_pAdapt->Lock );

		//等待发送和请求的全部完成
		while (_pAdapt->OutstandingSends != 0)
		{
			NdisMSleep( 1000 );
		}

		while (_pAdapt->OutstandingRequests != 0)
		{
			NdisMSleep( 1000 );
		}
		
		//如果存在当前的请求以失败的方式完成
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
		//如果物理小端口电源启动了
		if (PrevDeviceState > NdisDeviceStateD0)
			_pAdapt->StandingBy = FALSE;

		//下层物理小端口设备已经可以使用了
		//如果有请求就可以下发
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
该函数在底层真实小端口发送完数据包之后回调
要在其中恢复NetBufferList的source handle
参数:
ProtocolBindingContext	绑定上下文
NetBufferList	发送完成的数据包
SendCompleteFlags	发送完成标志位
返回值:无
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

		//恢复source handle
		pCurrentNetBufferList->SourceHandle =
			pSendContext->PreviousSourceHandle;

		NdisStatus =
			NET_BUFFER_LIST_STATUS( pCurrentNetBufferList );
		//释放自定义Context
		NdisFreeNetBufferListContext(
			pCurrentNetBufferList , sizeof( IM_NBLC ) );

		//再冒充小端口驱动向上层协议驱动递交完成
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
该函数处理接收数据包
参数:
ProtocolBindingContext     Pointer to our PADAPT structure
NetBufferLists             Net Buffer Lists received
PortNumber                 Port on which NBLS were received
NumberOfNetBufferLists     Number of Net Buffer Lists
ReceiveFlags               Flags associated with the receive
返回值:无
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

	//如果虚拟小端口部分没有初始化好,或者设备是低电源状态
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

	//冒充小端口向上层协议驱动发送收到的数据包
	NdisMIndicateReceiveNetBufferLists(
		pAdapt->MiniportAdapterHandle ,
		_pNetBufferLists ,
		_PortNumber ,
		_NumberOfNetBufferLists ,
		_ReceiveFlags );
}


/*
该函数具体完成要处理的Oid请求,作为子函数被RequestComplete调用
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
	//	完成旧的请求
	//
	Oid = pOidRequest->DATA.QUERY_INFORMATION.Oid;
	switch (pOidRequest->RequestType)
	{
		case NdisRequestQueryInformation:
		case NdisRequestQueryStatistics:
			//如果这是个查询类型的请求

			pOldOidRequest->DATA.QUERY_INFORMATION.BytesWritten =
				pOidRequest->DATA.QUERY_INFORMATION.BytesWritten;
			pOldOidRequest->DATA.QUERY_INFORMATION.BytesNeeded =
				pOidRequest->DATA.QUERY_INFORMATION.BytesNeeded;

			//如果是OID_PNP_CAPABILITIES,ndis用这个来询问小端口
			//驱动是否支持再唤醒功能,我们需要修改结果为不支持
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
					//重写下列标记
					pWakeUpCapabilities = &pPnpCapabilities->WakeUpCapabilities;
					pWakeUpCapabilities->MinMagicPacketWakeUp = NdisDeviceStateUnspecified;
					pWakeUpCapabilities->MinLinkChangeWakeUp = NdisDeviceStateUnspecified;
					pWakeUpCapabilities->MinPatternWakeUp = NdisDeviceStateUnspecified;
					_pAdapt->BytesReadOrWritten = sizeof( NDIS_PNP_CAPABILITIES );
					_pAdapt->BytesNeeded = 0;

					//设置我们的设备状态为启动
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
	//再完成请求
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