/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 University of Campinas (Unicamp)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Luciano Chaves <luciano@lrc.ic.unicamp.br>
 */

#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("OFSwitch13Tap");

using namespace ns3;

#if defined (NS3_OFSWITCH13) && (ENABLE_TAP)
#include "ns3/ofswitch13-module.h"

int 
main (int argc, char *argv[])
{
  bool verbose = true;
  std::string tapName = "thetap";

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tapName", "Name of the OS tap device", tapName);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("OFSwitch13Tap", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Helper", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13NetDevice", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Interface", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Controller", LOG_LEVEL_ALL);
    }
    
  // Enabling Checksum computations
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));
  // ns3::Packet::EnablePrinting ();

  // Create the terminal nodes
  NodeContainer terminals;
  terminals.Create (2);

  // Create the switches node
  NodeContainer switches;
  switches.Create (2);
  Ptr<Node> switchNode0 = switches.Get (0);
  Ptr<Node> switchNode1 = switches.Get (1);

  // Create the controller node
  NodeContainer controller;
  controller.Create (1);
  Ptr<Node> controllerNode = controller.Get (0);

  // Connect the terminals to the switches using csma links
  CsmaHelper csmaHelper;
  csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csmaHelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer terminalDevices;
  NetDeviceContainer switch0Devices, switch1Devices;
      
  NetDeviceContainer link0 = csmaHelper.Install (NodeContainer (terminals.Get (0), switches.Get (0)));
  NetDeviceContainer link1 = csmaHelper.Install (NodeContainer (terminals.Get (1), switches.Get (1)));
  NetDeviceContainer link2 = csmaHelper.Install (NodeContainer (switches.Get (0), switches.Get (1)));
  terminalDevices.Add (link0.Get (0));
  terminalDevices.Add (link1.Get (0));
  switch0Devices.Add (link0.Get (1));       // Switch 0 Port 1 to terminal 0
  switch0Devices.Add (link2.Get (0));       // Switch 0 Port 2 to switch 1
  switch1Devices.Add (link1.Get (1));       // Switch 1 Port 1 to terminal 1
  switch1Devices.Add (link2.Get (1));       // Switch 1 Port 2 to switch 0
 
   // Install the OFSwitch13NetDevice onto the switch
  NetDeviceContainer of13Device0, of13Device1;
  Ptr<OFSwitch13Helper> ofHelper = Create<OFSwitch13Helper> ();
  ofHelper->SetDeviceAttribute ("ControllerAddr", AddressValue (InetSocketAddress (Ipv4Address ("10.100.150.1"), 6633)));
  of13Device0 = ofHelper->InstallSwitch (switchNode0, switch0Devices);
  of13Device1 = ofHelper->InstallSwitch (switchNode1, switch1Devices);

  // Install the controller app (creating links between controller and switches)
  ofHelper->InstallExternalController (controllerNode);

  // Installing the tcp/ip stack onto terminals
  InternetStackHelper internet;
  internet.Install (terminals);

  // Set IPv4 terminal address
  Ipv4AddressHelper ipv4switches;
  Ipv4InterfaceContainer internetIpIfaces;
  ipv4switches.SetBase ("10.1.1.0", "255.255.255.0");
  internetIpIfaces = ipv4switches.Assign (terminalDevices);

  // Create a ping application from terminal 0 to 1 
  Ipv4Address destAddr = internetIpIfaces.GetAddress (1);
  V4PingHelper ping = V4PingHelper (destAddr);
  ApplicationContainer apps = ping.Install (terminals.Get (0));
  apps.Start (Seconds (10.0));

  // Enable pcap traces
  ofHelper->EnableOpenFlowPcap ();
  csmaHelper.EnablePcap ("ofswitch-l0", switch0Devices.Get (0));
  csmaHelper.EnablePcap ("ofswitch-l1", switch1Devices.Get (0));
  csmaHelper.EnablePcap ("ofswitch-l2", switch0Devices.Get (1));

  // Run the simulation
  Simulator::Stop (Seconds (600));
  Simulator::Run ();
  Simulator::Destroy ();
}

#else

int
main (int argc, char *argv[])
{
  NS_LOG_UNCOND ("This example requires both OpenFlow 1.3 and TapBridge modules! Aborting...");
}

#endif // NS3_OFSWITCH13
