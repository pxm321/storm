#include "storm/solver/StandardGameSolver.h"

#include "storm/solver/GmmxxLinearEquationSolver.h"
#include "storm/solver/EigenLinearEquationSolver.h"
#include "storm/solver/NativeLinearEquationSolver.h"
#include "storm/solver/EliminationLinearEquationSolver.h"

#include "storm/environment/solver/GameSolverEnvironment.h"

#include "storm/utility/vector.h"
#include "storm/utility/macros.h"
#include "storm/exceptions/InvalidEnvironmentException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/NotImplementedException.h"
namespace storm {
    namespace solver {
        
        template<typename ValueType>
        StandardGameSolver<ValueType>::StandardGameSolver(storm::storage::SparseMatrix<storm::storage::sparse::state_type> const& player1Matrix, storm::storage::SparseMatrix<ValueType> const& player2Matrix, std::unique_ptr<LinearEquationSolverFactory<ValueType>>&& linearEquationSolverFactory) : linearEquationSolverFactory(std::move(linearEquationSolverFactory)), localPlayer1Grouping(nullptr), localPlayer1Matrix(nullptr), localPlayer2Matrix(nullptr), player1Grouping(nullptr), player1Matrix(&player1Matrix), player2Matrix(player2Matrix) {
            // Intentionally left empty.
        }
        
        template<typename ValueType>
        StandardGameSolver<ValueType>::StandardGameSolver(storm::storage::SparseMatrix<storm::storage::sparse::state_type>&& player1Matrix, storm::storage::SparseMatrix<ValueType>&& player2Matrix, std::unique_ptr<LinearEquationSolverFactory<ValueType>>&& linearEquationSolverFactory) : linearEquationSolverFactory(std::move(linearEquationSolverFactory)), localPlayer1Grouping(nullptr), localPlayer1Matrix(std::make_unique<storm::storage::SparseMatrix<storm::storage::sparse::state_type>>(std::move(player1Matrix))), localPlayer2Matrix(std::make_unique<storm::storage::SparseMatrix<ValueType>>(std::move(player2Matrix))), player1Grouping(nullptr), player1Matrix(localPlayer1Matrix.get()), player2Matrix(*localPlayer2Matrix) {
            // Intentionally left empty.
        }
        
        template<typename ValueType>
        StandardGameSolver<ValueType>::StandardGameSolver(std::vector<uint64_t> const& player1Grouping, storm::storage::SparseMatrix<ValueType> const& player2Matrix, std::unique_ptr<LinearEquationSolverFactory<ValueType>>&& linearEquationSolverFactory) : linearEquationSolverFactory(std::move(linearEquationSolverFactory)), localPlayer1Grouping(nullptr), localPlayer1Matrix(nullptr), localPlayer2Matrix(nullptr), player1Grouping(&player1Grouping), player1Matrix(nullptr), player2Matrix(player2Matrix) {
            
        }
        
        template<typename ValueType>
        StandardGameSolver<ValueType>::StandardGameSolver(std::vector<uint64_t>&& player1Grouping, storm::storage::SparseMatrix<ValueType>&& player2Matrix, std::unique_ptr<LinearEquationSolverFactory<ValueType>>&& linearEquationSolverFactory) : linearEquationSolverFactory(std::move(linearEquationSolverFactory)), localPlayer1Grouping(std::make_unique<std::vector<uint64_t>>(std::move(player1Grouping))), localPlayer1Matrix(nullptr), localPlayer2Matrix(std::make_unique<storm::storage::SparseMatrix<ValueType>>(std::move(player2Matrix))), player1Grouping(localPlayer1Grouping.get()), player1Matrix(nullptr), player2Matrix(*localPlayer2Matrix) {
            // Intentionally left empty.
        }
        
