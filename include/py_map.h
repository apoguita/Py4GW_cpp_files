#pragma once
#include "Headers.h"

namespace py = pybind11;


// Renamed InstanceTypeClass -> InstanceType
class InstanceType {
private:
    GW::Constants::InstanceType instance_type;

public:
    InstanceType(GW::Constants::InstanceType type);
    InstanceType& operator=(GW::Constants::InstanceType type);
    void Set(GW::Constants::InstanceType type);
    GW::Constants::InstanceType Get() const;
    bool operator==(const InstanceType& other) const { return instance_type == other.instance_type; }
    bool operator!=(const InstanceType& other) const { return instance_type != other.instance_type; }
    int ToInt() const;
    std::string GetName() const;
};

class MapID {
private:
    GW::Constants::MapID id;

public:
    MapID(GW::Constants::MapID id);
    MapID(int id);
    void Set(GW::Constants::MapID new_id);
    GW::Constants::MapID Get() const;
    bool MapID::operator==(const MapID& other) const {return id == other.id;}
    bool MapID::operator!=(const MapID& other) const {return id != other.id;}
    int ToInt() const;
    std::string GetName() const;
    std::vector<int> GetOutpostIDs();
    std::vector<std::string> GetOutpostNames();
};





class ServerRegion {
private:
    SafeServerRegion safe_region;

    // Helper methods to convert between SafeServerRegion and int
    SafeServerRegion ToSafeRegion(int region) const;
    int ToServerRegion(SafeServerRegion region) const;

public:
    // Constructor
    ServerRegion(int region);

    // Setter and Getter
    void Set(int region);
    SafeServerRegion Get() const;

    bool ServerRegion::operator==(int region) const { return safe_region == ToSafeRegion(region); }

    bool ServerRegion::operator!=(int region) const { return safe_region != ToSafeRegion(region); }

    // Conversion methods
    int ToInt() const;
    std::string GetName() const;
};



class Language {
private:
    SafeLanguage language;

    // Helper methods to convert between SafeLanguage and int
    SafeLanguage ToSafeLanguage(int lang) const;
    int ToServerLanguage(SafeLanguage safe_lang) const;

public:
    // Constructor
    Language(int lang);

    // Setter and Getter
    void Set(int lang);
    SafeLanguage Get() const;

    bool Language::operator==(int lang) const { return language == ToSafeLanguage(lang); }
    bool Language::operator!=(int lang) const { return language != ToSafeLanguage(lang); }

    // Conversion methods
    int ToInt() const;
    std::string GetName() const;
};

class CampaignWrapper {
private:
    SafeCampaign safe_campaign_value;

    // Helper methods to convert between SafeCampaign and int
    SafeCampaign ToSafeCampaign(int campaign) const;
    int ToCampaignValue(SafeCampaign safe_campaign) const;

public:
    // Constructor
    CampaignWrapper(int campaign);

    // Setter and Getter
    void Set(int campaign);
    SafeCampaign Get() const;

    bool operator==(int campaign) const { return safe_campaign_value == ToSafeCampaign(campaign); }
    bool operator!=(int campaign) const { return safe_campaign_value != ToSafeCampaign(campaign); }

    // Conversion methods
    int ToInt() const;
    std::string GetName() const;
};



class RegionType {
private:
    SafeRegionType safe_region_type_value;

    // Helper methods to convert between SafeRegionType and int
    SafeRegionType ToSafeRegionType(int region_type) const;
    int ToRegionTypeValue(SafeRegionType safe_region_type) const;

public:
    // Constructor
    RegionType(int region_type);

    // Setter and Getter
    void Set(int region_type);
    SafeRegionType Get() const;

    // Operators
    bool operator==(int region_type) const;
    bool operator!=(int region_type) const;

    // Conversion methods
    int ToInt() const;
    std::string GetName() const;
};



class Continent {
private:
    SafeContinent safe_continent_value;

    // Helper methods to convert between SafeContinent and uint32_t
    SafeContinent ToSafeContinent(uint32_t continent) const;
    uint32_t ToContinentValue(SafeContinent safe_continent) const;

public:
    // Constructor
    Continent(int continent);

    // Setter and Getter
    void Set(int continent);
    SafeContinent Get() const;

    // Operators
    bool operator==(int continent) const;
    bool operator!=(int continent) const;

    // Conversion methods
    int ToInt() const;
    std::string GetName() const;
};


class PyMap {
public:
    InstanceType instance_type = GW::Constants::InstanceType::Loading;
    bool is_map_ready = false;
    uint32_t instance_time = 0;

    MapID map_id = GW::Constants::MapID::None;
    ServerRegion server_region = 255; // static_cast<int>(GW::Constants::ServerRegion::Unknown);
    int district = 0;
    Language language = 255;  //GW::Constants::Language::Unknown;

    uint32_t foes_killed = 0;
    uint32_t foes_to_kill = 0;
    bool is_in_cinematic = false;

    CampaignWrapper campaign = 0; // GW::Constants::Campaign::Core;
    Continent continent = 6; // GW::Continent::DevContinent;
    RegionType region_type = 20; // GW::RegionType::Unknown;
    uint32_t max_party_size = 0;
    bool has_enter_button = false;
    bool is_on_world_map = false;
    bool is_pvp = false;
    bool is_guild_hall = false;
    bool is_vanquishable_area = false;
    int amount_of_players_in_instance = 0;

    PyMap();
    void GetContext();
    bool Travel(int map_id, int district, int district_number);
    bool Travel(int map_id);
	bool Travel(int map_id, int server_region, int district_number, int language);
	bool TravelGH();
	bool LeaveGH();

    GW::Constants::ServerRegion RegionFromDistrict(GW::Constants::District _district);
    GW::Constants::Language LanguageFromDistrict(const GW::Constants::District _district);
    bool GetIsMapUnlocked(int map_id);
    bool SkipCinematic();
    bool EnterChallenge();
    bool CancelEnterChallenge();
};







