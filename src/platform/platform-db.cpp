// Copyright (c) 2014-2020 Crown Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <platform/platform-utils.h>
#include <platform/platform-db.h>
#include <platform/specialtx.h>
#include <platform/nf-token/nf-token-protocol-reg-tx.h>
#include <platform/nf-token/nf-token-reg-tx.h>

namespace Platform
{
    /*static*/ std::unique_ptr<PlatformDb> PlatformDb::s_instance;
    /*static*/ const char PlatformDb::DB_NFT = 'n';
    /*static*/ const char PlatformDb::DB_NFT_TOTAL = 't';
    /*static*/ const char PlatformDb::DB_NFT_PROTO = 'p';
    /*static*/ const char PlatformDb::DB_NFT_PROTO_TOTAL = 'c';

    PlatformDb::PlatformDb(size_t nCacheSize, PlatformOpt optSetting, bool fMemory, bool fWipe)
    : TransactionLevelDBWrapper("platform", nCacheSize, fMemory, fWipe)
    {
        m_optSetting = optSetting;
    }

    void PlatformDb::ProcessPlatformDbGuts(std::function<bool(const leveldb::Iterator &)> processor)
    {
        std::unique_ptr<leveldb::Iterator> dbIt(m_db.NewIterator());

        for (dbIt->SeekToFirst(); dbIt->Valid(); dbIt->Next())
        {

            if (!processor(*dbIt))
            {
                LogPrintf("%s : Cannot process a platform db record - %s", __func__, dbIt->key().ToString());
                continue;
            }
        }

        HandleError(dbIt->status());
    }

    void PlatformDb::ProcessNftIndexGutsOnly(std::function<bool(NfTokenIndex)> nftIndexHandler)
    {
        std::unique_ptr<leveldb::Iterator> dbIt(m_db.NewIterator());

        for (dbIt->SeekToFirst(); dbIt->Valid(); dbIt->Next())
        {

            if (!ProcessNftIndex(*dbIt, nftIndexHandler))
            {
                LogPrintf("%s : Cannot process a platform db record - %s", __func__, dbIt->key().ToString());
                continue;
            }
        }

        HandleError(dbIt->status());
    }

    void PlatformDb::ProcessNftProtoIndexGutsOnly(std::function<bool(NftProtoIndex)> protoIndexHandler)
    {
        std::unique_ptr<leveldb::Iterator> dbIt(m_db.NewIterator());

        for (dbIt->SeekToFirst(); dbIt->Valid(); dbIt->Next())
        {

            if (!ProcessNftProtoIndex(*dbIt, protoIndexHandler))
            {
                LogPrintf("%s : Cannot process a platform db record - %s", __func__, dbIt->key().ToString());
                continue;
            }
        }

        HandleError(dbIt->status());
    }

    bool PlatformDb::IsNftIndexEmpty()
    {
        std::unique_ptr<leveldb::Iterator> dbIt(m_db.NewIterator());

        for (dbIt->SeekToFirst(); dbIt->Valid(); dbIt->Next())
        {

            if (dbIt->key().starts_with(std::string(1, DB_NFT)))
            {
                HandleError(dbIt->status());
                return false;
            }
        }

        HandleError(dbIt->status());
        return true;
    }

    bool PlatformDb::ProcessNftIndex(const leveldb::Iterator & dbIt, std::function<bool(NfTokenIndex)> nftIndexHandler)
    {
        if (dbIt.key().starts_with(std::string(1, DB_NFT)))
        {
            leveldb::Slice sliceValue = dbIt.value();
            CDataStream streamValue(sliceValue.data(), sliceValue.data() + sliceValue.size(), SER_DISK, CLIENT_VERSION);
            NfTokenDiskIndex nftDiskIndex;

            try
            {
                streamValue >> nftDiskIndex;
            }
            catch (const std::exception & ex)
            {
                LogPrintf("%s : Deserialize or I/O error - %s", __func__, ex.what());
                return false;
            }

            NfTokenIndex nftIndex = NftDiskIndexToNftMemIndex(nftDiskIndex);
            if (nftIndex.IsNull())
            {
                LogPrintf("%s : Cannot build an NFT record, reg tx hash: %s", __func__, nftDiskIndex.RegTxHash().ToString());
                return false;
            }

            if (!nftIndexHandler(std::move(nftIndex)))
            {
                LogPrintf("%s : Cannot process an NFT index, reg tx hash: %s", __func__, nftDiskIndex.RegTxHash().ToString());
                return false;
            }
        }
        return true;
    }

