#ifndef SERVER_HPP
# define SERVER_HPP

# include <ctime>
# include <csignal>
# include <exception>
# include <cstdio>
# include <cstdlib>
# include <cstring>
# include <iostream>
# include <fcntl.h>
# include <set>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
# include <unistd.h>
# include <vector>
# include <map>
# include <algorithm>
# include <cerrno>
# include <sstream>
# include "../command/Command.hpp"
# include "../command/CommandHandler.hpp"
# include "../client/Client.hpp"

# ifdef __APPLE__
#  include <sys/event.h>
# endif

# ifdef __linux__
#  include <sys/epoll.h>
# endif

extern volatile sig_atomic_t g_shutdown;
extern void die(const char* msg);

# define MAX_EVENTS  64
# define BACKLOG     10
# define BUFFER_SIZE 512

class Client;

//서버 클래스
class Server
{
    private:
        std::string		_port; // 포트번호	
        std::string		_password; // 현재 들어오는 패스워드
        tm				*_time_local; // 로컬시간
        int             _server_fd; // 서버 소켓 파일디스크립터
        int             _event_fd; // 이벤트 감시 소켓 - kqueue / epoll
        std::string     _server_name; // 서버 이름
        std::map<int, Client> _clients; // 서버에 연결된 클라이언트를 관리하는 맵 
		//-> 각 클라이언트는 파일디스크립터 int를 키로 가지는 Client 객체

    public:
        // Orthodox Canonical Form
        Server(std::string port, std::string password, tm *time_local);
        ~Server();
        Server &operator=(const Server &other);
        Server(const Server &other);

        // Server setup
        void setupSocket();

        void setupKqueue();
        void stopKqueue();
        //getter and setter
        std::map<int, Client> &getClients();
        std::set<std::string> getNicknames();
        std::string getPort();
        std::string getServerName();
        time_t getLocalTime();
        
};

#endif