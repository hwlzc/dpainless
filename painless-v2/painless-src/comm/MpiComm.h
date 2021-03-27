// -----------------------------------------------------------------------------
// Copyright (C) 2021
//
// This file is part of PaInleSS.
//
// PaInleSS is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.
// -----------------------------------------------------------------------------

#include <thread>
#include <memory>
#include <mpi.h>

#include "../clauses/ClauseBuffer.h"
#include "../clauses/ClauseFilter.h"
#include "../clauses/ClauseDatabase.h"
#include "../utils/SatUtils.h"
#include "../working/WorkingStrategy.h"

extern atomic<bool> globalEnding;

// Communicator for inter-process communication
// TODO: optimize the class design
class MpiComm
{
public:
    static MpiComm *getInstance();

    ~MpiComm();
    void init(int rank, int size);

    void addClausesToExternal(const vector<ClauseExchange *> &clauses);
    void getClausesFromExternal(vector<ClauseExchange *> &clauses);
    void exportLearnedClauses();

    SatResult receiveIncomingMsg();
    void cleanReceivingBuffer();
    void updateWorkingStatus();
    void reportResult(SatResult currRes, const vector<int> &model);

    void registerParentWorkingStrategy(WorkingStrategy *strategy) { this->parentStrategy.push_back(strategy); }
    void registerChildWorkingStrategy(WorkingStrategy *strategy) { this->childStrategy = strategy; }

    void sendAssumption(const vector<int> &assumption, int targetRank);
    void sendInterrupt(int interrupt, int targetRank);

private:
    MpiComm();
    void handleReceivedClauses(int *clsBuf, int bufSize);

    ClauseFilter externalFilter;
    ClauseBuffer clausesToImport;

    // Shorter clauses can be export at the first time
    ClauseDatabase clausesToExport;

    int rank = -1;
    int size = -1;

    // limit clause sharing of current process within a range
    int clsShrLowerRank = -1;
    int clsShrUpperRank = -1;

    int *importBuf;
    int *exportBuf;
    int clauseBufLimit;

    vector<int> model;

    SatResult res = UNKNOWN;

    vector<WorkingStrategy *> parentStrategy;
    WorkingStrategy *childStrategy = nullptr;
};