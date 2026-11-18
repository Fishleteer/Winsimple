#ifndef WINSIMPLE_NETWORK_HPP
#define WINSIMPLE_NETWORK_HPP

#include <winsock2.h>
#include <WS2tcpip.h>
#include <string>
#include <iostream>
#include <cstdint>

#pragma comment (lib, "ws2_32.lib")

namespace ws
{
	class WinsockInit 
	{
		public:
		WinsockInit(); 
		~WinsockInit(); 
		
		WinsockInit(const WinsockInit&) = delete;
		WinsockInit& operator=(const WinsockInit&) = delete;
	};

	
	class Socket
	{
		public:
		
		Socket();
		~Socket();
		explicit Socket(SOCKET s) : m_sock(s){}

		Socket(Socket&& o) noexcept; 
		Socket& operator=(Socket&& o) noexcept; 
		Socket(const Socket&) = delete;
		Socket& operator=(const Socket&) = delete;

		enum class Event
		{
			Closed,
			Nothing,
			Connection,
			Data
		};
		
		enum class Role
		{None,Connection,Listener};
		
		
		Event pollEvent();
		bool isValid();
		Socket accept();
		SOCKET getHandle();
		
		
		bool setReuseIp(bool on = true);
		bool setBlocking(bool on = true);
		bool listen(int maximumConnects = SOMAXCONN);
		bool bind(const std::string &ip,uint16_t port);
		bool connect(const std::string &ip,uint16_t port);
		
		int sendData(const void* data,int len);
		int getData(void* buf,int len);
		
		std::string getIP();
		std::string getPort();
		std::string getPeerIP();
		std::string getPeerPort();
		
		
		private:
		void close();
		bool toIP(const std::string &name,std::string &address);

		static bool sockAddrInfo(SOCKET s, bool peer, sockaddr_in& out)
		{
			int len = sizeof(out);
			return (peer ? 
			::getpeername(s, reinterpret_cast<sockaddr*>(&out), &len)
			: 
			::getsockname(s, reinterpret_cast<sockaddr*>(&out), &len)) == 0;
		}

		static std::string ipString(const sockaddr_in& a)
		{
			char buf[INET_ADDRSTRLEN] = {};
			if(!inet_ntop(AF_INET, &a.sin_addr, buf, sizeof(buf))) return {};
			return buf;
		}		
		
		Role m_role = Role::None;
		SOCKET m_sock = INVALID_SOCKET;
	};


	#ifdef WINSIMPLE_NETWORK_IMPL
	
	WinsockInit winsockInit;
	
	WinsockInit::WinsockInit() 
	{
		WSADATA d;
		if(WSAStartup(MAKEWORD(2, 2), &d) != 0)
			std::cerr << "\nWinsimple Error: WSAStartup failed\n";
	}
	
