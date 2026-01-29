#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <Windows.h>

#include "afd_types.h"
#include "afd_funcs.h"

int main(int argc, char** argv)
{
	if (argc < 5) {
		printf("Error! Must call with a source and destination IPv4 and port\n");
		printf("Expected format %s <src_ip> <src_port> <dst_ip> <dst_port>\n", argv[0]);
		printf("e.g. %s 10.0.0.1 27000 10.0.0.2 1337\n", argv[0]);
		exit(1);
	}

	NTSTATUS Status;
	HANDLE hSocket = NULL;
	char src_addr[32] = { 0 };
	char dst_addr[32] = { 0 };
	uint8_t src_ipv4[4];
	uint8_t dst_ipv4[4];
	uint16_t sport, dport;

	// Parse command line IPs
	strncpy(src_addr, argv[1], 32);
	strncpy(dst_addr, argv[3], 32);
	if (!is_valid_ipv4(src_addr) || !is_valid_ipv4(dst_addr)) {
		printf("Error! Failed to parse IPv4 address\n");
		exit(1);
	}

	// Read the command line ports
	int sport_result = sscanf(argv[2], "%hu", &sport);
	int dport_result = sscanf(argv[4], "%hu", &dport);
	if (sport_result != 1 || dport_result != 1) {
		printf("Error! Failed to parse port\n");
		exit(1);
	}

	// Convert IPv4 to uint8_t arrays
	ipv4_to_arr(src_addr, src_ipv4);
	ipv4_to_arr(dst_addr, dst_ipv4);

	// Make a IPv4 TCP socket
	Status = create_socket(&hSocket, SOCK_STREAM, IPPROTO_TCP);
	if (Status < 0) {
		printf("Error! create_socket failed with 0x%X\n", Status);
		exit(1);
	}
	printf("[+] Socket created successfully.\n");

	// At this point we should be able to inspect hSocket
	if (hSocket == NULL) {
		printf("Error! Socket was NULL after create_socket!?\n");
		exit(1);
	}

	// Bind the newly created socket
	Status = bind_socket(&hSocket, src_ipv4, sport);
	if (Status < 0) {
		printf("Error! bind_socket failed with 0x%X\n", Status);
		exit(1);
	}
	printf("[+] Socket bound successfully.\n");

	// Connect to the target
	Status = connect_socket(&hSocket, dst_ipv4, dport);
	if (Status < 0) {
		printf("Error! connect_socket failed with 0x%X\n", Status);
		exit(1);
	}
	printf("[+] Socket connected successfully.\n");
	
	// Send a packet
	Status = send_packet(&hSocket, "HelloWorld\n", sizeof("HelloWorld\n"));
	if (Status < 0) {
		printf("Error! send_packet failed with 0x%X\n", Status);
		exit(1);
	}
	printf("[+] Sent packet successfully.\n");
	printf("[+] Waiting for server to send response...\n");

	// Recieve a response
	char response_buffer[32] = { 0 };
	Status = recv_packet(&hSocket, response_buffer, sizeof(response_buffer));
	if (Status < 0) {
		printf("Error! recv_packet failed with 0x%X\n", Status);
		exit(1);
	}
	printf("[+] Recieved packet successfully.\n");
	printf("[+] Packet contents are: %s\n", response_buffer);

	if (strncmp(response_buffer, "SuperSecretSecret\n", "32") == 0) {
		printf("Successfuly triggered sample!\n");
		send_packet(&hSocket, "Success!\n", sizeof("Success!\n"));
	}
	else
		printf("Incorrect response\n");
	return 0;
}