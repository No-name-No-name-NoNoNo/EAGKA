#include "AdhocUdpApplication.h"
#include <ostream>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cmath>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/stats-module.h"
#include "ns3/node.h"
#include "ns3/ptr.h"

using namespace ns3;


NS_LOG_COMPONENT_DEFINE("wifi-adhoc-app");


//------------------------------------------------------
//-- Sending application implementation
//------------------------------------------------------


TypeId AppSender::GetTypeId(void) {
	static TypeId tid =
			TypeId("AppSender").SetParent<Application>().AddConstructor<
					AppSender>().AddAttribute("PacketSize",	"The size of packets transmitted.", UintegerValue(96),
					MakeUintegerAccessor(&AppSender::m_pktSize),
					MakeUintegerChecker<uint32_t>(1)).AddAttribute("Destination", "Target host address.",
					Ipv4AddressValue("255.255.255.255"),
					MakeIpv4AddressAccessor(&AppSender::m_destAddr),
					MakeIpv4AddressChecker()).AddAttribute("Port","Destination app port.", UintegerValue(666),
					MakeUintegerAccessor(&AppSender::m_destPort),
					MakeUintegerChecker<uint32_t>()).AddAttribute("Interval",
					"Delay between transmissions.", UintegerValue(1),
					MakeUintegerAccessor(&AppSender::m_interval),
					MakeUintegerChecker<uint32_t>());
	return tid;
}

AppSender::AppSender() {
    m_sendCounter = 0;
    m_nodeId = 0;
    m_networkSize = 0;
    m_neighborList = new std::vector<Ipv4Address>();
}

AppSender::~AppSender() {}

void AppSender::SetNodeId(uint32_t id) {
    m_nodeId = id;
}

void AppSender::SetNetworkSize(uint32_t size) {
    m_networkSize = size;
    // Initialize KeyMatrix
    m_keyMatrix.InitializeMatrix(size, m_nodeId);
    // Initialize KeyGenerationTree
    m_keyTree = KeyGenerationTree(size, m_nodeId);
}

// Set send counter
void AppSender::SetSendCounter(Ptr<CounterCalculator<> > calc) {
	m_sendCounter = 0;
}   


void AppSender::DoDispose(void) {
	m_Socket = 0;
	Application::DoDispose();
}


// Start application
void AppSender::StartApplication() {
	
    // Create UDP socket and bind
	TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
	m_Socket = Socket::CreateSocket(GetNode(), tid);       // Socket for sending data
    m_Socket->Bind();

	// Set destination address and port
	InetSocketAddress dataRemote = InetSocketAddress(m_destAddr, m_destPort);	
	m_Socket->SetAllowBroadcast(true);
	m_Socket->Connect(dataRemote);

    // Initialize forwarding string, initialize as all 0s
    std::string forwardingContributions = std::string(m_networkSize, '0');
    // Set ID-th bit to 1
    forwardingContributions[m_nodeId] = '1';    
    // First send is broadcast, build packet with content: node ID + forwarding string + local KeyMatrix
    std::ostringstream packetContent;
    packetContent << m_nodeId << " " << forwardingContributions << " " << m_keyMatrix.MatrixToString();
    std::string content = packetContent.str();

    // Calculate computation delay
    double initDelay = (m_networkSize - 1) * 0.467 + (m_networkSize - 2) * 0.0635;
    // Send packet
    m_sendEvent = Simulator::Schedule(MilliSeconds(initDelay), &AppSender::SendPacket, this, m_destAddr, content);
    NS_LOG_INFO("Node " << m_nodeId << " starts sending first packet");

}

void AppSender::StopApplication() {
	Simulator::Cancel(m_sendEvent);
}


void AppSender::SendPacket(Ipv4Address neighborAddress, std::string packetContent) {
    Time currentTime = Simulator::Now();
    NS_LOG_INFO("Node " << m_nodeId << " current time: " << currentTime);
    Simulator::Schedule(MilliSeconds(1), &AppSender::DoSendPacket, this, neighborAddress, packetContent);
}

