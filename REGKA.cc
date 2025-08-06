
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/propagation-module.h"
#include "ns3/trace-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/stats-module.h"
#include <string>
#include <vector>
#include <sys/stat.h>
#include <fstream>  
#include <iomanip> 
#include <sys/types.h>
#include <unistd.h>
#include <sstream>

#include "AdhocUdpApplication.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("wifi-adhoc-UAV-experiment");

// Data rate
std::string phyMode("ErpOfdmRate12Mbps");  
std::string propagationLossModel("ns3::FriisPropagationLossModel");
std::string propagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
// Node movement speed and mobility model
std::string mobilitySpeed("ns3::UniformRandomVariable[Min=10|Max=50]");
std::string mobilityModel("ns3::GaussMarkovMobilityModel");
// Activity area
double areaLength = 500;   
double areaWidth = 500;    
double areaHeight = 100;   
// Number of nodes
uint32_t numNodes = 5;
// Simulation time
uint32_t simuTime = 60;
// Completion time
double CompletionTime = 0;


// ---------- Experiment data record labels ----------
// Experiment content
std::string experiment;
// Experiment parameters
std::string strategy;
// Experiment node count
std::string input;
// Experiment ID
std::string runId;
// ------------- End -----------------

// Check if all nodes have completed key collection
bool CheckAllNodesCompleted(const NodeContainer& nodes) {
	for (uint32_t i = 0; i < nodes.GetN(); i++) {
		Ptr<AppReceiver> receiver = DynamicCast<AppReceiver>(nodes.Get(i)->GetApplication(1));
		if (!receiver->IsCompleted()) {
			return false;
		}
	}
	return true;
}

void CheckCompletionAndStop(const NodeContainer& nodes) {
	if (CheckAllNodesCompleted(nodes)) {
		NS_LOG_INFO("All nodes have collected key contributions, ending simulation");
		CompletionTime = Simulator::Now().GetSeconds()-1;	
		Simulator::Stop();
	} else {
		double currentTime = Simulator::Now().GetSeconds();
		if (currentTime < 2.0) {
			Simulator::Schedule(Seconds(0.01), &CheckCompletionAndStop, nodes);
		} else {
			Simulator::Schedule(Seconds(0.1), &CheckCompletionAndStop, nodes);
		}
	}
}



void SetupLinkQuality (YansWifiPhyHelper &wifiPhy, const std::string &quality)
{
  /* Hardware constants (unchanged) */
  wifiPhy.Set ("RxNoiseFigure", DoubleValue (5.5));     // NF ≈ 5.5 dB
  wifiPhy.Set ("RxGain",        DoubleValue (3.0));     // 3 dBi omnidirectional antenna

  wifiPhy.Set ("TxPowerStart",  DoubleValue (21.0));    // 21 dBm EIRP upper‑layer budget
  wifiPhy.Set ("TxPowerEnd",    DoubleValue (21.0));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-84.0));

  /* Channel helper */
  YansWifiChannelHelper ch;
  ch.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

  /* Large‑scale + small‑scale fading parameters */
  double exponent, shadowVar, m0, m1, m2, d1, d2;

  if (quality == "LOS")            // wide‑open rural / over‑sea LOS
    {
      exponent  = 2.0;                 // from 3GPP RMa LoS
      shadowVar = 4.0;                 // σ = 2 dB  (variance = σ²)
      m0 = 6.0;  m1 = 4.0;  m2 = 3.0;  // strong Rice component (K≈8 dB)
      d1 = 400.0; d2 = 1000.0;         // 0.2·d_max / 0.5·d_max, d_max≈2 km
    }


  /* Attach the three loss models in sequence */
  ch.AddPropagationLoss ("ns3::LogDistancePropagationLossModel",
                         "Exponent",          DoubleValue (exponent),
                         "ReferenceDistance", DoubleValue (1.0),
                         "ReferenceLoss",     DoubleValue (40.05)); // FSPL @1 m (2.4 GHz)

  ch.AddPropagationLoss ("ns3::RandomPropagationLossModel",
                         "Variable", StringValue ("ns3::NormalRandomVariable[Mean=0|Variance="
                                                   + std::to_string (shadowVar) + "]"));

  ch.AddPropagationLoss ("ns3::NakagamiPropagationLossModel",
                         "m0", DoubleValue (m0),
                         "m1", DoubleValue (m1),
                         "m2", DoubleValue (m2),
                         "Distance1", DoubleValue (d1),
                         "Distance2", DoubleValue (d2));

  wifiPhy.SetChannel (ch.Create ());
}





