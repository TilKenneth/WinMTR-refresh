/*
WinMTR
Copyright (C)  2010-2019 Appnor MSP S.A. - http://www.appnor.com
Copyright (C) 2019-2021 Leetsoftwerx

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


//*****************************************************************************
// FILE:            WinMTRNet.cpp
//
//*****************************************************************************

#include "WinMTRGlobal.h"
#include "WinMTRNet.h"
#include "resource.h"
#include <icmpapi.h>
import WinMTRICMPUtils;

namespace {

	struct ICMPHandleTraits
	{
		using type = HANDLE;

		static void close(type value) noexcept
		{
			WINRT_VERIFY_(TRUE, ::IcmpCloseHandle(value));
		}

		[[nodiscard]]
		static type invalid() noexcept
		{
			return INVALID_HANDLE_VALUE;
		}
	};
	
	using IcmpHandle = winrt::handle_type<ICMPHandleTraits>;

	[[nodiscard]]
	inline bool operator==(const SOCKADDR_STORAGE& lhs, const SOCKADDR_STORAGE& rhs) noexcept {
		return memcmp(&lhs, &rhs, sizeof(SOCKADDR_STORAGE)) == 0;
	}
}

using namespace std::chrono_literals;
struct trace_thread final {
	trace_thread(const trace_thread&) = delete;
	trace_thread& operator=(const trace_thread&) = delete;
	trace_thread(trace_thread&&) = default;
	trace_thread& operator=(trace_thread&&) = default;

	trace_thread(ADDRESS_FAMILY af, WinMTRNet* winmtr, UCHAR ttl) noexcept
		:
		address(),
		winmtr(winmtr),
		ttl(ttl)
	{
		if (af == AF_INET) {
			icmpHandle.attach(IcmpCreateFile());
		}
		else if (af == AF_INET6) {
			icmpHandle.attach(Icmp6CreateFile());
		}
	}
	SOCKADDR_STORAGE address;
	IcmpHandle icmpHandle;
	WinMTRNet* winmtr;
	UCHAR		ttl;
};


WinMTRNet::WinMTRNet(const IWinMTROptionsProvider* wp)
	:host(),
	last_remote_addr(),
	options(wp),
	wsaHelper(MAKEWORD(2, 2)),
	tracing(false) {

	if (!wsaHelper) [[unlikely]] {
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return;
	}
}

WinMTRNet::~WinMTRNet() noexcept
{}

void WinMTRNet::ResetHops() noexcept
{
	for (auto& h : this->host) {
		h = s_nethost();
	}
}

template<class T>
winrt::Windows::Foundation::IAsyncAction WinMTRNet::handleICMP(trace_thread current) {
	using namespace std::literals;
	using traits = icmp_ping_traits<T>;
	trace_thread mine = std::move(current);
	co_await winrt::resume_background();
	using namespace std::string_view_literals;
	const auto				nDataLen = this->options->getPingSize();
	std::vector<std::byte>	achReqData(nDataLen, static_cast<std::byte>(32)); //whitespaces
	std::vector<std::byte> achRepData(reply_reply_buffer_size<T>(nDataLen));

	auto cancellation = co_await winrt::get_cancellation_token();
	T * addr = reinterpret_cast<T*>(&mine.address);
	while (this->tracing) {
		if (cancellation()) [[unlikely]]
		{
			co_return;
		}
			// For some strange reason, ICMP API is not filling the TTL for icmp echo reply
			// Check if the current thread should be closed
		if (mine.ttl > this->GetMax()) break;

		// NOTE: some servers does not respond back everytime, if TTL expires in transit; e.g. :
		// ping -n 20 -w 5000 -l 64 -i 7 www.chinapost.com.tw  -> less that half of the replies are coming back from 219.80.240.93
		// but if we are pinging ping -n 20 -w 5000 -l 64 219.80.240.93  we have 0% loss
		// A resolution would be:
		// - as soon as we get a hop, we start pinging directly that hop, with a greater TTL
		// - a drawback would be that, some servers are configured to reply for TTL transit expire, but not to ping requests, so,
		// for these servers we'll have 100% loss
		const auto dwReplyCount = co_await IcmpSendEchoAsync(mine.icmpHandle.get(), addr, mine.ttl, achReqData, achRepData);
		this->AddXmit(mine.ttl - 1);
		if (dwReplyCount) {
			auto icmp_echo_reply = reinterpret_cast<traits::reply_type_ptr>(achRepData.data());
			TRACE_MSG(L"TTL "sv << mine.ttl  << L" Status "sv << icmp_echo_reply->Status << L" Reply count "sv << dwReplyCount);

			switch (icmp_echo_reply->Status) {
			case IP_SUCCESS:
			[[likely]] case IP_TTL_EXPIRED_TRANSIT:
				this->addNewReturn(mine.ttl - 1, icmp_echo_reply->RoundTripTime);
				{
					auto naddr = traits::to_addr_from_ping(icmp_echo_reply);
					this->SetAddr(mine.ttl - 1, *reinterpret_cast<sockaddr*>(&naddr));
				}
				break;
			case IP_BUF_TOO_SMALL:
				this->SetName(current.ttl - 1, L"Reply buffer too small."s);
				break;
			case IP_DEST_NET_UNREACHABLE:
				this->SetName(current.ttl - 1, L"Destination network unreachable."s);
				break;
			case IP_DEST_HOST_UNREACHABLE:
				this->SetName(current.ttl - 1, L"Destination host unreachable."s);
				break;
			case IP_DEST_PROT_UNREACHABLE:
				this->SetName(current.ttl - 1, L"Destination protocol unreachable."s);
				break;
			case IP_DEST_PORT_UNREACHABLE:
				this->SetName(current.ttl - 1, L"Destination port unreachable."s);
				break;
			case IP_NO_RESOURCES:
				this->SetName(current.ttl - 1, L"Insufficient IP resources were available."s);
				break;
			case IP_BAD_OPTION:
				this->SetName(current.ttl - 1, L"Bad IP option was specified."s);
				break;
			case IP_HW_ERROR:
				this->SetName(current.ttl - 1, L"Hardware error occurred."s);
				break;
			case IP_PACKET_TOO_BIG:
				this->SetName(current.ttl - 1, L"Packet was too big."s);
				break;
			case IP_REQ_TIMED_OUT:
				this->SetName(current.ttl - 1, L"Request timed out."s);
				break;
			case IP_BAD_REQ:
				this->SetName(current.ttl - 1, L"Bad request."s);
				break;
			case IP_BAD_ROUTE:
				this->SetName(current.ttl - 1, L"Bad route."s);
				break;
			case IP_TTL_EXPIRED_REASSEM:
				this->SetName(current.ttl - 1, L"The time to live expired during fragment reassembly."s);
				break;
			case IP_PARAM_PROBLEM:
				this->SetName(current.ttl - 1, L"Parameter problem."s);
				break;
			case IP_SOURCE_QUENCH:
				this->SetName(current.ttl - 1, L"Datagrams are arriving too fast to be processed and datagrams may have been discarded."s);
				break;
			case IP_OPTION_TOO_BIG:
				this->SetName(current.ttl - 1, L"An IP option was too big."s);
				break;
			case IP_BAD_DESTINATION:
				this->SetName(current.ttl - 1, L"Bad destination."s);
				break;
			case IP_GENERAL_FAILURE:
			default:
				this->SetName(current.ttl - 1, L"General failure."s);
				break;
			}
			const auto intervalInSec = this->options->getInterval() * 1s;
			const auto roundTripDuration = std::chrono::milliseconds(icmp_echo_reply->RoundTripTime);
			if (intervalInSec > roundTripDuration) {
				const auto sleepTime = intervalInSec - roundTripDuration;
				co_await std::chrono::duration_cast<winrt::Windows::Foundation::TimeSpan>(sleepTime);
			}
		}

	} /* end ping loop */
}


