/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include "Network.h"
#include <net/if.h>
#include <arpa/inet.h>
#include "utils.h"

using namespace std;

#define DEFAULT_PING_PACKETS 15
#define CIDR_NETMASK_IP_LEN 32

const short WPEFramework::Plugin::Network::API_VERSION_NUMBER_MAJOR = 2;
const short WPEFramework::Plugin::Network::API_VERSION_NUMBER_MINOR = 0;

/* Netsrvmgr Based Macros & Structures */
#define NETSRVMGR_INTERFACES_MAX 16
#define IARM_BUS_NM_SRV_MGR_NAME "NET_SRV_MGR"
#define INTERFACE_SIZE 10
#define INTERFACE_LIST 50
#define MAX_IP_ADDRESS_LEN 46
#define MAX_IP_FAMILY_SIZE 10
#define MAX_HOST_NAME_LEN 128
#define MAX_ENDPOINTS 5
#define MAX_ENDPOINT_SIZE 260 // 253 + 1 + 5 + 1 (domain name max length + ':' + port number max chars + '\0')
#define IARM_BUS_NETSRVMGR_API_getActiveInterface "getActiveInterface"
#define IARM_BUS_NETSRVMGR_API_getNetworkInterfaces "getNetworkInterfaces"
#define IARM_BUS_NETSRVMGR_API_getInterfaceList "getInterfaceList"
#define IARM_BUS_NETSRVMGR_API_getDefaultInterface "getDefaultInterface"
#define IARM_BUS_NETSRVMGR_API_setDefaultInterface "setDefaultInterface"
#define IARM_BUS_NETSRVMGR_API_isInterfaceEnabled "isInterfaceEnabled"
#define IARM_BUS_NETSRVMGR_API_setInterfaceEnabled "setInterfaceEnabled"
#define IARM_BUS_NETSRVMGR_API_getSTBip "getSTBip"
#define IARM_BUS_NETSRVMGR_API_setIPSettings "setIPSettings"
#define IARM_BUS_NETSRVMGR_API_getIPSettings "getIPSettings"
#define IARM_BUS_NETSRVMGR_API_getSTBip_family "getSTBip_family"
#define IARM_BUS_NETSRVMGR_API_isConnectedToInternet "isConnectedToInternet"
#define IARM_BUS_NETSRVMGR_API_setConnectivityTestEndpoints "setConnectivityTestEndpoints"
#define IARM_BUS_NETSRVMGR_API_isAvailable "isAvailable"
#define IARM_BUS_NETSRVMGR_API_getPublicIP "getPublicIP"

typedef enum _NetworkManager_EventId_t {
    IARM_BUS_NETWORK_MANAGER_EVENT_SET_INTERFACE_ENABLED=50,
    IARM_BUS_NETWORK_MANAGER_EVENT_SET_INTERFACE_CONTROL_PERSISTENCE,
    IARM_BUS_NETWORK_MANAGER_EVENT_WIFI_INTERFACE_STATE,
    IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_ENABLED_STATUS,
    IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS,
    IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS,
    IARM_BUS_NETWORK_MANAGER_EVENT_DEFAULT_INTERFACE,
    IARM_BUS_NETWORK_MANAGER_MAX,
} IARM_Bus_NetworkManager_EventId_t;

typedef enum _NetworkManager_GetIPSettings_ErrorCode_t
{
  NETWORK_IPADDRESS_ACQUIRED,
  NETWORK_IPADDRESS_NOTFOUND,
  NETWORK_NO_ROUTE_INTERFACE,
  NETWORK_NO_DEFAULT_ROUTE,
  NETWORK_DNS_NOT_CONFIGURED,
  NETWORK_INVALID_IPADDRESS,
} NetworkManager_GetIPSettings_ErrorCode_t;

typedef struct _IARM_BUS_NetSrvMgr_Iface_EventData_t {
    union {
        char activeIface[INTERFACE_SIZE];
        char allNetworkInterfaces[INTERFACE_LIST];
        char setInterface[INTERFACE_SIZE];
        char activeIfaceIpaddr[MAX_IP_ADDRESS_LEN];
    };
    char interfaceCount;
    bool isInterfaceEnabled;
    bool persist;
    char ipfamily[MAX_IP_FAMILY_SIZE];
} IARM_BUS_NetSrvMgr_Iface_EventData_t;

typedef struct {
    char interface[16];
    char ipversion[16];
    bool autoconfig;
    char ipaddress[MAX_IP_ADDRESS_LEN];
    char netmask[MAX_IP_ADDRESS_LEN];
    char gateway[MAX_IP_ADDRESS_LEN];
    char dhcpserver[MAX_IP_ADDRESS_LEN];
    char primarydns[MAX_IP_ADDRESS_LEN];
    char secondarydns[MAX_IP_ADDRESS_LEN];
    bool isSupported;
    NetworkManager_GetIPSettings_ErrorCode_t errCode;
} IARM_BUS_NetSrvMgr_Iface_Settings_t;

typedef struct
{
    unsigned char size;
    char          endpoints[MAX_ENDPOINTS][MAX_ENDPOINT_SIZE];
} IARM_BUS_NetSrvMgr_Iface_TestEndpoints_t;

typedef struct {
    char name[16];
    char mac[20];
    unsigned int flags;
} NetSrvMgr_Interface_t;

typedef struct {
    unsigned char         size;
    NetSrvMgr_Interface_t interfaces[NETSRVMGR_INTERFACES_MAX];
} IARM_BUS_NetSrvMgr_InterfaceList_t;

typedef struct {
    char interface[16];
    char gateway[MAX_IP_ADDRESS_LEN];
} IARM_BUS_NetSrvMgr_DefaultRoute_t;

typedef struct {
    char interface[16];
    bool status;
} IARM_BUS_NetSrvMgr_Iface_EventInterfaceStatus_t;

typedef IARM_BUS_NetSrvMgr_Iface_EventInterfaceStatus_t IARM_BUS_NetSrvMgr_Iface_EventInterfaceEnabledStatus_t;
typedef IARM_BUS_NetSrvMgr_Iface_EventInterfaceStatus_t IARM_BUS_NetSrvMgr_Iface_EventInterfaceConnectionStatus_t;

typedef struct {
    char interface[16];
    char ip_address[MAX_IP_ADDRESS_LEN];
    bool is_ipv6;
    bool acquired;
} IARM_BUS_NetSrvMgr_Iface_EventInterfaceIPAddress_t;

