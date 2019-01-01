#pragma	once

#define	NDIS60	1	//开启ndis 6.0
#include	<ndis.h>


#define	POOL_TAG	'hqsb'

#define	MAJOR_DRIVER_VERSION	1
#define	MINOR_DRIVER_VERSION	0


typedef enum _ADAPTER_BINDING_STATE
{
	AdapterBindingPaused ,
	AdapterBindingPausing ,
	AdapterBindingRunning
}ADAPTER_BINDING_STATE , *PADAPTER_BINDING_STATE;


//
//	该结构体即用于协议驱动的打开上下文,
//	也用于小端口驱动的适配器上下文

typedef struct _ADAPT
{
	struct _ADAPT *                Next;

	NDIS_HANDLE                    BindingHandle;    // To the lower miniport
	NDIS_HANDLE                    MiniportAdapterHandle;    // NDIS Handle to for miniport up-calls
	NDIS_HANDLE                    SendNetBufferListPoolHandle;
	NDIS_HANDLE                    RecvNetBufferListPoolHandle;
	NDIS_STATUS                    Status;            // Open Status
	NDIS_EVENT                     Event;            // Used by bind/halt for Open/Close Adapter synch.
	NDIS_MEDIUM                    Medium;

	BOOLEAN						bRequestCanceled;
	ULONG						RequestRefCount;
	PNDIS_OID_REQUEST			pOldOidRequest;	//保存最近一次的Oid请求
	NDIS_OID_REQUEST			OidRequest;        // This is used to wrap a request coming down
												   // to us. This exploits the fact that requests
												   // are serialized down to us.
	ULONG                         BytesNeeded;
	ULONG                         BytesReadOrWritten;
	BOOLEAN                       ReceivedIndicationFlags[32];

	ULONG							OutstandingRequests;      
															 // at the miniport below
	BOOLEAN                        QueuedRequest;            // TRUE iff a request is queued at
															 // this IM miniport

	BOOLEAN                        StandingBy;                // True - When the miniport or protocol is transitioning from a D0 to Standby (>D0) State
	BOOLEAN                        UnbindingInProcess;
	NDIS_SPIN_LOCK                 Lock;
	// False - At all other times, - Flag is cleared after a transition to D0

	NDIS_DEVICE_POWER_STATE        MPDeviceState;            // Miniport's Device State 
	NDIS_DEVICE_POWER_STATE        PTDeviceState;            // Protocol's Device State 
	NDIS_STRING                    IMAdapterName;                // For initializing the miniport edge
	NDIS_EVENT                     MiniportInitEvent;        // For blocking UnbindAdapter while
															 // an IM Init is in progress.
	BOOLEAN                        MiniportInitPending;    // TRUE iff IMInit in progress
	NDIS_STATUS                    LastIndicatedMediaStatus;    // The last indicated media status
	NDIS_STATUS                    LatestUnIndicateMediaStatus; // The latest suppressed media status
	NDIS_LINK_STATE				   LatestUnIndicateLinkState;
	NDIS_LINK_STATE				   LastIndicatedLinkState;

	ULONG                          OutstandingSends;
	LONG                           RefCount;
	BOOLEAN                        MiniportIsHalted;


	//下面存放一些网卡的参数
	NDIS_BIND_PARAMETERS        BindParameters;
	ADAPTER_BINDING_STATE		BindingState;

	BOOLEAN						bPaused;
	PNDIS_EVENT					pPauseEvent;

} ADAPT , *PADAPT;


typedef	struct _IM_NBLC
{
	NDIS_HANDLE	PreviousSourceHandle;
	PADAPT	pAdapt;
	UCHAR	Pad[];
}IM_NBLC , *PIM_NBLC;

//函数声明
VOID	_ReferenceAdapt(
	IN	PADAPT	_pAdapt
);

BOOLEAN	_DerefenceAdapt(
	IN	PADAPT	_pAdapt
);

NDIS_STATUS	_RegisterDevice();

void	_MiniportUnload(
	IN	PDRIVER_OBJECT	_pDriverObject
);

VOID	_ProtocolUninstall();

VOID	_OpenAdapterComplete(
	IN	NDIS_HANDLE	_ProtocolBindingContext ,
	IN	NDIS_STATUS	_NdisStatus
);

NDIS_STATUS	_BindAdapterHandlerEx(
	IN	NDIS_HANDLE	_ProtocolDriverContext ,
	IN	NDIS_HANDLE	_BindContext ,
	IN	PNDIS_BIND_PARAMETERS	_pBindParameters
);

VOID	_CloseAdapterComplete(
	IN	NDIS_HANDLE	_ProtocolBindingContext
);

NDIS_STATUS	_UnbindAdapterHandlerEx(
	IN	NDIS_HANDLE	_UnbindContext ,
	IN	NDIS_HANDLE	_ProtocolBindingContext
);

