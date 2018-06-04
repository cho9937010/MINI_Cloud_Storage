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
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define PORT            5883
#define ID_LEN          4
#define PATH_BUFLEN     30
#define PARAM_BUFLEN    40
#define MAXLINE         99999
#define BUFSIZE         4096

#define CLOUD_ROOT      "/home/ubuntu/Cloud/" // modify for server environment

typedef enum { LIST, UPLOAD, DOWNLOAD, REMOVE, PRIVATE, SHARE, EXIT } command_t;

void error_handling(char *message);

int recvn(int s, char *buf, int len);

// int server_file_download(int sockfd, char* filename);
// int server_file_upload(int sockfd, char *filename);

int main()
{
    int server_sock;
    int client_sock;
    int retval;
    int len;
    int totalbytes = 0;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_addrlen;

    int pid;
    char id[ID_LEN + 1] = { 0 };
    char path[PATH_BUFLEN] = { 0 };
    char param[PARAM_BUFLEN] = { 0 };
    char filename[256] = { 0 };
    char buf[BUFSIZE];
    char msg[BUFSIZE];

    struct stat st;
    command_t command;

    // Cloud directory
    if (stat(CLOUD_ROOT, &st) == -1)  // if path dose not exist
      mkdir(path, 0777);

    // socket()
    if ((server_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        error_handling("socket() error");

    // set server_addr
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // bind(), listen()
    if (bind(server_sock , (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
        error_handling("bind() error");
    if (listen(server_sock, 5) == -1)
        error_handling("listen() error");

    // start accepting loop
    while (1) {
        // accept()
        if ((client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &client_addrlen)) == -1)
            error_handling("accept() error");

        // print ip and fd
        fputs(inet_ntoa(client_addr.sin_addr), stdout);
        printf(" connected to %d\n", client_sock);

        // fork()
        if ((pid = fork()) == -1)
            error_handling("fork() error");

        // parent process -> repeat accept()
        // child process -> do below
        /**** child process start ****/
        if (pid == 0) {
            // receive ID  (ID consists of 4 alphabets)
            if (read(client_sock, id, ID_LEN) <= 0) {
                // return value 0 -> client_sock is closed now
                // return value -1 -> error
                close(client_sock);
                exit(0);
            }
            printf("[%s] fd: %d\n", id, client_sock);

            // access user's directory
            strcpy(path, CLOUD_ROOT);
            strcat(path, id);

            if (stat(path, &st) == -1)  // if path dose not exist
              mkdir(path, 0777);
            chdir(path);

            // current working directory
            printf("[%s] path: %s\n", id, getcwd(NULL, 0));

            // start commands receiving loop
            while (1) {
                // read a command
                if (read(client_sock, &command, 4) <= 0) {
                    close(client_sock);
                    exit(0);
                }

                printf("[%s] ", id);

                // exit branch
                if (command == EXIT) {
                    printf("[%s] user exit\n", id);
                    break;
                }

                // command switching
                switch (command) {
                case LIST:   // ls
                    puts("-->클라이언트로부터 파일 리스트 전송 요청\n");
                    DIR *d = opendir(".");;
                    struct dirent *dir;
                    memset(filename, 0, sizeof(filename));

                    if (d) {
                      int i=0;
                      strcpy(msg, path);
                      strcat(msg, "\n");
                      printf("%s", msg);
                      while ((dir = readdir(d)) != NULL) {
                        char* tmp = dir->d_name;
                        if (!strcmp(tmp, ".") || !strcmp(tmp, ".."))
                          continue;
                        strcat(msg, tmp);
                        strcat(msg, "\n");
                      }
                      printf("%s", msg);
                      retval = write(client_sock, msg, sizeof(msg));
                      if(retval == -1){
                        error_handling("-->파일 이름 전송 실패");
                        break;
                      }
                      closedir(d);
                    }
                    /* 데이터 전송후 소켓의 일부(전송영역)를 닫음 */
                    if( shutdown(client_sock, SHUT_WR) == -1 )
                        error_handling("shutdown error");
                    printf("-->클라이언트에게 파일 리스트 전송 완료!\n");
                    break;

                case UPLOAD:   // upload
                    puts("클라이언트로부터 파일 업로드 요청\n");

                    // 파일 이름 받는 부분
                		memset(filename, 0, sizeof(filename));

                		retval = recvn(client_sock, filename, 256);
                    if(retval == -1) {
                      error_handling("파일 이름 전송 받기 실패");
                      break;
                		}
                		printf("--> filename : %s\n", filename);

                		totalbytes = 0;
                		retval = recvn(client_sock, (char *) &totalbytes, sizeof(totalbytes));
                		if(retval == -1) {
                      error_handling("파일 사이즈 받기 실패");
                		  break;
                		}
                		printf("--> filesize : %d\n", totalbytes);

                		FILE *fp = fopen(filename, "wb");
                		if(fp == NULL) {
                      error_handling("파일 오픈 에러");
                		  break;
                		}

                    /* 데이터를 전송 받아서 파일에 저장한다 */
                     while( (len=read(client_sock, buf, BUFSIZE )) != 0 )
                     {
                         fwrite(buf, 1, len, fp);
                     }

                		fclose(fp);
                    printf("서버에 파일 업로드 완료!\n");
                    break;

                case DOWNLOAD:   // download
                    puts("클라이언트로부터 파일 다운로드 요청\n");

                    // 파일 이름 받는 부분
                    memset(filename, 0, sizeof(filename));

                    retval = recvn(client_sock, filename, 256);
                    if(retval == -1) {
                      error_handling("파일 이름 전송 받기 실패");
                      break;
                    }
                    printf("--> filename : %s\n", filename);

                    // 파일 열기
                    fp = fopen(filename, "rb");
                    if(fp == NULL){
                      error_handling("file open error");
                      break;
                    }

                    //파일 크기 구하기
                    fseek(fp, 0, SEEK_END);
                    totalbytes = ftell(fp);

                    // 파일 크기 전송
                    retval = write(client_sock, (char *)&totalbytes, sizeof(totalbytes));
                  	if(retval == -1){
                      error_handling("file size send error");
                      break;
                    }


                    //피일 내용 전송
                    rewind(fp);
                    while( (len=fread(buf, 1, totalbytes, fp)) != 0 )
                    {
                        write(client_sock, buf, len);
                    }
                    fclose(fp);

                    /* 데이터 전송후 소켓의 일부(전송영역)를 닫음 */
                    if( shutdown(client_sock, SHUT_WR) == -1 )
                        error_handling("shutdown error");

                    printf("클라이언트에게 파일 전송 완료!\n");
                    break;

                case REMOVE:   // remove
                    // 파일 이름 받는 부분
                    memset(filename, 0, sizeof(filename));
                    retval = recvn(client_sock, filename, 256);
                    if(retval == -1) {
                      error_handling("파일 이름 전송 받기 실패");
                      break;
                    }
                    printf("remove %s\n", filename);

             		    if(!remove(filename))
              			     printf("File deleted successfully");
             		    else
                			   printf("Error: unable to delete the file");
                    break;

                default :
                    printf("invalid command\n");
                    break;
                }

                // 각 명령어 끝날 때마다 다시 연결
                close(client_sock);
                printf("연결 해제");
            }

            // child process terminates
            close(client_sock);
            return 0;
        }
        /**** child process end ****/
    }
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

void error_handling(char *message) {
    perror(message);
    exit(1);
}
