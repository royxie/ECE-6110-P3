/*
 * ECE 6110 Project3
 * Measuring Wireless Throughput Capacity
 * Author: Yang Zhang
 *         Xiang Hao
 *         Ning Wang
 *         Shiyu Xie
 * usage: ./waf --run "p3 --Size=1000 --numNodes=50 --intensity=0.5 --txPower=500 --routeProt=AODV"
 */

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <math.h>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/dsr-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include "ns3/netanim-module.h"
#include "ns3/object.h"
#include "ns3/uinteger.h"
#include "ns3/traced-value.h"
#include "ns3/trace-source-accessor.h"

using namespace ns3;
using namespace std;

uint32_t total = 0;

void Trace ( Ptr<const Packet> newValue)
{
  total = total + newValue->GetSize();
}


NS_LOG_COMPONENT_DEFINE ("p3");

int main (int argc, char *argv[])
{
  /*LogComponentEnable ("p3", LOG_LEVEL_ALL); de-comment to enable log information */
  SeedManager::SetSeed(1);
  std::string animFile = "AnimTrace.xml" ;  /* Name of file for animation output
  
  /*the parameters to tune ON/OFF application*/
  uint16_t runValue  = 0;
  uint16_t pktSize   = 512;
  double   onTime    = 0.1;
  double   offTime   = 0.1; /*equal means duty cycle is 0.5
  double   stopTime  = 10;

  /*experimental parameters*/
  uint16_t numNodes  = 25;
  string Size = "1000";
  double   intensity = 0.5;
  string   routeProt = "AODV";
  double   txPower   = 300;
  
  CommandLine cmd;
  cmd.AddValue ("runValue",  "Random seed run",                           runValue);
  cmd.AddValue ("numNodes",  "Number of nodes in simulation",             numNodes);
  cmd.AddValue ("pktSize",   "Size of packets sent",                      pktSize);
  cmd.AddValue ("Size",   "Size of simulation field in meters",        Size);
  cmd.AddValue ("intensity", "Traffic intensity (%)",                     intensity);
  cmd.AddValue ("routeProt", "Routing protocol to use: AODV, OLSR",       routeProt);
  cmd.AddValue ("txPower",   "Transmission power range in Watts",         txPower);
  cmd.AddValue ("onTime",    "Time ON for OnOffApplication in seconds",   onTime);
  cmd.AddValue ("offTime",    "Time OFF for OnOffApplication in seconds", offTime);
  cmd.AddValue ("stopTime",   "Simulation time in seconds",               stopTime);

  cmd.Parse (argc,argv);

  SeedManager::SetRun(runValue);

  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue(pktSize));

  ostringstream onTimeStr;
  onTimeStr << "ns3::ConstantRandomVariable[Constant=" << onTime << "]";
  Config::SetDefault ("ns3::OnOffApplication::OnTime",
                      StringValue(onTimeStr.str()));

  ostringstream offTimeStr;
  offTimeStr << "ns3::ConstantRandomVariable[Constant=" << offTime << "]";
  Config::SetDefault ("ns3::OnOffApplication::OffTime", StringValue(offTimeStr.str()));
  
  if (!(routeProt.compare("AODV") == 0) &&  
      !(routeProt.compare("OLSR") == 0)){
      NS_LOG_DEBUG ("Invalid routing protocol");
      exit (1);
  }

  txPower = 10 * log10 (txPower);

  /* Create nodes. */
  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create(numNodes);

  /* Create random square based on provided size.*/
  MobilityHelper mobility;
  string posMax = "ns3::UniformRandomVariable[Min=0.0|Max=" + Size + "]";
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue (posMax),
                                 "Y", StringValue (posMax));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  /* Set the Wifi MAC as adhoc.*/
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  /* Set Wifi PHY transmission power range (equal so no fluctuation).*/
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue(txPower));
  wifiPhy.Set ("TxPowerEnd", DoubleValue(txPower));

  /* Set Wifi channel.*/
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  /* Set Wifi to 802.11b and use a DSSS rate of 11Mbps.*/
  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("DsssRate11Mbps"),
                                "ControlMode", StringValue("DsssRate11Mbps"));

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  InternetStackHelper stack;
  
  if (routeProt.compare("AODV") == 0)
  {
      AodvHelper aodv;
      stack.SetRoutingHelper (aodv);
  }
  else if (routeProt.compare("OLSR") == 0)
  {
      OlsrHelper olsr;
      stack.SetRoutingHelper (olsr);
  }

  stack.Install (nodes);

  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);
  
  NS_LOG_INFO ("construct data flow");
  UniformVariable uv (0, numNodes - 1);
  map<uint32_t, uint32_t> nodePairs;
  for (uint32_t i=0; i<numNodes; ++i)
    {
      /* This node index has not been paired.*/
      if (nodePairs.find (i) == nodePairs.end ())
        {
          /* The random index to pair is not the same as the current index*/
          uint32_t pairNode = 0;
          map<uint32_t, uint32_t>::iterator iter;
          do
            {
              pairNode = uv.GetInteger (0, numNodes - 1);
              iter = nodePairs.find (pairNode);
            }
          while ((i == pairNode));
          nodePairs.insert (make_pair (i, pairNode));
          nodePairs.insert (make_pair (pairNode, i));
        }
    }

  NS_LOG_INFO ("Create Applications.");
  /* Determine the data rate to be used based on the intensity and network capacity.*/
  Ptr<WifiNetDevice> wifiDevice = DynamicCast<WifiNetDevice> (devices.Get(0)); 
  uint64_t networkCap = 11 * 1024 * 1024;
 
  uint64_t dataRate  = (uint64_t)(networkCap * intensity) / (0.5*numNodes); /*duty cycle is 0.5

  /* Create OnOffApplications and PacketSinks*/
  ApplicationContainer sourceApps;
  ApplicationContainer sinkApps;
  ExponentialVariable ev (0.1);
  for (uint32_t i=0; i<numNodes; ++i)
    {
      uint16_t port = 50000 + i;
      Ptr<Node> appSource = nodes.Get(nodePairs.find(i)->first);
      Ptr<Node> appSink = nodes.Get(nodePairs.find(i)->second);
      Ipv4Address remoteAddr = appSink->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();

      OnOffHelper onoff ("ns3::UdpSocketFactory", 
                         Address (InetSocketAddress (remoteAddr, port)));
      onoff.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));

      ApplicationContainer tempApp (onoff.Install (appSource));
      tempApp.Start(Seconds(ev.GetValue()));
      sourceApps.Add(tempApp);

      PacketSinkHelper sink ("ns3::UdpSocketFactory", 
                             InetSocketAddress (Ipv4Address::GetAny (), port));
      sinkApps.Add(sink.Install (appSink));
    }
  sourceApps.Stop (Seconds (stopTime));
  sinkApps.Start (Seconds (0.0));
  for (uint32_t i=0; i < numNodes; ++i){
    Ptr<OnOffApplication> onoff1 = DynamicCast<OnOffApplication> (sourceApps.Get (i));
    onoff1->TraceConnectWithoutContext("Tx", MakeCallback(&Trace));
  }
  
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop(Seconds(stopTime));
  /*AnimationInterface anim (animFile);*/
  Simulator::Run ();


  NS_LOG_INFO ("Done.");

  /* Results output*/
  cout << Size << "\t" << numNodes << "\t" << pow(10, txPower/10) << "\t" 
       << intensity << "\t" << routeProt << "\t";
  
  uint32_t totRx = 0;
  for (uint32_t i=0; i < numNodes; ++i)
  {
      Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (i));
      totRx += sink1->GetTotalRx ();
  }
  cout<<(100 * (double)totRx / (double)total) << endl;

  Simulator::Destroy ();
  return 0;
}