typedef struct {
    char oldInterface[16];
    char newInterface[16];
} IARM_BUS_NetSrvMgr_Iface_EventDefaultInterface_t;

typedef struct
{
    char server[MAX_HOST_NAME_LEN];
    uint16_t port;
    bool ipv6;
    char interface[16];
    uint16_t bind_timeout;
    uint16_t cache_timeout;
    bool sync;
    char public_ip[MAX_IP_ADDRESS_LEN];
} IARM_BUS_NetSrvMgr_Iface_StunRequest_t;

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(Network, Network::API_VERSION_NUMBER_MAJOR, Network::API_VERSION_NUMBER_MINOR);
        Network* Network::_instance = nullptr;

        Network::Network()
        : AbstractPlugin(Network::API_VERSION_NUMBER_MAJOR)
        , m_apiVersionNumber(API_VERSION_NUMBER_MAJOR)
        {
            Network::_instance = this;
            m_isPluginReady = false;

            // Quirk
            Register("getQuirks", &Network::getQuirks, this);

            // Network_API_Version_1
            Register("getInterfaces", &Network::getInterfaces, this);
            Register("isInterfaceEnabled", &Network::isInterfaceEnabled, this);
            Register("setInterfaceEnabled", &Network::setInterfaceEnabled, this);
            Register("getDefaultInterface", &Network::getDefaultInterface, this);
            Register("setDefaultInterface", &Network::setDefaultInterface, this);

            Register("getStbIp", &Network::getStbIp, this);

            Register("trace", &Network::trace, this);
            Register("traceNamedEndpoint", &Network::traceNamedEndpoint, this);

            Register("getNamedEndpoints", &Network::getNamedEndpoints, this);

            Register("ping",              &Network::ping, this);
            Register("pingNamedEndpoint", &Network::pingNamedEndpoint, this);

            registerMethod("setIPSettings", &Network::setIPSettings, this, {1});
            registerMethod("setIPSettings", &Network::setIPSettings2, this, {2});
            registerMethod("getIPSettings", &Network::getIPSettings, this, {1});
            registerMethod("getIPSettings", &Network::getIPSettings2, this, {2});

            Register("getSTBIPFamily", &Network::getSTBIPFamily, this);
            Register("isConnectedToInternet", &Network::isConnectedToInternet, this);
            Register("setConnectivityTestEndpoints", &Network::setConnectivityTestEndpoints, this);

            Register("getPublicIP", &Network::getPublicIP, this);
            Register("setStunEndPoint", &Network::setStunEndPoint, this);

            const char * script1 = R"(grep DEVICE_TYPE /etc/device.properties | cut -d "=" -f2 | tr -d '\n')";
            m_isHybridDevice = Utils::cRunScript(script1).substr();
            LOGWARN("script1 '%s' result: '%s'", script1, m_isHybridDevice.c_str());
            m_defaultInterface = "";
            m_gatewayInterface = "";

            m_netUtils.InitialiseNetUtils();
            m_stunEndPoint = "stun.l.google.com";
            m_stunPort = 19302;
            m_stunBindTimeout = 30;
            m_stunCacheTimeout = 0;
            m_stunSync = true;
        }

        Network::~Network()
        {
        }

        const string Network::Initialize(PluginHost::IShell* /* service */)
        {
            string msg;
            if (Utils::IARM::init())
            {
                IARM_Result_t res;
                IARM_Result_t retVal = IARM_RESULT_SUCCESS;

#ifndef NET_DISABLE_NETSRVMGR_CHECK
                char c;
                uint32_t retry = 0;
                do{
                    retVal = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_isAvailable, (void *)&c, sizeof(c), (1000*10));
                    if(retVal != IARM_RESULT_SUCCESS){
                        LOGDBG("NetSrvMgr is not available. Failed to activate Network Plugin, retry = %d", retry);
                        usleep(500*1000);
                        retry++;
                    }
                }while((retVal != IARM_RESULT_SUCCESS) && (retry < 20));
#endif

                if(retVal != IARM_RESULT_SUCCESS)
                {
                    /*return empty string for this failure & retry in another thread */
                    LOGINFO("NETWORK_NOT_READY: The NetSrvMgr Component is not available.Retrying in separate thread");
                    retryIarmEventRegistration();
                }
                else {
                    IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_ENABLED_STATUS, eventHandler) );
                    IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS, eventHandler) );
                    IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS, eventHandler) );
                    IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_DEFAULT_INTERFACE, eventHandler) );
                    LOGINFO("Successfully activated Network Plugin");
                    m_isPluginReady = true;
                }
            }
            else
            {
                msg = "IARM bus is not available";
                LOGERR("IARM bus is not available. Failed to activate Network Plugin");
            }

            return msg;
        }

        void Network::Deinitialize(PluginHost::IShell* /* service */)
        {
            m_isPluginReady = false;
            m_exitRetryThread = true;
            if(m_registrationThread.joinable())
            {
                m_registrationThread.join();
            }

            if (Utils::IARM::isConnected())
            {
                IARM_Result_t res;
                IARM_CHECK( IARM_Bus_UnRegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_ENABLED_STATUS) );
                IARM_CHECK( IARM_Bus_UnRegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS) );
                IARM_CHECK( IARM_Bus_UnRegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS) );
                IARM_CHECK( IARM_Bus_UnRegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_DEFAULT_INTERFACE) );
            }
            Unregister("getQuirks");
            Unregister("getInterfaces");
            Unregister("isInterfaceEnabled");
            Unregister("setInterfaceEnabled");
            Unregister("getDefaultInterface");
            Unregister("setDefaultInterface");
            Unregister("getStbIp");
            Unregister("trace");
            Unregister("traceNamedEndpoint");
            Unregister("getNamedEndpoints");
            Unregister("ping");
            Unregister("pingNamedEndpoint");
            Unregister("setIPSettings");
            Unregister("getIPSettings");
            Unregister("isConnectedToInternet");
            Unregister("setConnectivityTestEndpoints");
            Unregister("getPublicIP");
            Unregister("setStunEndPoint");

            Network::_instance = nullptr;
        }

        string Network::Information() const
        {
             return(string());
        }

        bool Network::isValidCIDRv4(string buf)
        {
            string CIDR_PREFIXES[CIDR_NETMASK_IP_LEN] = {
                                                     "128.0.0.0",
                                                     "192.0.0.0",
                                                     "224.0.0.0",
                                                     "240.0.0.0",
                                                     "248.0.0.0",
                                                     "252.0.0.0",
                                                     "254.0.0.0",
                                                     "255.0.0.0",
                                                     "255.128.0.0",
                                                     "255.192.0.0",
                                                     "255.224.0.0",
                                                     "255.240.0.0",
                                                     "255.248.0.0",
                                                     "255.252.0.0",
                                                     "255.254.0.0",
                                                     "255.255.0.0",
                                                     "255.255.128.0",
                                                     "255.255.192.0",
                                                     "255.255.224.0",
                                                     "255.255.240.0",
                                                     "255.255.248.0",
                                                     "255.255.252.0",
                                                     "255.255.254.0",
                                                     "255.255.255.0",
                                                     "255.255.255.128",
                                                     "255.255.255.192",
                                                     "255.255.255.224",
                                                     "255.255.255.240",
                                                     "255.255.255.248",
                                                     "255.255.255.252",
                                                     "255.255.255.254",
                                                     "255.255.255.255",
                                                   };
            int i = 0;
            bool retval = false;
            while(i < CIDR_NETMASK_IP_LEN)
            {
                if((buf.compare(CIDR_PREFIXES[i])) == 0)
                {
                    retval = true;
                    break;
                }
                i++;
            }
            return retval;
        }


        void  Network::retryIarmEventRegistration()
        {
            m_registrationThread = thread(&Network::threadEventRegistration, this);

        }
        void  Network::threadEventRegistration()
        {
            IARM_Result_t res = IARM_RESULT_SUCCESS;
            IARM_Result_t retVal = IARM_RESULT_SUCCESS;
            uint32_t retry = 0;
#ifndef NET_DISABLE_NETSRVMGR_CHECK
            do
            {
                char c;
                retVal = IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_isAvailable, (void *)&c, sizeof(c));
                if(retVal != IARM_RESULT_SUCCESS){
                    LOGDBG("threadEventRegistration NetSrvMgr is not available. Failed to activate Network Plugin, retrying count = %d", retry);
                    usleep(500*1000);
                    retry++;
                }
            }while((retVal != IARM_RESULT_SUCCESS) && (m_isPluginReady != true ) && (m_exitRetryThread != true));
