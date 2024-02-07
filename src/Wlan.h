#pragma once

#include <WString.h>
#include <functional>

// be very careful changing this struct, as it is used for NVS storage and will corrupt existing entries
struct WiFiSettings {
	struct StaticIp {
		static constexpr size_t numFields = 5;

		IPAddress addr {INADDR_NONE};
		IPAddress subnet {INADDR_NONE};
		IPAddress gateway {INADDR_NONE};
		IPAddress dns1 {INADDR_NONE};
		IPAddress dns2 {INADDR_NONE};

		StaticIp() = default;
		StaticIp(const IPAddress &addr, const IPAddress &subnet, const IPAddress &gateway = INADDR_NONE, const IPAddress &dns1 = INADDR_NONE, const IPAddress &dns2 = INADDR_NONE)
			: addr {addr}
			, subnet {subnet}
			, gateway {gateway}
			, dns1 {dns1}
			, dns2 {dns2} {
			if (dns1 == INADDR_NONE && dns2 != INADDR_NONE) {
				// swapp dns1 and dns2 if only dns2 is set
				this->dns1 = this->dns2;
				this->dns2 = INADDR_NONE;
			}
		}

		bool isValid() const { return addr != INADDR_NONE && subnet != INADDR_NONE; }

		void serialize(uint32_t *blob) const {
			blob[0] = addr;
			blob[1] = subnet;
			blob[2] = gateway;
			blob[3] = dns1;
			blob[4] = dns2;
		}
		void deserialize(const uint32_t *blob) {
			addr = blob[0];
			subnet = blob[1];
			gateway = blob[2];
			dns1 = blob[3];
			dns2 = blob[4];
		}
	};

	String ssid {String()};
	String password {String()};
	StaticIp staticIp;

	WiFiSettings() = default;
	WiFiSettings(const std::vector<uint8_t> &buffer) { deserialize(buffer); }

	bool isValid() const { return !ssid.isEmpty() && ssid.length() < 33 && password.length() < 65; }

	/**
	 * @brief Serialize the fields of this instance
	 * @return std::optional<std::vector<uint8_t>> std::vector with the binary data or std::nullopt if the instance is corrupted
	 */
	std::optional<std::vector<uint8_t>> serialize() const;

	/**
	 * @brief Deserialize data from the supplied NVS key
	 * @param buffer The buffer with the binary data to be used
	 */
	void deserialize(const std::vector<uint8_t> &buffer);

protected:
	/**
	 * @brief Serialize a String into a binary buffer. Format: [len:u8] + [len * u8] + '\0'
	 * @param it iterator to the start of the serialization
	 * @param s the string to be serialized
	 */
	void serializeString(std::vector<uint8_t>::iterator &it, const String &s) const;

	/**
	 * @brief Return the needed bytes to serialize the String
	 */
	size_t serializeStringLen(const String &s) const { return 1 + s.length() + 1; }

	/**
	 * @brief Deserialize a String from a binary buffer
	 * @see serializeString for further information
	 */
	void deserializeString(std::vector<uint8_t>::const_iterator &it, String &s);

	/**
	 * @brief Serialize a String into a binary buffer. Format: [bool:u8] + <numFields * u32>
	 * @warning the ip fields are optional any only present if the network has a static ip config
	 * @param it iteratot to the start of the serialization
	 * @param ip the static IP data to be serialized
	 */
	void serializeStaticIp(std::vector<uint8_t>::iterator &it, const StaticIp &ip) const;

	/**
	 * @brief Return the needed bytes to serialize the Static IP config
	 */
	size_t serializeStaticIpLen(const StaticIp &ip) const { return 1 + ((ip.isValid()) ? (StaticIp::numFields * sizeof(uint32_t)) : 0); }

	/**
	 * @brief Deserialize a static IP confgiuration from a binary buffer
	 * @see serializeStaticIp for further information
	 */
	void deserializeStaticIp(std::vector<uint8_t>::const_iterator &it, StaticIp &ip);
};

void Wlan_Init(void);
void Wlan_Cyclic(void);
bool Wlan_AddNetworkSettings(const WiFiSettings &);
uint8_t Wlan_NumSavedNetworks();
void Wlan_GetSavedNetworks(std::function<void(const WiFiSettings &)>);
const String Wlan_GetCurrentSSID();
const String Wlan_GetHostname();
bool Wlan_DeleteNetwork(String);
bool Wlan_ValidateHostname(String);
bool Wlan_SetHostname(String);
bool Wlan_IsConnected(void);
void Wlan_ToggleEnable(void);
String Wlan_GetIpAddress(void);
int8_t Wlan_GetRssi(void);
bool Wlan_ConnectionTryInProgress(void);
