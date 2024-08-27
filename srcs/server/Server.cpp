#include "Server.hpp"

// Orthodox Canonical Form
Server::Server(std::string port, std::string password, tm *time_local) : \
                _port(port), _password(password), _time_local(time_local), _server_name("ircserv") {}

Server::~Server() {}

Server &Server::operator=(const Server &other)
{
    if (this != &other) {
        _port = other._port;
        _password = other._password;
        _time_local = other._time_local;
        _server_fd = other._server_fd;
        _event_fd = other._event_fd;
        _clients = other._clients;
        _server_name = other._server_name;
		_channels = other._channels;
    }
    return *this;
}

Server::Server(const Server &other)
{
    *this = other;
}

std::map<int, Client> &Server::getClients()
{
    return _clients;
}

std::map<std::string, Channel> &Server::getChannels()
{
    return _channels;
}

// 닉이랑 관련된 함수같긴한데 뭔지 모르겠음
std::set<std::string> Server::getNicknames()
{
    std::set<std::string> nicknames;
    std::map<int, Client>::iterator it;
    for (it = _clients.begin(); it != _clients.end(); ++it)
    {
        if (it->second.getNickname() == "")
            continue ;
        nicknames.insert(it->second.getNickname());
    }
    return nicknames;
}

std::string Server::getPort()
{
    return _port;
}

std::string Server::getServerName()
{
    return _server_name;
}

time_t Server::getLocalTime()
{
    return mktime(_time_local);
}

// Member Function
void Server::setupSocket()
{
	// 소켓 설정 정보 저장
    struct addrinfo hints;
    struct addrinfo *res;
    int status;
    int yes = 1;

	// 구조체 초기화
    memset(&hints, 0, sizeof hints);

    // TCP IPv4 -> hints 에 초기 설정 넣기
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; //TCP

    // hints를 바탕으로 조건에 맞는 주소정보 저장 / 로컬호스트 사용 / 포트번호 설정  -> res 에 저장
    if ((status = getaddrinfo(NULL, _port.c_str(), &hints, &res)) != 0)
        die("getaddrinfo");

    // create socket (서버 소켓)
    _server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (_server_fd == -1)
        die("socket");

    // 소켓 종료 후에도 같은 주소와 포트 재사용할 수 있도록 방지
    if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        die("setsockopt");

    // 서버 소켓을 특정 IP 와 포트에 바인딩 
    if (bind(_server_fd, res->ai_addr, res->ai_addrlen) == -1)
        die("bind");

    // 앞서서 할당된 res 해제 -> 소켓 설정 완료
    freeaddrinfo(res);

    //start listening - 연결대기열의 크기 지정
    if (listen(_server_fd, 10) == -1)
        die("listen");

    //set non-blocking - 서버 소켓을 논블로킹 모드로 설정 - 서버가 멈추지 않고 다른 작업을 처리할 수 있음
    if (fcntl(_server_fd, F_SETFL, O_NONBLOCK) == -1)
        die("fcntl");
}

