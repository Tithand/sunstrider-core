#ifndef _AUCTION_HOUSE_MGR_H
#define _AUCTION_HOUSE_MGR_H

class Item;
class Player;
class WorldPacket;

#define MIN_AUCTION_TIME (12*HOUR)
#define MAX_AUCTIONS 240

enum AuctionError
{
    AUCTION_OK = 0,
    AUCTION_INTERNAL_ERROR = 2,
    AUCTION_NOT_ENOUGHT_MONEY = 3,
    AUCTION_ITEM_NOT_FOUND = 4,
    CANNOT_BID_YOUR_AUCTION_ERROR = 10
};

enum AuctionAction
{
    AUCTION_SELL_ITEM = 0,
    AUCTION_CANCEL = 1,
    AUCTION_PLACE_BID = 2
};

struct AuctionEntry
{
    uint32 Id;
    ObjectGuid::LowType auctioneer;                                      // creature low guid
    ObjectGuid::LowType item_guidlow;
    uint32 item_template;
    uint32 owner;
    uint32 startbid;                                        //maybe useless
    uint32 bid;
    uint32 buyout;
    time_t expire_time;
    uint32 bidder;
    uint32 deposit;                                         //deposit can be calculated only when creating auction
    AuctionHouseEntry const* auctionHouseEntry;             // in AuctionHouse.dbc
    time_t deposit_time;

    // helpers
    uint32 GetHouseId() const { return auctionHouseEntry->houseId; }
    uint32 GetHouseFaction() const { return auctionHouseEntry->faction; }
    uint32 GetAuctionCut() const;
    uint32 GetAuctionOutBid() const;
    bool BuildAuctionInfo(WorldPacket & data) const;
    void DeleteFromDB(SQLTransaction& trans) const;
    void SaveToDB(SQLTransaction& trans) const;
};

//this class is used as auctionhouse instance
class AuctionHouseObject
{
  public:
    AuctionHouseObject() {}
    ~AuctionHouseObject()
    {
        for (auto & itr : AuctionsMap)
            delete itr.second;
    }

    typedef std::map<uint32, AuctionEntry*> AuctionEntryMap;

    uint32 Getcount() { return AuctionsMap.size(); }

    AuctionEntryMap::iterator GetAuctionsBegin() {return AuctionsMap.begin();}
    AuctionEntryMap::iterator GetAuctionsEnd() {return AuctionsMap.end();}

    void AddAuction(AuctionEntry *ah)
    {
        ASSERT( ah );
        AuctionsMap[ah->Id] = ah;
    }

    AuctionEntry* GetAuction(uint32 id) const
    {
        auto itr = AuctionsMap.find( id );
        return itr != AuctionsMap.end() ? itr->second : nullptr;
    }

    bool RemoveAuction(uint32 id)
    {
        return AuctionsMap.erase(id) ? true : false;
    }
    
    void RemoveAllAuctionsOf(SQLTransaction& trans, ObjectGuid::LowType ownerGUID);

    void Update();

    void BuildListBidderItems(WorldPacket& data, Player* player, uint32& count, uint32& totalcount);
    void BuildListOwnerItems(WorldPacket& data, Player* player, uint32& count, uint32& totalcount);
    uint32 GetAuctionsCount(Player* p);
    void BuildListAuctionItems(WorldPacket& data, Player* player,
        std::wstring const& searchedname, uint32 listfrom, uint32 levelmin, uint32 levelmax, uint32 usable,
        uint32 inventoryType, uint32 itemClass, uint32 itemSubClass, uint32 quality,
        uint32& count, uint32& totalcount);

  private:
    AuctionEntryMap AuctionsMap;
};

class AuctionHouseMgr
{
    private:
        AuctionHouseMgr();
        ~AuctionHouseMgr();
        
    public:
        static AuctionHouseMgr* instance()
        {
            static AuctionHouseMgr instance;
            return &instance;
        }
        
        typedef std::unordered_map<uint32, Item*> ItemMap;

        AuctionHouseObject* GetAuctionsMap( uint32 factionTemplateId );

        Item* GetAItem(uint32 id)
        {
            ItemMap::const_iterator itr = mAitems.find(id);
            if (itr != mAitems.end())
            {
                return itr->second;
            }
            return nullptr;
        }

        //auction messages
        void SendAuctionWonMail( SQLTransaction& trans, AuctionEntry * auction );
        void SendAuctionSalePendingMail(SQLTransaction& trans, AuctionEntry * auction );
        void SendAuctionSuccessfulMail(SQLTransaction& trans, AuctionEntry * auction );
        void SendAuctionExpiredMail(SQLTransaction& trans, AuctionEntry * auction );
        static uint32 GetAuctionDeposit(AuctionHouseEntry const* entry, uint32 time, Item *pItem);
        static AuctionHouseEntry const* GetAuctionHouseEntry(uint32 factionTemplateId);
        void RemoveAllAuctionsOf(SQLTransaction& trans, ObjectGuid::LowType ownerGUID);

    public:
      //load first auction items, because of check if item exists, when loading
      void LoadAuctionItems();
      void LoadAuctions();

      void AddAItem(Item* it);
      bool RemoveAItem(uint32 id);

      void Update();

    private:
      AuctionHouseObject mHordeAuctions;
      AuctionHouseObject mAllianceAuctions;
      AuctionHouseObject mNeutralAuctions;

      ItemMap mAitems;
};

#define sAuctionMgr AuctionHouseMgr::instance()

#endif
