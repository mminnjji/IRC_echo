#ifndef CLIENT_HPP
# define CLIENT_HPP

# include "../server/Server.hpp"

class CommandHandler;
class Command;
class Server;
class Client
{
	private:
		// 현재 속한 서버 객체 가리키는 포인터
		Server 			*_server;
		// 클라이언트가 서버에 정식으로 등록되었는지 플래그
		bool		_is_registered;
		// 비밀번호 인증을 통과했는지 플래그
		bool		_is_passed;
		// 서버 연결 시간
		time_t		_established_time;
		// 마지막 활동 시간
		time_t		_last_active_time;
		// 클라이언트의 닉네임
		std::string _nickname;
		// 클라이언트의 사용자 명 - 시스템 사용자 명
		std::string	_username;
		//실제 이름
		std::string _realname;
		// 호스트 이름 - 컴퓨터 이름 혹은 IP 주소
		std::string _hostname;
		// 현재 모드 - 사용자 모드/관리자모드
		std::string _mode;
		// 클라이언트 IP 주소
		std::string _ip;
		// 서버에서 클라이언트를 인증하는데 사용되는 비밀번호
		std::string _password;
		// 시도한 비밀번호 저장
		std::string	_try_password;
		// 클라이언트 - 서버 간 통신에 사용되는 소켓 파일 디스크립터
		int			_socket_fd;

	
	public:
		Client(int fd, std::string password, Server *server);
		~Client();
		Client(const Client &other);
		Client &operator=(const Client &other);	
		void	execCommand(Command &cmd, Server &server);

		std::string	getSource() const; // client 의 정보를 source 로 만들어주는 함수

		/// Setters and Getters
		void	setTry_password(std::string password);
		void	setNickname(std::string nickname);
		void  	setUsername(std::string username);
		void  	setRealname(std::string realname);
		void  	setHostname(std::string hostname);
		void  	setMode(std::string mode);
		void  	setIp(std::string ip);
		void  	setPassword(std::string password);
		void  	setSocket_fd(int fd);
		void	setIs_registered(bool is_registered);
		void	setIs_passed(bool is_passed);
		void	setEstablished_time(time_t established_time);
		void	setLast_active_time(time_t last_active_time);
		std::string	getNickname() const;
		std::string	getUsername() const;
		std::string	getRealname() const;
		std::string	getHostname() const;
		std::string	getMode() const;
		std::string	getIp() const;
		std::string	getPassword() const;
		std::string	getTry_password() const;
		int			getSocket_fd() const;
		bool		getIs_registered() const;
		bool		getIs_passed() const;
		time_t		getEstablished_time() const;
		time_t		getLast_active_time() const;
		Server		*getServer() const;

		//debug

		void	showClient();

};

#endif