VOID	_OidRequestComplete(
	IN	NDIS_HANDLE	_ProtocolBindingContext ,
	IN	PNDIS_OID_REQUEST	_pNdisRequest ,
	IN	NDIS_STATUS	_NdisStatus
);

VOID	_StatusHandlerEx(
	IN	NDIS_HANDLE	_ProtocolBindingContext ,
	IN	PNDIS_STATUS_INDICATION	_pStatusIndication
);

NDIS_STATUS	_MiniportInitializeEx(
	IN	NDIS_HANDLE	_MiniportAdapterHandle ,
	IN	NDIS_HANDLE	_pMiniportDriverContext ,
	IN	PNDIS_MINIPORT_INIT_PARAMETERS _pMiniportInitParameters
);

NDIS_STATUS	_PnPNetEventSetPower(
	IN	PADAPT	_pAdapt ,
	IN	PNET_PNP_EVENT_NOTIFICATION	_pNetPnPEventNotification
);

NDIS_STATUS	_PnpEventHandler(
	IN	NDIS_HANDLE	_ProtocolBindingContext ,
	IN	PNET_PNP_EVENT_NOTIFICATION	_pNetPnpEventNotification
);

NDIS_STATUS	_DeregisterDevice();

VOID	_MiniportHaltEx(
	IN	NDIS_HANDLE		_MiniportAdapterContext ,
	IN	NDIS_HALT_ACTION	_HaltAction
);

VOID	_MiniportShutdownEx(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	NDIS_SHUTDOWN_ACTION	_ShutdownAction
);

VOID	_MiniportDevicePnPEventNotify(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	PNET_DEVICE_PNP_EVENT	_pNetDevicePnPEvent
);

NDIS_STATUS	_MiniportOidRequestHandler(
	IN	NDIS_HANDLE		_MiniportAdapterContext ,
	IN	PNDIS_OID_REQUEST	_pNdisOidRequest
);

NDIS_STATUS	_SetOptionsHandler(
	IN	NDIS_HANDLE	_NdisDriverHandle ,
	IN	NDIS_HANDLE	_DriverContext
);

NDIS_STATUS	_RequestQueryInformation(
	IN	PADAPT	_pAdapt ,
	IN	PNDIS_OID_REQUEST	_pOidRequest
);

VOID	_MiniportSendNetBufferLists(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	PNET_BUFFER_LIST	_pNetBufferList ,
	IN	NDIS_PORT_NUMBER	_PortNumber ,
	IN	ULONG	_SendFlags
);

VOID	_MiniportReturnNetBufferLists(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	PNET_BUFFER_LIST	_pNetBufferLists ,
	IN	ULONG	_ReturnFlags
);

VOID	_MiniportCancelSendNetBufferLists(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	PVOID	_CancelId
);

NDIS_STATUS	_MiniportPauseHandler(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	PNDIS_MINIPORT_PAUSE_PARAMETERS pPauseParameters
);

NDIS_STATUS	_MiniportRestartHandler(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	PNDIS_MINIPORT_RESTART_PARAMETERS pRestartParameters
);

VOID	_SendComplete(
	IN	NDIS_HANDLE	_ProtocolBindingContext ,
	IN	PNET_BUFFER_LIST	_pNetBufferLists ,
	IN	ULONG	_SendCompleteFlags
);

VOID	_ReceiveNetBufferList(
	IN	NDIS_HANDLE	_ProtocolBindingContext ,
	IN	PNET_BUFFER_LIST	_pNetBufferLists ,
	IN	NDIS_PORT_NUMBER	_PortNumber ,
	IN	ULONG	_NumberOfNetBufferLists ,
	IN	ULONG	_ReceiveFlags
);

NTSTATUS	_ControlDeviceDispatch(
	IN	PDEVICE_OBJECT	_pDeviceObject ,
	IN	PIRP	_pIrp
);

NDIS_STATUS	_ForwardOidRequest(
	IN	PADAPT	_pAdapt ,
	IN	PNDIS_OID_REQUEST	_pOidRequest
);

NDIS_STATUS	_RequestSetInformation(
	IN	PADAPT	_pAdapt ,
	IN	PNDIS_OID_REQUEST	_pNdisOidRequest
);

VOID	_CompleteForwardedRequest(
	IN	PADAPT	_pAdapt ,
	IN	NDIS_STATUS	_NdisStatus
);

VOID	_MiniportCancelOidRequest(
	IN	NDIS_HANDLE	_MiniportAdapterContext ,
	IN	PVOID	_RequestId
);

NDIS_STATUS	_SetOptionHandler(
	IN	NDIS_HANDLE	_DriverHandler ,
	IN	NDIS_HANDLE	_DriverContext
);
