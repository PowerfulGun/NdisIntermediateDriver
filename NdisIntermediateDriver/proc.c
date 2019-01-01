#include	"shared_head.h"

//
//	全局变量
//

NDIS_SPIN_LOCK	g_SpinLock;
PADAPT	g_pAdaptList = NULL;	//适配器上下文单链表
ULONG	g_MiniportCount = 0;	//初始化小端口的次数
NDIS_HANDLE	g_MiniportDriver = NULL;
NDIS_HANDLE	g_ProtocolDriver = NULL;
NDIS_HANDLE	g_hControlDevice = NULL;	//控制设备的句柄
PDEVICE_OBJECT	g_ControlDeviceObject = NULL;//控制设备对象

enum _DEVICE_STATE
{
	PS_DEVICE_STATE_READY = 0 ,    // ready for create/delete
	PS_DEVICE_STATE_CREATING ,    // create operation in progress
	PS_DEVICE_STATE_DELETING    // delete operation in progress
} ControlDeviceState = PS_DEVICE_STATE_READY;




/*
DriverEntry需要做的事:
1.调用NdisMRegisterMiniportDriver注册小端口驱动
2.调用NdisRegisterProtocolDriver注册协议驱动
3.调用NdisIMAssociateMiniport函数以通知 NDIS 
  有关驱动程序的微型端口上沿和下沿协议之间的关联
*/
NTSTATUS	DriverEntry(
	IN	PDRIVER_OBJECT	_pDriverObject ,
	IN	PUNICODE_STRING	_pRegistryPath
)
{
	NDIS_STATUS	NdisStatus = NDIS_STATUS_SUCCESS;
	NDIS_MINIPORT_DRIVER_CHARACTERISTICS	MPChar;
	NDIS_PROTOCOL_DRIVER_CHARACTERISTICS	ProtocolChar;
	NDIS_STRING	ProtocolName =
		NDIS_STRING_CONST( "PowerfulGun_Ndis" );

	//初始化全局自旋锁
	NdisAllocateSpinLock( &g_SpinLock );

	do
	{
		//为了注册小端口驱动需要先填写小端口特征结构
		NdisZeroMemory(
			&MPChar ,
			sizeof( NDIS_MINIPORT_DRIVER_CHARACTERISTICS ) );

		MPChar.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
		MPChar.Header.Size = NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1;
		MPChar.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1;

		MPChar.MajorNdisVersion = 6;
		MPChar.MinorNdisVersion = 0;

		MPChar.MajorDriverVersion = MAJOR_DRIVER_VERSION;
		MPChar.MinorDriverVersion = MINOR_DRIVER_VERSION;

		//以下是ndis6.0新的特征结构的函数指针赋值
		MPChar.SetOptionsHandler = _SetOptionsHandler;
		MPChar.InitializeHandlerEx = _MiniportInitializeEx;
		MPChar.UnloadHandler = _MiniportUnload;
		MPChar.HaltHandlerEx = _MiniportHaltEx;

		MPChar.OidRequestHandler = _MiniportOidRequestHandler;
		MPChar.CancelOidRequestHandler = _MiniportCancelOidRequest;

		MPChar.DevicePnPEventNotifyHandler = _MiniportDevicePnPEventNotify;
		MPChar.ShutdownHandlerEx = _MiniportShutdownEx;

		MPChar.CheckForHangHandlerEx = NULL;
		MPChar.ResetHandlerEx = NULL;

		MPChar.SendNetBufferListsHandler = _MiniportSendNetBufferLists;
		MPChar.CancelSendHandler = _MiniportCancelSendNetBufferLists;
		MPChar.ReturnNetBufferListsHandler = _MiniportReturnNetBufferLists;
		
		MPChar.PauseHandler = _MiniportPauseHandler;
		MPChar.RestartHandler = _MiniportRestartHandler;

		MPChar.Flags = NDIS_INTERMEDIATE_DRIVER;//中间层驱动标志

		//注册小端口驱动
		NdisStatus = NdisMRegisterMiniportDriver(
			_pDriverObject ,
			_pRegistryPath ,
			NULL ,	//该Context会传入MiniportInitializeEx的参数MiniportInitParameters.IMDeviceInstanceContext
			&MPChar,
			&g_MiniportDriver );
		if (NdisStatus != NDIS_STATUS_SUCCESS)
		{
			KdPrint( ("DriverEntry: Fail to register miniport driver,status=%x\n" ,
				NdisStatus) );
			break;
		}

		//为了注册协议驱动需要填写协议驱动特征结构体
		NdisZeroMemory(
			&ProtocolChar ,
			sizeof( NDIS_PROTOCOL_DRIVER_CHARACTERISTICS ) );

		ProtocolChar.Header.Type =
			NDIS_OBJECT_TYPE_PROTOCOL_DRIVER_CHARACTERISTICS;
		ProtocolChar.Header.Size =
			NDIS_SIZEOF_PROTOCOL_DRIVER_CHARACTERISTICS_REVISION_1;
		ProtocolChar.Header.Revision =
			NDIS_PROTOCOL_DRIVER_CHARACTERISTICS_REVISION_1;

		ProtocolChar.MajorNdisVersion = 6;
		ProtocolChar.MinorNdisVersion = 0;
		ProtocolChar.MajorDriverVersion = MAJOR_DRIVER_VERSION;
		ProtocolChar.MinorDriverVersion = MINOR_DRIVER_VERSION;

		ProtocolChar.Name = ProtocolName;

		ProtocolChar.SetOptionsHandler = _SetOptionHandler;
		ProtocolChar.OpenAdapterCompleteHandlerEx = _OpenAdapterComplete;
		ProtocolChar.CloseAdapterCompleteHandlerEx = _CloseAdapterComplete;
		ProtocolChar.SendNetBufferListsCompleteHandler = _SendComplete;
		ProtocolChar.OidRequestCompleteHandler = _OidRequestComplete;
		ProtocolChar.StatusHandlerEx = _StatusHandlerEx;
		ProtocolChar.BindAdapterHandlerEx = _BindAdapterHandlerEx;
		ProtocolChar.UnbindAdapterHandlerEx = _UnbindAdapterHandlerEx;
		ProtocolChar.UninstallHandler = _ProtocolUninstall;
		ProtocolChar.ReceiveNetBufferListsHandler = _ReceiveNetBufferList;
		ProtocolChar.NetPnPEventHandler = _PnpEventHandler;

		//注册网络协议
		NdisStatus = NdisRegisterProtocolDriver(
			NULL ,
			&ProtocolChar ,
			&g_ProtocolDriver );
		if (NdisStatus != NDIS_STATUS_SUCCESS)
		{
			KdPrint( ("DriverEntry: Fail to register protocol dirver,status=%x" ,
				NdisStatus) );
			break;
		}

		//关联协议驱动和小端口驱动
		NdisIMAssociateMiniport(
			g_MiniportDriver ,
			g_ProtocolDriver );

	} while (FALSE);

	//如果操作失败
	if (NdisStatus != NDIS_STATUS_SUCCESS)
	{
		if (g_MiniportDriver)
		{	//卸载小端口驱动
			NdisMDeregisterMiniportDriver( g_MiniportDriver );
		}
		
		if (g_ProtocolDriver)
		{	//卸载协议驱动
			NdisDeregisterProtocolDriver( g_ProtocolDriver );
		}
	}

	return	NdisStatus;
}


