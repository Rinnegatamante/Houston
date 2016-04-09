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

int sendData(int socket, int sendsize, FILE* handle, int cmd_socket) {
	char cmd_buf[32768];
	char* buffer = malloc(BUFFER_SIZE);
	int oldpos = sendsize;
	while(sendsize) {
		int count = recv(cmd_socket, &cmd_buf, 32768, 0);
		if (count > 0){
			int data_ptr = count;
			while (count > 0){
				count = recv(cmd_socket, &cmd_buf[data_ptr], 32768, 0);
				data_ptr = data_ptr + count;
			}
			printf("\nReceived: %s",cmd_buf);
			fflush(stdout);
			memset(cmd_buf,0, 32768);
			count = -1;
			data_ptr = 0;
			return -1;
		}
		fseek(handle, oldpos - sendsize, SEEK_SET);
		fread(buffer, BUFFER_SIZE, 1, handle);
		int len = 0;
		if (BUFFER_SIZE <= sendsize) len = send(socket, buffer, BUFFER_SIZE, 0);
		else len = send(socket, buffer, sendsize, 0);
		if (len <= 0) break;
		sendsize -= len;
	}
	free(buffer);
	return 0;
}

char ret_code[4];

void waitResponse(int socket){
	char data[32768];
	int data_ptr = 0;
	int count = -1;
	while (count < 0){
		count = recv(socket, &data, 32768, 0);
		data_ptr = count;
	}
	while (count > 0){
		count = recv(socket, &data[data_ptr], 32768, 0);
		data_ptr = data_ptr + count;
	}
	data_ptr = 0;
	count = -1;
	printf("\nReceived: %s",data);
	fflush(stdout);
	snprintf(ret_code,4,data);
	memset(data, 0, 32768);
}

int main(int argc,char** argv){

	// Getting arguments
	if (argc != 4){
		printf("Invalid syntax!\n\nUsage: houston 3DS_IP CIA_FILENAME INSTALLDIR");
		return -1;
	}
	char* host = (char*)(argv[1]);
	char* cia_file = (char*)(argv[2]);
	char* install_to = (char*)(argv[3]);
	
	// Writing info on the screen
	printf("Houston v.1.1\n");
	printf("-------------\n");
	printf("This version of Houston supports only NASA v.1.6 or higher!\n");
	
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
	
	// Setting as non-block the socket
	int flags = fcntl(my_socket.sock, F_GETFL, 0);	
	fcntl(my_socket.sock, F_SETFL, flags | O_NONBLOCK);
	
	// Waiting for greetings
	waitResponse(my_socket.sock);
	char cmd[32768];
	
	// Changing directory for NAND/SDMC selection
	sprintf(cmd,"CWD /%s",install_to);
	printf("\nExecuting: %s",cmd);
	fflush(stdout);
	send(my_socket.sock, cmd, strlen(cmd), 0);
	waitResponse(my_socket.sock);
	
	// Opening Data Socket
	Socket data_socket;
	memset(&data_socket.addrTo, 0, sizeof(data_socket.addrTo));
	data_socket.addrTo.sin_family = AF_INET;
	data_socket.addrTo.sin_port = htons(5001);
	data_socket.addrTo.sin_addr.s_addr = inet_addr(host);
	data_socket.sock = socket(AF_INET, SOCK_STREAM, 0);
	if (data_socket.sock < 0){
		printf("\nFailed creating socket.");	
		return -1;
	}else printf("\nData socket created on port 5001");
	fflush(stdout);
	
	// Initializing transfer
	sprintf(cmd,"STOR %s",cia_file);
	printf("\nExecuting: %s",cmd);
	fflush(stdout);
	send(my_socket.sock, cmd, strlen(cmd), 0);
	waitResponse(my_socket.sock);
	
	// Connecting Data Socket
	err = connect(data_socket.sock, (struct sockaddr*)&data_socket.addrTo, sizeof(data_socket.addrTo));
	if (err < 0 ){ 
		printf("\nFailed connecting server.");
		close(data_socket.sock);
		close(my_socket.sock);
		return -1;
	}
	
	// Transfering file
	FILE* input = fopen(cia_file,"r");
	if (input < 0){
		printf("\nFile not found.");
		close(data_socket.sock);
		close(my_socket.sock);
		return -1;
	}
	fseek(input, 0, SEEK_END);
	int size = ftell(input);
	fseek(input, 0, SEEK_SET);
	int res = sendData(data_socket.sock, size, input, my_socket.sock);
	fclose(input);
	close(data_socket.sock);
	
	// If NASA closed transfer, we need to reset STOR status
	if (res < 0){
		sprintf(cmd,"STOR %s",cia_file);
		send(my_socket.sock, cmd, strlen(cmd), 0);
	}
	
	// Finishing file transfer
	waitResponse(my_socket.sock);
	
	// If NASA closed transfer, we need to reset STOR status
	if (strncmp(ret_code,"426",3) == 0){
		sprintf(cmd,"STOR %s",cia_file);
		send(my_socket.sock, cmd, strlen(cmd), 0);
		waitResponse(my_socket.sock);
	}
	
	// Closing connection
	close(my_socket.sock);
	printf("\n"); // Needed for the shitty Mac shell :/
	return 0;
	
}