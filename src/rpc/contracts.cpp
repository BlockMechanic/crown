
#include <pubkey.h>
#include <core_io.h>
#include <key_io.h>
#include <logging.h>
#include <chainiddb.h>
#include <contractdb.h>

#include <rpc/rawtransaction_util.h>
#include <rpc/util.h>
#include <rpc/server.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/standard.h>

#include <crypto/sha1.h>

#include <wallet/coincontrol.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>
#include <util/translation.h>
#include <univalue.h>
#include <validation.h>
#include <boost/algorithm/string.hpp>

typedef std::vector<unsigned char> valtype;

static RPCHelpMan hashmessage()
{
    return RPCHelpMan{"hashmessage",
                "\nSign a message with the private key of an address\n",
                {
                    {"hashtype", RPCArg::Type::STR, RPCArg::Optional::NO, "The type of hash required. sha256, hash256, hash160, ripemd160, sha1."},
                    {"message", RPCArg::Type::STR, RPCArg::Optional::NO, "The message to hash."},
                },
                RPCResult{
                    RPCResult::Type::STR, "signature", "The signature of the message encoded in base 64"
                },
                RPCExamples{
            "\nCreate the signature\n"
            + HelpExampleCli("hashmessage", "\"hash type\" \"my message\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("hashmessage", "\"hash type\", \"my message\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::string hashtype = request.params[0].get_str();
    std::string svch = request.params[1].get_str();
    valtype vch(svch.begin(), svch.end());

    valtype vchHash((hashtype == "ripemd160" || hashtype == "sha1" || hashtype == "hash160") ? 20 : 32);
    if (hashtype == "ripemd160")
        CRIPEMD160().Write(vch.data(), vch.size()).Finalize(vchHash.data());
    else if (hashtype == "sha1")
        CSHA1().Write(vch.data(), vch.size()).Finalize(vchHash.data());
    else if (hashtype == "sha256")
        CSHA256().Write(vch.data(), vch.size()).Finalize(vchHash.data());
    else if (hashtype == "hash160")
        CHash160().Write(vch).Finalize(vchHash);
    else if (hashtype =="hash256")
        CHash256().Write(vch).Finalize(vchHash);

    return HexStr(vchHash);
},
    };
}

static RPCHelpMan getid()
{
    return RPCHelpMan{"getid",
                "\n Get Details of a chainID \n",
                {
                    {"Chain ID", RPCArg::Type::STR, RPCArg::Optional::NO, "The ID to retrieve."},
                },
                RPCResult{
                    RPCResult::Type::STR, "details", "The details of the ID requested"
                },
                RPCExamples{
            "\nCreate the signature\n"
            + HelpExampleCli("getid", "\"ChainID\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getid", "\"ChainID\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::string id = request.params[0].get_str();
    CChainID chainID = GetID(id);
    UniValue result(UniValue::VOBJ);

    ChainIDToUniv(&chainID,result);

    return result;
},
    };
}

static RPCHelpMan getcontract()
{
    return RPCHelpMan{"getcontract",
                "\n  Get Details of a chainID \n",
                {
                    {"contract", RPCArg::Type::STR, RPCArg::Optional::NO, "The contract to retrieve"},
                },
                RPCResult{
                    RPCResult::Type::STR, "details", "The contract "
                },
                RPCExamples{
            "\nRetrieve a Contract\n"
            + HelpExampleCli("getcontract", "\"Contract Name\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getcontract", "\"Contract Name\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::string contractstring = request.params[0].get_str();
    CContract contract = GetContract(contractstring);
    UniValue result(UniValue::VOBJ);

    ContractToUniv(&contract,result);

    return result;
},
    };
}

static RPCHelpMan registerchainid()
{
    return RPCHelpMan{"registerchainid",
                "\n Register a new ChainID on the network\n",
                {
                    {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The address to register"},
                    {"alias", RPCArg::Type::STR, RPCArg::Optional::NO, "The alias of this ID."},
                    {"email", RPCArg::Type::STR, RPCArg::Optional::NO, "The email address of this ID."},
                },
                RPCResult{
                    RPCResult::Type::STR, "signature", "The signature of the message encoded in base 64"
                },
                RPCExamples{
            "\nCreate the signature\n"
            + HelpExampleCli("registerchainid", "\"hash type\" \"my message\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("registerchainid", "\"hash type\", \"my message\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    if (!wallet) return NullUniValue;
    CWallet* const pwallet = wallet.get();

    std::string strAddress = request.params[0].get_str();
    std::string alias = request.params[1].get_str();
    std::string email = request.params[2].get_str();

    CTransactionRef tx;
    std::string strFailReason;
    CChainID chainID;
    
    if(!pwallet->CreateID(chainID, tx, strAddress, alias, email, strFailReason))
        throw JSONRPCError(RPC_MISC_ERROR, strFailReason);

    UniValue result(UniValue::VOBJ);

    CDataStream ssID(SER_NETWORK, PROTOCOL_VERSION | RPCSerializationFlags());
    ssID << chainID;

    result.pushKV("txid", tx->GetHash().GetHex());
    result.pushKV("hex", HexStr(ssID));
    return result;
},
    };
}

static RPCHelpMan createcontract()
{
    return RPCHelpMan{"createcontract",
                "\n Create a smart contract \n",
                {
                    {"ChainID", RPCArg::Type::STR, RPCArg::Optional::NO, "ChainID to create contract with"},
                    {"name", RPCArg::Type::STR, RPCArg::Optional::NO, "Max 10 characters"},
                    {"short_name", RPCArg::Type::STR, RPCArg::Optional::NO, "Max 4 characters"},
					{"contract_url", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "contract location online"},
					{"website_url", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Issuer website online"},
					{"description", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "contract decription"},
					{"scriptcode", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED, "contract script in hex"},
                },
                RPCResult{
                    RPCResult::Type::STR, "signature", "The signature of the message encoded in base 64"
                },
                RPCExamples{
            "\nCreate the contract\n"
            + HelpExampleCli("createcontract", "\"hash type\" \"my message\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("createcontract", "\"hash type\", \"my message\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    if (!wallet) return NullUniValue;
    CWallet* const pwallet = wallet.get();

    CChainID chainID;

    if (request.params[0].isStr()) {
		
		chainID = GetID(request.params[0].get_str());

		if(chainID == CChainID())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "invalid chainID");
    }

    std::string name = request.params[1].get_str();
    std::string shortname = request.params[2].get_str();
    std::string contract_url = request.params[3].get_str();
    std::string website_url = request.params[4].get_str();
    std::string description = request.params[5].get_str();
    std::string scriptcode = request.params[6].get_str();

    std::vector<unsigned char> scriptcodedata(ParseHex(scriptcode));
    CScript script(scriptcodedata.begin(), scriptcodedata.end());

    CContract contract;
    CTransactionRef tx;
    std::string strFailReason;
    if(!pwallet->CreateContract(contract, tx, chainID, contract_url, website_url, description, script, name, shortname, strFailReason))
        throw JSONRPCError(RPC_MISC_ERROR, strFailReason);

    CDataStream ssCt(SER_NETWORK, PROTOCOL_VERSION | RPCSerializationFlags());
    ssCt << contract;

    UniValue result(UniValue::VOBJ);

    result.pushKV("txid", tx->GetHash().GetHex());
    result.pushKV("hex", HexStr(ssCt));
    return result;
},
    };
}

static RPCHelpMan createasset()
{
    return RPCHelpMan{"createasset",
                "\ncreate a new asset\n",
                {
                    {"name", RPCArg::Type::STR, RPCArg::Optional::NO, "Max 10 characters"},
                    {"short_name", RPCArg::Type::STR, RPCArg::Optional::NO, "Max 4 characters"},
                    {"input_amount", RPCArg::Type::AMOUNT, RPCArg::Optional::NO, "In put amount in Crown. (minimum 1)"},
                    {"asset_amount", RPCArg::Type::AMOUNT, RPCArg::Optional::NO, "Amount of asset to generate. Note that the amount is Crown-like, with 8 decimal places."},
                    {"expiry", RPCArg::Type::STR, RPCArg::Optional::NO, "Expiry date of asset"},
                    {"type", RPCArg::Type::NUM, RPCArg::Optional::NO, "asset type TOKEN = 1, UNIQUE = 2, EQUITY = 3, POINTS = 4, CREDITS = 5"},
                    {"transferable", RPCArg::Type::BOOL, RPCArg::Optional::NO, "asset can be transfered to other addresses after initial creation"},
                    {"convertable", RPCArg::Type::BOOL, RPCArg::Optional::NO, "asset can be converted to another asset (set false for NFTs)"},
                    {"restricted", RPCArg::Type::BOOL, RPCArg::Optional::NO, "asset can only be issued/reissued by creation address"},
                    {"limited", RPCArg::Type::BOOL, RPCArg::Optional::NO, "other assets cannot be converted to this one"},
                    {"contract", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Address/ChainID to issue asset to"},
                },
                RPCResult{
                    RPCResult::Type::STR, "signature", "The signature of the message encoded in base 64"
                },
                RPCExamples{
            "\nCreate an asset\n"
            + HelpExampleCli("createasset", "\"hash type\" \"my message\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("createasset", "\"hash type\", \"my message\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    if (!wallet) return NullUniValue;
    CWallet* const pwallet = wallet.get();

    LegacyScriptPubKeyMan& spk_man = EnsureLegacyScriptPubKeyMan(*wallet);

    LOCK2(pwallet->cs_wallet, spk_man.cs_KeyStore);

    EnsureWalletIsUnlocked(pwallet);

    std::string name = request.params[0].get_str();
    std::string shortname = request.params[1].get_str();
    CAmount nAmount = AmountFromValue(request.params[2]);

    if(nAmount < 10*COIN)
        throw JSONRPCError(RPC_MISC_ERROR, "Input error, input amount must be greater than 10 Crown for asset creation");

    CAmount nAssetAmount = AmountFromValue(request.params[3]);
    int64_t expiry = request.params[4].get_int();
    int type = request.params[5].get_int();
    bool transferable = request.params[6].get_bool();
    bool convertable = request.params[7].get_bool();
    bool restricted = request.params[8].get_bool();
    bool limited = request.params[9].get_bool();
    std::string contractstring = request.params[10].get_str();

    std::vector<unsigned char> contractData(ParseHex(contractstring));

    CContract contract;

    CDataStream ssCt(contractData, SER_NETWORK, PROTOCOL_VERSION);
    ssCt >> contract;

    CAsset asset;
    CTransactionRef tx;
    std::string strFailReason;

    if(!pwallet->CreateAsset(asset, tx, name, shortname, nAmount, nAssetAmount, expiry, type, contract, strFailReason, transferable, convertable, restricted, limited))
        throw JSONRPCError(RPC_MISC_ERROR, strFailReason);

    return tx->GetHash().GetHex();
},
    };
}


void RegisterContractRPCCommands(CRPCTable &t)
{
// clang-format off
static const CRPCCommand commands[] =
{ //  category              actor (function)
  //  --------------------- ------------------------
    { "contracts",  "hashmessage",         &hashmessage,            {}    },
    { "contracts",  "registerchainid",     &registerchainid,        {}    },
    { "contracts",  "createcontract",      &createcontract,         {}    },
    { "contracts",  "createasset",         &createasset,            {}    },
    { "contracts",  "getid",               &getid,                  {}    },
    { "contracts",  "getcontract",         &getcontract,            {}    },

};

// clang-format on
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}