        template<typename ValueType>
        GameMethod StandardGameSolver<ValueType>::getMethod(Environment const& env, bool isExactMode) const {
            auto method = env.solver().game().getMethod();
            if (isExactMode && method != GameMethod::PolicyIteration) {
                if (env.solver().game().isMethodSetFromDefault()) {
                    method = GameMethod::PolicyIteration;
                    STORM_LOG_INFO("Changing game method to policy-iteration to guarantee exact results. If you want to override this, specify another method.");
                } else {
                    STORM_LOG_WARN("The selected game method does not guarantee exact results.");
                }
            } else if (env.solver().isForceSoundness() && method != GameMethod::PolicyIteration) {
                if (env.solver().game().isMethodSetFromDefault()) {
                    method = GameMethod::PolicyIteration;
                    STORM_LOG_INFO("Changing game method to policy-iteration to guarantee sound results. If you want to override this, specify another method.");
                } else {
                    STORM_LOG_WARN("The selected game method does not guarantee sound results.");
                }
            }
            return method;
        }
        
        template<typename ValueType>
        bool StandardGameSolver<ValueType>::solveGame(Environment const& env, OptimizationDirection player1Dir, OptimizationDirection player2Dir, std::vector<ValueType>& x, std::vector<ValueType> const& b) const {
            switch (getMethod(env, std::is_same<ValueType, storm::RationalNumber>::value)) {
                case GameMethod::ValueIteration:
                    return solveGameValueIteration(env, player1Dir, player2Dir, x, b);
                case GameMethod::PolicyIteration:
                    return solveGamePolicyIteration(env, player1Dir, player2Dir, x, b);
                default:
                    STORM_LOG_THROW(false, storm::exceptions::InvalidEnvironmentException, "This solver does not implement the selected solution method");
            }
            return false;
        }
        
        template<typename ValueType>
        bool StandardGameSolver<ValueType>::solveGamePolicyIteration(Environment const& env, OptimizationDirection player1Dir, OptimizationDirection player2Dir, std::vector<ValueType>& x, std::vector<ValueType> const& b) const {
            
            // Create the initial choice selections.
            std::vector<storm::storage::sparse::state_type> player1Choices;
            if (this->hasSchedulerHints()) {
                player1Choices = this->player1ChoicesHint.get();
            } else if (this->player1RepresentedByMatrix()) {
                // Player 1 represented by matrix.
                player1Choices = std::vector<storm::storage::sparse::state_type>(this->getPlayer1Matrix().getRowGroupCount(), 0);
            } else {
                // Player 1 represented by grouping of player 2 states.
                player1Choices = this->getPlayer1Grouping();
                player1Choices.resize(player1Choices.size() - 1);
            }
            std::vector<storm::storage::sparse::state_type> player2Choices = this->hasSchedulerHints() ? this->player2ChoicesHint.get() : std::vector<storm::storage::sparse::state_type>(this->player2Matrix.getRowGroupCount(), 0);
            
            if (!auxiliaryP2RowGroupVector) {
                auxiliaryP2RowGroupVector = std::make_unique<std::vector<ValueType>>(this->player2Matrix.getRowGroupCount());
            }
            if (!auxiliaryP1RowGroupVector) {
                auxiliaryP1RowGroupVector = std::make_unique<std::vector<ValueType>>(this->player1Matrix->getRowGroupCount());
            }
            std::vector<ValueType>& subB = *auxiliaryP1RowGroupVector;

            uint64_t maxIter = env.solver().game().getMaximalNumberOfIterations();
            
            // The linear equation solver should be at least as precise as this solver.
            std::unique_ptr<storm::Environment> environmentOfSolverStorage;
            auto precOfSolver = env.solver().getPrecisionOfLinearEquationSolver(env.solver().getLinearEquationSolverType());
            if (!storm::NumberTraits<ValueType>::IsExact) {
                bool changePrecision = precOfSolver.first && precOfSolver.first.get() > env.solver().game().getPrecision();
                bool changeRelative = precOfSolver.second && !precOfSolver.second.get() && env.solver().game().getRelativeTerminationCriterion();
                if (changePrecision || changeRelative) {
                    environmentOfSolverStorage = std::make_unique<storm::Environment>(env);
                    boost::optional<storm::RationalNumber> newPrecision;
                    boost::optional<bool> newRelative;
                    if (changePrecision) {
                        newPrecision = env.solver().game().getPrecision();
                    }
                    if (changeRelative) {
                        newRelative = true;
                    }
                    environmentOfSolverStorage->solver().setLinearEquationSolverPrecision(newPrecision, newRelative);
                }
            }
            storm::Environment const& environmentOfSolver = environmentOfSolverStorage ? *environmentOfSolverStorage : env;
            
            // Solve the equation system induced by the two schedulers.
            storm::storage::SparseMatrix<ValueType> submatrix;
            getInducedMatrixVector(x, b, player1Choices, player2Choices, submatrix, subB);
            if (this->linearEquationSolverFactory->getEquationProblemFormat(environmentOfSolver) == LinearEquationSolverProblemFormat::EquationSystem) {
                submatrix.convertToEquationSystem();
            }
            auto submatrixSolver = linearEquationSolverFactory->create(environmentOfSolver, std::move(submatrix));
            if (this->lowerBound) {
                submatrixSolver->setLowerBound(this->lowerBound.get());
                
            }
            if (this->upperBound) {
                submatrixSolver->setUpperBound(this->upperBound.get());
            }
            submatrixSolver->setCachingEnabled(true);
            
            Status status = Status::InProgress;
            uint64_t iterations = 0;
            do {
                // Solve the equation system for the 'DTMC'.
                // FIXME: we need to remove the 0- and 1- states to make the solution unique.
                submatrixSolver->solveEquations(environmentOfSolver, x, subB);
                
                bool schedulerImproved = extractChoices(player1Dir, player2Dir, x, b, *auxiliaryP2RowGroupVector, player1Choices, player2Choices);
                
                // If the scheduler did not improve, we are done.
                if (!schedulerImproved) {
                    status = Status::Converged;
                } else {
                    // Update the solver.
                    getInducedMatrixVector(x, b, player1Choices, player2Choices, submatrix, subB);
                    submatrix.convertToEquationSystem();
                    submatrixSolver->setMatrix(std::move(submatrix));
                }
                
                // Update environment variables.
                ++iterations;
                status = updateStatusIfNotConverged(status, x, iterations, maxIter);
            } while (status == Status::InProgress);
            
            reportStatus(status, iterations);
            
            // If requested, we store the scheduler for retrieval.
            if (this->isTrackSchedulersSet()) {
                this->player1SchedulerChoices = std::move(player1Choices);
                this->player2SchedulerChoices = std::move(player2Choices);
            }
            
            if (!this->isCachingEnabled()) {
                clearCache();
            }
            
            return status == Status::Converged || status == Status::TerminatedEarly;
        }
        