winrt::Windows::Foundation::IAsyncAction WinMTRNet::DoTrace(sockaddr& address)
{
	auto cancellation = co_await winrt::get_cancellation_token();
	cancellation.enable_propagation();
	tracing = true;
	ResetHops();
	last_remote_addr = {};
	memcpy(&last_remote_addr, &address, getAddressSize(address));
	
	auto threadMaker = [&address, this](UCHAR i) {
		trace_thread current(address.sa_family, this, i + 1);
		using namespace std::string_view_literals;
		TRACE_MSG(L"Thread with TTL="sv << current.ttl << L" started."sv);
		memcpy(&current.address, &address, getAddressSize(address));
		if (current.address.ss_family == AF_INET) {
			return this->handleICMP<sockaddr_in>(std::move(current));
		}
		else if (current.address.ss_family == AF_INET6) {
			return this->handleICMP<sockaddr_in6>(std::move(current));
		}
		winrt::throw_hresult(HRESULT_FROM_WIN32(WSAEOPNOTSUPP));
	};

	cancellation.callback([this] () noexcept {
		this->tracing = false;
		TRACE_MSG(L"Cancellation");
	});
	//// one thread per TTL value
	co_await std::invoke([]<UCHAR ...threads>(std::integer_sequence<UCHAR, threads...>, auto threadMaker) {
		return winrt::when_all(std::invoke(threadMaker, threads)...);
	}, std::make_integer_sequence<UCHAR, MAX_HOPS>(), threadMaker);
	TRACE_MSG(L"Tracing Ended");
}

