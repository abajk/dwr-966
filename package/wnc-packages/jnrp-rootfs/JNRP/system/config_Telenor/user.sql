/* user.sql - solution specific sql statements */

/* Copywrite (c) 2012 LAB-JNR, Inc. */

/* modification history
 * --------------------
 */

/*
 * DESCRIPTION
 * This solution specific files include SQL statements for this solution. Any
 * database insert operation or table specific to solution goes here.
 */

-- insert into unitName values ("SPIRIT");
BEGIN;
insert into dbUpdateRegisterTbl values ("/bin/voip_test.sh",0,1,"voipwatchdog", 0, 1,0,0,1);
insert into dbUpdateRegisterTbl values ("UMI_COMP_VOIP_WDOG",0,1,"voipwatchdog", 0, 1,1,1,0);
insert into dbUpdateRegisterTbl values ("UMI_COMP_TR069",0,1,"tr69Config", 0, 1,1,1,0);
insert into dbUpdateRegisterTbl values ("UMI_COMP_TR069",0,1,"lteIPStatus", 0, 1,0,0,0);
--insert into dbUpdateRegisterTbl values ("/usr/local/bin/tr69Init",0,1,"tr69Config", 0, 1,0,0,1);
insert into dbUpdateRegisterTbl values ("/usr/local/bin/dhcpdInit restart",0,1,"DhcpServerBasic", 0, 1,0,0,1);
insert into dbUpdateRegisterTbl values ("/usr/local/bin/dhcpdInit",0,1,"DhcpfixedIpAddress", 0, 1,1,0,1);
insert into dbUpdateRegisterTbl values ("/usr/local/bin/lanInit restart",0,1,"Lan", 0, 1,0,0,1);
insert into dbUpdateRegisterTbl values ("/usr/local/bin/lanInit",0,1,"system", 0, 1,0,0,1);
insert into dbUpdateRegisterTbl values ("/usr/local/bin/syslog.sh",0,1,"RemoteLog", 0, 1,0,0,1);
--insert into dbUpdateRegisterTbl values ("/usr/local/bin/fwnat.sh",0,1,"FirewallNatConfig", 0, 1,0,0,1);
--insert into dbUpdateRegisterTbl values ("/usr/local/bin/fwnat.sh",0,1,"FirewallConfig", 0, 1,0,0,1);
--insert into dbUpdateRegisterTbl values ("/usr/local/bin/firewall.sh",0,1,"PacketFilter", 0, 1,1,1,1);
--insert into dbUpdateRegisterTbl values ("/usr/local/bin/firewall.sh",0,1,"MacFilter", 0, 1,1,1,1);
--insert into dbUpdateRegisterTbl values ("/usr/local/bin/firewall.sh",0,1,"UrlFilter", 0, 1,1,1,1);
--insert into dbUpdateRegisterTbl values ("/usr/local/bin/nat.sh",0,1,"PortForwarding", 0, 1,1,1,1);
--insert into dbUpdateRegisterTbl values ("/usr/local/bin/nat.sh",0,1,"FirewallDmz", 0, 1,0,0,1);
insert into dbUpdateRegisterTbl values ("/usr/local/bin/ddns.sh",0,1,"ddnsConfig", 0, 1,0,0,1);
insert into dbUpdateRegisterTbl values ("/usr/local/bin/ddns.sh",0,1,"ddns", 0, 1,0,0,1);
insert into dbUpdateRegisterTbl values ("/usr/local/bin/ntp.sh",0,1,"ntpConfig", 0, 1,0,0,1);
--insert into dbUpdateRegisterTbl values ("/usr/local/bin/firewall.sh",0,1,"DoS", 0, 1,0,0,1);
insert into dbUpdateRegisterTbl values ("/usr/local/bin/dlna.sh",0,1,"dlnaConfig",0,1,0,0,1);
--insert into dbUpdateRegisterTbl values ("/usr/local/bin/wifiInit",0,1,"wifiAP", 0, 1,0,0,1);
--insert into dbUpdateRegisterTbl values ("/usr/local/bin/wifiInit",0,1,"wifiProfile", 0, 1,0,0,1);
--insert into dbUpdateRegisterTbl values ("/usr/local/bin/wifiInit",0,1,"wifiRadio", 0, 1,0,0,1);
insert into dbUpdateRegisterTbl values ("/usr/local/bin/wifiInit",0,1,"wifiProfilecommit", 0, 1,0,0,1);
insert into dbUpdateRegisterTbl values ("/usr/local/bin/wifiWPS.sh",0,1,"wifiWPS", 0, 1,0,0,1);
insert into dbUpdateRegisterTbl values ("/usr/local/bin/wifiCountSTA.sh",0,1,"wifiClient", 0, 1,0,0,1);
insert into dbUpdateRegisterTbl values ("/usr/local/bin/wifiACL.sh",0,1,"wifiACLconfig", 0, 1,0,0,1);
insert into dbUpdateRegisterTbl values ("/usr/local/bin/wifiActScan.sh",0,1,"wifiAOCStrigger", 0, 1,0,0,1);
--insert into dbUpdateRegisterTbl values ("/usr/local/bin/wifiACL.sh",0,1,"wifiACLTable1", 0, 1,0,0,1);
--insert into dbUpdateRegisterTbl values ("/usr/local/bin/wifiACL.sh",0,1,"wifiACLTable2", 0, 1,0,0,1);

