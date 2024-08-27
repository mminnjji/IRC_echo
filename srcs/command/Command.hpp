#ifndef COMMAND_HPP
# define COMMAND_HPP

# include "../server/Server.hpp"

class Client;
class Command 
{
	private:
		// 명령어 숫자 코드
		int			_numeric;
		// 명령어 태그 - 메타데이터
		std::string _tag;
		// 명령어 출처 - 서버 식별자
		std::string _source;
		// 실제 명령어 저장
		std::string _command;
		// 명령어와 함께 전달되는 매개변수 저장
		std::vector<std::string> _parameter;

	public:
		Command();
		~Command();
		Command &operator=(const Command &other);
		Command(const Command &other);

		// 초기화
		void		clearCommand();
		// 파싱
		void		parseCommand(std::string command);
		// 초기 세팅
		void		setCommand(std::string tag, std::string source, std::string command, std::vector<std::string> parameter);
		std::string	deparseCommand() const;
		std::string	getCommand() const;
		std::string	getTag() const;
		std::string	getMessage() const;
		std::vector<std::string>	getParams() const;

		void	showCommand();
};

#endif