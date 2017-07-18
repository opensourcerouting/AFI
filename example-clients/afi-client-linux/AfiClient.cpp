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

#include "AfiClient.h"

AfiClient *AfiClient::instance = NULL;

int AfiClient::setup(void)
{
	openSandbox("green", 8);
	getSandboxPtr()->outputPortByName("punt", _puntToken);

	_routeTableToken = addRouteTable("routeTable", _puntToken);

	// XXX unfortunately this is necessary due to a bug in JFI
	struct in_addr defaultRouteV4 = {0};
	struct in6_addr defaultRouteV6 = {0};
	Route route;
	route.prefix.set(defaultRouteV4);
	addRoute(route);
	route.prefix.set(defaultRouteV6);
	addRoute(route);
}

void AfiClient::setupInterface(std::string &interface, unsigned int port,
			       std::string &tap)
{
	setInputPortNextNode(port, _routeTableToken);
	_puntingPorts[port].init(port, tap, interface);
}

//
// @fn
// openSandbox
//
// @brief
// Open AFI sandbox created via Junos CLI configuration
//
// @param[in]
//     sandbox_name Sandbox name
// @param[in]
//     numCfgPorts Number of configured ports
// @return 0 - Success, -1 - Failure
//

int AfiClient::openSandbox(const std::string &sandbox_name,
			   u_int32_t numCfgPorts)
{
	u_int32_t numStdPorts = 1;
	bool status = _transport->alloc("trio", sandbox_name,
					(numCfgPorts + numStdPorts),
					(numCfgPorts + numStdPorts));
	if (status == 0) {
		std::cout << "_transport->alloc failed!" << std::endl;
		return -1;
	}

	if (_transport->open(sandbox_name, _sandbox)) {
		std::cout << "Got Sandbox handle!" << std::endl;
	} else {
		std::cout << "Open Sandbox failed!" << std::endl;
		return -1;
	}

	return 0;
}

//
// @fn
// addRouteTable
//
// @brief
// Add routing table
//
// @param[in]
//     rttName Routing table name
// @param[in]
//     defaultTragetToken  Default route target token
// @return routing table node token
//

AftNodeToken AfiClient::addRouteTable(const std::string &rttName,
				      AftNodeToken defaultTragetToken)
{
	AftNodeToken rttNodeToken;
	AftInsertPtr insert;

	//
	// Allocate an insert context
	//
	insert = AftInsert::create(_sandbox);

	//
	// Create a route lookup tree
	//
	auto treePtr = AftTree::create(AftField("packet.lookupkey"),
				       defaultTragetToken);
	//
	// Stich the optional params
	//
	uint64_t skipBits = 16;
	treePtr->setNodeParameter("rt.app", AftDataString::create("NH"));
	treePtr->setNodeParameter("rt.nhType", AftDataString::create("route"));
	treePtr->setNodeParameter("rt.skipBits", AftDataInt::create(skipBits));

	rttNodeToken = insert->push(treePtr, rttName);

	//
	// Send all the nodes to the sandbox
	//
	_sandbox->send(insert);

	return rttNodeToken;
}

//
// @fn
// setInputPortNextNode
//
// @brief
// Set next node for an input port
//
// @param[in]
//     inputPortIndex Input port index
// @param[in]
//     nextToken Next node token
// @return 0 - Success, -1 - Error
//

int AfiClient::setInputPortNextNode(AftIndex inputPortIndex,
				    AftNodeToken nextToken)
{
	_sandbox->setInputPortByIndex(inputPortIndex, nextToken);
	return 0;
}

//
// @fn
// getOuputPortToken
//
// @brief
// Get token for an output port
//
// @param[in]
//     outputPortIndex Output port index
// @return Outport port's token
//

AftNodeToken AfiClient::getOuputPortToken(AftIndex outputPortIndex)
{
	AftNodeToken outputPortToken;

	_sandbox->outputPortByIndex(outputPortIndex, outputPortToken);

	return outputPortToken;
}

//
// @fn
// addEtherEncapNode
//
// @brief
// Add Ethernet encap node
//
// @param[in]
//     dst_mac Destination MAC
// @param[in]
//     src_mac Source MAC
// @param[in]
//     nextToken Next node token
// @return Ethernet encap node's token
//

