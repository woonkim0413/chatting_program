#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>

#define BUFFER_SIZE 1024

void *receive_messages(void *arg);
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//------------ 함수 통신 규격 -------------------
// #quit : 프로그램 종료
// #position ~~ : 자신의 포지션 변경
// #info : 현재 접속중인 유저 정보 출력
// ~~~~ : 전체 채팅
//----------------------------------------------

char manual[] = "\n*********************manual*********************\n\
 !help -> Show command manual.\n\
 !quit -> Close chatting application.\n\
 !search -> Search old chat history.    [Usage: !search KEYWORD]\n\
 !showall -> Show all old chat history\n\
 !info -> Show all clients and positions\n\
 !position -> Change position    [Usage: !position POSITION]\n\
************************************************\n\n";

int main(int argc, char *argv[]) { // 실행파일 arg[1]으로 user name, arg[2]로 server IP, arg[3]로 server port를 받음
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char chat[BUFFER_SIZE];
    struct hostent* host;

    if (argc != 4) {
        printf("Usage: [filename] [name] [ip address] [port number]\n");
        exit(-1);
    }
    
    client_socket = socket(AF_INET, SOCK_STREAM, 0); //클라이언트 소켓 생성
    if (client_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[3])); // 서버 포트주소로 변경

    // 1) 도메인/호스트이름(또는 IP 주소)을 `gethostbyname()`로 변환
    host = gethostbyname(argv[2]); // 외부 ip나 domain 또는 localhost입력
    if (host == NULL) {
        herror("gethostbyname failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // 2) 변환된 IP 주소를 소켓 구조체에 복사
    memcpy(&server_addr.sin_addr, host->h_addr, host->h_length);

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        //클라이언트 소켓에서 서버 소켓으로 연결 요청을 보냄 -> 서버의 listen중인 소켓의 대기열로 들어가는 듯 함
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server. Type messages and press Enter to send.\n");
    printf(manual);

    // server client 구조체에 이름 저장하기 위해 사용
    write(client_socket, argv[1], strlen(argv[1]));

    pthread_t thread;
    pthread_create(&thread, NULL, receive_messages, (void *)&client_socket); 
    // receive_message함수를 수행하는 쓰레드 생성, 인자로 클라이언트 소켓 주소 전달함 해당 과정에서
    // 메세지를 보내는 thread와 메세지를 받는 thread가 분기되어 각자 동작함

    while (fgets(buffer, sizeof(buffer), stdin) != NULL) { // enter 개행 포함하여 인식함
        //표준입력으로 메세지를 입력받음, 이후 입력 대기상태

         size_t len = strlen(buffer);
         buffer[len - 1] = '\0'; //fgets로 받은 개행을 제거함

        if (!strcmp(buffer, "!help"))
        {
            printf(manual);
            continue;
        }
         sprintf(chat, "%s", buffer);
         send(client_socket, chat, strlen(chat), 0); //입력받은 메세지를 자기 자신 소켓으로 전달
         if (strcmp(buffer, "!quit") == 0)
            break;
    }

    close(client_socket);
    return 0;
}

// 데이터를 받아서 출력하는 로직을 처리
void *receive_messages(void *arg) {
    int socket = *(int *)arg;
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = recv(socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        //서버에서 온 데이터를 자기 자신의 터미널에 출력
        buffer[bytes_read] = '\0';
        printf("%s\n", buffer);
        fflush(stdout); // stdout buffer을 즉시 stdout에 출력하는 명령어임 출력 지연 방지

        if (!strcmp(buffer, "exit.... end")) {
            exit(0);
        }
    }
    return NULL;
}
