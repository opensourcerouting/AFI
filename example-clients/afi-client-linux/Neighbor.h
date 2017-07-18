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

#ifndef _NEIGHBOR_H_
#define _NEIGHBOR_H_

#include <iostream>
#include <sstream>
#include <iomanip>

#include <linux/if_ether.h>

#include "Address.h"

class Neighbor
{
      public:
	Neighbor(void)
	    : ipAddr()
	    , macAddr()
	    , ifIndex(){};

	std::string printMacAddress(void) const
	{
		std::stringstream output;

		for (int i = 0; i < ETH_ALEN; i++) {
			output << std::setw(2) << std::setfill('0') << std::hex;
			output << static_cast<unsigned int>(macAddr[i]);
			if (i + 1 != ETH_ALEN)
				output << ":";
		}

		return output.str();
	}

	bool operator<(const Neighbor &neighbor) const
	{
		if (ipAddr < neighbor.ipAddr)
			return true;
		if (ipAddr > neighbor.ipAddr)
			return false;

		int mac_cmp = memcmp(&macAddr, &neighbor.macAddr, ETH_ALEN);
		if (mac_cmp < 0)
			return true;
		if (mac_cmp > 0)
			return false;

		return ifIndex < neighbor.ifIndex;
	}

	bool operator>(const Neighbor &neighbor) const
	{
		return neighbor < *this;
	}

	bool operator==(const Neighbor &neighbor) const
	{
		return (*this < neighbor == neighbor < *this);
	}

	Address ipAddr;
	uint8_t macAddr[ETH_ALEN];
	int ifIndex;
};

struct NeighborCompare {
	bool operator()(const Neighbor &a, const Neighbor &b) const
	{
		return a.ipAddr < b.ipAddr;
	}
};

inline std::ostream &operator<<(std::ostream &os, const Neighbor &neighbor)
{
	os << "ip " << neighbor.ipAddr << " lladdr "
	   << neighbor.printMacAddress();
	os << " ifindex " << std::dec << neighbor.ifIndex;
	return os;
}

#endif /* _NEIGHBOR_H_ */