        template<typename ValueType>
        bool StandardGameSolver<ValueType>::valueImproved(OptimizationDirection dir, ValueType const& value1, ValueType const& value2) const {
            if (dir == OptimizationDirection::Minimize) {
                return value2 < value1;
            } else {
                return value2 > value1;
            }
        }

        template<typename ValueType>
        bool StandardGameSolver<ValueType>::solveGameValueIteration(Environment const& env, OptimizationDirection player1Dir, OptimizationDirection player2Dir, std::vector<ValueType>& x, std::vector<ValueType> const& b) const {
                         
            if (!multiplierPlayer2Matrix) {
                multiplierPlayer2Matrix = storm::solver::MultiplierFactory<ValueType>().create(env, player2Matrix);
            }
            
            if (!auxiliaryP2RowGroupVector) {
                auxiliaryP2RowGroupVector = std::make_unique<std::vector<ValueType>>(player2Matrix.getRowGroupCount());
            }
             
            if (!auxiliaryP1RowGroupVector) {
                auxiliaryP1RowGroupVector = std::make_unique<std::vector<ValueType>>(this->getNumberOfPlayer1States());
            }
            
            ValueType precision = storm::utility::convertNumber<ValueType>(env.solver().game().getPrecision());
            bool relative = env.solver().game().getRelativeTerminationCriterion();
            uint64_t maxIter = env.solver().game().getMaximalNumberOfIterations();
            
            std::vector<ValueType>& reducedPlayer2Result = *auxiliaryP2RowGroupVector;
            
            bool trackSchedulersInValueIteration = this->isTrackSchedulersSet() && !this->hasUniqueSolution();
            if (this->hasSchedulerHints()) {
                // Solve the equation system induced by the two schedulers.
                storm::storage::SparseMatrix<ValueType> submatrix;
                getInducedMatrixVector(x, b, this->player1ChoicesHint.get(), this->player2ChoicesHint.get(), submatrix, *auxiliaryP1RowGroupVector);
                if (this->linearEquationSolverFactory->getEquationProblemFormat(env) == LinearEquationSolverProblemFormat::EquationSystem) {
                    submatrix.convertToEquationSystem();
                }
                auto submatrixSolver = linearEquationSolverFactory->create(env, std::move(submatrix));
                if (this->lowerBound) {
                    submatrixSolver->setLowerBound(this->lowerBound.get());
                    
                }
                if (this->upperBound) {
                    submatrixSolver->setUpperBound(this->upperBound.get());
                }
                submatrixSolver->solveEquations(env, x, *auxiliaryP1RowGroupVector);
                
                // If requested, we store the scheduler for retrieval. Initialize the schedulers to the hint we have.
                if (trackSchedulersInValueIteration) {
                    this->player1SchedulerChoices = this->player1ChoicesHint.get();
                    this->player2SchedulerChoices = this->player2ChoicesHint.get();
                }
            } else if (trackSchedulersInValueIteration) {
                // If requested, we store the scheduler for retrieval. Create empty schedulers here so we can fill them
                // during VI.
                this->player1SchedulerChoices = std::vector<uint_fast64_t>(this->getNumberOfPlayer1States(), 0);
                this->player2SchedulerChoices = std::vector<uint_fast64_t>(this->getNumberOfPlayer2States(), 0);
            }
             
            std::vector<ValueType>* newX = auxiliaryP1RowGroupVector.get();
            std::vector<ValueType>* currentX = &x;
             
            // Proceed with the iterations as long as the method did not converge or reach the maximum number of iterations.
            uint64_t iterations = 0;

            Status status = Status::InProgress;
            while (status == Status::InProgress) {
                multiplyAndReduce(env, player1Dir, player2Dir, *currentX, &b, *multiplierPlayer2Matrix, reducedPlayer2Result, *newX, trackSchedulersInValueIteration ? &this->getPlayer1SchedulerChoices() : nullptr, trackSchedulersInValueIteration ? &this->getPlayer2SchedulerChoices() : nullptr);

                // Determine whether the method converged.
                if (storm::utility::vector::equalModuloPrecision<ValueType>(*currentX, *newX, precision, relative)) {
                    status = Status::Converged;
                }
                
                // Update environment variables.
                std::swap(currentX, newX);
                ++iterations;
                status = updateStatusIfNotConverged(status, *currentX, iterations, maxIter);
            }
                        
            reportStatus(status, iterations);
            
            // If we performed an odd number of iterations, we need to swap the x and currentX, because the newest result
            // is currently stored in currentX, but x is the output vector.
            if (currentX == auxiliaryP1RowGroupVector.get()) {
                std::swap(x, *currentX);
            }
            
            // If requested, we store the scheduler for retrieval.
            if (this->isTrackSchedulersSet() && this->hasUniqueSolution()) {
                this->player1SchedulerChoices = std::vector<uint_fast64_t>(this->getNumberOfPlayer1States(), 0);
                this->player2SchedulerChoices = std::vector<uint_fast64_t>(this->getNumberOfPlayer2States(), 0);
                extractChoices(player1Dir, player2Dir, x, b, *auxiliaryP2RowGroupVector, this->player1SchedulerChoices.get(), this->player2SchedulerChoices.get());
            }
            
            if (!this->isCachingEnabled()) {
                clearCache();
            }
            
            return (status == Status::Converged || status == Status::TerminatedEarly);
        }
        
