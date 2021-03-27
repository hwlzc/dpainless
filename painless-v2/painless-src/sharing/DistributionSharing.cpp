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

#include "../clauses/ClauseManager.h"
#include "../comm/MpiComm.h"
#include "../sharing/DistributionSharing.h"
#include "../solvers/SolverFactory.h"
#include "../utils/Logger.h"
#include "../utils/Parameters.h"

#include <mutex>
#include <cassert>

std::once_flag filterFlag;

DistributionSharing::DistributionSharing()
{
   literalPerRound = Parameters::getIntParam("shr-lit", 1500);
}

DistributionSharing::~DistributionSharing()
{
}

void DistributionSharing::doSharing(int idSharer, const vector<SolverInterface *> &from,
                                    const vector<SolverInterface *> &to)
{
   // Assume that there is one distributed sharer
   // which is response for the sharing of the entire process
   assert(from.size() == to.size());
   int nSolvers = to.size();
   std::call_once(filterFlag, [this, nSolvers] {
      for (int i = 0; i < nSolvers; i++)
      {
         this->solverFilters.push_back(new ClauseFilter());
      }
   });
   assert(solverFilters.size() == nSolvers);

   // Get learned clauses from solvers
   for (int i = 0; i < nSolvers; i++)
   {
      tmp.clear();

      from[i]->getLearnedClauses(tmp);

      stats.receivedClauses += tmp.size();

      for (size_t k = 0; k < tmp.size(); k++)
      {
         if (solverFilters[i]->registerClause(tmp[k]->lits, tmp[k]->size) && localFilter.registerClause(tmp[k]->lits, tmp[k]->size))
         {
            database.addClause(tmp[k]);
         }
         else
         {
            ClauseManager::releaseClause(tmp[k]);
         }
      }
   }
   tmp.clear();

   int selectCount;
   database.giveSelection(tmp, literalPerRound, &selectCount);
   stats.sharedClauses += selectCount;

   // Add clauses to communicator clauseDB where clauses will be export
   MpiComm::getInstance()->addClausesToExternal(tmp);

   // Import clauses from communicator
   int old = tmp.size();
   MpiComm::getInstance()->getClausesFromExternal(tmp);

   // Add clauses to solvers buffer
   for (int i = 0; i < nSolvers; i++)
   {
      vector<ClauseExchange *> clausesToAdd;
      for (auto cls : tmp)
      {
         if (cls->from != i && solverFilters[i]->registerClause(cls->lits, cls->size))
         {
            clausesToAdd.push_back(cls);
         }
      }

      for (auto cls : clausesToAdd)
      {
         ClauseManager::increaseClause(cls, 1);
      }
      to[i]->addLearnedClauses(clausesToAdd);
   }
   for (auto cls : tmp) {
      ClauseManager::releaseClause(cls);
   }
   tmp.clear();
}

SharingStatistics
DistributionSharing::getStatistics()
{
   return stats;
}
