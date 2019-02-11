// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <checkpoints.h>

#include <chain.h>
#include <chainparams.h>
#include <reverse_iterator.h>
#include <validation.h>

#include <stdint.h>


namespace Checkpoints {

    CBlockIndex* GetLastCheckpoint(const CCheckpointData& data)
    {
        const MapCheckpoints& checkpoints = data.mapCheckpoints;

        for (const MapCheckpoints::value_type& i : reverse_iterate(checkpoints))
        {
            const uint256& hash = i.second;
            CBlockIndex* pindex = LookupBlockIndex(hash);
            if (pindex) {
                return pindex;
            }
        }
        return nullptr;
    }

    //Memory only!
    void AddCheckPoint(const CCheckpointData& data, int64_t height, uint256 hash)
    {
        MapCheckpoints& checkpoints = const_cast<MapCheckpoints&>(data.mapCheckpoints);
        checkpoints.insert(std::pair<int64_t,uint256>(height, hash));
    }

    //Memory only!
    void AddBadPoint(const CBadpointData& data, int64_t height, uint256 hash) 
    {
        const_cast<MapCheckpoints&>(data.mapBadpoints).insert(std::pair<int64_t,uint256>(height, hash));
    }
} // namespace Checkpoints
