// Copyright (c) 2014-2019 The Goldcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GOLDCOIN_CHAINPARAMBLOCKS_H
#define GOLDCOIN_CHAINPARAMBLOCKS_H

#include <stdint.h>
#include <utility>
#include <string>
#include <vector>

class ReverseHardForkBlocks
{
public:
    static const std::vector<std::pair<int64_t, std::string> >* testnetTransactions();
};

#endif // GOLDCOIN_CHAINPARAMBLOCKS_H
