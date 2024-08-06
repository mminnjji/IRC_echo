#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <poll.h>

#define BUF_SIZE 512

void error_handling(char* message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

int main(int argc, char* argv[])
{
    int serv_sock;
    int clnt_sock;

    // sockaddr은 주소정보를 담는 기본형태이다.
    // 주소체계에 따라 sockaddr_in, sockaddr_un, sockaddr_in6 등을 sockaddr로 형변환해서 사용하면 편리하다.
    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size;

    // TCP연결, IPv4 도메인을 위한 소켓 생성
    serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        error_handling("socket error");
    
    //서버의 주소 정보 설정
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; // 주소패밀리
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 서버의 ip주소
    serv_addr.sin_port = htons(atoi(argv[1])); // 서버 프로그램의 포트
    //htonl, htos - 빅엔디안 타입으로 변환

    // 소켓과 주소정보를 결합
    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind error");
    
    // 연결요청 대기
    if (listen(serv_sock, 5) == -1)
        error_handling("listen error");

    // pollfd 배열 생성 및 초기화
    struct pollfd clients[512];
    for (int i = 0; i < 512; ++i)
        clients[i].fd = -1;

    // 배열의 첫번째에 서버 소켓을 등록
    clients[0].fd = serv_sock;
    clients[0].events = POLLIN;

    int max_i = 0; // pollfd에 등록되어 있는 가장 높은 인덱스
    int i;

    while (1)
    {
        if (poll(clients, max_i + 1, 5000) <= 0)
            continue;
        
        // 연결 수락
        if (clients[0].revents & POLLIN)
        {
            clnt_addr_size = sizeof(clnt_addr);
            clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);

            for (i = 1; i < 512; ++i)
            {
                if (clients[i].fd < 0)
                {
                    clients[i].fd = clnt_sock;
                    clients[i].events = POLLIN;
                    break;
                }
            }

            if (i == 512)
                error_handling("too many clients");

            if (i > max_i)
                max_i = i;
            printf("fd %d connected\n", clnt_sock);
            continue;
        }

        // 클라이언트 요청 처리
        // 등록된 모든 pollfd들을 검사한다.
        for (i = 1; i <= max_i; ++i)
        {
            if (clients[i].fd < 0)
                continue;
            // i번째 소켓에 POLLIN 이벤트가 발생
            if (clients[i].revents & POLLIN)
            {
                char* buf[BUF_SIZE];
                memset(buf, 0, BUF_SIZE);

				// 이벤트가 발생한 소켓을 읽어들임
                if (read(clients[i].fd, buf, BUF_SIZE) <= 0)
                // 0 또는 -1을 반환했을 때 연결 종료
                {
                    close(clients[i].fd);
                    printf("%d : client disconnected\n", clients[i].fd);
                    clients[i].fd = -1;

                    if (i == max_i)
                    {
                        for (; clients[max_i].fd < 0; --max_i)
                        {}
                    }
                }
                else //echo
                    write(clients[i].fd, buf, strlen((const char*)buf));
            }
        }

    }

    close(serv_sock);

    return 0;
}