        template<typename ValueType>
        void StandardGameSolver<ValueType>::repeatedMultiply(Environment const& env, OptimizationDirection player1Dir, OptimizationDirection player2Dir, std::vector<ValueType>& x, std::vector<ValueType> const* b, uint_fast64_t n) const {
            
            if (!multiplierPlayer2Matrix) {
                multiplierPlayer2Matrix = storm::solver::MultiplierFactory<ValueType>().create(env, player2Matrix);
            }
            
            if (!auxiliaryP2RowGroupVector) {
                auxiliaryP2RowGroupVector = std::make_unique<std::vector<ValueType>>(player2Matrix.getRowGroupCount());
            }
            std::vector<ValueType>& reducedPlayer2Result = *auxiliaryP2RowGroupVector;
            
            for (uint_fast64_t iteration = 0; iteration < n; ++iteration) {
                multiplyAndReduce(env, player1Dir, player2Dir, x, b, *multiplierPlayer2Matrix, reducedPlayer2Result, x);
            }
            
            if (!this->isCachingEnabled()) {
                clearCache();
            }
        }
        
        template<typename ValueType>
        void StandardGameSolver<ValueType>::multiplyAndReduce(Environment const& env, OptimizationDirection player1Dir, OptimizationDirection player2Dir, std::vector<ValueType>& x, std::vector<ValueType> const* b, storm::solver::Multiplier<ValueType> const& multiplier, std::vector<ValueType>& player2ReducedResult, std::vector<ValueType>& player1ReducedResult, std::vector<uint64_t>* player1SchedulerChoices, std::vector<uint64_t>* player2SchedulerChoices) const {
            
            multiplier.multiplyAndReduce(env, player2Dir, x, b, player2ReducedResult, player2SchedulerChoices);
            
            if (this->player1RepresentedByMatrix()) {
                // Player 1 represented by matrix.
                uint_fast64_t player1State = 0;
                for (auto& result : player1ReducedResult) {
                    storm::storage::SparseMatrix<storm::storage::sparse::state_type>::const_rows relevantRows = this->getPlayer1Matrix().getRowGroup(player1State);
                    STORM_LOG_ASSERT(relevantRows.getNumberOfEntries() != 0, "There is a choice of player 1 that does not lead to any player 2 choice");
                    auto it = relevantRows.begin();
                    auto ite = relevantRows.end();
                    
                    // Set the first value.
                    result = player2ReducedResult[it->getColumn()];
                    ++it;
                    
                    // Now iterate through the different values and pick the extremal one.
                    if (player1Dir == OptimizationDirection::Minimize) {
                        for (; it != ite; ++it) {
                            result = std::min(result, player2ReducedResult[it->getColumn()]);
                        }
                    } else {
                        for (; it != ite; ++it) {
                            result = std::max(result, player2ReducedResult[it->getColumn()]);
                        }
                    }
                    ++player1State;
                }
            } else {
                // Player 1 represented by grouping of player 2 states (vector).
                storm::utility::vector::reduceVectorMinOrMax(player1Dir, player2ReducedResult, player1ReducedResult, this->getPlayer1Grouping(), player1SchedulerChoices);
            }
        }

