#include	"shared_head.h"

//
//	ȫ�ֱ���
//

NDIS_SPIN_LOCK	g_SpinLock;
PADAPT	g_pAdaptList = NULL;	//�����������ĵ�����
ULONG	g_MiniportCount = 0;	//��ʼ��С�˿ڵĴ���
NDIS_HANDLE	g_MiniportDriver = NULL;
NDIS_HANDLE	g_ProtocolDriver = NULL;
NDIS_HANDLE	g_hControlDevice = NULL;	//�����豸�ľ��
PDEVICE_OBJECT	g_ControlDeviceObject = NULL;//�����豸����

enum _DEVICE_STATE
{
	PS_DEVICE_STATE_READY = 0 ,    // ready for create/delete
	PS_DEVICE_STATE_CREATING ,    // create operation in progress
	PS_DEVICE_STATE_DELETING    // delete operation in progress
} ControlDeviceState = PS_DEVICE_STATE_READY;




/*
DriverEntry��Ҫ������:
1.����NdisMRegisterMiniportDriverע��С�˿�����
2.����NdisRegisterProtocolDriverע��Э������
3.����NdisIMAssociateMiniport������֪ͨ NDIS 
  �й����������΢�Ͷ˿����غ�����Э��֮��Ĺ���
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

	//��ʼ��ȫ��������
	NdisAllocateSpinLock( &g_SpinLock );

	do
	{
		//Ϊ��ע��С�˿�������Ҫ����дС�˿������ṹ
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

		//������ndis6.0�µ������ṹ�ĺ���ָ�븳ֵ
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

		MPChar.Flags = NDIS_INTERMEDIATE_DRIVER;//�м��������־

		//ע��С�˿�����
		NdisStatus = NdisMRegisterMiniportDriver(
			_pDriverObject ,
			_pRegistryPath ,
			NULL ,	//��Context�ᴫ��MiniportInitializeEx�Ĳ���MiniportInitParameters.IMDeviceInstanceContext
			&MPChar,
			&g_MiniportDriver );
		if (NdisStatus != NDIS_STATUS_SUCCESS)
		{
			KdPrint( ("DriverEntry: Fail to register miniport driver,status=%x\n" ,
				NdisStatus) );
			break;
		}

		//Ϊ��ע��Э��������Ҫ��дЭ�����������ṹ��
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

		//ע������Э��
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

		//����Э��������С�˿�����
		NdisIMAssociateMiniport(
			g_MiniportDriver ,
			g_ProtocolDriver );

	} while (FALSE);

	//�������ʧ��
	if (NdisStatus != NDIS_STATUS_SUCCESS)
	{
		if (g_MiniportDriver)
		{	//ж��С�˿�����
			NdisMDeregisterMiniportDriver( g_MiniportDriver );
		}
		
		if (g_ProtocolDriver)
		{	//ж��Э������
			NdisDeregisterProtocolDriver( g_ProtocolDriver );
		}
	}

	return	NdisStatus;
}


/*
�ú���ͨ������NdisRegisterDeviceEx������һ���豸,
�ú����ᱻ���ǵ�miniportInitializeEx����
NOTE: do not call this from DriverEntry; it will prevent the driver
from being unloaded (e.g. on uninstall)
����:��
����ֵ:	����״̬
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

		//��дDOATTR
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
		DeviceObjectAttr.ExtensionSize = 0;	//û���豸��չ
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
�ú����������ǵĿ����豸�յ���IRP����
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
�ú���ע�����ǵĿ����豸,�ú�������С�˿��豸ֹͣ��ʱ�򱻵���,
���С�˿ڵ�������֮0��ɾ�����ǵĿ����豸
����ֵ: ����״̬
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

