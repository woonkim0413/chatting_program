#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>

#define MAX_CLIENTS 10
#define MAX_CHARS 100
#define BUFFER_SIZE 1024
#define INVALID_SOCKET -1

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //쓰레드 간에 공유 데이터 동기화를 위해 사용, mutex초기화
int fd = 0; //대화 내용 저장할 파일 디스크립터

// client 구조체
typedef struct client_List{
    int socket_num;
    char ip[40];
    int port;
    char name[MAX_CHARS];
    char position_description[BUFFER_SIZE];
}client_List;

client_List client_list[MAX_CLIENTS]; // client info를 저장하는 구조체 배열


void *handle_client(client_List *client); // client 관리 함수
void info(client_List *client); // 현재 접속 중인 client 출력
void change_position(char *token, client_List *client);
void showall(client_List *client);
void search_func(char *token, client_List *client);
void syntax_error_print(client_List *client);
void server_quit(void *arg);

int main(int argc, char *argv[]) {
    int server_socket, new_socket; //서버 소켓, 클라이언트 소켓 디스크립터
    struct sockaddr_in server_addr, client_addr; //소켓을 바인드 할 때 특성으로 넣을 구조체(?), 소켓의 주소 정보가 저장되는 구조체
    socklen_t addr_len = sizeof(client_addr); //위 구조체 크기
    char buf1[MAX_CHARS]; //client_list에 정보 넣을 때 주로 사용

    if (argc != 2) {
        printf("Usage: [filename] [port number]\n");
        exit(-1);
    }

    if((fd = open("log.txt", O_RDWR|O_APPEND, 0666)) == -1) { //log를 저장할 "log.txt" 열기
        if((fd = open("log.txt", O_RDWR|O_CREAT, 0666)) == -1) { //"log.txt" 없으면 생성
            perror("LOG file open failed");
            exit(EXIT_FAILURE);
        }
    }

    // client_list의 socket_num에 INVALID_SOCKET(-1)을 넣어서 모두 초기화한다
    for (int i = 0; i < MAX_CLIENTS; i++) { // int i는 해당 for문에서만 유효
        client_list[i].socket_num = INVALID_SOCKET;
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0); //서버 소켓 만들기, IPv4 프로토콜, TCP방식
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET; //주소체계 설정, IPv4 프로토콜 사용하겠다는 뜻
    server_addr.sin_addr.s_addr = INADDR_ANY; //소켓을 바인딩(특정 IP주소, 포트 번호에 연결), 모든 주소에 바인딩할 수 있도록 함
    server_addr.sin_port = htons(atoi(argv[1])); //서버가 사용할 포트 결정, 바이트 순서를 네트워크 통신에 맞게 변경하는 함수

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) == -1) { //서버 소켓을 수신 대기 상태로 전환, backlog는 대기열의 최대크기
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", atoi(argv[1]));

    // todo 서버에서 종료할 수 있도록 함
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, server_quit, NULL);
    pthread_detach(server_thread);

    while (1) {
        new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len); //서버 소켓 대기열에 연결 요청이 있으면 새로운 소켓으로 할당(클라이언트 요청)
        //대기열에 연결 요청이 없는 경우 block모드에서 프로세스 대기
        if (new_socket == -1) {
            perror("Accept failed");
            continue;
        }

        pthread_mutex_lock(&mutex);

        // 아래 코드는 client에 정보 넣는 기능
        int i;
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (client_list[i].socket_num == INVALID_SOCKET) {
                client_list[i].socket_num = new_socket; // socket 넣기
                // 처음에 client에서 이름 받아옴
		        memset(buf1, 0, sizeof(buf1));
                recv(new_socket, buf1, sizeof(buf1) - 1, 0);
                // buf1을 그대로 넣어버리면 주소 값이 전달되기에 다음 client를 추가할 때 덮어씌어짐 그래서 strcpy사용함
                strcpy(client_list[i].name, buf1);
                // network to presentation
		        memset(buf1, 0, sizeof(buf1));
                inet_ntop(AF_INET, &client_addr.sin_addr, buf1, sizeof(buf1));
                strcpy(client_list[i].ip, buf1);
                client_list[i].port = client_addr.sin_port;
                strcpy(client_list[i].position_description, "normal_development");
                break;
            }
        }
        pthread_mutex_unlock(&mutex); //mutex 잠금해제

        if (i == MAX_CLIENTS) { 
            //클라이언트가 최대가 된 경우 연결 거부, 소켓 제거
            printf("Maximum clients connected. Connection refused.\n");
            close(new_socket);
        } else {
            //각 클라이언트 소켓 디스크립터에 쓰레드를 할당
            pthread_t thread;
            pthread_create(&thread, NULL, handle_client, &client_list[i]); //handle_client 함수를 독립적으로 사용하는 쓰레드 생성, 함수에 전달할 인자 설정
            pthread_detach(thread); //쓰레드를 독립적으로 실행되도록 함, 종료 시 리소스 자동 반환
            printf("New client connected.\n");
        }
    }
    close(fd);
    close(server_socket);
    return 0;
}