/*
该函数通过调用NdisRegisterDeviceEx来生成一个设备,
该函数会被我们的miniportInitializeEx调用
NOTE: do not call this from DriverEntry; it will prevent the driver
from being unloaded (e.g. on uninstall)
参数:无
返回值:	操作状态
*/
NDIS_STATUS	_RegisterDevice()
{
	NDIS_STATUS	NdisStatus = NDIS_STATUS_SUCCESS;
	UNICODE_STRING	DeviceName;
	UNICODE_STRING	Win32DeviceName;
	NDIS_DEVICE_OBJECT_ATTRIBUTES	DeviceObjectAttr;
	PDRIVER_DISPATCH	DispatchTable[IRP_MJ_MAXIMUM_FUNCTION + 1];

	KdPrint( ("[_RegisterDeivce]\n") );

	NdisAcquireSpinLock( &g_SpinLock );
	g_MiniportCount++;
	NdisReleaseSpinLock( &g_SpinLock );
	if (g_MiniportCount == 1)
	{
		NdisZeroMemory(
			DispatchTable ,
			IRP_MJ_MAXIMUM_FUNCTION + 1 );

		DispatchTable[IRP_MJ_CREATE] = _ControlDeviceDispatch;
		DispatchTable[IRP_MJ_CLEANUP] = _ControlDeviceDispatch;
		DispatchTable[IRP_MJ_CLOSE] = _ControlDeviceDispatch;
		DispatchTable[IRP_MJ_DEVICE_CONTROL] = _ControlDeviceDispatch;

		NdisInitUnicodeString(
			&DeviceName ,
			L"\\Device\\PowerfulGun_NdisIMDriver_Control" );
		NdisInitUnicodeString(
			&Win32DeviceName ,
			L"\\DosDevices\\PowerfulGun_NdisIMDriver_Control" );

		//填写DOATTR
		NdisZeroMemory(
			&DeviceObjectAttr ,
			sizeof( NDIS_DEVICE_OBJECT_ATTRIBUTES ) );

		DeviceObjectAttr.Header.Type =
			NDIS_OBJECT_TYPE_DEFAULT;
		DeviceObjectAttr.Header.Revision=
			NDIS_DEVICE_OBJECT_ATTRIBUTES_REVISION_1;
		DeviceObjectAttr.Header.Size = 
			sizeof( NDIS_DEVICE_OBJECT_ATTRIBUTES );
		DeviceObjectAttr.DeviceName = &DeviceName;
		DeviceObjectAttr.SymbolicName = &Win32DeviceName;
		DeviceObjectAttr.MajorFunctions = &DispatchTable[0];
		DeviceObjectAttr.ExtensionSize = 0;	//没有设备扩展
		DeviceObjectAttr.DefaultSDDLString = NULL;
		DeviceObjectAttr.DeviceClassGuid = 0;

		NdisStatus = NdisRegisterDeviceEx(
			g_MiniportDriver ,
			&DeviceObjectAttr ,
			&g_ControlDeviceObject ,
			&g_hControlDevice );
		if (NdisStatus != NDIS_STATUS_SUCCESS)
			KdPrint( ("_RegisterDevice.\
			NdisRegisterDeviceEx fail,status=%x\n" ,
			NdisStatus) );
	}// if (g_MiniportCount == 1)

	return	NdisStatus;
}


