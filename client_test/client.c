/*
    Computer Network Term Project
    Mini Cloud Storage

    client.c
    2018/05/20
    Cho Jeong Jae, Jeong Min Hyuk, Kim Hyeon Jin, Lee Sang Yeop
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define SHARE_MOD       "SHRE"
#define PORT            5883
#define ID_LEN          4
#define COMMAND_BUFLEN  50
#define PARAM_BUFLEN    40
#define FILE_PATH_LEN   256
#define MAXLINE         99999
#define BUFSIZE         4096
typedef enum { LIST, UPLOAD, DOWNLOAD, REMOVE, PRIVATE, SHARE, EXIT } command_t;

command_t str_to_command(char *str);
int input_str(char *strbuf, int buflen);
char* input_file_path(char* strbuf, int buflen);
int recvn(int s, char *buf, int len);
void error_handling(char *str);

int main(int argc, char* argv[])
{
    int i;
    int sock;
    int retval;
    int len;
    int totalbytes = 0;

    struct sockaddr_in server_addr;

    char id[ID_LEN + 1] = { 0 };
    char mod[ID_LEN + 1] = { 0 };
    char command_str[COMMAND_BUFLEN] = { 0 };
    char param[PARAM_BUFLEN] = { 0 };
    char file_path[FILE_PATH_LEN] = { 0 };
    char buf[BUFSIZE] = { 0 };
    char filename[256] = { 0 };

    command_t command;

    if(argc != 2){
      printf("Usage : %s <SERVER IP>\n",argv[0]);
      exit(1);
    }

    // get an id and transfer it to server
    fputs("ID: ", stdout);
    while (input_str(id, ID_LEN + 1) == 0 || !strcmp(id, SHARE_MOD))
      fputs("invalid ID\nID: ",stdout); // forbid null id
    strcpy(mod, id);

    // start command line loop
    while (1) {
        // create socket
        if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
            error_handling("sock() error");

        // set server_addr
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(argv[1]);
        server_addr.sin_port = htons(PORT);

        // connect to server
        if (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
            error_handling("connect() error");

        write(sock, mod, 4);
        // get a command
        fputs(">>> ", stdout);
        input_str(command_str, COMMAND_BUFLEN);

        // distinguish command from parameter
        for (i = 0; command_str[i] != '\0'; i++) {
            if (command_str[i] == ' ') {
                command_str[i++] = '\0';
                break;
            }
        }
        memset(param, 0, PARAM_BUFLEN);
        for (; command_str[i] != '\0'; i++) {
            if (command_str[i] != ' ') {
                strcpy(param, &command_str[i]);
                break;
            }
        }

        command = str_to_command(command_str);

        // exit branch
        if (command == EXIT) {
            write(sock, &command, 4);
            break;
        }

        // command switching
        switch (command) {
        case LIST:
            write(sock, &command, 4);
            read(sock, buf, BUFSIZE);
              printf("%s\n", buf);
            break;
        case UPLOAD:
            write(sock, &command, 4);

            puts("upload 실행\n");
            printf("업로드할 파일명 입력 : ");
            // 파일 이름 입력 받기
            memset(filename, 0, sizeof(filename));
            char* tmp = input_file_path(file_path, FILE_PATH_LEN);
            strcpy(filename,tmp);

            // 파일 열기
            FILE *fp = fopen(filename, "rb");
            if(fp == NULL){
              error_handling("file open error");
              break;
            }

            // 파일 이름 전송
            retval = write(sock, filename, sizeof(filename));
            if(retval == -1){
              error_handling("파일 이름 전송 실패");
              break;
            }

            //파일 크기 구하기
            fseek(fp, 0, SEEK_END);
            int totalbytes = ftell(fp);

            // 파일 크기 전송
            retval = write(sock, (char *)&totalbytes, sizeof(totalbytes));
          	if(retval == -1){
              error_handling("file size send error");
              break;
            }

            // 파일 내용 전송
            rewind(fp);
            while( (len=fread(buf, 1, BUFSIZE, fp)) != 0 )
            {
                write(sock, buf, len);
            }
            fclose(fp);

            /* 데이터 전송후 소켓의 일부(전송영역)를 닫음 */
            if( shutdown(sock, SHUT_WR) == -1 )
                error_handling("shutdown error");

            printf("서버에 파일 전송 완료\n");

            break;
        case DOWNLOAD:
            write(sock, &command, 4);

            puts("download 실행\n");
            printf("다운로드할 파일명 입력 : ");
            // 파일 이름 입력 받기
            memset(filename, 0, sizeof(filename));
            char* temp = input_file_path(file_path, FILE_PATH_LEN);
            strcpy(filename,temp);

            // 파일 이름 전송
            retval = write(sock, filename, sizeof(filename));
            if(retval == -1){
              error_handling("파일 이름 전송 실패");
              break;
            }

            totalbytes = 0;
            retval = recvn(sock, (char *) &totalbytes, sizeof(totalbytes));
            if(retval == -1) {
              error_handling("파일 사이즈 받기 실패");
              break;
            }
            printf("--> filesize : %d\n", totalbytes);

            fp = fopen(filename, "wb");
            if(fp == NULL) {
              error_handling("파일 오픈 에러");
              break;
            }

            /* 데이터를 전송 받아서 파일에 저장한다 */
             while( (len=read(sock, buf, BUFSIZE )) != 0 )
             {
                 fwrite(buf, 1, len, fp);
             }

            fclose(fp);
            printf("서버로부터 파일 다운로드 완료\n");
            break;
        case REMOVE:
            write(sock, &command, 4);
            printf("삭제할 파일명 입력 : ");

            // 파일 이름 입력 받기
            memset(filename, 0, sizeof(filename));
            char* temp1 = input_file_path(file_path, FILE_PATH_LEN);
            strcpy(filename,temp1);

            // 파일 이름 전송
            retval = write(sock, filename, sizeof(filename));
            if(retval == -1)
              error_handling("파일 이름 전송 실패");
            break;
        case PRIVATE:
            strcpy(mod, id);
            break;
        case SHARE:
            strcpy(mod, SHARE_MOD);
            break;
        default:
            puts("Invalid command");
        }
        // 각 명령어 끝날때마다 다시 연결
        close(sock);
    }

    // program terminates
    close(sock);
    return 0;
}

