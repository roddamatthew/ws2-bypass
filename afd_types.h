#pragma once

#include <stdbool.h>
#include <stdint.h>

#define IOCTL_AFD_BIND		0x12003
#define IOCTL_AFD_CONNECT	0x12007
#define IOCTL_AFD_SEND		0x1201F
#define IOCTL_AFD_RECV		0x12017

#define AFD_NORMALADDRUSE 0

typedef enum _EVENT_TYPE
{
	NotificationEvent,
	SynchronizationEvent
} EVENT_TYPE;

#pragma pack(push, 1)
typedef struct _AFD_CREATE_PACKET
{
	uint32_t NextEntryOffset;
	uint8_t Flags;
	uint8_t EaNameLength;
	uint16_t EaValueLength;
	char EaName[16];
	uint16_t unknown1;
	uint16_t EndpointFlags;
	uint32_t GroupID;
	uint32_t AddressFamily;
	uint32_t SocketType;
	uint32_t Protocol;
	uint64_t unknown2;
	uint64_t unknown3;
	uint32_t unknown4;
} AFD_CREATE_PACKET;
#pragma pack(pop)

typedef struct _AFD_BIND_DATA
{
	uint32_t flags;
	SOCKADDR addr;
} AFD_BIND_DATA;

typedef struct _AFD_CONNECT_DATA
{
	uint64_t sanActive;
	uint64_t rootEndpoint;
	uint64_t connectEndpoint;
	SOCKADDR addr;
} AFD_CONNECT_DATA;

typedef struct {
	uint64_t len;
	uint8_t* buf;
} AFD_BUF;

typedef struct _AFD_SEND_DATA {
	AFD_BUF* buffersArray;
	uint64_t buffersCount;
	uint64_t afdFlags;
	uint64_t tdiFlags; // Optional
} AFD_SEND_DATA;