--insert into dbUpdateRegisterTbl values ('UMI_COMP_TR069',0,0,'dot11VAP', 0, 1,1,1);
--insert into dbUpdateRegisterTbl values ('UMI_COMP_TR069',0,0,'dot11Radio', 0, 1,1,1);
--insert into dbUpdateRegisterTbl values ('UMI_COMP_TR069',0,0,'dot11Interface', 0, 1,1,1);
--insert into dbUpdateRegisterTbl values ('UMI_COMP_TR069',0,0,'dot11Profile', 0, 1,1,1);
--insert into dbUpdateRegisterTbl values ('UMI_COMP_TR069',0,0,'dot11STA', 0, 1,1,1);

insert into dbUpdateRegisterTbl values ("/usr/local/bin/routing.sh",0,1,"StaticRoute", 0, 1,1,0,1);

-- test table
--insert into saveTables values("voipwatchdog");
-- lan
insert into saveTables values("Lan");
insert into saveTables values("DhcpServerBasic");
insert into saveTables values("DhcpfixedIpAddress");
insert into saveTables values("StaticRoute");
-- firewall
insert into saveTables values("FirewallDmz");
insert into saveTables values("FirewallNatConfig");
insert into saveTables values("FirewallConfig");
insert into saveTables values("PacketFilter");
insert into saveTables values("MacFilter");
insert into saveTables values("UrlFilter");
insert into saveTables values("PortForwarding");
insert into saveTables values("ddnsConfig");
insert into saveTables values("ddns");
-- tr69
insert into saveTables values("tr69Config");
-- system
insert into saveTables values("system");
insert into saveTables values("users");
insert into saveTables values("groups");
insert into saveTables values("MacTable");
insert into saveTables values("ntpConfig");
insert into saveTables values("RemoteLog");
-- dlna
insert into saveTables values("dlnaConfig");
-- lte
insert into saveTables values("ltePinStatus");
insert into saveTables values("lteNwProfile");
insert into saveTables values("lteHwInfo");
insert into saveTables values("lteRadio");
insert into saveTables values("lteNwStatus");
insert into saveTables values("lteIPStatus");
-- wifi
insert into saveTables values("wifiProfile");
insert into saveTables values("wifiAP");
insert into saveTables values("wifiRadio");
insert into saveTables values("wifiProfilecommit");
insert into saveTables values("wifiWPS");
insert into saveTables values("wifiClient");
insert into saveTables values("wifiACLconfig");
insert into saveTables values("wifiACLTable1");
insert into saveTables values("wifiACLTable2");
insert into saveTables values("wifiEnableStatus");
insert into saveTables values("wifiStatus");
insert into saveTables values("wifiAOCSstatus");
insert into saveTables values("wifiAOCStrigger");
-- voip
insert into saveTables values("DbgSetting");
insert into saveTables values("VoiceProfile");
insert into saveTables values("VoiceProfileSignaling");
insert into saveTables values("VoiceLine");
insert into saveTables values("VoiceLineSignaling");
insert into saveTables values("AuthCfg");