#ifdef __linux__
void Server::setupEpoll()
{
    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];
    std::string buffer;
    std::string leftover;
    Command     cmd;        // 명령어 임시 저장소

    setupSocket();
    if ((_event_fd = epoll_create1(0)) == -1) 
    {
         die("epoll_create1");
    }
    // readable | edge-triggered
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = _server_fd;
    if (epoll_ctl(_event_fd, EPOLL_CTL_ADD, _server_fd, &ev) == -1) 
    {
        die("epoll_ctl: listen_sock");
    }
    while (g_shutdown == false)
    {
        int n = epoll_wait(_event_fd, events, MAX_EVENTS, -1);
        if (n == -1)
        {
            die("epoll_wait");
        }

        for (int i = 0; i < n; ++i)
        {
            if (events[i].data.fd == _server_fd)
            {
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);

                int client = accept(_server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
                if (client < 0)
                {
                    perror("accept");
                    continue;
                }
                // Set the client socket timeout to 15 seconds
                struct timeval tv = {15, 0};
                if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0)
                    die("setsockopt");
                // Set the client socket to non-blocking mode
                if (fcntl(client, F_SETFL, O_NONBLOCK) == -1)
                    die("fcntl");
                // Add the new client socket to the epoll instance
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client;
                if (epoll_ctl(_event_fd, EPOLL_CTL_ADD, client, &ev) == -1)
                {
                    die("epoll_ctl: client");
                }
                Client new_client(client, _password, this);
                _clients.insert(std::pair<int, Client>(client, new_client)); // 클라이언트 클래스 추가
            }
            else
            {
                int client = events[i].data.fd;
                buffer.resize(BUFFER_SIZE);
                
                ssize_t bytes_received = recv(client, &buffer[0], BUFFER_SIZE - 1, 0);
                if (bytes_received < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        printf("Client timeout\n");
                    else
                        perror("recv");
                    epoll_ctl(_event_fd, EPOLL_CTL_DEL, client, NULL);
                       close(client);
                    std::map<int, Client>::iterator it = _clients.find(client);
                    if (it != _clients.end()) {
                        _clients.erase(it);
                    }
                }
                else if (bytes_received == 0)
                {
                    printf("Client closed connection.\n");
                    epoll_ctl(_event_fd, EPOLL_CTL_DEL, client, NULL);
                    close(client);
                    std::map<int, Client>::iterator it = _clients.find(client);
                    if (it != _clients.end()) {
                        _clients.erase(it);
                    }
                }
                else
                {
                    buffer.resize(bytes_received);
                    std::string data = leftover + buffer;
                    leftover.clear();

                    size_t pos;
                    std::map<int, Client>::iterator it = _clients.find(client);
                    Client &tmp_client = it->second;
                    if (it != _clients.end())
                        tmp_client = it->second;
                    else
                        continue; // 이게 가능한 얘긴가?
                    while ((pos = data.find("\r\n")) != std::string::npos || (pos = data.find("\n")) != std::string::npos)
                    {
                        
                        std::string message = data.substr(0, pos);
                        printf("Received message: %s\n", message.c_str());

                        // Process the message ////////////////////////////////
                        ///////////////////////////////////////////////////////
                        cmd.clearCommand();
                        cmd.parseCommand(message);
                        cmd.showCommand();
                        tmp_client.execCommand(cmd);
                        // ssize_t sent_bytes = send(client, message.c_str(), message.size(), 0);
                        // printf("Sent %ld bytes\n", sent_bytes);
                        // if (sent_bytes < 0)
                        // {
                        //     perror("send");
                        // }
                        // else if (sent_bytes != static_cast<ssize_t>(message.size()))
                        // {
                        //     fprintf(stderr, "Warning: Not all data was sent.\n");
                        // }
                        // end of processing //////////////////////////////////
                        ///////////////////////////////////////////////////////
                        data.erase(0, pos + (data[pos] == '\r' ? 2 : 1));  // Remove the processed message
                    }
                    // Remaining data is saved in leftover
                    leftover = data;
                }
            }
        }
    }
    stopEpoll(); // Clean up
}

void Server::stopEpoll()
{
    epoll_ctl(_event_fd, EPOLL_CTL_DEL, _server_fd, NULL);
    close(_server_fd);
    std::map<int, Client>::iterator it;
    for (it = _clients.begin(); it != _clients.end(); ++it) {
        int client_fd = it->first;
        epoll_ctl(_event_fd, EPOLL_CTL_DEL, client_fd, NULL);
        close(client_fd);
    }
    close(_event_fd);
}
#endif


#ifdef __APPLE__

