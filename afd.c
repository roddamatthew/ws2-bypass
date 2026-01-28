#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <Windows.h>
#include <winternl.h>

#include "afd_types.h"
#include "afd_funcs.h"

#pragma comment(lib, "ntdll.lib")

int is_valid_ipv4(char *host)
{
	const char delimiter[2] = ".";
	char *token;
	uint8_t octet;

	// Make a local copy of the string to edit
	char buf[32];
	strncpy(buf, host, sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';

	// Iterate through the four octets
	token = strtok(buf, delimiter);
	for (int i = 0; i < 4; i++) {
		if (token == NULL) return FALSE;

		// Check each is an octet
		int result = sscanf(token, "%hhu", &octet);
		if (result != 1) return FALSE;

		token = strtok(NULL, delimiter);
	}
	return TRUE;
}

// Convert an IPv4 address from a string to uint8_t[4] array
int ipv4_to_arr(char* host, uint8_t* arr)
{
	int count = sscanf(
		host, "%hhu.%hhu.%hhu.%hhu",
		&(arr[0]), &(arr[1]), &(arr[2]), &(arr[3])
	);
	if (count == 4) return TRUE;
	return FALSE;
}

// WARNING: Non-IPv4 TCP sockets aren't supported.
NTSTATUS create_socket(
	PHANDLE SocketHandle,
	int SocketType,
	int SocketProtocol
) {
	HANDLE hEvent = NULL;
	PHANDLE hSocket = SocketHandle;
	DWORD dwStatus = -1;

	IO_STATUS_BLOCK IoStatusBlock = { 0 };
	OBJECT_ATTRIBUTES ObjectAttributes = { 0 };
	UNICODE_STRING ObjectFilePath = { 0 };

	// Initialize the AFD_CREATE_PACKET structure
	AFD_CREATE_PACKET packet = {
		.NextEntryOffset = 0,
		.Flags = 0,
		.EaNameLength = sizeof("AfdOpenPacketXX") - 1,
		.EaValueLength = 0x1E,
		.EaName = "AfdOpenPacketXX",
		.EndpointFlags = 0,
		.GroupID = 0,
		.AddressFamily = AF_INET,
		.SocketType = SocketType,
		.Protocol = SocketProtocol
	};

	// Set AFD endpoint path
	memset((void*)&ObjectFilePath, 0, sizeof(ObjectFilePath));
	ObjectFilePath.Buffer = L"\\Device\\Afd\\Endpoint";
	ObjectFilePath.Length = wcslen(ObjectFilePath.Buffer) * sizeof(wchar_t);
	ObjectFilePath.MaximumLength = ObjectFilePath.Length;

	// Initialize ObjectAttributes struct
	memset((void*)&ObjectAttributes, 0, sizeof(ObjectAttributes));
	ObjectAttributes.Length = sizeof(ObjectAttributes);
	ObjectAttributes.ObjectName = &ObjectFilePath;
	ObjectAttributes.Attributes = 0x40;

	// Create socket handle
	IoStatusBlock.Status = 0;
	IoStatusBlock.Information = NULL;

	// Make syscall directly
	dwStatus = NtCreateFile(SocketHandle,
		GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE | WRITE_DAC,
		&ObjectAttributes, &IoStatusBlock, NULL, 0,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN_IF, 0, &packet, sizeof(packet)
	);

	return dwStatus;
}

NTSTATUS bind_socket(
	PHANDLE SocketHandle,
	uint8_t* src_addr,
	uint16_t sport
) {
	// Construct the input to send with the IOCTL
	AFD_BIND_DATA bind_data = { 0 };
	bind_data.flags = AFD_NORMALADDRUSE;
	bind_data.addr.sa_family = AF_INET;

	printf(
		"Attempting to bind to %hhu.%hhu.%hhu.%hhu:%hu\n",
		src_addr[0], src_addr[1], src_addr[2], src_addr[3], sport
	);
	
	// Convert source port to big endian
	bind_data.addr.sa_data[0] = sport >> 8; // Upper byte
	bind_data.addr.sa_data[1] = sport & 255; // Lower byte

	// Bind to source address
	bind_data.addr.sa_data[2] = src_addr[0];
	bind_data.addr.sa_data[3] = src_addr[1];
	bind_data.addr.sa_data[4] = src_addr[2];
	bind_data.addr.sa_data[5] = src_addr[3];

	// Make an empty buffer to store the output
	uint8_t OutputBufferUnused[0x10] = { 0 };
	IO_STATUS_BLOCK IoStatusBlock = { 0 };

	// Send the IOCTL manually
	NTSTATUS Status = NtDeviceIoControlFile(
		*SocketHandle, NULL, NULL, NULL,
		&IoStatusBlock, IOCTL_AFD_BIND,
		&bind_data, sizeof(bind_data),
		OutputBufferUnused, sizeof(OutputBufferUnused)
	);

	// Wait for the status while it's blocking
	if (Status == STATUS_PENDING) {
		WaitForSingleObject(*SocketHandle,INFINITE);
		Status = IoStatusBlock.Status;
	}
	return Status;
}

NTSTATUS connect_socket(
	PHANDLE SocketHandle,
	uint8_t* dst_addr,
	uint16_t dport
) {
	AFD_CONNECT_DATA connect_data = { 0 };
	connect_data.sanActive = 0;
	connect_data.rootEndpoint = 0;
	connect_data.connectEndpoint = 0;
	connect_data.addr.sa_family = AF_INET;

	// Convert destination port to big endian
	connect_data.addr.sa_data[0] = dport >> 8;
	connect_data.addr.sa_data[1] = dport & 255;

	// Set target IPv4 address
	connect_data.addr.sa_data[2] = dst_addr[0];
	connect_data.addr.sa_data[3] = dst_addr[1];
	connect_data.addr.sa_data[4] = dst_addr[2];
	connect_data.addr.sa_data[5] = dst_addr[3];

	printf(
		"Attempting to connect to %hhu.%hhu.%hhu.%hhu:%hu\n",
		dst_addr[0], dst_addr[1], dst_addr[2], dst_addr[3], dport
	);

	IO_STATUS_BLOCK IoStatusBlock = { 0 };

	// Send the IOCTL manually
	NTSTATUS Status = NtDeviceIoControlFile(
		*SocketHandle, NULL, NULL, NULL,
		&IoStatusBlock, IOCTL_AFD_CONNECT,
		&connect_data, sizeof(connect_data),
		NULL, NULL
	);

	// Wait for the status while it's blocking
	if (Status == STATUS_PENDING) {
		WaitForSingleObject(*SocketHandle, INFINITE);
		Status = IoStatusBlock.Status;
	}
	return Status;
}

NTSTATUS send_packet(PHANDLE SocketHandle, char* pkt, uint64_t len)
{
	// Make the packet buffer
	AFD_BUF buf = { 0 };
	buf.buf = pkt;
	buf.len = len;
	
	// Make the packet buffer holder
	AFD_SEND_DATA send_data = { 0 };
	send_data.buffersArray = &buf;
	send_data.buffersCount = 1; // Always just send one at a time
	send_data.afdFlags = 0;

	printf("Attempting to send packet: %s\n", pkt);

	// Make the IOCTL manually
	IO_STATUS_BLOCK IoStatusBlock = { 0 };
	NTSTATUS Status = NtDeviceIoControlFile(
		*SocketHandle, NULL, NULL, NULL,
		&IoStatusBlock, IOCTL_AFD_SEND,
		&send_data, sizeof(AFD_SEND_DATA),
		NULL, NULL
	);

	if (Status == STATUS_PENDING) {
		WaitForSingleObject(*SocketHandle, INFINITE);
		Status = IoStatusBlock.Status;
	}

	return Status;
}

NTSTATUS recv_packet(PHANDLE SocketHandle, char* data, uint64_t len)
{
	// Make the buffer to recieve into
	AFD_BUF buf = { 0 };
	buf.buf = data;
	buf.len = len;

	// Make the recieve buffer holder
	AFD_SEND_DATA recv_data = { 0 };
	recv_data.buffersArray = &buf;
	recv_data.buffersCount = 1; // Always just recv one packet
	recv_data.afdFlags = 0x20; // RECEIVE_NORMAL flag

	// Make the IOCTL manually
	IO_STATUS_BLOCK IoStatusBlock = { 0 };
	NTSTATUS Status = NtDeviceIoControlFile(
		*SocketHandle, NULL, NULL, NULL,
		&IoStatusBlock, IOCTL_AFD_RECV,
		&recv_data, sizeof(AFD_SEND_DATA),
		NULL, NULL
	);

	if (Status == STATUS_PENDING) {
		WaitForSingleObject(*SocketHandle, INFINITE);
		Status = IoStatusBlock.Status;
	}

	return Status;
}