/*
该函数处理我们的控制设备收到的IRP请求
*/
NTSTATUS	_ControlDeviceDispatch(
	IN	PDEVICE_OBJECT	_pDeviceObject ,
	IN	PIRP	_pIrp
)
{
	NTSTATUS	Status = STATUS_SUCCESS;
	PIO_STACK_LOCATION	pIrpStack =
		IoGetCurrentIrpStackLocation( _pIrp );

	KdPrint( ("[_ControlDeviceDispatch]\n") );

	switch (pIrpStack->MajorFunction)
	{
		case IRP_MJ_CREATE:
			break;

		case IRP_MJ_CLEANUP:
			break;
			
		case IRP_MJ_DEVICE_CONTROL:
			break;

		case IRP_MJ_CLOSE:
			break;

		default:
			break;
	}

	_pIrp->IoStatus.Status = Status;
	IoCompleteRequest( _pIrp , IO_NO_INCREMENT );

	return	Status;
}


/*
该函数注销我们的控制设备,该函数会在小端口设备停止的时候被调用,
如果小端口的数量减之0就删除我们的控制设备
返回值: 操作状态
*/
NDIS_STATUS	_DeregisterDevice()
{
	NDIS_STATUS	NdisStatus = NDIS_STATUS_SUCCESS;

	KdPrint( ("[_DeregisterDevice]\n") );

	ASSERT( g_MiniportCount > 0 );

	NdisInterlockedDecrement( &g_MiniportCount );
	if (g_MiniportCount == 0)
	{
		ASSERT( ControlDeviceState == PS_DEVICE_STATE_READY );

		NdisAcquireSpinLock( &g_SpinLock );
		ControlDeviceState = PS_DEVICE_STATE_DELETING;
		NdisReleaseSpinLock( &g_SpinLock );

		if (g_hControlDevice != NULL)
		{
			NdisDeregisterDeviceEx( g_hControlDevice );
			g_hControlDevice = NULL;
		}

		NdisAcquireSpinLock( &g_SpinLock );
		ControlDeviceState = PS_DEVICE_STATE_READY;
		NdisReleaseSpinLock( &g_SpinLock );
	}

	return	NdisStatus;
}