void startSimulation(const std::string& linkQuality) {
    
	NodeContainer nodes;
	nodes.Create(numNodes);

	// --------------------------------------------------------------
	// ---------- Setup physical layer and data link layer ----------
	// --------------------------------------------------------------
	WifiHelper wifi;
	wifi.SetStandard(WIFI_PHY_STANDARD_80211g);  // Use 802.11g standard
	YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
	SetupLinkQuality(wifiPhy, linkQuality);

	NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
	wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");		
	wifiMac.SetType("ns3::AdhocWifiMac");

	NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);
	// -------------- End ----------------


	// ------------------------------------------------------------
	// ---------- Setup network layer and transport layer protocols -------------
	// ------------------------------------------------------------
	InternetStackHelper internet;
	internet.Install(nodes);
	Ipv4AddressHelper ipv4;
	ipv4.SetBase("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer ipv4Container = ipv4.Assign(devices);
	// ------------- End -----------------


	// ------------------------------------------------------------
	// ---------- Setup application layer -------------
	// ------------------------------------------------------------

	Ptr<CounterCalculator<> > totalSentPackets = CreateObject<CounterCalculator<> >();
	Ptr<CounterCalculator<> > totalRecvPackets = CreateObject<CounterCalculator<> >();
	totalSentPackets->SetKey("Sender");
	totalSentPackets->SetContext("Total sent packets");
	totalRecvPackets->SetKey("Receiver");
	totalRecvPackets->SetContext("Total received packets");

			// Setup application layer information
	for (uint32_t i = 0; i < numNodes; i++) {
		Ptr<Node> nodeToInstallApp = nodes.Get(i);
		Ptr<AppSender> sender = CreateObject<AppSender>();
		Ptr<AppReceiver> receiver = CreateObject<AppReceiver>();

		sender->SetSendCounter(totalSentPackets);
		receiver->SetReceiveCounter(totalRecvPackets);
		receiver->SetNumNodes(numNodes);
		receiver->SetNodeId(i);
        sender->SetNodeId(i);
        
        // Initialize KeyMatrix
        receiver->SetNetworkSize(numNodes);
        sender->SetNetworkSize(numNodes);

		nodeToInstallApp->AddApplication(sender);
		nodeToInstallApp->AddApplication(receiver);

		receiver->SetStartTime(Seconds(0));
		sender->SetStartTime(Seconds(1 + 0.00001 * i));

		receiver->SetStopTime(Seconds(simuTime));
		sender->SetStopTime(Seconds(simuTime));
	}
	// ------------- End -----------------


	// ------------------------------------------------------------
	// ---------- Setup mobility model -------------
	// ------------------------------------------------------------
	MobilityHelper mobility;

	// RandomBoxPositionAllocator implements random position selection within a cube
	Ptr<RandomBoxPositionAllocator> randPosLocator = CreateObject<RandomBoxPositionAllocator>();
	// Randomly set node positions
	std::ostringstream playGround;
	playGround << "ns3::UniformRandomVariable[Min=0.0|" << "Max=" << areaLength << "]";
	randPosLocator->SetAttribute("X", StringValue(playGround.str()));
	playGround.str("");
	playGround << "ns3::UniformRandomVariable[Min=0.0|" << "Max=" << areaWidth << "]";
	randPosLocator->SetAttribute("Y", StringValue(playGround.str()));
	playGround.str("");
	playGround << "ns3::UniformRandomVariable[Min=0.0|" << "Max=" << areaHeight << "]";
	randPosLocator->SetAttribute("Z", StringValue(playGround.str()));
	mobility.SetPositionAllocator(randPosLocator);

	// Set mobility model
	mobility.SetMobilityModel(mobilityModel,									
			"Bounds", BoxValue(												
					Box(0, areaLength, 0, areaWidth, 0, areaHeight)),
			"Alpha", DoubleValue (0.85),										
			"TimeStep", TimeValue(Seconds(1)),								
			"MeanVelocity", StringValue(mobilitySpeed),						
			"MeanDirection", StringValue(										
					"ns3::UniformRandomVariable[Min=0|Max=6.283185307]"),
			"MeanPitch", StringValue(											
					"ns3::UniformRandomVariable[Min=0.05|Max=0.05]"),
			"NormalVelocity", StringValue(										
					"ns3::NormalRandomVariable[Mean=0.0|Variance=0.0|Bound=0.0]"),
			"NormalDirection", StringValue(									
					"ns3::NormalRandomVariable[Mean=0.0|Variance=0.2|Bound=0.4]"),
			"NormalPitch", StringValue(										
					"ns3::NormalRandomVariable[Mean=0.0|Variance=0.02|Bound=0.04]")
			);

	for (uint32_t i = 0; i < numNodes; ++i) {
		mobility.Install(nodes.Get(i));
	}
	// -------------- End ----------------


	// ------------------------------------------------------------
	// ---------- Experiment data collection -------------
	// ------------------------------------------------------------
	DataCollector dataCollector;
	dataCollector.DescribeRun(experiment, strategy, input, runId);
	dataCollector.AddDataCalculator(totalSentPackets);
	dataCollector.AddDataCalculator(totalRecvPackets);
	// -------------- End ----------------


	// ------------------------------------------------------------
	// ---------- Start simulation -------------
	// ------------------------------------------------------------
    
	// Set simulation end time
	Simulator::Stop(Seconds(simuTime));
	// Start periodic check
	Simulator::Schedule(Seconds(0.001), &CheckCompletionAndStop, nodes);
	// Start simulation
	Simulator::Run();

	// Count packet statistics
	uint32_t totalSent = 0;
	uint32_t totalReceived = 0;
	uint32_t successfulNodes = 0; 

	// Used to record sent and received packet count for each node
	std::vector<uint32_t> sentPackets;
	std::vector<uint32_t> receivedPackets;
	std::vector<uint32_t> uniqueContributions; 
	sentPackets.reserve(numNodes);
	receivedPackets.reserve(numNodes);
	uniqueContributions.reserve(numNodes);

	for (uint32_t i = 0; i < numNodes; i++) {
		Ptr<AppReceiver> receiver = DynamicCast<AppReceiver>(nodes.Get(i)->GetApplication(1));
		Ptr<AppSender> sender = DynamicCast<AppSender>(nodes.Get(i)->GetApplication(0));
		// Get receiver and sender packet counts
		uint32_t packetsReceived = receiver->GetReceivedPackets();
		NS_LOG_INFO("Node " << i << " received packet count: " << packetsReceived);
		uint32_t packetsSent = sender->GetSentPackets();
		NS_LOG_INFO("Node " << i << " sent packet count: " << packetsSent);
		
		sentPackets.push_back(packetsSent);
		receivedPackets.push_back(packetsReceived);
		const KeyMatrix& keyMatrix = receiver->GetKeyMatrix();
		
		// Count different key contributions received by node i
		uint32_t contributionCount = 0;
		for (uint32_t j = 0; j < numNodes; j++) {
			if (keyMatrix.HasKeyContribution(i, j)) {
				contributionCount++;
			}	
		}
		uniqueContributions.push_back(contributionCount);
		NS_LOG_INFO("Node " << i << " different key contribution count: " << contributionCount);
		
		// Check if node i has received all key contributions
		bool allContributionsReceived = true;
		for (uint32_t j = 0; j < numNodes; j++) {
			if (!keyMatrix.HasKeyContribution(i, j)) {
				allContributionsReceived = false;
				break;
			}	
		}
		if (allContributionsReceived) {
			successfulNodes++;
			NS_LOG_INFO("Node " << i << " successfully collected all key contributions");
			
		}
		// Count total sent and received packet numbers	
		totalSent += packetsSent;
		totalReceived += packetsReceived;
		
	}
	
	// Output packet information received by each node
	NS_LOG_INFO("------------ Node Packet Statistics ------------");
	NS_LOG_INFO("+--------+-------------+-------------+------------------+----------+");
	NS_LOG_INFO("| Node ID | Sent Packets | Recv Packets | Key Contributions | Completed |");
	NS_LOG_INFO("+--------+-------------+-------------+------------------+----------+");

	for (uint32_t i = 0; i < numNodes; i++) {
		Ptr<AppReceiver> receiver = DynamicCast<AppReceiver>(nodes.Get(i)->GetApplication(1));
		Ptr<AppSender> sender = DynamicCast<AppSender>(nodes.Get(i)->GetApplication(0));
		NS_LOG_INFO("| " << std::setw(6) << i << " | " << std::setw(11) << sender->GetSentPackets() << " | " 
			<< std::setw(11) << receiver->GetReceivedPackets() << " | " 
			<< std::setw(16) << uniqueContributions[i] << " | " 
			<< (receiver->IsCompleted() ? "Yes" : "No") << " |");
	}
	NS_LOG_INFO("+--------+-------------+-------------+------------------+----------+");

	// Output summary information
	NS_LOG_INFO("Summary Statistics:");
	NS_LOG_INFO("  Total sent packets: " << totalSent);
	NS_LOG_INFO("  Total received packets: " << totalReceived);
	// NS_LOG_INFO("  Duplicate received packets: " << totalDuplicates);
	
	// Calculate more statistical metrics
	double avgSent = (double)totalSent / numNodes;
	double avgReceived = (double)totalReceived / numNodes;
	double overheadRatio = (totalSent > 0) ? (double)totalReceived / totalSent : 0;
	
	// Calculate average different key contributions received per node
	uint32_t totalUniqueContributions = 0;
	for (uint32_t i = 0; i < numNodes; i++) {
		totalUniqueContributions += uniqueContributions[i];
	}
	double avgUniqueContributions = (double)totalUniqueContributions / numNodes;
	
	NS_LOG_INFO("Analysis Metrics:");
	NS_LOG_INFO("  Average packets sent per node: " << std::fixed << std::setprecision(2) << avgSent);
	NS_LOG_INFO("  Average packets received per node: " << std::fixed << std::setprecision(2) << avgReceived);
	NS_LOG_INFO("  Average different key contributions per node: " << std::fixed << std::setprecision(2) << avgUniqueContributions);
	NS_LOG_INFO("  Communication overhead ratio (recv/sent): " << std::fixed << std::setprecision(2) << overheadRatio);
	
	// Calculate success rate
	double successRate = (double)successfulNodes / numNodes * 100;
	NS_LOG_INFO("  Nodes successfully received all packets: " << successfulNodes << "/" << numNodes 
				<< " (" << successRate << "%)");
	NS_LOG_INFO("----------------------------------------");
    
	// Key agreement completion delay
	double keyAgreementDelay = CompletionTime;
	NS_LOG_INFO("Key agreement completion delay: " << keyAgreementDelay << " seconds");
    
	// ------------------------------------------------------------
	// ---------- Output results to local file -------------
	// ------------------------------------------------------------
	Ptr<DataOutputInterface> output = 0;
	output = CreateObject<SqliteDataOutput>();
	if (output != 0)
		output->Output(dataCollector);
	NS_LOG_INFO("Data successfully written");
	// ------------- End -----------------

	const char* cacheDir = "./results_cache";
	struct stat st;
	if (stat(cacheDir, &st) != 0) {
		mkdir(cacheDir, 0777);
	}
	std::ostringstream cacheFileName;
	cacheFileName << cacheDir << "/" << areaLength << "*" << areaWidth << "*" << areaHeight << "_" << numNodes << "_" << linkQuality << "_" << runId << ".csv";
	std::ofstream out(cacheFileName.str(), std::ios::app);

	// Generate timestamp
	std::time_t now = std::time(nullptr);
	char buf[32];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

	// Write one CSV line
	out << buf << "," << areaLength << "," << areaWidth << "," << areaHeight << "," << numNodes << "," << linkQuality << "," << runId << "," << keyAgreementDelay << "," << totalSent << "," << totalReceived << "," << overheadRatio << "," << successRate << "," << avgUniqueContributions << std::endl;
	out.close();

	// NS_LOG_INFO("-----------------Simulation ended-------------------");
	Simulator::Destroy();
}