// handle_client는 여러 thread가 공유하는 함수이다
// 여러 thread가 공유하는 함수일지라도 arg는 thread마다 독립적인 값을 갖는다.
void *handle_client(client_List *client) {
    char buffer1[BUFFER_SIZE];
    char buffer2[BUFFER_SIZE];
    int bytes_read;

    // 입력 실패인지, client측 연결 해제인지 확인하기 위해서 recv를 밑으로 뺌
    while (1) {
        bytes_read = recv(client->socket_num, buffer1, sizeof(buffer1) - 1, 0);
        //클라이언트 소켓으로부터 send된 데이터를 수신, 성공 시 읽은 바이트 수를 리턴, 데이터 들어올 때까지 대기(블로킹모드)
        //클라이언트가 연결을 종료하면 0이 되고 while문 빠져나감
        if (bytes_read <= 0) {
            if (bytes_read == 0) 
                printf("Client disconnected.\n");
            else perror("recv error");
                break;
        }
        buffer1[bytes_read] = '\0';

        // 서버 터미널에 출력
        memset(buffer2, 0, sizeof(buffer2));
        sprintf(buffer2,"[%s] %s", client->name, buffer1);
        printf("%s\n", buffer2);

        // 명령어
        if(!strncmp(buffer1, "!quit", 5))
        {
            send(client->socket_num, "exit.... end", 12, 0);//이거 어차피 출력되기전에 클라이언트 프로세스 종료됨
            break;
        }

        else if(!strncmp(buffer1, "!showall", 8))
        {
            pthread_mutex_lock(&mutex);
            showall(client);
            pthread_mutex_unlock(&mutex);
            continue;
        }

        else if(!strncmp(buffer1, "!info", 5))
        {
            info(client);
            continue;
        }

        else if(!strncmp(buffer1, "!position", 9))
        {
            if (!strchr(buffer1, ' '))
            {
                syntax_error_print(client);
                continue;
            }
            char *token = strtok(buffer1, " ");
    	    token = strtok(NULL, "\0");
            if (token == NULL)
            {
                syntax_error_print(client);
                continue;
            }
            change_position(token, client);
            continue;
        }
        
        else if(!strncmp(buffer1, "!search", 7))
        {
            if (!strchr(buffer1, ' '))
            {
                syntax_error_print(client);
                continue;
            }
            char *token = strtok(buffer1, " ");
    	    token = strtok(NULL, "\0");
            if (token == NULL)
            {
                syntax_error_print(client);
                continue;
            }
            pthread_mutex_lock(&mutex);
            search_func(token, client);
            pthread_mutex_unlock(&mutex);
            continue;
        }
        
        // ==todo== 
        // #quit 값이 있다면 이를 인식하고 break하도록 코드 작성
        // #info 값이 있다면 이를 인식하고 접속되어 있는 user들의 정보 출력하도록 설정
        // #position ~~ 값이 있다면 이를 인식하고 해당 user 구조체 멤버 변수에 position정보 입력        

        pthread_mutex_lock(&mutex); // 여러 thread가 공유하는 함수기에 mutex_lock필요
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_list[i].socket_num != INVALID_SOCKET && client_list[i].socket_num != client->socket_num) {
                //클라이언트 소켓이 할당되어 있는 만큼, 자기 자신 클라이언트에게는 전송안함
                send(client_list[i].socket_num, buffer2, strlen(buffer2), 0); //메세지를 다른 클라이언트에게 전송
            }
        }
        pthread_mutex_unlock(&mutex);

        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char chat[BUFFER_SIZE];
        int chat_len;
        chat_len = sprintf(chat, "(%04d-%02d-%02d %02d:%02d:%02d) %s\n",tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, buffer2);
        write(fd, chat, chat_len); // sprintf로 chat에 현재 날짜와 전송된 문자 저장한 뒤에 파일에 해당 메세지 저장함
    }

    // 종료 로직
    pthread_mutex_lock(&mutex);
    for (int j = 0; j < MAX_CLIENTS; j++) {
        if (client_list[j].socket_num == client->socket_num) {
            client_list[j].socket_num = INVALID_SOCKET; //연결이 종료된 클라이언트 소켓 값을 INVALID_SOCKET으로 바꿈
            printf("%s client is disconnected\n", client->name);
            break;
        }
    }
    pthread_mutex_unlock(&mutex);

    close(client->socket_num);
    return NULL;
}

