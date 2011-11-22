#include <sstream>
#include "BaseSocket.h"

int CBaseSocket::nAmountSockets = 0;
CBaseSocket::CBaseSocket(int nSocketType, int nProtocol)
{
	++nAmountSockets;
	rSocket = socket(AF_INET, nSocketType, nProtocol);
	if(rSocket == INVALID_SOCKET)
	{
		std::ostringstream szError;
		szError << "Unable to create new socket. Reason: " << GetSocketError();
		throw new SocketException(GetSocketErrorCode(), szError.str());
	}
}

CBaseSocket::CBaseSocket(SOCKET rNewSocket)
{
	++nAmountSockets;
	rSocket = rNewSocket;
}
CBaseSocket::~CBaseSocket()
{
	if(rSocket != INVALID_SOCKET)
	{
		close(rSocket);
	}
	--nAmountSockets;
}

std::string CBaseSocket::GetPeerName()
{
	sockaddr_in name;
	std::string ip;
	int namelen = sizeof(name);
	if(getpeername(rSocket, (sockaddr *)&name, (socklen_t *)&namelen) == SOCKET_ERROR)
	{
		std::ostringstream szError;
		szError << "Error while getting peer name: " << GetSocketError();
		throw new SocketException(GetSocketErrorCode(), szError.str());
	}
	ip.assign(inet_ntoa(name.sin_addr));
	return ip;
}
void CBaseSocket::operator <<(std::string Line)
{
	if(send(rSocket, Line.c_str(), (int)Line.length(), 0) == SOCKET_ERROR)
	{
		std::ostringstream szError;
		szError << "Error while sending data: " << GetSocketError();
		throw new SocketException(GetSocketErrorCode(), szError.str());
	}
}

void CBaseSocket::operator <<(std::vector<unsigned char> Data)
{
	char *szData = new char[Data.size()];
    unsigned int i;
    for (i=0; i < Data.size(); i++)
	{
         szData[i] = Data[i];
	}
	if(send(rSocket, szData, Data.size(), 0) == SOCKET_ERROR)
	{
		delete szData;
		std::ostringstream szError;
		szError << "Error while sending data: " << GetSocketError();
		throw new SocketException(GetSocketErrorCode(), szError.str());
	}	
	delete szData;
}
void CBaseSocket::operator >>(std::string &Line)
{
	char buf[8096];
	memset(buf, 0x00, sizeof(buf));
	int nBytes = recv(rSocket, buf, sizeof(buf), 0);
	if(nBytes == SOCKET_ERROR)
	{
		std::ostringstream szError;
		szError << "Error while recieving data from client: " << GetSocketError();
		throw new SocketException(GetSocketErrorCode(), szError.str());
	}
	Line.assign(buf, nBytes);

}
void CBaseSocket::operator >>(std::vector<unsigned char> &Data)
{
    u_long arg = 0;
	while(arg == 0)
	{
		if (ioctl(rSocket, FIONREAD, &arg) == SOCKET_ERROR)
		{
			std::ostringstream szError;
			szError << "Error while ioctlsocket(): " << GetSocketError();
			throw new SocketException(GetSocketErrorCode(), szError.str());		
		}
	}
	char buf[8096];
	int nBytes = recv(rSocket, buf, sizeof(buf), 0);
	if(nBytes == SOCKET_ERROR)
	{
		std::ostringstream szError;
		szError << "Error while recieving data from client: " << GetSocketError();
		throw new SocketException(GetSocketErrorCode(), szError.str());
	}
	for (int i=0; i < nBytes; i++)
	{
        Data.push_back(buf[i]); 	
	}
}
std::string CBaseSocket::ReceiveBytes(void *argPtr, void (*callback)(void *arg, size_t TotalByteCount), int amountBytes)
{
	std::string ret;
	char buf[8192];
	for ( ; ; )
	{
		u_long arg = 0;
		if (ioctl(rSocket, FIONREAD, &arg) == SOCKET_ERROR)
		{
			std::ostringstream szError;
			szError << "Error while running ioctl(): " << GetSocketError();
			throw new SocketException(GetSocketErrorCode(), szError.str());
		}
		
		if (arg > 8192) arg = 8192;

		int rv = recv(rSocket, buf, sizeof(buf), 0);
		if (rv <= 0) break;
		std::string t;
		t.assign (buf, rv);
		ret += t;
		callback(argPtr, ret.size());
		if(amountBytes == (int)ret.size()) break;
	}
	return ret;
}

void CBaseSocket::SendBytes(const char *s, int length)
{
	if(send(rSocket,s,length,0) == SOCKET_ERROR)
	{
		std::ostringstream szError;
		szError << "Error while running ioctl(): " << GetSocketError();
		throw new SocketException(GetSocketErrorCode(), szError.str());		
	}
}
