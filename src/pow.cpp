// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
#include <util.h>

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);

    if (pindexLast->nHeight + 1 >= params.goldcoinRBH)
    {
        // Reset diff for 240 blocks, the max sample size of Golden River
        if (pindexLast->nHeight + 1 <= params.goldcoinRBH + 240)
            return UintToArith256(params.powScryptLimit).GetCompact();

        return GoldenRiver(pindexLast, params);
    }

    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

bool comp64(const int64_t& num1, const int64_t& num2)
{
    return num1 > num2;
}

unsigned int GoldenRiver(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    arith_uint256 bnProofOfWorkLimit = UintToArith256(params.powScryptLimit);

    // Whether or not we had a massive difficulty fall authorized
    bool didHalfAdjust = false;

    int64_t averageTime = 120;
    const int64_t nTargetTimespanCurrent = 2 * 60 * 60; // Two hours
    const int64_t nTargetSpacingCurrent  = 2 * 60; // Two minutes
    int64_t nInterval = nTargetTimespanCurrent / nTargetSpacingCurrent;

    // GoldCoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = nInterval - 1;
    if ((pindexLast->nHeight + 1) != nInterval)
        blockstogoback = nInterval;
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < blockstogoback; ++i)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    //We need to set this in a way that reflects how fast blocks are actually being solved..
    //First we find the last 60 blocks and take the time between blocks
    //That gives us a list of 59 time differences
    //Then we take the median of those times and multiply it by 60 to get our actualtimespan
    // We want to limit the possible difficulty raise/fall over 60 and 240 blocks here
    // So we get the difficulty at 60 and 240 blocks ago
    CBlockIndex tblock1 = *pindexLast;//We want to copy pindexLast to avoid changing it accidentally
    CBlockIndex* tblock2 = &tblock1;
    std::vector<int64_t> last60BlockTimes;
    std::vector<int64_t> last120BlockTimes;
    int64_t nbits60ago = 0ULL;
    int64_t nbits240ago = 0ULL;
    int counter = 0;
    while (counter <= 240)
    {
        if (counter == 60)
            nbits60ago = tblock2->nBits;

        if (counter == 240)
            nbits240ago = tblock2->nBits;

        if (last60BlockTimes.size() < 60)
            last60BlockTimes.push_back(tblock2->GetBlockTime());

        if ((last120BlockTimes.size() < 120))
            last120BlockTimes.push_back(tblock2->GetBlockTime());

        if (tblock2->pprev)   //should always be so
            tblock2 = tblock2->pprev;

        ++counter;
    }

    std::vector<int64_t> last59TimeDifferences;
    std::vector<int64_t> last119TimeDifferences;
    int64_t total = 0;
    int xy = 0;

    while (xy < 119)
    {
        if (xy < 59)
            last59TimeDifferences.push_back(llabs(last60BlockTimes[xy] - last60BlockTimes[xy + 1]));

        last119TimeDifferences.push_back(llabs(last120BlockTimes[xy] - last120BlockTimes[xy + 1]));
        total += last119TimeDifferences[xy];

        ++xy;
    }
    sort(last59TimeDifferences.begin(), last59TimeDifferences.end(), comp64);
    LogPrint(BCLog::DIFFICULTY, "Median Time between blocks is: %d \n", last59TimeDifferences[29]);

    int64_t nActualTimespan = llabs((last59TimeDifferences[29]));
    int64_t medTime = nActualTimespan;
    averageTime = total / 119;

    LogPrint(BCLog::DIFFICULTY, "Average time between blocks: %d\n", averageTime);

    medTime = (medTime > averageTime) ? averageTime : medTime;

    if (averageTime >= 180 && last119TimeDifferences[0] >= 1200 && last119TimeDifferences[1] >= 1200)
    {
        didHalfAdjust = true;
        medTime = 240;
    }

    //Fixes an issue where median time between blocks is greater than 120 seconds and is not permitted to be lower by the defence system
    //Causing difficulty to drop without end
    if (medTime >= 120)
    {
        //Check to see whether we are in a deadlock situation with the 51% defense system
        int numTooClose = 0;
        int index = 1;

        while (index != 55)
        {
            if (llabs(last60BlockTimes.at(last60BlockTimes.size() - index) - last60BlockTimes.at(last60BlockTimes.size() - (index + 5))) == 600)
            {
                ++numTooClose;
            }

            ++index;
        }

        if (numTooClose > 0)
        {
            //We found 6 blocks that were solved in exactly 10 minutes
            //Averaging 1.66 minutes per block
            LogPrint(BCLog::DIFFICULTY, "DeadLock detected and fixed - Difficulty Increased\n");
            medTime = 119;
        }
        else
        {
            LogPrint(BCLog::DIFFICULTY, "DeadLock not detected. \n");
        }
    }

    //216 == (int64) 180.0/100.0 * 120
    //122 == (int64) 102.0/100.0 * 120 == 122.4
    if (averageTime > 216 || medTime > 122)
    {
        if (didHalfAdjust)
        {
            // If the average time between blocks was
            // too high.. allow a dramatic difficulty
            // fall..
            medTime = (int64_t)(120 * 142.0 / 100.0);

        }
        else
        {
            // Otherwise only allow a 120/119 fall per block
            // maximum.. As we now adjust per block..
            // 121 == (int64) 120 * 120.0/119.0
            medTime = 121;
        }
    }
    // 117 -- (int64) 120.0 * 98.0/100.0
    else if (averageTime < 117 || medTime < 117)
    {
        // If the average time between blocks is within 2% of target value
        // Or if the median time stamp between blocks is within 2% of the target value
        // Limit diff increase to 2%
        medTime = 117;
    }

    nActualTimespan = medTime * 60;

    //Now we get the old targets
    arith_uint256 bn60ago = 0, bn240ago = 0, bnLast = 0;
    bn60ago.SetCompact(nbits60ago);
    bn240ago.SetCompact(nbits240ago);
    bnLast.SetCompact(pindexLast->nBits);

    //Set the new target
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespanCurrent;

    // Set a floor on difficulty decreases per block(20% lower maximum
    // than the previous block difficulty).. when there was no halfing
    // necessary.. 10/8 == 1.0/0.8
    bnLast *= 10;
    bnLast /= 8;

    if (!didHalfAdjust && bnNew > bnLast)
        bnNew.SetCompact(bnLast.GetCompact());

    // Set ceilings on difficulty increases per block
    //1.0/1.02 == 100/102
    bn60ago *= 100;
    bn60ago /= 102;

    if (bnNew < bn60ago)
        bnNew.SetCompact(bn60ago.GetCompact());

    //1.0/(1.02*4) ==  100 / 408
    bn240ago *= 100;
    bn240ago /= 408;

    if (bnNew < bn240ago)
        bnNew.SetCompact(bn240ago.GetCompact());

    //Sets a ceiling on highest target value (lowest possible difficulty)
    if (bnNew > bnProofOfWorkLimit)
        bnNew = bnProofOfWorkLimit;

    return bnNew.GetCompact();
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