void AppSender::DoSendPacket(Ipv4Address neighborAddress, std::string packetContent) {
    // Perform key aggregation before sending packet
    std::string forwardingContributions = packetContent.substr(packetContent.find(" ") + 1, packetContent.find(" ", packetContent.find(" ") + 1) - packetContent.find(" ") - 1);
    m_keyTree.AddMultipleContributions(forwardingContributions);
    
    std::ostringstream msg;
    msg << packetContent;

    int numContributions = 0;
    if (forwardingContributions == std::string(m_networkSize, '1')) {
        numContributions = 1;
    } else {
        numContributions = std::count(forwardingContributions.begin(), forwardingContributions.end(), '1');
    }
    int additionalBytes = 8*(160 + 64 * (std::ceil(log2(numContributions)) + 1));
    std::string padding(additionalBytes, '0');
    msg << padding;

    std::string content = msg.str();
    Ptr<Packet> packet = Create<Packet>((uint8_t*) content.c_str(), content.size());
    NS_LOG_INFO("Node " << m_nodeId << " sends packet size: " << packet->GetSize());
    InetSocketAddress remote = InetSocketAddress(neighborAddress, m_destPort);
    m_Socket->Connect(remote);
    m_Socket->Send(packet);
    m_sendCounter++;
    Time sendTime = Simulator::Now();
    NS_LOG_INFO("Node " << m_nodeId << " send time: " << sendTime);
}



//------------------------------------------------------
//-- Receiving application implementation
//------------------------------------------------------
TypeId AppReceiver::GetTypeId(void) {
	static TypeId tid =
			TypeId("AppReceiver").SetParent<Application>().AddConstructor<
					AppReceiver>()
					.AddAttribute("Port", "Listening port.", UintegerValue(666),
							MakeUintegerAccessor(&AppReceiver::m_port),
							MakeUintegerChecker<uint32_t>())
					.AddAttribute("Destination", "Target host address.",
							Ipv4AddressValue("255.255.255.255"),
							MakeIpv4AddressAccessor(&AppReceiver::m_destAddr),
							MakeIpv4AddressChecker());
	return tid;
}

AppReceiver::AppReceiver() {
    m_packetBuffer = new std::map<std::string, int>;
    m_keyAgreementDelay = 0;
    m_receivedCounter = 0;   
    m_isCompleted = false;
    m_nodeId = 0;
    m_networkSize = 0;
    m_neighborList = new std::vector<Ipv4Address>();
}

AppReceiver::~AppReceiver() {
    delete m_packetBuffer;
}

// Set node count
void AppReceiver::SetNumNodes(uint32_t num) {
	m_numNodes = num;
}

// Set node ID
void AppReceiver::SetNodeId(uint32_t id) {
	m_nodeId = id;
}

// Set network size
void AppReceiver::SetNetworkSize(uint32_t size) {
    m_networkSize = size;
    // Initialize KeyMatrix
    m_keyMatrix.InitializeMatrix(size, m_nodeId);
    // Initialize KeyGenerationTree
    m_keyTree = KeyGenerationTree(size, m_nodeId);
}

// Set receive counter
void AppReceiver::SetReceiveCounter(Ptr<CounterCalculator<> > calc){
    m_receivedCounter = 0;
}


// Get receive counter
uint32_t AppReceiver::GetReceivedPackets() const {
    return m_receivedCounter;
}

// Get key agreement completion time
double AppReceiver::GetKeyAgreementDelay() const {
    return m_keyAgreementDelay;
}

// Check if completed
bool AppReceiver::IsCompleted() const {
    return m_isCompleted;
}


// Used to release resources
void AppReceiver::DoDispose(void) {
	m_socket = 0;
	Application::DoDispose();
}

    // Update neighbor list, keep only the latest N/2 neighbors, N is node count
void AppReceiver::UpdateNeighborList(Ipv4Address neighborAddress) {
    // If neighbor is in neighbor list, don't add
    if (std::find(m_neighborList->begin(), m_neighborList->end(), neighborAddress) != m_neighborList->end()) {
        // Move this neighbor's position in neighbor list to end of list
        m_neighborList->erase(std::find(m_neighborList->begin(), m_neighborList->end(), neighborAddress));
        m_neighborList->push_back(neighborAddress);
    } else {
        // Otherwise add neighbor, keep only latest N/2 neighbors
        m_neighborList->push_back(neighborAddress);
        if (m_neighborList->size() > m_networkSize / 2) {
            m_neighborList->erase(m_neighborList->begin());
        }
    }
}

