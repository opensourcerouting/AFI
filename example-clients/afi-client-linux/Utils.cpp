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

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_ether.h>

#include "Utils.h"

std::string printMacAddress(const uint8_t *mac)
{
	std::stringstream output;

	for (int i = 0; i < ETH_ALEN; i++) {
		output << std::setw(2) << std::setfill('0') << std::hex;
		output << static_cast<unsigned int>(mac[i]);
		if (i + 1 != ETH_ALEN)
			output << ":";
	}

	return output.str();
}

//
// @fn
// getMacAddress
//
// @brief
// get MAC address of the interface
//
// @param[in]
//     dev Device name
// @return  MAC address
//

uint8_t *getMacAddress(std::string &ifName)
{
	static struct ifreq ifr;
	int fd;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		warn("socket");
		return NULL;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifName.c_str(), IFNAMSIZ);
	if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
		warn("ioctl");
		close(fd);
		return NULL;
	}
	close(fd);

	return (uint8_t *)ifr.ifr_hwaddr.sa_data;
}
