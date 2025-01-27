#ifndef STUN_H
#define STUN_H

#include <time.h>
#include "Common.h"
// if you change this version, change in makefile too 
#define STUN_VERSION "0.96"

#define STUN_MAX_STRING 256
#define STUN_MAX_UNKNOWN_ATTRIBUTES 8
#define STUN_MAX_MESSAGE_SIZE 2048

#define STUN_PORT 3478

// define some basic types
typedef unsigned char  UInt8;
typedef unsigned short UInt16;
typedef unsigned int   UInt32;
#if defined( WIN32 )
typedef unsigned __int64 UInt64;
#else
typedef unsigned long long UInt64;
#endif
typedef struct { unsigned char octet[16]; }  UInt128;

/// define a structure to hold a stun address 
#define  IPv4Family (0x01)
#define  IPv6Family (0x02)

// define  flags  
#define ChangeIpFlag   (0x04)
#define ChangePortFlag (0x02)

// define  stun attribute
#define STUN_MappedAddress    (0x0001)
#define STUN_ResponseAddress  (0x0002)
#define STUN_ChangeRequest    (0x0003)
#define STUN_SourceAddress    (0x0004)
#define STUN_ChangedAddress   (0x0005)
#define STUN_Username         (0x0006)
#define STUN_Password         (0x0007)
#define STUN_MessageIntegrity (0x0008)
#define STUN_ErrorCode        (0x0009)
#define STUN_UnknownAttribute (0x000A)
#define STUN_ReflectedFrom    (0x000B)
#define STUN_XorMappedAddress (0x8020)
#define STUN_XorOnly          (0x0021)
#define STUN_ServerName       (0x8022)
#define STUN_SecondaryAddress (0x8050) // Non standard extention

// define types for a stun message 
#define BindRequestMsg               (0x0001)
#define BindResponseMsg              (0x0101)
#define BindErrorResponseMsg         (0x0111)
#define SharedSecretRequestMsg       (0x0002)
#define SharedSecretResponseMsg      (0x0102)
#define SharedSecretErrorResponseMsg (0x0112)

typedef struct 
{
	UInt16 msgType;
	UInt16 msgLength;
	UInt128 id;
} StunMsgHdr;


typedef struct
{
      UInt16 type;
      UInt16 length;
} StunAtrHdr;

typedef struct
{
      UInt16 port;
      UInt32 addr;
} StunAddress4;

typedef struct
{
      UInt8 pad;
      UInt8 family;
      StunAddress4 ipv4;
} StunAtrAddress4;

typedef struct
{
      UInt32 value;
} StunAtrChangeRequest;

typedef struct
{
      UInt16 pad; // all 0
      UInt8 errorClass;
      UInt8 number;
      char reason[STUN_MAX_STRING];
      UInt16 sizeReason;
} StunAtrError;

typedef struct
{
      UInt16 attrType[STUN_MAX_UNKNOWN_ATTRIBUTES];
      UInt16 numAttributes;
} StunAtrUnknown;

typedef struct
{
      char value[STUN_MAX_STRING];      
      UInt16 sizeValue;
} StunAtrString;

typedef struct
{
      char hash[20];
} StunAtrIntegrity;

typedef enum 
{
   HmacUnkown=0,
   HmacOK,
   HmacBadUserName,
   HmacUnkownUserName,
   HmacFailed,
} StunHmacStatus;

typedef struct
{
	StunMsgHdr msgHdr;
	BOOL hasMappedAddress;
	StunAtrAddress4  mappedAddress;
	
      BOOL hasResponseAddress;
      StunAtrAddress4  responseAddress;
	
      BOOL hasChangeRequest;
      StunAtrChangeRequest changeRequest;
	
      BOOL hasSourceAddress;
      StunAtrAddress4 sourceAddress;
	
      BOOL hasChangedAddress;
      StunAtrAddress4 changedAddress;
	
      BOOL hasUsername;
      StunAtrString username;
	
      BOOL hasPassword;
      StunAtrString password;
	
      BOOL hasMessageIntegrity;
      StunAtrIntegrity messageIntegrity;
	
      BOOL hasErrorCode;
      StunAtrError errorCode;
	
      BOOL hasUnknownAttributes;
      StunAtrUnknown unknownAttributes;
	
      BOOL hasReflectedFrom;
      StunAtrAddress4 reflectedFrom;

      BOOL hasXorMappedAddress;
      StunAtrAddress4  xorMappedAddress;
	
      BOOL xorOnly;

      BOOL hasServerName;
      StunAtrString serverName;
      
      BOOL hasSecondaryAddress;
      StunAtrAddress4 secondaryAddress;
} StunMessage; 


// Define enum with different types of NAT 
typedef enum 
{
   StunTypeUnknown=0,
   StunTypeFailure,
   StunTypeOpen,
   StunTypeBlocked,

   StunTypeIndependentFilter,
   StunTypeDependentFilter,
   StunTypePortDependedFilter,
   StunTypeDependentMapping,

   //StunTypeConeNat,
   //StunTypeRestrictedNat,
   //StunTypePortRestrictedNat,
   //StunTypeSymNat,
   
   StunTypeFirewall,
} NatType;