AftNodeToken AfiClient::addEtherEncapNode(const AftDataBytes &dst_mac,
					  const AftDataBytes &src_mac,
					  AftNodeToken nextToken)
{
	AftNodeToken outListToken;
	AftInsertPtr insert;

	//
	// Allocate an insert context
	//
	insert = AftInsert::create(_sandbox);

	//
	// Create a key vector of ethernet data
	//
	AftKeyVector encapKeys = {AftKey(AftField("packet.ether.saddr"),
					 AftDataEtherAddr::create(src_mac)),
				  AftKey(AftField("packet.ether.daddr"),
					 AftDataEtherAddr::create(dst_mac))};
	//
	// Create aft encap node
	//
	auto aftEncapPtr = AftEncap::create("ethernet", encapKeys);

	//
	// Stich the optional params
	//
	static u_int64_t nhId = 100;
	nhId++;
	aftEncapPtr->setNodeParameter("meta.nhid", AftDataInt::create(nhId));

	//
	// Set the List node token as next pointer to encap
	//
	aftEncapPtr->setNodeNext(nextToken);

	AftNodeToken nhEncapToken = insert->push(aftEncapPtr);

	//
	// Send all the nodes to the sandbox
	//
	_sandbox->send(insert);

	return nhEncapToken;
}

//
// @fn
// recvHostPathPacket
//
// @brief
// Receive hostpath packet
//
// @param[in]
//     pkt Aft packet where received packet will be copied to
// @return 0 - Success, -1 - Error
//

int AfiClient::recvHostPathPacket(AftPacketPtr &pkt)
{
	size_t recvlen;
	enum { max_length = 2000 };
	char _data[max_length];
	BOOST_UDP::endpoint sender_endpoint;

	// Block until data has been received successfully or an error occurs.
	recvlen = _hpUdpSock.receive_from(
		boost::asio::buffer(_data, max_length), sender_endpoint);

	memcpy(pkt->header(), _data, pkt->headerSize());
	memcpy(pkt->data(), _data + pkt->headerSize(),
	       (recvlen - pkt->headerSize()));

	pkt->headerParse();

	TapIf &tapIf = _puntingPorts[pkt->portIndex()];
	if (tapIf.isInitialized())
		tapIf.ifWrite(pkt->dataSize(), pkt->data());

	return 0;
}

//
// @fn
// addNeighbor
//
// @brief
// Add or update neighbor
//
// @param[in]
//     neighbor Neighbor
// @return void
//

void AfiClient::addNeighbor(Neighbor &neighbor)
{
	auto old = neighbors.find(neighbor);
	if (old != neighbors.end()) {
		// Ignore message if neighbor exists and didn't change
		if (*old == neighbor)
			return;

		std::cout << "[netlink-neighbor] update: " << neighbor << "\n";
		neighbors.erase(neighbor);
	} else
		std::cout << "[netlink-neighbor] add: " << neighbor << "\n";

	neighbors.insert(neighbor);

	for (auto it = routes.begin(); it != routes.end(); ++it) {
		Route route = it->first;
		if (route.nhAddr == neighbor.ipAddr)
			installRoute(route);
	}
}

//
// @fn
// delNeighbor
//
// @brief
// Delete neighbor
//
// @param[in]
//     neighbor Neighbor
// @return void
//

void AfiClient::delNeighbor(Neighbor &neighbor)
{
	std::cout << "[netlink-neighbor] del: " << neighbor << "\n";
	neighbors.erase(neighbor);

	for (auto it = routes.begin(); it != routes.end(); ++it) {
		Route route = it->first;
		if (route.nhAddr == neighbor.ipAddr)
			uninstallRoute(route);
	}
}

//
// @fn
// printNeighbors
//
// @brief
// Display existing neighbors
//
// @param[in] void
// @return void
//

void AfiClient::printNeighbors(void)
{
	for (std::set<Neighbor>::iterator it = neighbors.begin();
	     it != neighbors.end(); ++it) {
		Neighbor nbr = *it;
		std::cout << "neighbor: " << nbr << std::endl;
	}
}

//
// @fn
// installRoute
//
// @brief
// Install route to the routing table
//
// @param[in]
//     route Route
// @return 0 - Success, -1 - Error
//

