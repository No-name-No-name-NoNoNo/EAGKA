#ifndef ADHOC_UDP_APPLICATION_H_
#define ADHOC_UDP_APPLICATION_H_

#include "KeyMatrix.h"
#include "KeyGenerationTree.h"
#include "ns3/core-module.h"
#include "ns3/application.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/stats-module.h"
#include <map>
#include <string>
#include <fstream>
#include <vector>
#include <set>

using namespace ns3;

/**
 * Sending application
 */
class AppSender: public Application {
public:
	static TypeId GetTypeId(void);
	AppSender();
	virtual ~AppSender();
	void SetSendCounter(Ptr<CounterCalculator<> > sendCounter); // Set send counter
	void SetNodeId(uint32_t id); // Set node ID
	void SetNetworkSize(uint32_t size); // Set network size
	void AddNeighbor(Ipv4Address neighbor); // Add neighbor
	void UpdateNeighborList(Ipv4Address neighborAddress); // Update neighbor list
	void SendPacket(Ipv4Address neighborAddress, std::string packetContent); 
	void DoSendPacket(Ipv4Address neighborAddress, std::string packetContent); // Send packet to specified neighbor
	
	// Get sent packet count
	uint32_t GetSentPackets() const { return m_sendCounter; }
	// Get neighbor list
	std::vector<Ipv4Address>* GetNeighborList() { return m_neighborList; }
	
	// Get key generation tree status
	const KeyGenerationTree& GetKeyTree() const { return m_keyTree; }
	bool HasCompleteKey() const { return m_keyTree.HasCompleteKey(); }

protected:
	virtual void DoDispose(void);

private:
	virtual void StartApplication(void);
	virtual void StopApplication(void);

	uint32_t m_pktSize;		// Packet size
	Ipv4Address m_destAddr;	// Destination address
	uint16_t m_destPort;		// Destination port
	uint32_t m_interval;       // Send interval
	Ptr<Socket> m_Socket; 	// Socket for sending data
	EventId m_sendEvent;			// Send event
	uint32_t m_sendCounter;		// Send counter
	uint32_t m_nodeId;			// Node ID
	std::vector<Ipv4Address>* m_neighborList;	// Neighbor list
	uint32_t m_networkSize;		// Network size
	KeyMatrix m_keyMatrix;		// KeyMatrix
	KeyGenerationTree m_keyTree;	// Key generation tree
};

// -------------------------------------------------------------------

/**
 * Receiving application
 */
class AppReceiver: public Application {
public:
	static TypeId GetTypeId(void);
	AppReceiver();
	virtual ~AppReceiver();
	void SetReceiveCounter(Ptr<CounterCalculator<> > calc); 
	void SetNumNodes(uint32_t num);
	void SetNodeId(uint32_t id);
	void SetNetworkSize(uint32_t size);
	uint32_t GetReceivedPackets() const; 
	bool IsCompleted() const; 
	double GetKeyAgreementDelay() const; 
	const KeyMatrix& GetKeyMatrix() const { return m_keyMatrix; }
	const KeyGenerationTree& GetKeyTree() const { return m_keyTree; }
	bool HasCompleteKey() const { return m_keyTree.HasCompleteKey(); }

	void UpdateNeighborList(Ipv4Address neighborAddress);

protected:
	virtual void DoDispose(void);

private:
	virtual void StartApplication(void);
	virtual void StopApplication(void);

	void Receive(Ptr<Socket> socket);

	Ptr<Socket> m_socket; 
	Ipv4Address m_destAddr;

	uint16_t m_port; 
	// Receive counter
	Ptr<CounterCalculator<> > receivedCounter;
	// Total network node count
	uint32_t m_numNodes;
	// UAV ID
	uint32_t m_nodeId;
	// Whether received all nodes' packets
	bool m_isCompleted;
	// Network size
	uint32_t m_networkSize;
	// KeyMatrix
	KeyMatrix m_keyMatrix;
	// Key generation tree
	KeyGenerationTree m_keyTree;
	// Receive counter
	uint32_t m_receivedCounter;
	// Neighbor list
	std::vector<Ipv4Address>* m_neighborList;
	// Packet buffer
	std::map<std::string, int>* m_packetBuffer;
	// Key agreement completion time
	double m_keyAgreementDelay;
};


#endif /* ADHOC_UDP_APPLICATION_H_ */