#endif

            if((retVal == IARM_RESULT_SUCCESS) && (m_exitRetryThread != true))
            {
                IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_ENABLED_STATUS, eventHandler) );
                IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS, eventHandler) );
                IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS, eventHandler) );
                IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_DEFAULT_INTERFACE, eventHandler) );
                LOGINFO("NETWORK_AVAILABILITY_RETRY_SUCCESS: threadEventRegistration successfully subscribed to IARM event for Network Plugin after %d retries", retry);
                m_isPluginReady = true;
            }
            else
            {
                LOGINFO("Exiting threadEventRegistration");
            }

        }
        // Wrapper methods
        uint32_t Network::getQuirks(const JsonObject& parameters, JsonObject& response)
        {
            JsonArray array;
            array.Add("RDK-20093");
            response["quirks"] = array;
            returnResponse(true)
        }

        uint32_t Network::getInterfaces (const JsonObject& parameters, JsonObject& response)
        {
            IARM_BUS_NetSrvMgr_InterfaceList_t list;
            bool result = false;

            if(m_isPluginReady)
            {
                if (IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_getInterfaceList, (void*)&list, sizeof(list)))
                {
                    JsonArray networkInterfaces;

                    for (int i = 0; i < list.size; i++)
                    {
                        JsonObject interface;
                        string iface = m_netUtils.getInterfaceDescription(list.interfaces[i].name);
#ifdef NET_DEFINED_INTERFACES_ONLY
                        if (iface == "")
                            continue;                    // Skip unrecognised interfaces...
#endif
                        interface["interface"] = iface;
                        interface["macAddress"] = string(list.interfaces[i].mac);
                        interface["enabled"] = ((list.interfaces[i].flags & IFF_UP) != 0);
                        interface["connected"] = ((list.interfaces[i].flags & IFF_RUNNING) != 0);

                        networkInterfaces.Add(interface);
                    }

                    response["interfaces"] = networkInterfaces;
                    result = true;
                }
                else
                {
                    LOGWARN ("Call to %s for %s failed", IARM_BUS_NM_SRV_MGR_NAME, __FUNCTION__);
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }

            returnResponse(result)
        }

        uint32_t Network::getDefaultInterface (const JsonObject& parameters, JsonObject& response)
        {
            string interface;
            string gateway;

            bool result = false;
            if(m_isPluginReady)
            {
                if (_getDefaultInterface(interface, gateway))
                {
                    response["interface"] = m_netUtils.getInterfaceDescription(interface);
                    result = true;
                }
                else
                {
                    LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
                }

                returnResponse(result)
            }
        }

        uint32_t Network::setDefaultInterface (const JsonObject& parameters, JsonObject& response)
        {
            bool result = false;

            if(m_isPluginReady)
            {
                if ((parameters.HasLabel("interface")) && (parameters.HasLabel("persist")))
                {
                    string interface = "";
                    bool persist = false;

                    getStringParameter("interface", interface)

                        if (!(strcmp (interface.c_str(), "ETHERNET") == 0 || strcmp (interface.c_str(), "WIFI") == 0))
                        {
                            LOGERR ("Call for %s failed due to invalid interface [%s]", IARM_BUS_NETSRVMGR_API_setDefaultInterface, interface.c_str());
                            returnResponse (result)
                        }

                    getBoolParameter("persist", persist)

                        IARM_BUS_NetSrvMgr_Iface_EventData_t iarmData = { 0 };
                    strncpy(iarmData.setInterface, interface.c_str(), INTERFACE_SIZE);
                    iarmData.persist = persist;

                    if (IARM_RESULT_SUCCESS == IARM_Bus_Call (IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_setDefaultInterface, (void *)&iarmData, sizeof(iarmData)))
                        result = true;
                    else
                        LOGWARN ("Call to %s for %s failed", IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_setDefaultInterface);
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }

            returnResponse(result)
        }

        uint32_t Network::getStbIp(const JsonObject &parameters, JsonObject &response)
        {
            IARM_BUS_NetSrvMgr_Iface_EventData_t param;
            memset(&param, 0, sizeof(param));

            bool result = false;

            if(m_isPluginReady)
            {
                if (IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_getSTBip, (void*)&param, sizeof(param)))
                {
                    response["ip"] = string(param.activeIfaceIpaddr, MAX_IP_ADDRESS_LEN - 1);
                    result = true;
                }
                else
                {
                    response["ip"] = "";
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }

            returnResponse(result)
        }

        uint32_t Network::getSTBIPFamily(const JsonObject &parameters, JsonObject &response)
        {
            bool result = false;

            if(m_isPluginReady)
            {
                if (parameters.HasLabel("family"))
                {
                    IARM_BUS_NetSrvMgr_Iface_EventData_t param;
                    memset(&param, 0, sizeof(param));

                    string ipfamily("");
                    getStringParameter("family", ipfamily);
                    strncpy(param.ipfamily,ipfamily.c_str(),MAX_IP_FAMILY_SIZE);

                    if (IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_getSTBip_family, (void*)&param, sizeof(param)))
                    {
                        response["ip"] = string(param.activeIfaceIpaddr, MAX_IP_ADDRESS_LEN - 1);
                        result = true;
                    }
                    else
                    {
                        LOGWARN ("Query to get IPaddress by Family Failed..");
                        response["ip"] = "";
                    }
                }
                else
                {
                    LOGWARN ("Required Family Attribute is not provided.");
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }

            returnResponse(result)
        }
        
        uint32_t Network::isInterfaceEnabled (const JsonObject& parameters, JsonObject& response)
        {
            bool result = false;

            if(m_isPluginReady)
            {
                if (parameters.HasLabel("interface"))
                {
                    string interface = "";
                    getStringParameter("interface", interface)

                        if (!(strcmp (interface.c_str(), "ETHERNET") == 0 || strcmp (interface.c_str(), "WIFI") == 0))
                        {
                            LOGERR ("Call for %s failed due to invalid interface [%s]", IARM_BUS_NETSRVMGR_API_isInterfaceEnabled, interface.c_str());
                            returnResponse (result)
                        }

                    IARM_BUS_NetSrvMgr_Iface_EventData_t param = {0};
                    strncpy(param.setInterface, interface.c_str(), INTERFACE_SIZE);

                    if (IARM_RESULT_SUCCESS == IARM_Bus_Call (IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_isInterfaceEnabled, (void*)&param, sizeof(param)))
                    {
                        LOGINFO("%s :: Enabled = %d ",__FUNCTION__,param.isInterfaceEnabled);
                        response["enabled"] = param.isInterfaceEnabled;
                        result = true;
                    }
                    else
                        LOGWARN ("Call to %s for %s failed", IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_isInterfaceEnabled);
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }

            returnResponse(result)
        }

        uint32_t Network::setInterfaceEnabled (const JsonObject& parameters, JsonObject& response)
        {
            bool result = false;

            if(m_isPluginReady)
            {
                if ((parameters.HasLabel("interface")) && (parameters.HasLabel("enabled")) && (parameters.HasLabel("persist")))
                {
                    string interface = "";
                    bool enabled = false;
                    bool persist = false;

                    getStringParameter("interface", interface)

                        if (!(strcmp (interface.c_str(), "ETHERNET") == 0 || strcmp (interface.c_str(), "WIFI") == 0))
                        {
                            LOGERR ("Call for %s failed due to invalid interface [%s]", IARM_BUS_NETSRVMGR_API_setInterfaceEnabled, interface.c_str());
                            returnResponse (result)
                        }

                    getBoolParameter("enabled", enabled)
                        getBoolParameter("persist", persist)

                        IARM_BUS_NetSrvMgr_Iface_EventData_t iarmData = { 0 };
                    strncpy(iarmData.setInterface, interface.c_str(), INTERFACE_SIZE);
                    iarmData.isInterfaceEnabled = enabled;
                    iarmData.persist = persist;

                    if (IARM_RESULT_SUCCESS == IARM_Bus_Call (IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_setInterfaceEnabled, (void *)&iarmData, sizeof(iarmData)))
                        result = true;
                    else
                        LOGWARN ("Call to %s for %s failed", IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_setInterfaceEnabled);
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }

            returnResponse(result)
        }

        uint32_t Network::getNamedEndpoints(const JsonObject& parameters, JsonObject& response)
        {
            JsonArray namedEndpoints;
            namedEndpoints.Add("CMTS");

            response["endpoints"] = namedEndpoints;
            returnResponse(true)
        }

        uint32_t Network::trace(const JsonObject& parameters, JsonObject& response)
        {
            bool result = false;

            if(m_isPluginReady)
            {
                if (!parameters.HasLabel("endpoint"))
                    LOGERR("No endpoint specified");
                else
                {
                    string endpoint = "";
                    int packets = 0;

                    getStringParameter("endpoint", endpoint);
                    if (parameters.HasLabel("packets")) // packets is optional?
                        getNumberParameter("packets", packets);

                    if (_doTrace(endpoint, packets, response))
                        result = true;
                    else
                        LOGERR("Failed to perform network trace");
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }

            returnResponse(result)
        }

        uint32_t Network::traceNamedEndpoint(const JsonObject& parameters, JsonObject& response)
        {
            bool result = false;

            if(m_isPluginReady)
            {
                if (!parameters.HasLabel("endpointName"))
                    LOGERR("No endpointName specified");
                else
                {
                    string endpointName = "";
                    int packets = 0;

                    getStringParameter("endpointName", endpointName);
                    if (parameters.HasLabel("packets")) // packets is optional?
                        getNumberParameter("packets", packets);

                    if (_doTraceNamedEndpoint(endpointName, packets, response))
                        result = true;
                    else
                        LOGERR("Failed to perform network trace names endpoint");
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }

            returnResponse(result)
        }

        uint32_t Network::ping (const JsonObject& parameters, JsonObject& response)
        {
            string guid;
            getStringParameter("guid", guid)

                uint32_t packets;
            getDefaultNumberParameter("packets", packets, DEFAULT_PING_PACKETS);

            bool result = false;

            if(m_isPluginReady)
            {
                if (parameters.HasLabel("endpoint"))
                {
                    string endpoint;
                    getStringParameter("endpoint", endpoint);
                    response = _doPing(guid, endpoint, packets);
                    result = response["success"].Boolean();
                }
                else
                {
                    LOGERR("No endpoint argument");
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }
            returnResponse(result)
        }

        uint32_t Network::pingNamedEndpoint (const JsonObject& parameters, JsonObject& response)
        {
            string guid;
            getStringParameter("guid", guid)

            uint32_t packets;
            getDefaultNumberParameter("packets", packets, DEFAULT_PING_PACKETS);

            bool result = false;

            if(m_isPluginReady)
            {
                if (parameters.HasLabel("endpointName"))
                {
                    string endpointName;
                    getDefaultStringParameter("endpointName", endpointName, "")

                        response = _doPingNamedEndpoint(guid, endpointName, packets);
                    result = response["success"].Boolean();
                }
                else
                {
                    LOGERR("No endpointName argument");
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }

            returnResponse(result)
        }

        uint32_t Network::setIPSettings(const JsonObject& parameters, JsonObject& response)
        {
            bool result = false;
            if(m_isPluginReady)
                return  setIPSettingsInternal(parameters, response);
            else
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);

            returnResponse(result)
        }

        uint32_t Network::setIPSettings2(const JsonObject& parameters, JsonObject& response)
        {
            JsonObject internal;
            string interface = "";
            string ipversion = "";
            string netmask = "";
            string gateway = "";
            string ipaddr = "";
            string primarydns = "";
            string secondarydns = "";
            bool autoconfig = true;
            bool result = false;

            if(m_isPluginReady)
            {
                getDefaultStringParameter("interface", interface, "");
                internal ["interface"] = interface;
                getDefaultStringParameter("ipversion", ipversion, "");
                internal ["ipversion"] = ipversion;
                getDefaultBoolParameter("autoconfig", autoconfig, true);
                internal ["autoconfig"] = autoconfig;
                getDefaultStringParameter("ipaddr", ipaddr, "");
                internal ["ipaddr"] = ipaddr;
                getDefaultStringParameter("netmask", netmask, "");
                internal ["netmask"] = netmask;
                getDefaultStringParameter("gateway", gateway, "");
                internal ["gateway"] = gateway;
                getDefaultStringParameter("primarydns", primarydns, "0.0.0.0");
                internal ["primarydns"] = primarydns;
                getDefaultStringParameter("secondarydns", secondarydns, "");
                internal ["secondarydns"] = secondarydns;

                return  setIPSettingsInternal(internal, response);
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }
            returnResponse(result)
        }

        uint32_t Network::setIPSettingsInternal(const JsonObject& parameters, JsonObject& response)
        {
            bool result = false;
            struct in_addr ip_address, gateway_address, mask;
            struct in_addr broadcast_addr1, broadcast_addr2;

            if ((parameters.HasLabel("interface")) && (parameters.HasLabel("ipversion")) && (parameters.HasLabel("autoconfig")) &&
                (parameters.HasLabel("ipaddr")) && (parameters.HasLabel("netmask")) && (parameters.HasLabel("gateway")) &&
                (parameters.HasLabel("primarydns")) && (parameters.HasLabel("secondarydns")))
            {
                string interface = "";
                string ipversion = "";
                bool autoconfig = false;
                string ipaddr = "";
                string netmask = "";
                string gateway = "";
                string primarydns = "";
                string secondarydns = "";

                getStringParameter("interface", interface);
                getStringParameter("ipversion", ipversion);
                getBoolParameter("autoconfig", autoconfig);
                getStringParameter("ipaddr", ipaddr);
                getStringParameter("netmask", netmask);
                getStringParameter("gateway", gateway);
                getStringParameter("primarydns", primarydns);
                getStringParameter("secondarydns", secondarydns);

                IARM_BUS_NetSrvMgr_Iface_Settings_t iarmData = {0};
                strncpy(iarmData.interface, interface.c_str(), 16);
                strncpy(iarmData.ipversion, ipversion.c_str(), 16);
                iarmData.autoconfig = autoconfig;
                strncpy(iarmData.ipaddress, ipaddr.c_str(), 16);
                strncpy(iarmData.netmask, netmask.c_str(), 16);
                strncpy(iarmData.gateway, gateway.c_str(), 16);
                strncpy(iarmData.primarydns, primarydns.c_str(), 16);
                strncpy(iarmData.secondarydns, secondarydns.c_str(), 16);
                iarmData.isSupported = false;

                if (!autoconfig)
                {
                    RFC_ParamData_t param;
                    if (Utils::getRFCConfig("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Network.ManualIPSettings.Enable", param))
                    {
                        if (param.type == WDMP_BOOLEAN && (strncasecmp(param.value,"true",4) == 0))
                        {
                            iarmData.isSupported  = true;
                        }
                    }
                    if (false == iarmData.isSupported)
                    {
                        LOGWARN("Manual IP Settings not Enabled..\n");
                        response["supported"] = iarmData.isSupported;
                        result = false;
                        returnResponse(result)
                    }
                    bool mask_validation;
                    mask_validation = isValidCIDRv4(netmask.c_str());
                    if (false == mask_validation)
                    {
                       LOGWARN("Netmask is not valid ..\n");
                       response["supported"] = iarmData.isSupported;
                       result = false;
                       returnResponse(result)
                    }

                    if (inet_pton(AF_INET, ipaddr.c_str(), &ip_address) == 1 &&
                        inet_pton(AF_INET, netmask.c_str(), &mask) == 1 &&
                        inet_pton(AF_INET, gateway.c_str(), &gateway_address) == 1)
                    {
                        broadcast_addr1.s_addr = ip_address.s_addr | ~mask.s_addr;
                        broadcast_addr2.s_addr = gateway_address.s_addr | ~mask.s_addr;

                        if (ip_address.s_addr == gateway_address.s_addr)
                        {
                            LOGWARN("Interface and Gateway IP are same , return false \n");
                            response["supported"] = iarmData.isSupported;
                            result = false;
                            returnResponse(result)
                         }
                         if (broadcast_addr1.s_addr != broadcast_addr2.s_addr)
                         {
                             LOGWARN("Interface and Gateway IP is not in same broadcast domain, return false \n");
                             response["supported"] = iarmData.isSupported;
                             result = false;
                             returnResponse(result)
                          }
                          if (ip_address.s_addr == broadcast_addr1.s_addr)
                          {
                              LOGWARN("Interface and Broadcast IP is same, return false \n");
                              response["supported"] = iarmData.isSupported;
                              result = false;
                              returnResponse(result)
                          }
                          if (gateway_address.s_addr == broadcast_addr2.s_addr)
                          {
                              LOGWARN("Gateway and Broadcast IP is same, return false \n");
                              response["supported"] = iarmData.isSupported;
                              result = false;
                              returnResponse(result)
                          }
                     }
                 }
                 if (IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_setIPSettings, (void *) &iarmData, sizeof(iarmData)))
                 {
                     response["supported"] = iarmData.isSupported;
                     result = true;
                 }
                 else
                     response["supported"] = iarmData.isSupported;
            }

            returnResponse(result)
        }

        uint32_t Network::getIPSettings(const JsonObject& parameters, JsonObject& response)
        {
            JsonObject internal;
            JsonObject InternalResponse;
            int errCode;
            bool result = false;
            string interface = "";
            string ipversion = "";
            if(m_isPluginReady)
            {
                getDefaultStringParameter("interface", interface,"");
                internal ["interface"] = interface;
                getDefaultStringParameter("ipversion", ipversion,"");
                internal ["ipversion"] = ipversion;

                if (getIPSettingsInternal(internal, InternalResponse, errCode))
                {
                    if (NETWORK_IPADDRESS_ACQUIRED == errCode)
                    {
                        response["interface"] = InternalResponse["interface"];
                        response["ipversion"] = InternalResponse["ipversion"];
                        response["autoconfig"] = InternalResponse["autoconfig"];
                        response["ipaddr"] = InternalResponse["ipaddr"];
                        response["netmask"] = InternalResponse["netmask"];
                        response["gateway"] = InternalResponse["gateway"];
                        response["primarydns"] = InternalResponse["primarydns"];
                        response["secondarydns"] = InternalResponse["secondarydns"];
                        result = true;
                    }
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }

            returnResponse(result)
        }

        uint32_t Network::getIPSettings2(const JsonObject& parameters, JsonObject& response)
        {
            JsonObject internal;
            JsonObject InternalResponse;
            int errCode;
            bool result = false;
            string interface = "";
            string ipversion = "";
            if(m_isPluginReady)
            {
                getDefaultStringParameter("interface", interface, "");
                internal ["interface"] = interface;
                getDefaultStringParameter("ipversion", ipversion, "");
                internal ["ipversion"] = ipversion;

                if (getIPSettingsInternal(internal,InternalResponse,errCode))
                {
                    /*If the device was configured to use autoconfig IP but device does not have valid IP yet Could be the Router does not have DHCP Server running or
                     * the device is in the process of acquiring it from the router),it must return only the autoconfig and the interface name.
                     */
                     if (errCode == NETWORK_IPADDRESS_NOTFOUND)
                     {
                         response["interface"] = InternalResponse["interface"];
                         response["autoconfig"] = InternalResponse["autoconfig"];
                         result = true;
                         returnResponse(result)
                     }
                     if (errCode == NETWORK_IPADDRESS_ACQUIRED)
                     {
                         response["interface"] = InternalResponse["interface"];
                         response["ipversion"] = InternalResponse["ipversion"];
                         std::string sIPVersion = InternalResponse["ipversion"].String();
                         response["autoconfig"] = InternalResponse["autoconfig"];
                         std::string sAutoconfig =  InternalResponse["autoconfig"].String();
                         if (!(strcasecmp(sAutoconfig.c_str(), "true")) && !(strcasecmp(sIPVersion.c_str(), "IPv4")))
                             response["dhcpserver"] = InternalResponse["dhcpserver"];
                         response["ipaddr"] = InternalResponse["ipaddr"];
                         response["netmask"] = InternalResponse["netmask"];
                         response["gateway"] = InternalResponse["gateway"];
                         response["primarydns"] = InternalResponse["primarydns"];
                         string secondarydns = InternalResponse["secondarydns"].String();
                        //If the secondaryDNS was not set , it shouldn't return secondaryDNS in response.
                         if (!secondarydns.empty())
                             response["secondarydns"] = secondarydns;
                        result = true;
                     }
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }

            returnResponse(result)
        }

        bool Network::getIPSettingsInternal(const JsonObject& parameters, JsonObject& response,int errCode)
        {
            string interface = "";
            string ipversion = "";
            bool result = false;

            getStringParameter("interface", interface);
            getStringParameter("ipversion", ipversion);

            IARM_BUS_NetSrvMgr_Iface_Settings_t iarmData = { 0 };
            strncpy(iarmData.interface, interface.c_str(), 16);
            strncpy(iarmData.ipversion, ipversion.c_str(), 16);
            iarmData.isSupported = true;
            if (IARM_RESULT_SUCCESS == IARM_Bus_Call (IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_getIPSettings, (void *)&iarmData, sizeof(iarmData)))
            {
                response["interface"] = string(iarmData.interface);
                response["ipversion"] = string(iarmData.ipversion);
                response["autoconfig"] = iarmData.autoconfig;
                response["ipaddr"] = string(iarmData.ipaddress,MAX_IP_ADDRESS_LEN - 1);
                response["netmask"] = string(iarmData.netmask,MAX_IP_ADDRESS_LEN - 1);
                response["dhcpserver"] = string(iarmData.dhcpserver,MAX_IP_ADDRESS_LEN - 1);
                response["gateway"] = string(iarmData.gateway,MAX_IP_ADDRESS_LEN - 1);
                response["primarydns"] = string(iarmData.primarydns,MAX_IP_ADDRESS_LEN - 1);
                response["secondarydns"] = string(iarmData.secondarydns,MAX_IP_ADDRESS_LEN - 1);
                errCode = iarmData.errCode;
                result = true;
            }
            return result;
        }

        uint32_t Network::isConnectedToInternet (const JsonObject &parameters, JsonObject &response)
        {
            bool result = false;
            bool isconnected = false;

            if(m_isPluginReady)
            {
                if (IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_isConnectedToInternet, (void*) &isconnected, sizeof(isconnected)))
                {
                    LOGINFO("%s :: isconnected = %d \n",__FUNCTION__,isconnected);
                    response["connectedToInternet"] = isconnected;
                    result = true;
                }
                else
                {
                    LOGWARN("Call to %s for %s failed\n", IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_isConnectedToInternet);
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }
            returnResponse(result);
        }

        uint32_t Network::setConnectivityTestEndpoints (const JsonObject &parameters, JsonObject &response)
        {
            bool result = false;
            JsonArray endpoints = parameters["endpoints"].Array();
            if(m_isPluginReady)
            {
                if (0 == endpoints.Length() || MAX_ENDPOINTS < endpoints.Length())
                {
                    LOGWARN("1 to %d TestUrls are allowed", MAX_ENDPOINTS);
                    returnResponse(result);
                }
                IARM_BUS_NetSrvMgr_Iface_TestEndpoints_t iarmData;
                JsonArray::Iterator index(endpoints.Elements());
                iarmData.size = 0;
                while (index.Next() == true)
                {
                    if (Core::JSON::Variant::type::STRING == index.Current().Content())
                    {
                        strncpy(iarmData.endpoints[iarmData.size], index.Current().String().c_str(), MAX_ENDPOINT_SIZE);
                        iarmData.size++;
                    }
                    else
                    {
                        LOGWARN("Unexpected variant type");
                        returnResponse(result);
                    }
                }
                if (IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_setConnectivityTestEndpoints, (void*) &iarmData, sizeof(iarmData)))
                {
                    result = true;
                }
                else
                {
                    LOGWARN("Call to %s for %s failed\n", IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_setConnectivityTestEndpoints);
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }

            returnResponse(result);
        }

        uint32_t Network::getPublicIP(const JsonObject& parameters, JsonObject& response)
        {
            JsonObject internal;
            internal ["server"] = m_stunEndPoint;
            internal ["port"] = m_stunPort;
            internal ["timeout"] = m_stunBindTimeout;
            internal ["cache_timeout"] = m_stunCacheTimeout;
            internal ["sync"] = m_stunSync;

            if (parameters.HasLabel("iface"))
                internal ["iface"] = parameters["iface"];

            if (parameters.HasLabel("ipv6"))
                internal ["ipv6"] = parameters["ipv6"];

            return  getPublicIPInternal(internal, response);
        }

        uint32_t Network::setStunEndPoint(const JsonObject& parameters, JsonObject& response)
        {
            getDefaultStringParameter("server", m_stunEndPoint, "stun.l.google.com");
            getDefaultNumberParameter("port", m_stunPort, 19302);
            getDefaultBoolParameter("sync", m_stunSync, true);
            getDefaultNumberParameter("timeout", m_stunBindTimeout, 30);
            getDefaultNumberParameter("cache_timeout", m_stunCacheTimeout, 0);

            returnResponse(true);
    }

        uint32_t Network::getPublicIPInternal(const JsonObject& parameters, JsonObject& response)
        {
            bool result = false;

            IARM_BUS_NetSrvMgr_Iface_StunRequest_t iarmData = { 0 };
            string server, iface;

            if(m_isPluginReady)
            {
                getStringParameter("server", server);
                if (server.length() > MAX_HOST_NAME_LEN - 1)
                {
                    LOGWARN("invalid args: server exceeds max length of %u", MAX_HOST_NAME_LEN);
                    returnResponse(result)
                }

                getNumberParameter("port", iarmData.port);

                /*only makes sense to get both server and port or neither*/
                if (!server.empty() && !iarmData.port)
                {
                    LOGWARN("invalid args: port missing");
                    returnResponse(result)
                } 
                if (iarmData.port && server.empty())
                {
                    LOGWARN("invalid args: server missing");
                    returnResponse(result)
                }

                getDefaultStringParameter("iface", iface, "");
                if (iface.length() > 16 - 1)
                {
                    LOGWARN("invalid args: interface exceeds max length of 16");
                    returnResponse(result)
                }
            
                getBoolParameter("ipv6", iarmData.ipv6);
                getBoolParameter("sync", iarmData.sync);
                getNumberParameter("timeout", iarmData.bind_timeout);
                getNumberParameter("cache_timeout", iarmData.cache_timeout);

                strncpy(iarmData.server, server.c_str(), MAX_HOST_NAME_LEN);
                strncpy(iarmData.interface, iface.c_str(), 16);

                iarmData.public_ip[0] = '\0';

                LOGWARN("getPublicIP called with server=%s port=%u iface=%s ipv6=%u sync=%u timeout=%u cache_timeout=%u\n", 
                        iarmData.server, iarmData.port, iarmData.interface, iarmData.ipv6, iarmData.sync, iarmData.bind_timeout, iarmData.cache_timeout);

                if (IARM_RESULT_SUCCESS == IARM_Bus_Call (IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_getPublicIP, (void *)&iarmData, sizeof(iarmData)))
                {
                    response["public_ip"] = string(iarmData.public_ip);
                    result = true;
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }

            response["success"] = result;
            return (Core::ERROR_NONE);
        }

        /*
         * Notifications
         */

        void Network::onInterfaceEnabledStatusChanged(string interface, bool enabled)
        {
            JsonObject params;
            params["interface"] = m_netUtils.getInterfaceDescription(interface);
            params["enabled"] = enabled;
            sendNotify("onInterfaceStatusChanged", params);
        }

        void Network::onInterfaceConnectionStatusChanged(string interface, bool connected)
        {
            JsonObject params;
            params["interface"] = m_netUtils.getInterfaceDescription(interface);
            params["status"] = string (connected ? "CONNECTED" : "DISCONNECTED");
            sendNotify("onConnectionStatusChanged", params);
        }

        void Network::onInterfaceIPAddressChanged(string interface, string ipv6Addr, string ipv4Addr, bool acquired)
        {
            JsonObject params;
            params["interface"] = m_netUtils.getInterfaceDescription(interface);
            if (ipv6Addr != "")
            {
                params["ip6Address"] = ipv6Addr;
            }
            if (ipv4Addr != "")
            {
                params["ip4Address"] = ipv4Addr;
            }
            params["status"] = string (acquired ? "ACQUIRED" : "LOST");
            sendNotify("onIPAddressStatusChanged", params);
        }

        void Network::onDefaultInterfaceChanged(string oldInterface, string newInterface)
        {
            JsonObject params;
            params["oldInterfaceName"] = m_netUtils.getInterfaceDescription(oldInterface);
            params["newInterfaceName"] = m_netUtils.getInterfaceDescription(newInterface);
            sendNotify("onDefaultInterfaceChanged", params);
        }

        void Network::eventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
        {
            if (Network::_instance)
                Network::_instance->iarmEventHandler(owner, eventId, data, len);
            else
                LOGWARN("WARNING - cannot handle IARM events without a Network plugin instance!");
        }

        void Network::iarmEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
        {
            if (strcmp(owner, IARM_BUS_NM_SRV_MGR_NAME) != 0)
            {
                LOGERR("ERROR - unexpected event: owner %s, eventId: %d, data: %p, size: %ld.", owner, (int)eventId, data, len);
                return;
            }
            if (data == nullptr || len == 0)
            {
                LOGERR("ERROR - event with NO DATA: eventId: %d, data: %p, size: %ld.", (int)eventId, data, len);
                return;
            }

            switch (eventId)
            {
            case IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_ENABLED_STATUS:
            {
                IARM_BUS_NetSrvMgr_Iface_EventInterfaceEnabledStatus_t *e = (IARM_BUS_NetSrvMgr_Iface_EventInterfaceEnabledStatus_t*) data;
#ifdef NET_DEFINED_INTERFACES_ONLY
                if (m_netUtils.getInterfaceDescription(e->interface) == "")
                    break;
#endif
                onInterfaceEnabledStatusChanged(e->interface, e->status);
                break;
            }
            case IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS:
            {
                IARM_BUS_NetSrvMgr_Iface_EventInterfaceConnectionStatus_t *e = (IARM_BUS_NetSrvMgr_Iface_EventInterfaceConnectionStatus_t*) data;
#ifdef NET_DEFINED_INTERFACES_ONLY
                if (m_netUtils.getInterfaceDescription(e->interface) == "")
                    break;
#endif
                onInterfaceConnectionStatusChanged(e->interface, e->status);
                break;
            }
            case IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS:
            {
                IARM_BUS_NetSrvMgr_Iface_EventInterfaceIPAddress_t *e = (IARM_BUS_NetSrvMgr_Iface_EventInterfaceIPAddress_t*) data;
#ifdef NET_DEFINED_INTERFACES_ONLY
                if (m_netUtils.getInterfaceDescription(e->interface) == "")
                    break;
#endif
                if (e->is_ipv6)
                {
#ifdef NET_NO_LINK_LOCAL_ANNOUNCE
                    if (!m_netUtils.isIPV6LinkLocal(e->ip_address))
#endif
                        onInterfaceIPAddressChanged(e->interface, e->ip_address, "", e->acquired);
                }
                else
                {
#ifdef NET_NO_LINK_LOCAL_ANNOUNCE
                    if (!m_netUtils.isIPV4LinkLocal(e->ip_address))
#endif
                        onInterfaceIPAddressChanged(e->interface, "", e->ip_address, e->acquired);
                }
                break;
            }
            case IARM_BUS_NETWORK_MANAGER_EVENT_DEFAULT_INTERFACE:
            {
                IARM_BUS_NetSrvMgr_Iface_EventDefaultInterface_t *e = (IARM_BUS_NetSrvMgr_Iface_EventDefaultInterface_t*) data;
                onDefaultInterfaceChanged(e->oldInterface, e->newInterface);
                break;
            }
            }
        }

        /*
         * Internal functions
         */

        bool Network::_getDefaultInterface(string& interface, string& gateway)
        {
            bool result = false;

            if(m_isPluginReady)
            {
                if (m_isHybridDevice == "hybrid")
                {
                    LOGINFO("Identified as hybrid device type");
                    if (m_defaultInterface.length() == 0)
                    {
                        const char * script2 = R"(ip -6 route | grep ^default | tr -d "\n")";

                        string res = Utils::cRunScript(script2).substr();
                        LOGWARN("script2 '%s' result: '%s'", script2, res.c_str());

                        size_t pos = res.find("via");
                        if (pos != string::npos)
                        {
                            m_gatewayInterface  = res.substr(pos + 3);
                            pos = m_gatewayInterface.find("dev");
                            m_gatewayInterface = pos != string::npos ? m_gatewayInterface.substr(0, pos) : "";
                        }

                        pos = res.find("dev");
                        if (pos != string::npos)
                        {
                            m_defaultInterface  = res.substr(pos + 3);
                            pos = m_defaultInterface .find("metric");
                            m_defaultInterface  = pos != string::npos ? m_defaultInterface .substr(0, pos) : "";
                        }

                        if (m_defaultInterface.length() == 0)
                        {
                            const char * script3 = R"(route -n | grep 'UG[ \\t]' | tr -d "\n")";
                            string res = Utils::cRunScript(script3).substr();
                            LOGWARN("script3 '%s' result: '%s'", script3, res.c_str());

                            pos = res.find(" ");
                            if (pos != string::npos)
                            {
                                m_gatewayInterface = res.substr(pos + 3);
                                Utils::String::trim(m_gatewayInterface);
                                pos = m_gatewayInterface.find(" ");
                                m_gatewayInterface = pos != string::npos ? m_gatewayInterface.substr(0, pos) : "";
                            }

                            pos = res.find_last_of(" ");
                            if (pos != string::npos)
                                m_defaultInterface = res.substr(pos);
                        }

                        Utils::String::trim(m_gatewayInterface);
                        Utils::String::trim(m_defaultInterface);

                    }

                    interface = m_defaultInterface;
                    gateway = m_gatewayInterface;

                    if (interface.length() > 0)
                        result = true;
                }
                else
                {
                    LOGINFO("Identified as mediaclient device type");

                    IARM_BUS_NetSrvMgr_DefaultRoute_t defaultRoute = {0};
                    if (IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_getDefaultInterface
                                , (void*)&defaultRoute, sizeof(defaultRoute)))
                    {
                        LOGWARN ("Call to %s for %s returned interface = %s, gateway = %s", IARM_BUS_NM_SRV_MGR_NAME
                                , IARM_BUS_NETSRVMGR_API_getDefaultInterface, defaultRoute.interface, defaultRoute.gateway);
                        interface = defaultRoute.interface;
                        gateway = defaultRoute.gateway;
                        result = true;
                    }
                    else
                        LOGWARN ("Call to %s for %s failed", IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_getDefaultInterface);
                }

                if (interface.length() == 0)
                {
                    LOGWARN("Unable to detect default network interface");
                }
                else
                {
                    LOGWARN("Evaluated default network interface: '%s' and gateway: '%s'", interface.c_str(), gateway.c_str());
                }
            }
            else
            {
                LOGWARN ("Network plugin not initialised yet returning from %s", __FUNCTION__);
            }

            return result;
        }

    } // namespace Plugin
} // namespace WPEFramework