-- UPNP
insert into saveTables values("UPNP");

-- SPI
insert into saveTables values("SPI");

-- Routing
insert into saveTables values("RoutingConfig");
insert into saveTables values("StaticRouting");

-- Remote Manager
insert into saveTables values("RmtMgrConfig");


-- Ap Fw Upgrade KEY
insert into saveTables values("ApFwUpgrade");

insert into networkInterface values ("br0", "LAN1", "bdg1", "ifStatic", "", "bridge",
                                     1500, "192.168.0.1", "255.255.255.0",
                                     "0.0.0.0", "0.0.0.0", "0.0.0.0", 0, 1, 5,
                                     0,1);

insert into networkInterface values ("eth1", "WAN", "eth1", "dhcpc",
                                     "","ethernet", 1500, "0.0.0.0", "0.0.0.0",
                                     "0.0.0.0", "0.0.0.0", "0.0.0.0",
                                     0, 0, 5, 0,1);

insert into networkInterface values ("sit0", "sit0-WAN", "sit0", "",
                                     "","tunnel", 1500, "0.0.0.0", "0.0.0.0",
                                     "0.0.0.0", "0.0.0.0", "0.0.0.0",
                                     0, 0, 5, 0,2);

insert into networkInterface values ("usb0", "LTE-WAN1", "usb0", "",
                                     "","lte1", 1500, "0.0.0.0", "0.0.0.0",
                                     "0.0.0.0", "0.0.0.0", "0.0.0.0",
                                     0, 0, 5, 0,1);
insert into networkInterface values ("usb1", "LTE-WAN2", "usb1", "",
                                     "","lte2", 1500, "0.0.0.0", "0.0.0.0",
                                     "0.0.0.0", "0.0.0.0", "0.0.0.0",
                                     0, 0, 5, 0,1);

insert into LANStatus values ('0','0.0.0.0','','',0);
insert into LANStatus values ('1','0.0.0.0','','',0);
insert into LANStatus values ('2','0.0.0.0','','',0);
insert into LANStatus values ('3','0.0.0.0','','',0);

insert into lteIPv6Status values(1,'no connection','0000:0000:0000:0000:0000:0000:0000:0000/00','0000:0000:0000:0000:0000:0000:0000:0000/00','0000:0000:0000:0000:0000:0000:0000:0000','0000:0000:0000:0000:0000:0000:0000:0000');
insert into lteIPv6Status values(2,'no connection','0000:0000:0000:0000:0000:0000:0000:0000/00','0000:0000:0000:0000:0000:0000:0000:0000/00','0000:0000:0000:0000:0000:0000:0000:0000','0000:0000:0000:0000:0000:0000:0000:0000');
-- this part should move to default ascii
--insert into voipwatchdog values (1,"test1");
--insert into voipwatchdog values (2,"test2");
--insert into Lan values ("LAN1",1500,"192.168.0.1","255.255.255.0",1);
--insert into DhcpServerBasic values (1,1,"192.168.0.2","192.168.0.254",253,24);
--insert into FirewallNatConfig values (1,1);
--insert into FirewallConfig values (1,1);
--insert into tr69Config values('http://cosmos.bredband.com:8080/ACS-server/ACS','','','0','132.177.123.13','','','','','','/flash/ca.pem','/flash/agent.pem','','og4610-TR069');
COMMIT;

-- Trigger part
BEGIN;
CREATE TRIGGER on_update_set_networkInterface AFTER UPDATE ON Lan
BEGIN
UPDATE networkInterface SET ipaddr = new.IpAddress, subnetmask =  new.SubnetMask WHERE LogicalIfName = 'LAN1';
END;
CREATE TRIGGER on_insert_set_networkInterface AFTER INSERT ON Lan
BEGIN
UPDATE networkInterface SET ipaddr = new.IpAddress, subnetmask =  new.SubnetMask WHERE LogicalIfName = 'LAN1';
END;
COMMIT;

