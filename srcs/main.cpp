#include "./server/Server.hpp"

// global variable to check if the server is shutting down
volatile sig_atomic_t g_shutdown = false; 

void die(const char* msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

void check_os()
{
    #ifdef __linux__
        std::cout << "OS: Linux" << std::endl;
    #elif __APPLE__
        std::cout << "OS: MacOS" << std::endl;
    #else
        std::cout << "OS: not supported" << std::endl;
    #endif
}

static void	signal_handler(int signal)
{
	if (g_shutdown == false && signal == SIGTERM)
	{
		//std::cout << "Shutting down server..." << std::endl;
		g_shutdown = true; // sigterm 일 때 shutdown -> 클라이언트 응답 수신 종료
	}
}

int main (int argc, char **argv)
{
	if (argc == 3)
	{
		// 운영체제 확인
        check_os();
		// 현재시간 설정
		time_t time_raw = {0};
		tm *time_local;

		time(&time_raw);
		time_local = localtime(&time_raw);

		// handle signals
		signal(SIGINT, SIG_IGN); // SIGINT ignore
		signal(SIGTERM, signal_handler); // SIGTERM 일 때 signal_handler 로 전송

		// create server
		Server server(argv[1], argv[2], time_local);

        server.setupKqueue();
		return (0);
	}
	else
	{
		std::cout << "input ./ircserv [port] [password]" << std::endl;
		return (1);
	}
}