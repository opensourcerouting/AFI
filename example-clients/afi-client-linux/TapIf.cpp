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

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <linux/if_packet.h>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "TapIf.h"

TapIf::TapIf(void)
{
	_tapFd = -1;
}

TapIf::~TapIf(void)
{
	if (!isInitialized())
		return;

	_tapThread->join();
	delete _tapThread;
	close(_tapFd);
}

int TapIf::init(AftIndex portIndex, std::string ifName, std::string outputIfName)
{
	int tapFd;
	uint8_t *macAddr;

	if (ifName.length() >= IFNAMSIZ) {
		std::cout << "Tap interface name is too long" << std::endl;
		return -1;
	}

	if ((tapFd = tapAlloc(ifName, IFF_TAP | IFF_NO_PI)) < 0) {
		std::cout << "Error connecting to tap interface " << ifName
			  << std::endl;
		return -1;
	}

	macAddr = getMacAddress(ifName);
	if (macAddr)
		memcpy(_macAddr, macAddr, ETH_ALEN);
	else
		memset(_macAddr, 0, ETH_ALEN);

	_ifName = ifName;
	_ifIndex = if_nametoindex(ifName.c_str());
	_portIndex = portIndex;
	_outputIfName = outputIfName;
	_tapFd = tapFd;
	_tapThread = new boost::thread(boost::bind(&TapIf::ifRead, this));

	return _tapFd;
}

//
// @fn
// tapAlloc
//
// @brief
// allocates or reconnects to a tun/tap device. The caller
// The caller must reserve enough space in *dev.
//
// @param[in]
//     dev Device name
// @param[in]
//     flags Flags
// @return  file descriptor
//

int TapIf::tapAlloc(std::string ifName, int flags)
{
	struct ifreq ifr;
	int fd, err;
#define DEVICE_FILE_NAME_MAX_LEN 50
	char clonedev[DEVICE_FILE_NAME_MAX_LEN];

	snprintf(clonedev, DEVICE_FILE_NAME_MAX_LEN, "%s", "/dev/net/tun");

	if ((fd = open(clonedev, O_RDWR)) < 0) {
		warn("Opening /dev/net/tun");
		return fd;
	}

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = flags;
	strncpy(ifr.ifr_name, ifName.c_str(), IFNAMSIZ);

	if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) {
		warn("ioctl(TUNSETIFF)");
		close(fd);
		return err;
	}

	return fd;
}

//
// @fn
// tapAlloc
//
// @brief
// get MAC address of the interface
//
// @param[in]
//     dev Device name
// @return  file descriptor
//

uint8_t *TapIf::getMacAddress(std::string ifName)
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

//
// @fn
// isInitialized
//
// @brief
// Check if the interface is initialized or not.
//
// @param[in] void
// @return  1 - Yes, 0 - No
//

bool TapIf::isInitialized(void)
{
	return _tapFd != -1;
}

//
// @fn
// ifRead
//
// @brief
// Reads interface
//
// @param[in] void
// @return  0 - Success, -1 - Error
//

int TapIf::ifRead(void)
{
	uint16_t n;
	char data[2000];
	int raw_socket;
	struct sockaddr_ll sa;

	raw_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (raw_socket < 0)
		err(1, "socket");

	memset(&sa, 0, sizeof(struct sockaddr_ll));
	sa.sll_family = AF_PACKET;
	sa.sll_halen = ETH_ALEN;
	sa.sll_ifindex = if_nametoindex(_outputIfName.c_str());

	while (1) {
		n = read(_tapFd, data, sizeof(data));
		if (n < 0) {
			warn("read");
			continue;
		}

		n = sendto(raw_socket, data, n, 0, (struct sockaddr *)&sa,
			   sizeof(sa));
		if (n < 0)
			warn("sendto");
	}

	return 0;
}

//
// @fn
// ifWrite
//
// @brief
// Writes data to interface
//
// @param[in]
//     size Number of bytes to write
// @param[in]
//     data Buffer with actual data
// @return  0 - Success, -1 - Error
//

int TapIf::ifWrite(int size, uint8_t *data)
{
	int n = write(_tapFd, data, size);
	if (n < 0) {
		warn("write");
		return -1;
	}

	return 0;
}