    bool PlatformDb::ProcessNftProtoIndex(const leveldb::Iterator & dbIt, std::function<bool(NftProtoIndex)> protoIndexHandler)
    {
        if (dbIt.key().starts_with(std::string(1, DB_NFT_PROTO)))
        {
            leveldb::Slice sliceValue = dbIt.value();
            CDataStream streamValue(sliceValue.data(), sliceValue.data() + sliceValue.size(), SER_DISK, CLIENT_VERSION);
            NftProtoDiskIndex protoDiskIndex;

            try
            {
                // TODO fix
                streamValue >> protoDiskIndex;
            }
            catch (const std::exception & ex)
            {
                LogPrintf("%s : Deserialize or I/O error - %s", __func__, ex.what());
                return false;
            }

            NftProtoIndex protoIndex = NftProtoDiskIndexToNftProtoMemIndex(protoDiskIndex);
            if (protoIndex.IsNull())
            {
                LogPrintf("%s : Cannot build an NFT proto record, reg tx hash: %s", __func__, protoDiskIndex.RegTxHash().ToString());
                return false;
            }

            if (!protoIndexHandler(std::move(protoIndex)))
            {
                LogPrintf("%s : Cannot process an NFT proto index, reg tx hash: %s", __func__, protoDiskIndex.RegTxHash().ToString());
                return false;
            }
        }
        return true;
    }

    bool PlatformDb::ProcessNftSupply(const leveldb::Iterator & dbIt, std::function<bool(uint64_t, unsigned int)> protoSupplyHandler)
    {
        if (dbIt.key().starts_with(std::string(1, DB_NFT_TOTAL)))
        {
            leveldb::Slice sliceKey = dbIt.key();
            CDataStream streamKey(sliceKey.data(), sliceKey.data() + sliceKey.size(), SER_DISK, CLIENT_VERSION);
            std::pair<char, uint64_t> key;

            leveldb::Slice sliceValue = dbIt.value();
            CDataStream streamValue(sliceValue.data(), sliceValue.data() + sliceValue.size(), SER_DISK, CLIENT_VERSION);
            unsigned int protoSupply = 0;

            try
            {
                streamKey >> key;
		// TODO fix
                streamValue >> protoSupply;
            }
            catch (const std::exception & ex)
            {
                LogPrintf("%s : Deserialize or I/O error - %s", __func__, ex.what());
                return false;
            }

            if (!protoSupplyHandler(key.second, protoSupply))
            {
                LogPrintf("%s : Cannot process protocol supply: %s", __func__, ProtocolName(key.second).ToString());
                return false;
            }
        }
        return true;
    }

    void PlatformDb::WriteNftDiskIndex(const NfTokenDiskIndex & nftDiskIndex)
    {
        this->Write(std::make_tuple(DB_NFT,
              nftDiskIndex.NfTokenPtr()->tokenProtocolId,
              nftDiskIndex.NfTokenPtr()->tokenId),
              nftDiskIndex
              );
    }

    void PlatformDb::EraseNftDiskIndex(const uint64_t &protocolId, const uint256 &tokenId)
    {
        this->Erase(std::make_tuple(DB_NFT, protocolId, tokenId));
    }

    NfTokenIndex PlatformDb::ReadNftIndex(const uint64_t &protocolId, const uint256 &tokenId)
    {
        NfTokenDiskIndex nftDiskIndex;
        if (this->Read(std::make_tuple(DB_NFT, protocolId, tokenId), nftDiskIndex))
        {
            return NftDiskIndexToNftMemIndex(nftDiskIndex);
        }
        return NfTokenIndex();
    }

    void PlatformDb::WriteNftProtoDiskIndex(const NftProtoDiskIndex & protoDiskIndex)
    {
        this->Write(std::make_pair(DB_NFT_PROTO, protoDiskIndex.NftProtoPtr()->tokenProtocolId), protoDiskIndex);
    }

    void PlatformDb::EraseNftProtoDiskIndex(const uint64_t &protocolId)
    {
        Erase(std::make_pair(DB_NFT_PROTO, protocolId));
    }

    NftProtoIndex PlatformDb::ReadNftProtoIndex(const uint64_t &protocolId)
    {
        NftProtoDiskIndex protoDiskIndex;
        if (Read(std::make_pair(DB_NFT_PROTO, protocolId), protoDiskIndex))
        {
            return NftProtoDiskIndexToNftProtoMemIndex(protoDiskIndex);
        }
        return NftProtoIndex();
    }

    void PlatformDb::WriteTotalSupply(unsigned int count, uint64_t nftProtocolId)
    {
        this->Write(std::make_pair(DB_NFT_TOTAL, nftProtocolId), count);
    }

