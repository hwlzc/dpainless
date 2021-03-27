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

#pragma once

#include "../clauses/ClauseDatabase.h"
#include "../clauses/ClauseFilter.h"
#include "../sharing/SharingStrategy.h"
#include "../solvers/SolverInterface.h"

#include <vector>

/// A hordesat like sharing strategy.
class DistributionSharing : public SharingStrategy
{
public:
   /// Constructor.
   DistributionSharing();

   /// Destructor.
   ~DistributionSharing();

   /// This method shared clauses from the producers to the consumers.
   void doSharing(int idSharer, const vector<SolverInterface *> &from,
                  const vector<SolverInterface *> &to);

   /// Return the sharing statistics of this sharng strategy.
   SharingStatistics getStatistics();

protected:
   int literalPerRound;

   /// Databse used to store the clauses.
   ClauseDatabase database;

   vector<ClauseFilter* > solverFilters;
   ClauseFilter localFilter;

   /// Sharing statistics.
   SharingStatistics stats;

   /// Used to manipulate clauses.
   vector<ClauseExchange *> tmp;
};
