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
#include <fstream>

#include <linux/rtnetlink.h>

#include "yaml-cpp/yaml.h"

#include "AfiClient.h"
#include "Netlink.h"
#include "FpmServer.h"

struct config_if {
	std::string name;
	unsigned int portIndex;
	std::string tap;
};

struct config {
	unsigned int netlink_buffer_size;
	bool use_fpm_interface;
	std::string jfi_server_address;
	std::string jfi_hostpath_address;
	std::vector<struct config_if> interfaces;
};

int readConfig(const std::string &path, struct config &config)
{
	YAML::Node yaml = YAML::LoadFile(path);

	try {
		config.netlink_buffer_size =
			yaml["netlink-buffer-size"].as<unsigned int>();
		config.use_fpm_interface = yaml["use-fpm-interface"].as<bool>();
		config.jfi_server_address =
			yaml["afi-server-address"].as<std::string>();
		config.jfi_hostpath_address =
			yaml["afi-hostpath-address"].as<std::string>();

		const YAML::Node &interfaces = yaml["interfaces"];
		for (YAML::const_iterator it = interfaces.begin();
		     it != interfaces.end(); ++it) {
			const YAML::Node &interface = *it;
			struct config_if config_if;

			config_if.name = interface["name"].as<std::string>();
			config_if.portIndex =
				interface["port"].as<unsigned int>();
			config_if.tap = interface["tap"].as<std::string>();
			config.interfaces.push_back(config_if);
		}
	} catch (YAML::Exception &e) {
		std::cerr << e.what() << std::endl;
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct config config;

	if (readConfig("config.yml", config) < 0)
		errx(1, "Failed reading configuration file");

	std::string afiServerAddr(config.jfi_server_address);
	std::string afiHostpathAddr(config.jfi_hostpath_address);
	boost::asio::io_service io_service;
	AfiClient afiClient(io_service, afiServerAddr, afiHostpathAddr,
			    AFT_CLIENT_HOSTPATH_PORT);
	AfiClient::setInstance(&afiClient);
	afiClient.setup();
	for (auto it = config.interfaces.begin(); it != config.interfaces.end();
	     ++it)
		afiClient.setupInterface(it->name, it->portIndex, it->tap);

	Netlink netlink;
	netlink.setRecvBuf(config.netlink_buffer_size);

	unsigned int netlink_groups = RTMGRP_NEIGH;
	if (!config.use_fpm_interface)
		netlink_groups |= (RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_ROUTE);

	netlink.bind(netlink_groups);
	netlink.fetch(RTM_GETNEIGH, AF_INET);
	netlink.fetch(RTM_GETNEIGH, AF_INET6);
	if (!config.use_fpm_interface) {
		netlink.fetch(RTM_GETROUTE, AF_INET);
		netlink.fetch(RTM_GETROUTE, AF_INET6);
	} else {
		FpmServer::initialize();
	}

	int netlink_fd, maxfd;
	netlink_fd = netlink.getFd();

	std::vector<int> fds;
	fds.push_back(netlink_fd);
	if (config.use_fpm_interface)
		fds.push_back(FpmServer::getFd());

	while (1) {
		int ret;
		fd_set rd_set;
		int newfd = -1, delfd = -1;

		FD_ZERO(&rd_set);
		for (auto i_fd = fds.begin(); i_fd != fds.end(); i_fd++)
			FD_SET(*i_fd, &rd_set);
		maxfd = *std::max_element(fds.begin(), fds.end());

		ret = select(maxfd + 1, &rd_set, NULL, NULL, NULL);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			err(1, "select");
		}

		for (auto i_fd = fds.begin(); i_fd != fds.end(); i_fd++) {
			int fd = *i_fd;

			if (!FD_ISSET(fd, &rd_set))
				continue;

			if (fd == netlink_fd)
				netlink.handle();
			else if (fd == FpmServer::getFd())
				newfd = FpmServer::accept();
			else {
				FpmConn fpmConn = FpmServer::getConn(fd);
				if (fpmConn.handle() < 0) {
					FpmServer::deleteConn(fd);
					delfd = fd;
				}
			}
		}
		if (newfd != -1)
			fds.push_back(newfd);
		if (delfd != -1)
			fds.erase(std::remove(fds.begin(), fds.end(), delfd),
				  fds.end());
	}

	exit(EXIT_SUCCESS);
}