        template<typename ValueType>
        bool StandardGameSolver<ValueType>::extractChoices(OptimizationDirection player1Dir, OptimizationDirection player2Dir, std::vector<ValueType> const& x, std::vector<ValueType> const& b, std::vector<ValueType>& player2ChoiceValues, std::vector<uint_fast64_t>& player1Choices, std::vector<uint_fast64_t>& player2Choices) const {
            
            // get the choices of player 2 and the corresponding values.
            bool schedulerImproved = false;
            auto currentValueIt = player2ChoiceValues.begin();
            for (uint_fast64_t p2Group = 0; p2Group < this->player2Matrix.getRowGroupCount(); ++p2Group) {
                uint_fast64_t firstRowInGroup = this->player2Matrix.getRowGroupIndices()[p2Group];
                uint_fast64_t rowGroupSize = this->player2Matrix.getRowGroupIndices()[p2Group + 1] - firstRowInGroup;
                
                // We need to check whether the scheduler improved. Therefore, we first have to evaluate the current choice
                uint_fast64_t currentP2Choice = player2Choices[p2Group];
                *currentValueIt = storm::utility::zero<ValueType>();
                for (auto const& entry : this->player2Matrix.getRow(firstRowInGroup + currentP2Choice)) {
                    *currentValueIt += entry.getValue() * x[entry.getColumn()];
                }
                *currentValueIt += b[firstRowInGroup + currentP2Choice];
                
                // now check the other choices
                for (uint_fast64_t p2Choice = 0; p2Choice < rowGroupSize; ++p2Choice) {
                    if (p2Choice == currentP2Choice) {
                        continue;
                    }
                    ValueType choiceValue = storm::utility::zero<ValueType>();
                    for (auto const& entry : this->player2Matrix.getRow(firstRowInGroup + p2Choice)) {
                        choiceValue += entry.getValue() * x[entry.getColumn()];
                    }
                    choiceValue += b[firstRowInGroup + p2Choice];
                        
                    if (valueImproved(player2Dir, *currentValueIt, choiceValue)) {
                        schedulerImproved = true;
                        player2Choices[p2Group] = p2Choice;
                        *currentValueIt = std::move(choiceValue);
                    }
                }
            }
            
            // Now extract the choices of player 1.
            if (this->player1RepresentedByMatrix()) {
                // Player 1 represented by matrix.
                for (uint_fast64_t p1Group = 0; p1Group < this->getPlayer1Matrix().getRowGroupCount(); ++p1Group) {
                    uint_fast64_t firstRowInGroup = this->getPlayer1Matrix().getRowGroupIndices()[p1Group];
                    uint_fast64_t rowGroupSize = this->getPlayer1Matrix().getRowGroupIndices()[p1Group + 1] - firstRowInGroup;
                    uint_fast64_t currentChoice = player1Choices[p1Group];
                    ValueType currentValue = player2ChoiceValues[this->getPlayer1Matrix().getRow(firstRowInGroup + currentChoice).begin()->getColumn()];
                    for (uint_fast64_t p1Choice = 0; p1Choice < rowGroupSize; ++p1Choice) {
                        // If the choice is the currently selected one, we can skip it.
                        if (p1Choice == currentChoice) {
                            continue;
                        }
                        ValueType const& choiceValue = player2ChoiceValues[this->getPlayer1Matrix().getRow(firstRowInGroup + p1Choice).begin()->getColumn()];
                        if (valueImproved(player1Dir, currentValue, choiceValue)) {
                            schedulerImproved = true;
                            player1Choices[p1Group] = p1Choice;
                            currentValue = choiceValue;
                        }
                    }
                }
            } else {
                // Player 1 represented by grouping of player 2 states (vector).
                for (uint64_t player1State = 0; player1State < this->getPlayer1Grouping().size() - 1; ++player1State) {
                    uint64_t currentChoice = player1Choices[player1State];
                    ValueType currentValue = player2ChoiceValues[currentChoice];
                    uint64_t numberOfPlayer2Successors = this->getPlayer1Grouping()[player1State + 1] - this->getPlayer1Grouping()[player1State];
                    for (uint64_t player2State = 0; player2State < numberOfPlayer2Successors; ++player2State) {
                        // If the choice is the currently selected one, we can skip it.
                        if (currentChoice == player2State + this->getPlayer1Grouping()[player1State]) {
                            continue;
                        }
                        
                        ValueType const& choiceValue = player2ChoiceValues[this->getPlayer1Grouping()[player1State] + player2State];
                        if (valueImproved(player1Dir, currentValue, choiceValue)) {
                            schedulerImproved = true;
                            player1Choices[player1State] = player2State;
                            currentValue = choiceValue;
                        }
                    }
                }
            }
            
            return schedulerImproved;
        }
        
