/* 
 * File:   BooleanExpression.h
 * Author: Lukas Westhofen
 *
 * Created on 11. April 2015, 17:44
 */

#ifndef BOOLEANEXPRESSION_H
#define	BOOLEANEXPRESSION_H

#include "src/storage/expressions/Expression.h"

namespace storm {
    namespace pgcl {
        /**
         * This class wraps an ordinary expression but allows only for boolean
         * expressions to be handled, e.g. expressions of the form (x < 4) or
         * (x != y), but not (x + 5).
         */
        class BooleanExpression {
        public:
            BooleanExpression() = default;
            /**
             * Constructs a boolean expression if the given expression is of a
             * boolean type. Note that it is not checked whether the expression
             * has a boolean type.
             * @param booleanExpression The expression of a boolean type.
             */
            BooleanExpression(storm::expressions::Expression const& booleanExpression);
            /**
             * Returns the expression.
             * @return The expression of boolean type.
             */
            storm::expressions::Expression& getBooleanExpression();
        private:
            storm::expressions::Expression booleanExpression;
        };
    }
}

#endif	/* BOOLEANEXPRESSION_H */
