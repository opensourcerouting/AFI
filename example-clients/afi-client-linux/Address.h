//
// Copyright (C) 2017 by Open Source Routing.
//
// All rights reserved.
//
// Notice and Disclaimer: This code is licensed to you under the Apache
// License 2.0 (the "License"). You may not use this code except in compliance
// with the License. This code is not an official Juniper product. You can
// obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
// Third-Party Code: This code may depend on other components under separate
// copyright notice and license terms. Your use of the source code for those
// components is subject to the terms and conditions of the respective license
// as noted in the Third-Party source code file.
//

#ifndef _ADDRESS_H_
#define _ADDRESS_H_

#include <iostream>
#include <sstream>

#include <arpa/inet.h>
#include <cstring>
#include <err.h>

class Address
{
      public:
	Address(void)
	{
		af = AF_UNSPEC;
		memset(&u, 0, sizeof(u));
	};

	Address(int af, const void *address, int length)
	{
		set(af, address, length);
	}

	Address(const struct in_addr &address)
	{
		set(address);
	}

	Address(const struct in6_addr &address)
	{
		set(address);
	}

	void set(int af, const void *address, int length)
	{
		this->af = af;

		switch (af) {
		case AF_INET:
			memcpy(&u.v4, address, length);
			break;
		case AF_INET6:
			memcpy(&u.v6, address, length);
			break;
		default:
			errx(1, "%s: unknown address-family", __func__);
		}
	};

	void set(const struct in_addr &address)
	{
		af = AF_INET;
		u.v4 = address;
	};

	void set(const struct in6_addr &address)
	{
		af = AF_INET6;
		u.v6 = address;
	};

	bool isSet(void) const
	{
		switch (af) {
		case AF_UNSPEC:
			return false;
		case AF_INET:
			return (u.v4.s_addr != INADDR_ANY);
		case AF_INET6:
			return (!IN6_IS_ADDR_UNSPECIFIED(&u.v6));
		default:
			errx(1, "%s: unknown address-family", __func__);
		}
	}

	bool isSpecial(void) const
	{
		uint32_t a;

		switch (af) {
		case AF_INET:
			a = ntohl(u.v4.s_addr);
			if (((a >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
			    || IN_MULTICAST(a) || IN_BADCLASS(a))
				return true;
			break;
		case AF_INET6:
			if (IN6_IS_ADDR_LOOPBACK(&u.v6)
			    || IN6_IS_ADDR_MULTICAST(&u.v6)
			    || IN6_IS_ADDR_SITELOCAL(&u.v6)
			    || IN6_IS_ADDR_V4MAPPED(&u.v6)
			    || IN6_IS_ADDR_V4COMPAT(&u.v6))
				return true;
			break;
		default:
			errx(1, "%s: unknown address-family", __func__);
		}

		return false;
	}

	static const char *printAf(int af)
	{
		switch (af) {
		case AF_INET:
			return "ipv4";
		case AF_INET6:
			return "ipv6";
		default:
			errx(1, "%s: unknown address-family", __func__);
		}
	}

	bool operator<(const Address &address) const
	{
		if (af < address.af)
			return true;
		if (af > address.af)
			return false;

		switch (af) {
		case AF_UNSPEC:
			return false;
		case AF_INET:
			return (u.v4.s_addr < address.u.v4.s_addr);
		case AF_INET6:
			return (memcmp(&u.v6, &address.u.v6,
				       sizeof(struct in6_addr))
				< 0);
		default:
			errx(1, "%s: unknown address-family", __func__);
		}
	}

	bool operator>(const Address &address) const
	{
		return address < *this;
	}

	bool operator==(const Address &address) const
	{
		return (*this < address == address < *this);
	}

	std::string str(void) const
	{
		std::stringstream ss;
		char buf[64];

		ss << inet_ntop(af, &u, buf, sizeof(buf));
		return ss.str();
	}

	int af;
	union {
		struct in_addr v4;
		struct in6_addr v6;
		uint8_t bytes[16];
	} u;
};

inline std::ostream &operator<<(std::ostream &os, const Address &address)
{
	os << address.str();
	return os;
}

#endif /* _ADDRESS_H_ */