    bool PlatformDb::ReadTotalSupply(unsigned int & count, uint64_t nftProtocolId)
    {
        return this->Read(std::make_pair(DB_NFT_TOTAL, nftProtocolId), count);
    }

    void PlatformDb::WriteTotalProtocolCount(unsigned int count)
    {
        this->Write(DB_NFT_PROTO_TOTAL, count);   
    }

    bool PlatformDb::ReadTotalProtocolCount(unsigned int & count)
    {
        return this->Read(DB_NFT_PROTO_TOTAL, count);
    }

    BlockIndex * PlatformDb::FindBlockIndex(const uint256 & blockHash)
    {
        auto blockIndexIt = g_chainman.m_blockman.m_block_index.find(blockHash);
        //if (blockIndexIt != g_chainman.m_blockman.m_block_index.end())
        //    blockIndexIt->second;
        return nullptr;
    }

    NfTokenIndex PlatformDb::NftDiskIndexToNftMemIndex(const NfTokenDiskIndex &nftDiskIndex)
    {
        auto blockIndexIt = g_chainman.m_blockman.m_block_index.find(nftDiskIndex.BlockHash());
        if (blockIndexIt == g_chainman.m_blockman.m_block_index.end())
        {
            LogPrintf("%s: Block index for NFT transaction cannot be found, block hash: %s, tx hash: %s",
                      __func__, nftDiskIndex.BlockHash().ToString(), nftDiskIndex.RegTxHash().ToString());
            return NfTokenIndex();
        }

        /// Not sure if this case is even possible
        if (nftDiskIndex.NfTokenPtr() == nullptr)
        {
            uint256 txBlockHash;
            CTransactionRef tx = GetTransaction(nullptr, nullptr, nftDiskIndex.RegTxHash(), Params().GetConsensus(), txBlockHash);
            if (!tx)
            {
                LogPrintf("%s: Transaction for NFT cannot be found, block hash: %s, tx hash: %s",
                          __func__, nftDiskIndex.BlockHash().ToString(), nftDiskIndex.RegTxHash().ToString());
                return NfTokenIndex();
            }
            assert(txBlockHash == nftDiskIndex.BlockHash());

            NfTokenRegTx nftRegTx;
            bool res = GetNftTxPayload(*tx, nftRegTx);
            assert(res);

            std::shared_ptr<NfToken> nfTokenPtr(new NfToken(nftRegTx.GetNfToken()));
            return {blockIndexIt->second, nftDiskIndex.RegTxHash(), nfTokenPtr};
        }

        return {blockIndexIt->second, nftDiskIndex.RegTxHash(), nftDiskIndex.NfTokenPtr()};
    }

    NftProtoIndex PlatformDb::NftProtoDiskIndexToNftProtoMemIndex(const NftProtoDiskIndex &protoDiskIndex)
    {
        auto blockIndexIt = g_chainman.m_blockman.m_block_index.find(protoDiskIndex.BlockHash());
        if (blockIndexIt == g_chainman.m_blockman.m_block_index.end())
        {
            LogPrintf("%s: Block index for NFT proto transaction cannot be found, block hash: %s, tx hash: %s",
                      __func__, protoDiskIndex.BlockHash().ToString(), protoDiskIndex.RegTxHash().ToString());
            return NftProtoIndex();
        }

        /// Not sure if this case is even possible
        if (protoDiskIndex.NftProtoPtr() == nullptr)
        {
            uint256 txBlockHash;
            CTransactionRef tx = GetTransaction(nullptr, nullptr, protoDiskIndex.RegTxHash(), Params().GetConsensus(), txBlockHash);

            if (!tx)
            {
                LogPrintf("%s: Transaction for NFT proto cannot be found, block hash: %s, tx hash: %s",
                          __func__, protoDiskIndex.BlockHash().ToString(), protoDiskIndex.RegTxHash().ToString());
                return NftProtoIndex();
            }
            assert(txBlockHash == protoDiskIndex.BlockHash());

            // TODO fix
            NfTokenProtocolRegTx protoRegTx;
            bool res = GetNftTxPayload(*tx, protoRegTx);
            assert(res);

            std::shared_ptr<NfTokenProtocol> nftProtoPtr(new NfTokenProtocol(protoRegTx.GetNftProto()));
            return {blockIndexIt->second, protoDiskIndex.RegTxHash(), nftProtoPtr};
        }

        return {blockIndexIt->second, protoDiskIndex.RegTxHash(), protoDiskIndex.NftProtoPtr()};
    }
}
