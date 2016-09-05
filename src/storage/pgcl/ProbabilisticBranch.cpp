/* 
 * File:   ProbabilisticBranch.cpp
 * Author: Lukas Westhofen
 * 
 * Created on 11. April 2015, 17:43
 */

#include "src/storage/pgcl/ProbabilisticBranch.h"
#include "src/storage/pgcl/AbstractStatementVisitor.h"

namespace storm {
    namespace pgcl {
        ProbabilisticBranch::ProbabilisticBranch(storm::expressions::Expression const& probability, std::shared_ptr<storm::pgcl::PgclProgram> const& left, std::shared_ptr<storm::pgcl::PgclProgram> const& right) :
        probability(probability) {
            rightBranch = right;
            leftBranch = left;
        }
        
        storm::expressions::Expression& ProbabilisticBranch::getProbability() {
            return this->probability;
        }

        void ProbabilisticBranch::accept(storm::pgcl::AbstractStatementVisitor& visitor) {
            visitor.visit(*this);
        }
    }
}
