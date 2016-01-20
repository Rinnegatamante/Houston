#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define BUFFER_SIZE 10485760

typedef struct
{
	int sock;
	struct sockaddr_in addrTo;
} Socket;

int sendData(int socket, int sendsize, FILE* handle) {
	char* buffer = malloc(BUFFER_SIZE);
	int oldpos = sendsize;
	while(sendsize) {
		fseek(handle, oldpos - sendsize, SEEK_SET);
		fread(buffer, BUFFER_SIZE, 1, handle);
		int len = 0;
		if (BUFFER_SIZE <= sendsize) len = send(socket, buffer, BUFFER_SIZE, 0);
		else len = send(socket, buffer, sendsize, 0);
		if (len <= 0) break;
		sendsize -= len;
	}
	free(buffer);
	return sendsize <= 0;
}

int main(int argc,char** argv){

	// Getting arguments
	if (argc != 3){
		printf("Invalid syntax!\n\nUsage: houston 3DS_IP CIA_FILENAME");
		return -1;
	}
	char* host = (char*)(argv[1]);
	char* cia_file = (char*)(argv[2]);
	
	// Writing info on the screen
	printf("Houston v.1.0\n");
	
	// Creating client socket
	Socket my_socket;
	memset(&my_socket.addrTo, 0, sizeof(my_socket.addrTo));
	my_socket.addrTo.sin_family = AF_INET;
	my_socket.addrTo.sin_port = htons(5000);
	my_socket.addrTo.sin_addr.s_addr = inet_addr(host);
	my_socket.sock = socket(AF_INET, SOCK_STREAM, 0);
	if (my_socket.sock < 0){
		printf("\nFailed creating socket.");	
		return -1;
	}else printf("\nClient socket created on port 5000");
	fflush(stdout);
	
	// Connecting to NASA
	int err = connect(my_socket.sock, (struct sockaddr*)&my_socket.addrTo, sizeof(my_socket.addrTo));
	if (err < 0 ){ 
		printf("\nFailed connecting server.");
		close(my_socket.sock);
		return -1;
	}else printf("\nConnection estabilished, waiting for NASA response...");
	fflush(stdout);
	
	// Waiting for magic
	char data[25];
	int count = recv(my_socket.sock, &data, 25, 0);
	while (count < 0){
		int count = recv(my_socket.sock, &data, 25, 0);
	}
	if (strncmp(data,"HOUSTON, WE GOT A PROBLEM",25) == 0) printf("\nMagic received, starting transfer...");
	else{
		printf("\nWrong magic received, connection aborted.");
		close(my_socket.sock);
		return -1;
	}
	fflush(stdout);
	
	// Transfering CIA file
	printf("\nOpening %s ... ",cia_file);
	FILE* input = fopen(cia_file,"r");
	if (input < 0){
		printf("\nFile not found.");
		close(my_socket.sock);
		return -1;
	}
	fseek(input, 0, SEEK_END);
	int size = ftell(input);
	fseek(input, 0, SEEK_SET);
	printf("Done! (%i KBs)\nSending filesize... ",(size/1024));
	fflush(stdout);
	send(my_socket.sock, &size, 4, 0);
	count = recv(my_socket.sock, &data, 8, 0);
	while (count < 0){
		count = recv(my_socket.sock, &data, 8, 0);
	}
	printf("Done!\nSending file... ");
	fflush(stdout);
	sendData(my_socket.sock, size, input);
	count = recv(my_socket.sock, &data, 8, 0);
	while (count < 0){
		count = recv(my_socket.sock, &data, 8, 0);
	}
	printf("Done!\nFile successfully sent!\n\nSee you Space Cowboy...");
	fflush(stdout);
	fclose(input);
	close(my_socket.sock);
	return 0;
	
}