void info(client_List *client)
{
    char buf[BUFFER_SIZE];
    char start[] = "\n********************User info********************\n";
    char end[] = "\n************************************************\n";

    send(client->socket_num, start, strlen(start), 0); 
    for(int i = 0; i < MAX_CLIENTS; i ++)
    {
        if (client_list[i].socket_num != INVALID_SOCKET)
        {
            sprintf(buf, "[name : %s] [position : %s]\n", client_list[i].name, client_list[i].position_description);
            send(client->socket_num, buf, strlen(buf), 0);
        }
    }
    send(client->socket_num, end, strlen(end), 0);
}

void change_position(char *token, client_List *client)
{
    char previous_position[BUFFER_SIZE];
    char buf[BUFFER_SIZE];
    char start[] = "\n********************Change position********************\n";
    char end[] = "\n************************************************\n";

    send(client->socket_num, start, strlen(start), 0);
    strcpy(previous_position, client->position_description); // 이전 position 저장
    strcpy(client->position_description, token); // 새로운 포지션 저장

    for(int i = 0; i < MAX_CLIENTS; i ++)
    {
        if (client_list[i].socket_num != INVALID_SOCKET)
        {
            sprintf(buf, "%s's position : [%s] -> [%s]", client->name, previous_position, client->position_description);
            send(client_list[i].socket_num, buf, strlen(buf), 0);
        }
    }
    send(client->socket_num, end, strlen(end), 0);
}

void showall(client_List *client) 
{
    char buf[BUFFER_SIZE];
    int len = 0;

    char start[] = "\n********************Show all Texts********************\n";
    char end[] = "\n************************************************\n";

    lseek(fd, 0, SEEK_SET);
    send(client->socket_num, start, strlen(start), 0); // start 메세지 전송
    while ((len = read(fd, buf, sizeof(buf))) > 0) {
        send(client->socket_num, buf, len, 0); //읽어온 log 전송
    }
    send(client->socket_num, end, strlen(end), 0); // end 메세지 전송
    lseek(fd, 0, SEEK_END);
}

void search_func(char *token, client_List *client)
{
    char buf[BUFFER_SIZE];
    char line[BUFFER_SIZE];
    char keyword[BUFFER_SIZE];
    int len;
    char start[] = "\n*********************Search*********************\n";
    char end[] = "\n************************************************\n";

    strcpy(keyword, token);
    printf("\nSearching for keyword: %s\n", keyword); //서버 터미널에 출력 
    //keyword[strcspn(keyword, "\n")] = '\0';  // 개행 문자 제거

    // 파일 포인터를 처음으로 이동
    lseek(fd, 0, SEEK_SET);
    send(client->socket_num, start, strlen(start), 0); // start 메세지 전송
    // 한 줄씩 파일을 읽으며 검색
    int line_index = 0;  // line 버퍼의 인덱스
    while ((len = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < len; i++) {
            if (buf[i] == '\n' || line_index == sizeof(line) - 1) { // buf에서 한 줄이 끝나거나 line 버퍼가 꽉차면
                line[line_index] = '\0';  // 한 줄의 끝을 문자열로 처리
                if (strstr(line, keyword) != NULL) { //키워드가 포함된 줄이라면
                    printf("Line found: %s\n", line); //서버 터미널에 출력
                    sprintf(line, "%s\n", line); //line 버퍼에 저장(\n 추가)
                    send(client->socket_num, line, strlen(line), 0); //클라이언트에게 line 전송
                }
                line_index = 0;  // 다음 줄 처리를 위해 초기화
                memset(line, 0, sizeof(line)); //line 버퍼 초기화

            } else { //buf에서 한 줄이 끝나지 않았다면
                line[line_index++] = buf[i]; //line 버퍼에 buf저장(한 글자씩 저장)
            }
        }
    }
    printf("\n");
    send(client->socket_num, end, strlen(end), 0); // end 메세지 전송
    // 파일 포인터를 다시 파일 끝으로 이동
    lseek(fd, 0, SEEK_END);

    return NULL;
}

void syntax_error_print(client_List *client) 
{
    char buffer3[BUFFER_SIZE];
    memset(buffer3, 0, sizeof(buffer3));
    sprintf(buffer3, "command not invalid please retry", 32);
    send(client->socket_num, buffer3, strlen(buffer3), 0);
}

void server_quit(void *arg)
{
    char buffer[BUFFER_SIZE];
    
    while(fgets(buffer, sizeof(buffer), stdin) != NULL)
    {
        if (!strcmp(buffer, "!quit\n"))
        {
            printf("exit.... end");
            exit(0);
        }
    }
}