        template<typename ValueType>
        void StandardGameSolver<ValueType>::getInducedMatrixVector(std::vector<ValueType>& x, std::vector<ValueType> const& b, std::vector<uint_fast64_t> const& player1Choices, std::vector<uint_fast64_t> const& player2Choices, storm::storage::SparseMatrix<ValueType>& inducedMatrix, std::vector<ValueType>& inducedVector) const {
            // Get the rows of the player 2 matrix that are selected by the schedulers.
            // Note that rows can be selected more than once and in an arbitrary order.
            std::vector<storm::storage::sparse::state_type> selectedRows;
            if (this->player1RepresentedByMatrix()) {
                // Player 1 is represented by a matrix.
                selectedRows.reserve(this->getPlayer1Matrix().getRowGroupCount());
                uint_fast64_t player1State = 0;
                for (auto const& player1Choice : player1Choices) {
                    auto const& player1Row = this->getPlayer1Matrix().getRow(player1State, player1Choice);
                    STORM_LOG_ASSERT(player1Row.getNumberOfEntries() == 1, "It is assumed that rows of player one have one entry, but this is not the case.");
                    uint_fast64_t player2State = player1Row.begin()->getColumn();
                    selectedRows.push_back(player2Matrix.getRowGroupIndices()[player2State] + player2Choices[player2State]);
                    ++player1State;
                }
            } else {
                // Player 1 is represented by the grouping of player 2 states (vector).
                selectedRows.reserve(this->player2Matrix.getRowGroupCount());
                for (uint64_t player1State = 0; player1State < this->getPlayer1Grouping().size() - 1; ++player1State) {
                    uint64_t player2State = player1Choices[player1State];
                    selectedRows.emplace_back(player2Matrix.getRowGroupIndices()[player2State] + player2Choices[player2State]);
                }
            }
            
            // Get the matrix and the vector induced by this selection and add entries on the diagonal in the process.
            inducedMatrix = player2Matrix.selectRowsFromRowIndexSequence(selectedRows, true);
            inducedVector.resize(inducedMatrix.getRowCount());
            storm::utility::vector::selectVectorValues<ValueType>(inducedVector, selectedRows, b);
        }
        
