#include "CommandHandler.hpp"

// void	comJoin(Client const &client, std::vector<std::string> const &param)
void CommandHandler::join(Command &cmd, Client &client, Server &server)
{
	std::string channel_name = "";  // 채널 이름을 저장할 문자열 초기화
	std::string mode = "";  // 채널 모드를 저장할 문자열 초기화
	size_t comma_pos = 0;  // 콤마 위치를 저장할 변수 초기화
	size_t mode_pos = 0;  // 모드에서 콤마 위치를 저장할 변수 초기화
	std::vector<std::string> _tem;  // 명령어 파라미터를 저장할 벡터 초기화
	std::map<std::string, Channel> &channels = server.getChannels();  // 서버에서 채널 목록을 참조하는 레퍼런스 초기화

	_tem = cmd.getParams();  // 명령어에서 파라미터 벡터를 가져옴
	if (_tem.size() < 1)  // 파라미터가 하나도 없는 경우
	{
		com461(client.getNickname(), "JOIN");  // 클라이언트에게 오류 메시지 전송 ("JOIN" 명령어에 파라미터가 없다는 의미)
		return;  // 함수 종료
	}

	while (_tem[0].length() > 0)  // 파라미터의 첫 번째 요소가 비어있지 않을 때까지 반복
	{
		if ((comma_pos = _tem[0].find(',')) != std::string::npos)  // 콤마가 있는지 확인
		{
			channel_name = _tem[0].substr(0, comma_pos);  // 첫 번째 채널 이름 추출
			_tem[0].erase(1, comma_pos + 1);  // 추출된 채널 이름과 콤마를 파라미터에서 삭제
		}
		else  // 더 이상 콤마가 없을 경우
		{
			channel_name = _tem[0];  // 남은 문자열을 채널 이름으로 설정
			_tem[0] = "";  // 파라미터를 비움
		}

		if (channel_name[0] != '#')  // 채널 이름이 '#'으로 시작하지 않는 경우
		{
			_reply += ":localhost 476 " + client.getNickname() + " " + channel_name + " :Invalid channel name\r\n";  // 오류 메시지 작성 (잘못된 채널 이름)
			return;  // 함수 종료
		}

		// mode 처리 필요함 (미완성 부분)
		if (_tem.size() > 1)  // 파라미터가 2개 이상인 경우 (모드가 제공된 경우)
		{
			mode_pos = _tem[1].find(',');  // 모드에서 콤마 위치 찾기
			if (mode_pos > 0)  // 콤마가 발견된 경우
			{
				mode = _tem[1].substr(0, mode_pos);  // 모드를 추출
				_tem[1].erase(0, mode_pos + 1);  // 추출된 모드와 콤마를 파라미터에서 삭제
			}
		}

		if (channels.find(channel_name) != channels.end())  // 채널이 이미 존재하는지 확인
		{
			if (channels[channel_name].addClient(client) == -1)  // 채널에 클라이언트를 추가, 실패 시 계속 진행
				continue;  // 다음 채널 이름으로 넘어감
		}
		else  // 채널이 존재하지 않는 경우
		{
			channels[channel_name] = *(new Channel(channel_name));  // 새로운 채널을 생성하여 채널 목록에 추가
			if (channels[channel_name].addClient(client) == -1)  // 새 채널에 클라이언트를 추가, 실패 시 계속 진행
				continue;  // 다음 채널 이름으로 넘어감
		}

		if (channels.find(channel_name) != channels.end() && channels[channel_name].getChannelTopic() != "")  // 채널이 존재하고, 주제가 설정된 경우
			com332(client, channels[channel_name]);  // 클라이언트에게 채널 주제를 알림

		_reply += client.getSource() + " JOIN :" + channel_name + "\r\n";  // 클라이언트의 채널 참여 메시지 작성
		com353(server, channels[channel_name]);  // 채널의 현재 사용자 목록을 클라이언트에게 전송
		com366(client, channel_name);  // 클라이언트에게 채널에 대한 추가 정보를 전송
		channels[channel_name].messageToMembers(client, "JOIN", channel_name);  // 해당 채널의 다른 멤버들에게 클라이언트의 참여를 알림
	}
	return;  // 함수 종료
}