#define MAX_MEDIA_RELAYS 500
#define MAX_RTP_MSG_SIZE 1500
#define MEDIA_RELAY_TIMEOUT 3*60

typedef struct 
{
      int relayPort;       // media relay port
      int fd;              // media relay file descriptor
      StunAddress4 destination; // NAT IP:port
      time_t expireTime;      // if no activity after time, close the socket 
} StunMediaRelay;

typedef struct
{
      StunAddress4 myAddr;
      StunAddress4 altAddr;
      SOCKET myFd;
      SOCKET altPortFd;
      SOCKET altIpFd;
      SOCKET altIpPortFd;
      BOOL relay; // true if media relaying is to be done
      StunMediaRelay relays[MAX_MEDIA_RELAYS];
} StunServerInfo;

// header file 
#ifdef __cplusplus 
	extern "C" { // only need to export C interface if  used by C++ source code 
#endif 

BOOL
stunParseMessage( char* buf, 
                  unsigned int bufLen, 
                  StunMessage* message, 
                  BOOL verbose );

void
stunBuildReqSimple( StunMessage* msg,
                    const StunAtrString* username,
                    BOOL changePort, BOOL changeIp, unsigned int id);

unsigned int
stunEncodeMessage( const StunMessage* message, 
                   char* buf, 
                   unsigned int bufLen, 
                   const StunAtrString* password,
                   BOOL verbose);

void
stunCreateUserName(const StunAddress4* addr, StunAtrString* username);

void 
stunGetUserNameAndPassword(  const StunAddress4* dest, 
                             StunAtrString* username,
                             StunAtrString* password);

void
stunCreatePassword(const StunAtrString* username, StunAtrString* password);

int 
stunRand();

UInt64
stunGetSystemTimeSecs();

/// find the IP address of a the specified stun server - return false is fails parse 
BOOL  
stunParseServerName( char* serverName, StunAddress4* stunServerAddr);

BOOL 
stunParseHostName( char* peerName,
                   UInt32* ip,
                   UInt16* portVal,
                   UInt16 defaultPort );

/// return true if all is OK
/// Create a media relay and do the STERN thing if startMediaPort is non-zero
/*BOOL
stunInitServer(StunServerInfo* info, 
               const StunAddress4* myAddr, 
               const StunAddress4* altAddr,
               int startMediaPort,
               BOOL verbose);
*/
//void
//stunStopServer(StunServerInfo* info);

/// return true if all is OK 
//BOOL
//stunServerProcess(StunServerInfo* info, BOOL verbose);

/// returns number of address found - take array or addres 

int 
stunFindLocalInterfaces(UInt32* addresses, int maxSize );

void stunTest( StunAddress4* dest, int testNum, BOOL verbose, StunAddress4* srcAddr );

NatType
stunNatType( StunAddress4* dest, BOOL verbose, 
             BOOL* preservePort, // if set, is return for if NAT preservers ports or not
             BOOL* hairpin ,  // if set, is the return for if NAT will hairpin packets
             int port, // port to use for the test, 0 to choose random port
             StunAddress4* sAddr // NIC to use 
   );

/// prints a StunAddress
//std::ostream&
//operator<<( std::ostream& strm, const StunAddress4& addr);

//std::ostream* 
//operator<< ( std::ostream& strm, const UInt128& );


/*
BOOL
stunServerProcessMsg( char* buf,
                      unsigned int bufLen,
                      StunAddress4* from, 
                      StunAddress4* myAddr,
                      StunAddress4* altAddr, 
                      StunMessage* resp,
                      StunAddress4* destination,
                      StunAtrString* hmacPassword,
                      BOOL* changePort,
                      BOOL* changeIp,
                      BOOL verbose);
*/

int
stunOpenSocket( StunAddress4* dest, 
                StunAddress4* mappedAddr, 
                int port, 
                StunAddress4* srcAddr, 
                BOOL verbose );

BOOL
stunOpenSocketPair( StunAddress4* dest, StunAddress4* mappedAddr, 
                    int* fd1, int* fd2, 
                    int srcPort,  StunAddress4* srcAddr,
                    BOOL verbose);

int
stunRandomPort();

#ifdef __cplusplus 
} 
#endif

#endif


/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * 3. The names "VOCAL", "Vovida Open Communication Application Library",
 *    and "Vovida Open Communication Application Library (VOCAL)" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact vocal@vovida.org.
 *
 * 4. Products derived from this software may not be called "VOCAL", nor
 *    may "VOCAL" appear in their name, without prior written
 *    permission of Vovida Networks, Inc.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
 * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES
 * IN EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * 
 * ====================================================================
 * 
 * This software consists of voluntary contributions made by Vovida
 * Networks, Inc. and many individuals on behalf of Vovida Networks,
 * Inc.  For more information on Vovida Networks, Inc., please see
 * <http://www.vovida.org/>.
 *
 */

// Local Variables:
// mode:c++
// c-file-style:"ellemtel"
// c-file-offsets:((case-label . +))
// indent-tabs-mode:nil
// End:

