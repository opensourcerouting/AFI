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

#ifndef __AfiClient__
#define __AfiClient__

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>

#include "jnx/AfiTransport.h"
#include "jnx/Aft.h"

#include "TapIf.h"
#include "Netlink.h"
#include "Route.h"
#include "Neighbor.h"

#define BOOST_UDP boost::asio::ip::udp::udp

//
// @class   AfiClient
// @brief   Implements a sample AFI client
//
class AfiClient
{
      public:
	//
	// Constructor
	//
	AfiClient(boost::asio::io_service &ioService,
		  const std::string &afiServerAddr,
		  const std::string &afiHostpathAddr, short port)
	    : _afiServerAddr(afiServerAddr)
	    , _afiHostpathAddr(afiHostpathAddr)
	    , _ioService(ioService)
	    , _hpUdpSock(ioService, BOOST_UDP::endpoint(BOOST_UDP::v4(), port))
	{
		BOOST_UDP::resolver resolver(_ioService);

		std::vector<std::string> hostpathAddr_substrings;
		boost::split(hostpathAddr_substrings, _afiHostpathAddr,
			     boost::is_any_of(":"));
		std::string &hpIpStr = hostpathAddr_substrings.at(0);
		std::string &hpUDPPortStr = hostpathAddr_substrings.at(1);

		BOOST_UDP::resolver::query query(BOOST_UDP::v4(), hpIpStr,
						 hpUDPPortStr);
		_puntingPorts.resize(8);

		//
		// Initialize transport connection to AFI server
		//
		_transport = AfiTransport::create(_afiServerAddr);
		assert(_transport != nullptr);

		//
		// Start AFI packer receiver
		//
		std::thread udpSrvr([this] { this->hostPathUDPSrvr(); });
		udpSrvr.detach();
	}

	//
	// Destructor
	//
	~AfiClient();

	//
	// Setup
	//
	int setup(void);

	//
	// Setup Interface
	//
	void setupInterface(std::string &interface, unsigned int port,
			    std::string &tap);

	//
	// Set singleton instance
	//
	static void setInstance(AfiClient *afiClient)
	{
		instance = afiClient;
	}

	//
	// Get singleton instance
	//
	static AfiClient *getInstance(void)
	{
		return instance;
	}

	//
	// Create sandbox
	//
	int openSandbox(const std::string &sandbox_name, u_int32_t numPorts);

	//
	// Get pointer to the sandbox
	//
	AftSandboxPtr getSandboxPtr(void)
	{
		return _sandbox;
	}

	//
	// Add a routing table to the sandbox
	//
	AftNodeToken addRouteTable(const std::string &rttName,
				   AftNodeToken defaultTragetToken);

	//
	// Set next node for an input port
	//
	int setInputPortNextNode(AftIndex inputPortIndex,
				 AftNodeToken nextToken);

	//
	// Get token for an output port
	//
	AftNodeToken getOuputPortToken(AftIndex outputPortIndex);

	//
	// Add ethernet encapsulation node
	//
	AftNodeToken addEtherEncapNode(const AftDataBytes &dst_mac,
				       const AftDataBytes &src_mac,
				       AftNodeToken nextToken);

	//
	// Handler hostpath packet from sandbox
	//
	int recvHostPathPacket(AftPacketPtr &pkt);

	//
	// Add or update neighbor
	//
	void addNeighbor(Neighbor &neighbor);

	//
	// Delete neighbor
	//
	void delNeighbor(Neighbor &neighbor);

	//
	// Print all neighbors
	//
	void printNeighbors(void);

	//
	// Install route to a routing table
	//
	int installRoute(const Route &route);

	//
	// Add route to a routing table
	//
	int addRoute(const Route &route);

	//
	// Uninstall route from the routing table
	//
	int uninstallRoute(Route &route);

	//
	// Remove route from the routing table
	//
	int delRoute(Route &route);

	//
	// Print all routes
	//
	void printRoutes(void);

      private:
	static AfiClient *instance;

	std::string _afiServerAddr;   //< AFI server address
	std::string _afiHostpathAddr; //< AFI hostpath address
	boost::asio::io_service &_ioService;
	BOOST_UDP::socket _hpUdpSock; //< Hospath UDP socket
	AftSandboxPtr _sandbox;
	AftTransportPtr _transport;

	AftNodeToken _puntToken;
	AftNodeToken _routeTableToken;

	std::vector<TapIf> _puntingPorts;
	std::map<Route, AftInsertPtr, RouteCompare> routes;
	std::set<Neighbor, NeighborCompare> neighbors;

	void hostPathUDPSrvr(void); //< Hostpath UDP server
};

#endif // __AfiClient__