        template<typename ValueType>
        bool StandardGameSolver<ValueType>::player1RepresentedByMatrix() const {
            return player1Matrix != nullptr;
        }
        
        template<typename ValueType>
        storm::storage::SparseMatrix<storm::storage::sparse::state_type> const& StandardGameSolver<ValueType>::getPlayer1Matrix() const {
            STORM_LOG_ASSERT(player1RepresentedByMatrix(), "Player 1 is represented by a matrix.");
            return *player1Matrix;
        }
        
        template<typename ValueType>
        std::vector<uint64_t> const& StandardGameSolver<ValueType>::getPlayer1Grouping() const {
            STORM_LOG_ASSERT(!player1RepresentedByMatrix(), "Player 1 is represented by a matrix.");
            return *player1Grouping;
        }
        
        template<typename ValueType>
        uint64_t StandardGameSolver<ValueType>::getNumberOfPlayer1States() const {
            if (this->player1RepresentedByMatrix()) {
                return this->getPlayer1Matrix().getRowGroupCount();
            } else {
                return this->getPlayer1Grouping().size() - 1;
            }
        }
        
        template<typename ValueType>
        uint64_t StandardGameSolver<ValueType>::getNumberOfPlayer2States() const {
            return this->player2Matrix.getRowGroupCount();
        }
        
        template<typename ValueType>
        typename StandardGameSolver<ValueType>::Status StandardGameSolver<ValueType>::updateStatusIfNotConverged(Status status, std::vector<ValueType> const& x, uint64_t iterations, uint64_t maximalNumberOfIterations) const {
            if (status != Status::Converged) {
                if (this->hasCustomTerminationCondition() && this->getTerminationCondition().terminateNow(x)) {
                    status = Status::TerminatedEarly;
                } else if (iterations >= maximalNumberOfIterations) {
                    status = Status::MaximalIterationsExceeded;
                }
            }
            return status;
        }
        
        template<typename ValueType>
        void StandardGameSolver<ValueType>::reportStatus(Status status, uint64_t iterations) const {
            switch (status) {
                case Status::Converged: STORM_LOG_INFO("Iterative solver converged after " << iterations << " iterations."); break;
                case Status::TerminatedEarly: STORM_LOG_INFO("Iterative solver terminated early after " << iterations << " iterations."); break;
                case Status::MaximalIterationsExceeded: STORM_LOG_WARN("Iterative solver did not converge after " << iterations << " iterations."); break;
                default:
                    STORM_LOG_THROW(false, storm::exceptions::InvalidStateException, "Iterative solver terminated unexpectedly.");
            }
        }
        
        template<typename ValueType>
        void StandardGameSolver<ValueType>::clearCache() const {
            multiplierPlayer2Matrix.reset();
            auxiliaryP2RowVector.reset();
            auxiliaryP2RowGroupVector.reset();
            auxiliaryP1RowGroupVector.reset();
            GameSolver<ValueType>::clearCache();
        }
        
        template class StandardGameSolver<double>;
        template class StandardGameSolver<storm::RationalNumber>;
    }
}
