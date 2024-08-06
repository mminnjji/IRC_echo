#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <vector>

using namespace std;

// 오류 메시지 출력
void exit_with_perror(const string& msg)
{
    cerr << msg << endl; // 오류 메시지 출력
    exit(EXIT_FAILURE); // 프로그램 종료
}

// kevent 구조체 초기화 + change_list에 추가
void change_events(vector<struct kevent>& change_list, uintptr_t ident, int16_t filter,
        uint16_t flags, uint32_t fflags, intptr_t data, void *udata)
{
    struct kevent temp_event; // 임시 kevent 구조체

    EV_SET(&temp_event, ident, filter, flags, fflags, data, udata); // kevent 설정
    change_list.push_back(temp_event); // change_list에 kevent 추가
}

// 클라이언트 연결을 종료
void disconnect_client(int client_fd, map<int, string>& clients)
{
    cout << "client disconnected: " << client_fd << endl; // 클라이언트 종료 메시지 출력
    close(client_fd); // 소켓 닫기
    clients.erase(client_fd); // 클라이언트 목록에서 제거
}

int main()
{
    /* 서버 소켓 초기화 및 리슨 설정 */
    int server_socket; // 서버 소켓 파일 디스크립터
    struct sockaddr_in server_addr; // 서버 주소 구조체

    if ((server_socket = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        exit_with_perror("socket() error\n" + string(strerror(errno))); // 소켓 생성 오류 처리

    memset(&server_addr, 0, sizeof(server_addr)); // 서버 주소 구조체 초기화
    server_addr.sin_family = AF_INET; // 주소 체계 설정
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 모든 인터페이스에 바인딩
    server_addr.sin_port = htons(8080); // 포트 번호 설정
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
        exit_with_perror("bind() error\n" + string(strerror(errno))); // 소켓 바인딩 오류 처리

    if (listen(server_socket, 5) == -1)
        exit_with_perror("listen() error\n" + string(strerror(errno))); // 리슨 오류 처리
    fcntl(server_socket, F_SETFL, O_NONBLOCK); // 소켓을 논블로킹 모드로 설정
    
    /* kqueue 초기화 */
    int kq; // kqueue 파일 디스크립터
    if ((kq = kqueue()) == -1)
        exit_with_perror("kqueue() error\n" + string(strerror(errno))); // kqueue 생성 오류 처리

    map<int, string> clients; // 클라이언트 소켓과 데이터를 저장할 맵
    vector<struct kevent> change_list; // 변경 사항을 저장할 벡터
    struct kevent event_list[8]; // 이벤트 목록을 저장할 배열

    /* 서버 소켓에 대한 이벤트 추가 */
    change_events(change_list, server_socket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL); // 서버 소켓 읽기 이벤트 추가
    cout << "echo server started" << endl; // 서버 시작 메시지 출력

    /* 메인 루프 */
    int new_events; // 새로운 이벤트 수
    struct kevent* curr_event; // 현재 이벤트 포인터
    while (1)
    {
        /* 변경 사항 적용 및 새로운 이벤트 반환 */
        new_events = kevent(kq, &change_list[0], change_list.size(), event_list, 8, NULL); // kevent 호출
        if (new_events == -1)
            exit_with_perror("kevent() error\n" + string(strerror(errno))); // kevent 오류 처리

        change_list.clear(); // 새로운 변경 사항을 위해 change_list 초기화

        for (int i = 0; i < new_events; ++i)
        {
            curr_event = &event_list[i]; // 현재 이벤트 설정

            /* 오류 이벤트 확인 */
            if (curr_event->flags & EV_ERROR)
            {
                if (curr_event->ident == server_socket)
                    exit_with_perror("server socket error"); // 서버 소켓 오류 처리
                else
                {
                    cerr << "client socket error" << endl; // 클라이언트 소켓 오류 메시지 출력
                    disconnect_client(curr_event->ident, clients); // 클라이언트 연결 종료
                }
            }
            else if (curr_event->filter == EVFILT_READ)
            {
                if (curr_event->ident == server_socket)
                {
                    /* 새로운 클라이언트 수락 */
                    int client_socket; // 클라이언트 소켓 파일 디스크립터
                    if ((client_socket = accept(server_socket, NULL, NULL)) == -1)
                        exit_with_perror("accept() error\n" + string(strerror(errno))); // 클라이언트 수락 오류 처리
                    cout << "accept new client: " << client_socket << endl; // 새로운 클라이언트 수락 메시지 출력
                    fcntl(client_socket, F_SETFL, O_NONBLOCK); // 클라이언트 소켓을 논블로킹 모드로 설정

                    /* 클라이언트 소켓에 대한 읽기 및 쓰기 이벤트 추가 */
                    change_events(change_list, client_socket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL); // 읽기 이벤트 추가
                    change_events(change_list, client_socket, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL); // 쓰기 이벤트 추가
                    clients[client_socket] = ""; // 클라이언트 목록에 추가
                }
                else if (clients.find(curr_event->ident) != clients.end())
                {
                    /* 클라이언트로부터 데이터 읽기 */
                    char buf[1024]; // 버퍼
                    int n = read(curr_event->ident, buf, sizeof(buf)); // 데이터 읽기

                    if (n <= 0)
                    {
                        if (n < 0)
                            cerr << "client read error!" << endl; // 읽기 오류 메시지 출력
                        disconnect_client(curr_event->ident, clients); // 클라이언트 연결 종료
                    }
                    else
                    {
                        buf[n] = '\0'; // 문자열 종료
                        clients[curr_event->ident] += buf; // 데이터 저장
                        cout << "received data from " << curr_event->ident << ": " << clients[curr_event->ident] << endl; // 데이터 수신 메시지 출력
                    }
                }
            }
            else if (curr_event->filter == EVFILT_WRITE)
            {
                /* 클라이언트로 데이터 전송 */
                map<int, string>::iterator it = clients.find(curr_event->ident); // 클라이언트 찾기
                if (it != clients.end())
                {
                    if (clients[curr_event->ident] != "")
                    {
                        int n;
                        if ((n = write(curr_event->ident, clients[curr_event->ident].c_str(),
                                        clients[curr_event->ident].size()) == -1))
                        {
                            cerr << "client write error!" << endl; // 쓰기 오류 메시지 출력
                            disconnect_client(curr_event->ident, clients); // 클라이언트 연결 종료
                        }
                        else
                            clients[curr_event->ident].clear(); // 데이터 초기화
                    }
                }
            }
        }

    }
    return (0); // 프로그램 종료
}