int AfiClient::installRoute(const Route &route)
{
	AftNodeToken routeTargetToken;

	auto it = find_if(_puntingPorts.begin(), _puntingPorts.end(),
			  [&](const TapIf &t) -> bool {
				  return t.ifIndex() == route.nhDev;
			  });

	//
	// Routes that don't have a nexthop address can't be installed in the
	// fast-path because ARP is required to resolve MAC addresses on a
	// per-packet basis.
	//
	if (!route.nhAddr.isSet())
		routeTargetToken = _puntToken;
	else {
		AftNodeToken outputPortToken;
		AftNodeToken nhEncapToken;
		AftIndex outputPortIndex = it->portIndex();
		getSandboxPtr()->outputPortByIndex(outputPortIndex,
						   outputPortToken);

		Neighbor n;
		n.ipAddr = route.nhAddr;
		auto neighbor = neighbors.find(n);
		if (neighbor == neighbors.end())
			return -1;

		AftDataBytes src_mac(it->macAddr(), it->macAddr() + ETH_ALEN);
		AftDataBytes dst_mac(neighbor->macAddr,
				     neighbor->macAddr + ETH_ALEN);
		nhEncapToken =
			addEtherEncapNode(dst_mac, src_mac, outputPortToken);
		routeTargetToken = nhEncapToken;
	}

	int prefix_len;
	std::string key_name;
	switch (route.prefix.af) {
	case AF_INET:
		prefix_len = 4;
		key_name = "packet.ip4.daddr";
		break;
	case AF_INET6:
		prefix_len = 16;
		key_name = "packet.ipv6.daddr";
		break;
	default:
		warnx("%s: unknown address-family", __func__);
		return -1;
	}
	AftDataBytes prefix_bytes(route.prefix.u.bytes,
				  route.prefix.u.bytes + prefix_len);
	AftDataPtr data_prefix =
		AftDataPrefix::create(prefix_bytes, route.prefixLen);
	AftKey key = AftKey(AftField(key_name), data_prefix);

	//
	// Allocate an insert context
	//
	AftInsertPtr insert = AftInsert::create(_sandbox);

	//
	// Create a route
	//
	AftEntryPtr entryPtr =
		AftEntry::create(_routeTableToken, key, routeTargetToken);

	//
	// Set the optional params for Entry
	//
	uint8_t hwFlush = 0;
	entryPtr->setEntryParameter("route.string",
				    AftDataString::create("IPv4 route"));
	entryPtr->setEntryParameter("route.hwFlush",
				    AftDataInt::create(hwFlush));
	insert->push(entryPtr);

	//
	// Send all the nodes to the sandbox
	//
	_sandbox->send(insert);
	routes.find(route)->second = insert;

	std::cout << "[afi-route] install: " << route << std::endl;

	return 0;
}

//
// @fn
// addRoute
//
// @brief
// Add or update route
//
// @param[in]
//     route Route
// @return 0 - Success, -1 - Error
//

int AfiClient::addRoute(const Route &route)
{
	auto it = find_if(_puntingPorts.begin(), _puntingPorts.end(),
			  [&](const TapIf &t) -> bool {
				  return t.ifIndex() == route.nhDev;
			  });
	// nexthop interface not found
	if (it == std::end(_puntingPorts))
		return -1;

	auto old = routes.find(route);
	if (old != routes.end()) {
		// Ignore message if route exists and didn't change
		if (old->first == route)
			return 0;

		std::cout << "[netlink-route] update: " << route << "\n";
	} else
		std::cout << "[netlink-route] add: " << route << "\n";

	AftInsertPtr node = NULL;
	routes[route] = node;

	return installRoute(route);
}

//
// @fn
// uninstallRoute
//
// @brief
// Delete route from the routing table
//
// @param[in]
//     route Route
// @return 0 - Success, -1 - Error
//

int AfiClient::uninstallRoute(Route &route)
{
	std::cout << "[afi-route] uninstall: " << route << std::endl;

	AftInsertPtr node = routes.find(route)->second;
	AftRemovePtr remove = AftRemove::create(node);
	_sandbox->send(remove);

	return 0;
}

//
// @fn
// delRoute
//
// @brief
// Delete route from the routing table
//
// @param[in]
//     route Route
// @return 0 - Success, -1 - Error
//

int AfiClient::delRoute(Route &route)
{
	std::cout << "[netlink-route] del: " << route << std::endl;

	uninstallRoute(route);

	routes.erase(route);

	return 0;
}

//
// @fn
// printRoutes
//
// @brief
// Display existing routes
//
// @param[in] void
// @return void
//

void AfiClient::printRoutes(void)
{
	for (std::map<Route, AftInsertPtr>::iterator it = routes.begin();
	     it != routes.end(); ++it) {
		Route route = it->first;
		std::cout << "route: " << route << std::endl;
	}
}

//
// @fn
// hostPathUDPSrvr
//
// @brief
// Hostpath UDP server
//
// @param[in] void
// @return void
//

void AfiClient::hostPathUDPSrvr(void)
{
	for (;;) {
		//
		// Allocate packet context *and* buffer
		//
		AftPacketPtr pkt = AftPacket::createReceive();

		recvHostPathPacket(pkt);
	}
}