// Start application
void AppReceiver::StartApplication() {
	TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
	m_socket = Socket::CreateSocket(GetNode(), tid);
    
    // Get node IP address
    Ipv4Address address = GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
    // Bind IP address and port
    InetSocketAddress local = InetSocketAddress(address, m_port);
	m_socket->Bind(local);
    NS_LOG_INFO("Node " << m_nodeId << " starts listening: " << address << ":" << m_port);
	    // Set callback (what for?)
    m_socket->SetRecvCallback(MakeCallback(&AppReceiver::Receive, this));
}

void AppReceiver::StopApplication() {
	if (m_socket != 0) {
		m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
	}
}

void AppReceiver::Receive(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;

    while ((packet = socket->RecvFrom(from))) {
        m_receivedCounter++;
        Ipv4Address senderAddr = InetSocketAddress::ConvertFrom(from).GetIpv4();
        // Add sender address to neighbor list
        UpdateNeighborList(senderAddr);
        // Node received message packet from XX neighbor
        NS_LOG_INFO("Node " << m_nodeId << " received message packet from " << senderAddr);
               
        // Extract data from packet
        uint8_t *buffer = new uint8_t[packet->GetSize()];
        packet->CopyData(buffer, packet->GetSize());
        std::string msg = std::string((char*)buffer, packet->GetSize());
        delete[] buffer;

        std::istringstream senderIdStream(msg.substr(0, msg.find(" ")));
        uint32_t senderId;
        senderIdStream >> senderId;

        std::string ReceivedKeyContributions = msg.substr(msg.find(" ") + 1, msg.find(" ", msg.find(" ") + 1) - msg.find(" ") - 1);
        NS_LOG_INFO("Node " << m_nodeId << " received key contributions: " << ReceivedKeyContributions);

        std::string ReceivedKeyMatrixString = msg.substr(msg.find(" ", msg.find(" ") + 1) + 1, m_networkSize * m_networkSize);
        NS_LOG_INFO("Node " << m_nodeId << " received key matrix: " << ReceivedKeyMatrixString);

        // Create received matrix
        KeyMatrix ReceivedMatrix = m_keyMatrix.StringToMatrix(ReceivedKeyMatrixString);
        
        // Use KeyGenerationTree to process received key contributions
        m_keyTree.AddMultipleContributions(ReceivedKeyContributions);
        
        // Iterate through received key contribution ID set, check if local has that key contribution
        for (uint32_t i = 0; i < ReceivedKeyContributions.size(); i++) {
            if (m_keyMatrix.HasKeyContribution(m_nodeId, i)) {
                continue;
            } else {
                m_keyMatrix.ReceiveKeyContribution(i);
                NS_LOG_INFO("Node " << m_nodeId << " does not have key contribution " << i << ", accepting this key contribution");
            }   
        }   


        // Merge received KeyMatrix to local KeyMatrix
        m_keyMatrix.MergeMatrix(ReceivedMatrix);	
        NS_LOG_INFO("Node " << m_nodeId << " merged KeyMatrix state: " << m_keyMatrix.MatrixToString());

        // Check if KeyGenerationTree already has complete group key
        if(m_keyTree.HasCompleteKey()) {
            NS_LOG_INFO("Node " << m_nodeId << " has collected all key contributions through KeyGenerationTree");
            m_isCompleted = true;
        }
        
        // If self has all key contributions, set m_isCompleted to true
        if(m_keyMatrix.SelfIsFull1()) {
                         NS_LOG_INFO("Node " << m_nodeId << " has collected all key contributions");
            m_isCompleted = true;
        }

        // Forwarding key contributions to neighbors
        for (uint32_t i = 0; i < m_neighborList->size(); i++) {     
            Ipv4Address neighborAddr = m_neighborList->at(i);
            uint8_t ipBytes[4];
            neighborAddr.Serialize(ipBytes);
            uint32_t neighborId = ipBytes[3]-1;

            std::string forwardingContributions = m_keyMatrix.GetForwardingContributions(neighborId);
            if (forwardingContributions != std::string(m_networkSize, '0')) {               
                std::ostringstream msg; 
                msg << m_nodeId << " " << forwardingContributions << " " << m_keyMatrix.MatrixToString();
                std::string content = msg.str();
                
                Ptr<AppSender> sender = DynamicCast<AppSender>(GetNode()->GetApplication(0));
                sender->SendPacket(neighborAddr, content);
                NS_LOG_INFO("Node " << m_nodeId << " sent packet to " << neighborAddr);
            }
        }
    }
}