sockaddr_storage WinMTRNet::GetAddr(int at) const
{
	std::unique_lock lock(ghMutex);
	return host[at].addr;
}

int WinMTRNet::GetMax() const
{
	std::unique_lock lock(ghMutex);
	int max = MAX_HOPS;

	// first match: traced address responds on ping requests, and the address is in the hosts list
	for (int i = 1; const auto & h : host) {
		if (h.addr == last_remote_addr) {
			max = i;
			break;
		}
		++i;
	}

	// second match:  traced address doesn't responds on ping requests
	if (max == MAX_HOPS) {
		while ((max > 1) && (host[max - 1].addr == host[max - 2].addr && isValidAddress(host[max - 1].addr))) max--;
	}
	return max;
}

std::vector<s_nethost> WinMTRNet::getCurrentState() const
{
	std::unique_lock lock(ghMutex);
	auto max = GetMax();
	auto end = std::cbegin(host);
	std::advance(end, max);
	return std::vector<s_nethost>(std::cbegin(host), end);
}

s_nethost WinMTRNet::getStateAt(int at) const
{
	std::unique_lock lock(ghMutex);
	return host[at];
}

void WinMTRNet::SetAddr(int at, sockaddr& addr)
{
	std::unique_lock lock(ghMutex);
	if (!isValidAddress(host[at].addr) && isValidAddress(addr)) {
		host[at].addr = {};
		//TRACE_MSG(L"Start DnsResolverThread for new address " << addr << L". Old addr value was " << host[at].addr);
		memcpy(&host[at].addr, &addr, getAddressSize(addr));

		if (options->getUseDNS()) {
			Concurrency::create_task([at, sharedThis = shared_from_this()] {
					TRACE_MSG(L"DNS resolver thread started.");

					wchar_t buf[NI_MAXHOST] = {};
					sockaddr_storage addr = sharedThis->GetAddr(at);

					if (const auto nresult = GetNameInfoW(
						reinterpret_cast<sockaddr*>(&addr)
						, static_cast<socklen_t>(getAddressSize(addr))
						, buf
						, static_cast<DWORD>(std::size(buf))
						, nullptr
						, 0
						, 0);
						// zero on success
						!nresult) {
						sharedThis->SetName(at, buf);
					}
					else {
						std::wstring out;
						out.resize(40);
						auto addrlen = static_cast<DWORD>(getAddressSize(addr));
						DWORD addrstrsize = static_cast<DWORD>(out.size());
						if (const auto result = WSAAddressToStringW(
							reinterpret_cast<LPSOCKADDR>(&addr)
							,addrlen
							,nullptr
							,out.data()
							,&addrstrsize);
							// zero on success
							!result) {
							out.resize(addrstrsize - 1);
						}
						sharedThis->SetName(at, std::move(out));
					}

					TRACE_MSG(L"DNS resolver thread stopped.");
				});
		}
	}
}

void WinMTRNet::SetName(int at, std::wstring n)
{
	std::unique_lock lock(ghMutex);
	host[at].name = std::move(n);
}

void WinMTRNet::addNewReturn(int at, int last) {
	std::unique_lock lock(ghMutex);
	host[at].last = last;
	host[at].total += last;
	if (host[at].best > last || host[at].xmit == 1) {
		host[at].best = last;
	};
	if (host[at].worst < last) {
		host[at].worst = last;
	}
	host[at].returned++;
}

void WinMTRNet::AddXmit(int at)
{
	std::unique_lock lock(ghMutex);
	host[at].xmit++;
}

std::wstring s_nethost::getName() const
{
	if (name.empty()) {
		SOCKADDR_STORAGE laddr = addr;
		if (!isValidAddress(addr)) {
			return {};
		}
		std::wstring out;
		out.resize(40);
		auto addrlen = getAddressSize(addr);
		DWORD addrstrsize = static_cast<DWORD>(out.size());
		if (auto result = WSAAddressToStringW(reinterpret_cast<LPSOCKADDR>(&laddr), static_cast<DWORD>(addrlen), nullptr, out.data(), &addrstrsize); !result) {
			out.resize(addrstrsize - 1);
		}

		return out;
	}
	return name;
}