	WinsockInit::~WinsockInit() 
	{ WSACleanup(); }	
	
	
	Socket::Socket()
	{m_sock = ::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);setReuseIp();}
	
	Socket::~Socket()
	{close();}

	Socket::Socket(Socket&& o) noexcept : m_sock(o.m_sock),m_role(o.m_role)
	{ o.m_sock = INVALID_SOCKET; o.m_role = Role::None;}

	Socket& Socket::operator=(Socket&& o) noexcept 
	{
		if(this != &o) 
		{ 
			close(); 
			m_sock = o.m_sock;
			m_role = o.m_role;
			o.m_sock = INVALID_SOCKET;
			o.m_role = Role::None;
		}
		return *this;
	}
	
	
	
	bool Socket::bind(const std::string &ip,uint16_t port)
	{
		std::string ipa = "";
		if(!toIP(ip,ipa))
			return false;
		sockaddr_in hint{};
		hint.sin_family = AF_INET;
		hint.sin_port = htons(port);
		if(inet_pton(AF_INET,ipa.c_str(),&hint.sin_addr) != 1) return false;
		return ::bind(m_sock,reinterpret_cast<sockaddr*>(&hint),sizeof(hint)) == 0;
	}
	
	bool Socket::connect(const std::string &ip,uint16_t port)
	{
		std::string ipa = "";
		if(!toIP(ip,ipa))
			return false;
		sockaddr_in hint{};
		hint.sin_family = AF_INET;
		hint.sin_port = htons(port);
		if(inet_pton(AF_INET,ipa.c_str(),&hint.sin_addr) != 1) return false;
		if(::connect(m_sock,reinterpret_cast<sockaddr*>(&hint),sizeof(hint)) != 0) return false;
		m_role = Role::Connection;
		return true;
	}
	
	bool Socket::listen(int maximumConnects)
	{
		//SOMAXCONN means SOCKET MAXIMUM CONNECTS.
		if(::listen(m_sock,maximumConnects) != 0) return false;
		m_role = Role::Listener;
		return true;
	}		
	
	bool Socket::isValid()
	{return m_sock != INVALID_SOCKET;}
	
	SOCKET Socket::getHandle()
	{return m_sock;}
	
	Socket Socket::accept()
	{
		SOCKET s = ::accept(m_sock,nullptr,nullptr);
		Socket result(s);
		if(result.isValid()) result.m_role = Role::Connection;
		return result;
	}
	
	bool Socket::setBlocking(bool on)
	{
		u_long v = on ? 0u : 1u;
		return ioctlsocket(m_sock,FIONBIO,&v) == 0;
	}

    bool Socket::setReuseIp(bool on) 
	{
        BOOL v = on ? TRUE : FALSE;
        return setsockopt(m_sock,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&v),sizeof(v)) == 0;
    }

	int Socket::sendData(const void* data,int len)
	{return ::send(m_sock,static_cast<const char*>(data),len,0);}
	
	int Socket::getData(void* buf,int len)
	{return ::recv(m_sock,static_cast<char*>(buf),len,0);}
	
	void Socket::close()
	{
		if(isValid()) 
			::closesocket(m_sock); 
		m_sock = INVALID_SOCKET; 
		m_role = Role::None;
	}
	
	bool Socket::toIP(const std::string &name,std::string &address)
	{
		addrinfo hints = {};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		
		addrinfo* result = nullptr;
		int status = getaddrinfo(name.c_str(),nullptr,&hints,&result);
		if(status != 0)
			return false;
		
		for(addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) 
		{
			//only IPv4 allowed for now. I will fix this later.
			char ipString[INET6_ADDRSTRLEN] = { 0 };
			void* addrPtr = nullptr;
			std::string ipVersion;			
			
			if(ptr->ai_family == AF_INET)
			{
				sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
				addrPtr = &(ipv4->sin_addr);
				ipVersion = "IPv4";				
			}
	
			if(addrPtr != nullptr) 
			{
				inet_ntop(ptr->ai_family, addrPtr, ipString, sizeof(ipString));
				address = ipString;
				return true; 
			}	
		}
		return false;
	}
	
	ws::Socket::Event Socket::pollEvent()
	{
		if(!isValid()) return Event::Closed;
		if(m_role == Role::None) return Event::Nothing;
		
		fd_set fd;
		FD_ZERO(&fd);
		FD_SET(m_sock,&fd);
		
		timeval timeout = {0,0};//return immediately
		int result = select(0,&fd,nullptr,nullptr,&timeout);
		
		if(result == SOCKET_ERROR) return Event::Closed;
		
		if(result <= 0 || !FD_ISSET(m_sock,&fd)) return Event::Nothing;
		
		if(m_role == Role::Listener) return Event::Connection;
		
		u_long avail = 0;
		if(ioctlsocket(m_sock,FIONREAD,&avail) == SOCKET_ERROR)
			return Event::Closed;
		return (avail > 0) ? Event::Data : Event::Closed;
	}
	
	std::string Socket::getIP()
	{
		sockaddr_in a{}; 
		return sockAddrInfo(m_sock, false, a) ? ipString(a) : std::string{};	
	}
	std::string Socket::getPort()
	{
		sockaddr_in a{}; 
		return sockAddrInfo(m_sock, false, a) ? std::to_string(ntohs(a.sin_port)) : std::string{};;	
	}
	std::string Socket::getPeerIP()
	{
		sockaddr_in a{}; 
		return sockAddrInfo(m_sock, true, a) ? ipString(a) : std::string{};	
	}
	std::string Socket::getPeerPort()
	{
		sockaddr_in a{}; 
		return sockAddrInfo(m_sock, true, a) ? std::to_string(ntohs(a.sin_port)) : std::string{};;	
	}
	
	
	#endif
}

#endif