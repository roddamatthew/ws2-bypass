#pragma once

#include <stdbool.h>

NTSTATUS create_socket(PHANDLE SocketHandle, int SocketType, int SocketProtocol);
NTSTATUS bind_socket(PHANDLE SocketHandle, uint8_t* src_addr, uint16_t sport);
NTSTATUS connect_socket(PHANDLE SocketHandle, uint8_t* dst_addr, uint16_t dport);
NTSTATUS send_packet(PHANDLE SocketHandle, char* pkt, uint64_t len);
NTSTATUS recv_packet(PHANDLE SocketHandle, char* buf, uint64_t len);

int is_valid_ipv4(char* host);
int ipv4_to_arr(char* host, uint8_t* arr);