command_t str_to_command(char *str) // (char *) to (command_t); for easy progress
{
    if (!strcmp(str, "list") || !strcmp(str, "ls")) {
        return LIST;
    } else if (!strcmp(str, "upload") || !strcmp(str, "up")) {
        return UPLOAD;
    } else if (!strcmp(str, "download") || !strcmp(str, "down")) {
        return DOWNLOAD;
    } else if (!strcmp(str, "remove") || !strcmp(str, "rm")) {
        return REMOVE;
    } else if (!strcmp(str, "private")) {
        return PRIVATE;
    } else if (!strcmp(str, "share")) {
        return SHARE;
    } else if (!strcmp(str, "exit") || !strcmp(str, "quit")) {
        return EXIT;
    } else {
        return -1;
    }
}

int input_str(char *strbuf, int buflen) // get a string with flush and without newline character
{
    fgets(strbuf, buflen, stdin);

    if (strbuf[strlen(strbuf) - 1] == '\n')
        strbuf[strlen(strbuf) - 1] = '\0';
    else
        while (getchar() != '\n');

    return strlen(strbuf);
}

//파일 이름 반환 함수
char* input_file_path(char* strbuf, int buflen) // get a string with flush and without newline character
{
    fgets(strbuf, buflen, stdin);

    if (strbuf[strlen(strbuf) - 1] == '\n')
        strbuf[strlen(strbuf) - 1] = '\0';
    else
        while (getchar() != '\n');

    printf("%s\n",strbuf);
    return strbuf;
}

int recvn(int s, char *buf, int len) {
	int received;
	char *ptr = buf;
	int left = len;

	while(left > 0) {
		received = read(s, ptr, left);
    if(received == -1)
			return -1;
		else if(received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}


void error_handling(char *message)
{
    perror(message);
    exit(1);
}
