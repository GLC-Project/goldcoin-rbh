// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHECKPOINTS_H
#define BITCOIN_CHECKPOINTS_H

#include <uint256.h>

#include <map>

class CBlockIndex;
struct CCheckpointData;
struct CBadpointData;

/**
 * Block-chain checkpoints are compiled-in sanity checks.
 * They are updated every release or three.
 */
namespace Checkpoints
{

//! Returns last CBlockIndex* that is a checkpoint
CBlockIndex* GetLastCheckpoint(const CCheckpointData& data);

//! Add an in-memory checkpoint
void AddCheckPoint(const CCheckpointData& data, int64_t height, uint256 hash);

//! Add an in-memory badpoint
void AddBadPoint(const CBadpointData& data, int64_t height, uint256 hash);

} //namespace Checkpoints

#endif // BITCOIN_CHECKPOINTS_H
