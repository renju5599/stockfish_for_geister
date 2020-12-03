#pragma once
#pragma comment(lib, "wsock32.lib")
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>

namespace tcp {
	void mySend(int dstSocket, std::string str = "")
	{
		if (str.length() == 0) {	//null�����Ȃ�u���́v���󂯕t����
			std::cin >> str;
		}
		if (str.length() < 2 || str[str.length() - 2] != '\r' || str[str.length() - 1] != '\n') {	//\r\n�������ɂȂ���Βǉ�
			str += '\r';
			str += '\n';
		}
		int byte = send(dstSocket, str.c_str(), str.length(), 0);	//������𑗐M
		if (byte <= 0) {
			std::cout << "���M�G���[" << std::endl;
		}
	}

	std::string myRecv(int dstSocket)
	{
		char buffer[10];
		std::string msg;
		
		do {
			int byte = recv(dstSocket, buffer, 1, 0);	//��������M
			
			if (byte == 0) break;
			if (byte < 0) { std::cout << "��M�Ɏ��s���܂���" << std::endl; return msg; }
			
			msg += buffer[0];
		} while (msg.length() < 2 || msg[msg.length() - 2] != '\r' || msg[msg.length() - 1] != '\n');
		
		std::cout << "��M = " << msg << std::endl;
		
		return msg;
	}

	bool openPort(int &dstSocket, int port = -1, std::string dest = "")
	{
		// IP �A�h���X�C�|�[�g�ԍ��C�\�P�b�g�Csockaddr_in �\����
		char destination[32];
		struct sockaddr_in dstAddr;
		int PORT;
		
		// Windows �̏ꍇ
		WSADATA data;
		WSAStartup(MAKEWORD(2,0), &data);

		// �����A�h���X�̓��͂Ƒ��镶���̓���
		if (port == -1) {
			printf("�|�[�g�ԍ��́H�F");
			scanf("%d", &PORT);
		} else {
			PORT = port;
		}
		
		if (dest.length() == 0) {
			printf("�T�[�o�[�}�V����IP�́H:");
			scanf("%s", destination);
		} else {
			for (int i = 0; i < dest.size(); i++) destination[i] = dest[i];
			destination[dest.size()] = '\0';
		}
		
		// sockaddr_in �\���̂̃Z�b�g
		memset(&dstAddr, 0, sizeof(dstAddr));
		dstAddr.sin_port = htons(PORT);
		dstAddr.sin_family = AF_INET;
		dstAddr.sin_addr.s_addr = inet_addr(destination);

		// �\�P�b�g�̐���
		dstSocket = socket(AF_INET, SOCK_STREAM, 0);

		//�ڑ�
		if(connect(dstSocket, (struct sockaddr *) &dstAddr, sizeof(dstAddr))){
			printf("%s�@�ɐڑ��ł��܂���ł���\n",destination);
			return false;
		}
	  	printf("%s �ɐڑ����܂���\n", destination);

		return true;
	}

	void closePort(int &dstSocket)
	{
		// Windows �ł̃\�P�b�g�̏I��
		closesocket(dstSocket);
		WSACleanup();
	}
}