int main(int argc, char *argv[]) {
	std::string linkQuality = "LOS";  
	LogComponentEnable("wifi-adhoc-UAV-experiment", LOG_LEVEL_INFO);
	LogComponentEnable("wifi-adhoc-app", LOG_LEVEL_INFO);
	
	const char* logDir = "./Log";
	struct stat st;
	if (stat(logDir, &st) != 0) {
		mkdir(logDir, 0777);
					NS_LOG_INFO("Created log directory: " << logDir);
	}
	
	std::ostringstream logFileNameStream;
	logFileNameStream << logDir << "/simulation_nodes" << numNodes 
		<< "_area" << (int)areaLength << "*" << (int)areaWidth << "*" << (int)areaHeight
		<< "_linkQuality" << linkQuality
		<< "_run" << runId << ".txt";
	std::string logFileName = logFileNameStream.str();
	
			// Redirect log output to file
	std::ofstream logFile(logFileName.c_str());
	if (logFile.is_open()) {
		chmod(logFileName.c_str(), 0666);
		
		std::streambuf* originalBuffer = std::clog.rdbuf();
		std::clog.rdbuf(logFile.rdbuf());
		std::clog.setf(std::ios::unitbuf);
		
		NS_LOG_INFO("=====================================");
		NS_LOG_INFO("Experiment started: numNodes=" << numNodes << ", areaLength=" << areaLength << ", areaWidth=" << areaWidth << ", areaHeight=" << areaHeight << ", runId=" << runId);
		NS_LOG_INFO("=====================================");
		
		input = numNodes;
		std::ostringstream ss;
		ss << "numNodes:" << numNodes << ";areaLength:" << areaLength << ";areaWidth:" << areaWidth << ";areaHeight:" << areaHeight;
		experiment = ss.str();
		ss.str("");
		strategy = "Single Round Communication";
		
		// Run simulation
		startSimulation(linkQuality);

		std::clog.flush();
		NS_LOG_INFO("=====================================");
		NS_LOG_INFO("Experiment ended");
		NS_LOG_INFO("=====================================");
		
		// Restore original output
		std::clog.rdbuf(originalBuffer);
		logFile.close();
		
		std::cout << "Log saved to: " << logFileName << std::endl;
	} else {
		NS_LOG_ERROR("Unable to create log file: " << logFileName);
	}
    std::cout << areaLength << "*" << areaWidth << "*" << areaHeight << "_" << numNodes << "_" << linkQuality << "_" << runId << " Success" << std::endl;
    return 0;
}