void Server::setupKqueue()
{
    struct kevent change_list;
    struct kevent event_list[MAX_EVENTS];
    std::string buffer;
    std::string leftover;
    Command     cmd;        // 명령어 임시 저장소
    
    setupSocket();

    // kqueue 생성
    _event_fd = kqueue(); 
    if (_event_fd == -1)
    {
        die("kqueue");
    }

    // 서버 소켓을 kqueue에 등록 (readable 이벤트)
    EV_SET(&change_list, _server_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);

	// change_list 에 등록된 서버 소켓에 대한 이벤트 kqueue 에 등록
    if (kevent(_event_fd, &change_list, 1, NULL, 0, NULL) == -1)
    {
        die("kevent: listen_sock");
    }

	// 셧다운이 아닐때 ~ (SIGTERM 일때 셧다운)
    while (g_shutdown == false)
    {
		// kqueue 에서 발생한 이벤트 (event_fd) 를 event_list 에 채워넣음
        int n = kevent(_event_fd, NULL, 0, event_list, MAX_EVENTS, NULL);
        if (n == -1)
        {
            die("kevent");
        }

		// 이벤트 갯수만큼 확인
        for (int i = 0; i < n; ++i)
        {
			// 클라이언트 연결 처리
            if (event_list[i].ident == (unsigned int)_server_fd)
            {
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);

				// 클라이언트 연결 요청 수신 시, accept 로 호출, 연결 수락
                int client = accept(_server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
                if (client < 0)
                {
                    perror("accept");
                    continue;
                }

                // 클라이언트 소켓 15초 수신 타임아웃 - 이 시간동안 데이터를 수신하지 못하면 타임아웃
                struct timeval tv = {15, 0};
                if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0)
                    die("setsockopt");

                // 클라이언트 소켓을 non-blocking으로 설정
                if (fcntl(client, F_SETFL, O_NONBLOCK) == -1)
                    die("fcntl");

                // 새 클라이언트 소켓을 kqueue에 등록 (readable 이벤트)
                EV_SET(&change_list, client, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
                if (kevent(_event_fd, &change_list, 1, NULL, 0, NULL) == -1)
                {
                    die("kevent: client");
                }

				// 이거 수정
                Client new_client(client, _password, this);
                _clients.insert(std::pair<int, Client>(client, new_client)); // 클라이언트 클래스 추가
            }

			// 클라이언트 소켓에서 이벤트 발생 - 클라이언트로부터 데이터 수신 및 처리
            else 
            {
                int client = event_list[i].ident;
                buffer.resize(BUFFER_SIZE);

				// 클라이언트에서 수신
                ssize_t bytes_received = recv(client, &buffer[0], BUFFER_SIZE - 1, 0);
                
				// 오류 발생시 
				if (bytes_received < 0)
                {
                    perror("recv");
                    EV_SET(&change_list, client, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                    kevent(_event_fd, &change_list, 1, NULL, 0, NULL);
                    close(client);
                    std::map<int, Client>::iterator it = _clients.find(client);
                    if (it != _clients.end()) {
                        _clients.erase(it);
                    }
                }
				// 연결 종료 
                else if (bytes_received == 0)
                {
                    printf("Client closed connection.\n");
                    EV_SET(&change_list, client, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                    kevent(_event_fd, &change_list, 1, NULL, 0, NULL);
                    close(client);
                    std::map<int, Client>::iterator it = _clients.find(client);
                    if (it != _clients.end()) {
                        _clients.erase(it);
                    }
                }
				// 데이터 수신
                else
                {
                    buffer.resize(bytes_received);
                    std::string data = leftover + buffer;
                    leftover.clear();
                    
                    size_t pos;
                    std::map<int, Client>::iterator it = _clients.find(client);
                    Client &tmp_client = it->second;
                    if (it != _clients.end())
                        tmp_client = it->second;
                    else
                        continue; // 이게 가능한 얘긴가?
                    while ((pos = data.find("\r\n")) != std::string::npos || (pos = data.find("\n")) != std::string::npos)
                    {
                        std::string message = data.substr(0, pos);
                        printf("Received message: %s\n", message.c_str());
                        
                        // Process the message ////////////////////////////////
                        ///////////////////////////////////////////////////////
                        cmd.clearCommand();
                        cmd.parseCommand(message);
                        cmd.showCommand();
                        tmp_client.execCommand(cmd, *this);
                        // ssize_t sent_bytes = send(client, message.c_str(), message.size(), 0);
                        // printf("Sent %ld bytes\n", sent_bytes);
                        // if (sent_bytes < 0)
                        // {
                        //     perror("send");
                        // }
                        // else if (sent_bytes != static_cast<ssize_t>(message.size()))
                        // {
                        //     fprintf(stderr, "Warning: Not all data was sent.\n");
                        // }
                        // end of processing //////////////////////////////////
                        ///////////////////////////////////////////////////////
                        data.erase(0, pos + (data[pos] == '\r' ? 2 : 1));  // Remove the processed message
                    }
                    // 남은 데이터를 leftover에 저장
                    leftover = data;
                }
            }
        }
    }
    stopKqueue(); // Clean up
}

void Server::stopKqueue()
{
    close(_server_fd);
    std::map<int, Client>::iterator it;
	// 모든 클라이언트 소켓 종료
    for (it = _clients.begin(); it != _clients.end(); ++it) {
        int client_fd = it->first;
        close(client_fd);
    }
    close(_event_fd);
}
#endif
