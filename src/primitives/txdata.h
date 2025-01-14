#ifndef CROWN_PRIMITIVES_TXDATA_H
#define CROWN_PRIMITIVES_TXDATA_H

#include <script/script.h>

#include <serialize.h>
#include <uint256.h>
#include <pubkey.h>
#include <amount.h>

class CContract;
class CTxData;

enum DataTypes
{
    OUTPUT_NULL             = 0, // Marker for CCoinsView
    OUTPUT_DATA             = 1,
    OUTPUT_CONTRACT         = 2,
    OUTPUT_ID               = 3,
    OUTPUT_VOTE             = 4,
};

class CTxDataBase
{
public:
    explicit CTxDataBase(uint8_t v) : nVersion(v) {};
    explicit CTxDataBase() {};
    virtual ~CTxDataBase() {};
    uint8_t nVersion;

    SERIALIZE_METHODS(CTxDataBase, obj)
    {
        READWRITE(obj.nVersion);
        switch (obj.nVersion) {
            case OUTPUT_DATA:
                READWRITE(*(CTxData*) &obj);
                break;
            case OUTPUT_CONTRACT:
                READWRITE(*(CContract*) &obj);
                break;
            default:
                assert(false);
        }
    }

    uint8_t GetVersion() const
    {
        return nVersion;
    }

    bool IsVersion(uint8_t _nVersion) const
    {
        return nVersion == _nVersion;
    }

    virtual std::vector<uint8_t> *GetPData() { return nullptr; };
    virtual const std::vector<uint8_t> *GetPData() const { return nullptr; };

    virtual void SetEmpty() {};

    virtual bool IsEmpty() const { return false; };

    uint256 GetHashWithoutSign() const;

    uint256 GetHash() const;

    std::string ToString() const;
};

#define OUTPUT_PTR std::shared_ptr
typedef OUTPUT_PTR<CTxDataBase> CTxDataBaseRef;
#define MAKE_OUTPUT std::make_shared

class CContract : public CTxDataBase
{
public:
    CContract() : CTxDataBase(OUTPUT_CONTRACT) {};
    CContract (std::string alias);

    std::string contract_url; //online human readable contract
    std::string asset_symbol;
    std::string asset_name;
    std::string sIssuingaddress;
    std::string description;
    std::string website_url;
    CScript scriptcode;

    std::vector<unsigned char> vchContractSig; // contract signature

    SERIALIZE_METHODS(CContract, obj) { READWRITE(obj.contract_url, obj.asset_symbol, obj.asset_name, obj.sIssuingaddress, obj.description, obj.website_url, obj.scriptcode, obj.vchContractSig); }

    void SetEmpty() override
    {
        contract_url=""; //online human readable contract
        asset_symbol="";
        asset_name="";
        sIssuingaddress="";
        description="";
        website_url="";
        scriptcode= CScript();
        vchContractSig.clear();
    }

    bool IsEmpty() const override {
        return sIssuingaddress=="" || asset_name=="";
    }

    uint256 GetHashWithoutSign() const;

    uint256 GetHash() const;

    std::string ToString();

};

class CTxData : public CTxDataBase
{
public:
    CTxData() : CTxDataBase(OUTPUT_DATA) {};
    explicit CTxData(const std::vector<uint8_t> &vData_) : CTxDataBase(OUTPUT_DATA), vData(vData_) {};

    uint8_t nType;
    std::vector<uint8_t> vData;

    SERIALIZE_METHODS(CTxData, obj) { READWRITE(obj.nType, obj.vData); }

    std::vector<uint8_t> *GetPData() override
    {
        return &vData;
    }
    const std::vector<uint8_t> *GetPData() const override
    {
        return &vData;
    }

    uint8_t GetType() const
    {
        return nType;
    }

    bool IsType(uint8_t _nType) const
    {
        return nType == _nType;
    }

    void SetEmpty() override
    {
        vData.clear();
    }

    bool IsEmpty() const override
    {
        return vData.empty();
    }

    uint256 GetHash() const;

    std::string ToString() const;
};

#endif
