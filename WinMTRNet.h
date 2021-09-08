//*****************************************************************************
// FILE:            WinMTRNet.h
//
//
// DESCRIPTION:
//   
//
// NOTES:
//    
//
//*****************************************************************************

#ifndef WINMTRNET_H_
#define WINMTRNET_H_
#include <string>
#include <atomic>
#include <mutex>
#include <array>
#include <vector>
#include "WinMTRWSAhelper.h"


constexpr auto MAX_HOPS = 30;

struct __declspec(novtable) IWinMTROptionsProvider {
	virtual int getPingSize() const noexcept = 0;
	virtual double getInterval() const noexcept = 0;
	virtual bool getUseDNS() const noexcept = 0;
};

struct s_nethost {
  SOCKADDR_STORAGE addr = {};
  std::wstring name;
  int xmit = 0;			// number of PING packets sent
  int returned = 0;		// number of ICMP echo replies received
  unsigned long total = 0;	// total time
  int last = 0;				// last time
  int best = 0;				// best time
  int worst = 0;			// worst time
  inline auto getPercent() const noexcept {
	  return (xmit == 0) ? 0 : (100 - (100 * returned / xmit));
  }
  inline int getAvg() const noexcept {
	  return returned == 0 ? 0 : total / returned;
  }
  std::wstring getName() const;
};

template<class T>
concept socket_type = requires(T a, ADDRESS_FAMILY afam) {
	a.sa_family;
	std::convertible_to<decltype(a.sa_family), ADDRESS_FAMILY>;
};

template<typename T>
inline constexpr auto getAddressFamily(const T& addr) noexcept {
	if constexpr (socket_type<T>) {
		return addr.sa_family;
	}
	else if (std::is_same_v<SOCKADDR_STORAGE, T>) {
		return addr.ss_family;
	}
}

template<typename T>
inline constexpr size_t getAddressSize(const T& addr) noexcept {
	return getAddressFamily(addr) == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
}

template<typename T>
inline constexpr bool isValidAddress(const T& addr) noexcept {
	const ADDRESS_FAMILY family = getAddressFamily(addr);
	
	return (family == AF_INET || family == AF_INET6);
}
struct trace_thread;

//*****************************************************************************
// CLASS:  WinMTRNet
//
//
//*****************************************************************************

class WinMTRNet final: public std::enable_shared_from_this<WinMTRNet>{
	WinMTRNet(const WinMTRNet&) = delete;
	WinMTRNet& operator=(const WinMTRNet&) = delete;
public:

	WinMTRNet(const IWinMTROptionsProvider*wp);
	~WinMTRNet() noexcept;
	[[nodiscard("The task should be awaited")]]
	winrt::Windows::Foundation::IAsyncAction	DoTrace(sockaddr& address);
	void	ResetHops() noexcept;
	//winrt::fire_and_forget	StopTrace() noexcept;

	SOCKADDR_STORAGE GetAddr(int at) const;
	std::wstring GetName(int at);
	int		GetBest(int at) const;
	int		GetWorst(int at) const;
	int		GetAvg(int at) const;
	int		GetPercent(int at) const;
	int		GetLast(int at) const;
	int		GetReturned(int at) const;
	int		GetXmit(int at) const;
	[[nodiscard]]
	int		GetMax() const;
	[[nodiscard]]
	std::vector<s_nethost> getCurrentState() const;

private:
	std::array<s_nethost, MAX_HOPS>	host;
	SOCKADDR_STORAGE last_remote_addr;
	mutable std::recursive_mutex	ghMutex;
	const IWinMTROptionsProvider* options;
	winmtr::helper::WSAHelper wsaHelper;
	std::atomic_bool	tracing;
	std::optional<winrt::Windows::Foundation::IAsyncAction> tracer;
	std::optional<winrt::apartment_context> context;

	void	SetAddr(int at, sockaddr& addr);
	void	SetName(int at, std::wstring n);
	void	SetBest(int at, int current);
	void	SetWorst(int at, int current);
	void addNewReturn(int ttl, int last);
	void	SetLast(int at, int last);
	void	AddReturned(int at);
	void	AddXmit(int at);

	template<class T>
	[[nodiscard("The task should be awaited")]]
	winrt::Windows::Foundation::IAsyncAction handleICMP(trace_thread current);
};

#endif	// ifndef WINMTRNET_H_
