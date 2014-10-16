#include "src/storage/DeterministicModelStrongBisimulationDecomposition.h"

#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <iomanip>

#include "src/utility/graph.h"
#include "src/exceptions/IllegalFunctionCallException.h"

namespace storm {
    namespace storage {
        
        template<typename ValueType>
        std::size_t DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::blockId = 0;
        
        template<typename ValueType>
        DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::Block(storm::storage::sparse::state_type begin, storm::storage::sparse::state_type end, Block* prev, Block* next, std::shared_ptr<std::string> const& label) : next(next), prev(prev), begin(begin), end(end), markedAsSplitter(false), markedAsPredecessorBlock(false), markedPosition(begin), absorbing(false), id(blockId++), label(label) {
            if (next != nullptr) {
                next->prev = this;
            }
            if (prev != nullptr) {
                prev->next = this;
            }
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::print(Partition const& partition) const {
            std::cout << "block " << this->getId() << " with marked position " << this->getMarkedPosition() << " and original begin " << this->getOriginalBegin() << std::endl;
            std::cout << "begin: " << this->begin << " and end: " << this->end << " (number of states: " << this->getNumberOfStates() << ")" << std::endl;
            std::cout << "states:" << std::endl;
            for (storm::storage::sparse::state_type index = this->begin; index < this->end; ++index) {
                std::cout << partition.states[index] << " ";
            }
            std::cout << std::endl << "original: " << std::endl;
            for (storm::storage::sparse::state_type index = this->getOriginalBegin(); index < this->end; ++index) {
                std::cout << partition.states[index] << " ";
            }
            std::cout << std::endl << "values:" << std::endl;
            for (storm::storage::sparse::state_type index = this->begin; index < this->end; ++index) {
                std::cout << std::setprecision(3) << partition.values[index] << " ";
            }
            std::cout << std::endl;
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::setBegin(storm::storage::sparse::state_type begin) {
            this->begin = begin;
            this->markedPosition = begin;
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::setEnd(storm::storage::sparse::state_type end) {
            this->end = end;
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::incrementBegin() {
            ++this->begin;
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::decrementEnd() {
            ++this->begin;
        }
        
        template<typename ValueType>
        storm::storage::sparse::state_type DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getBegin() const {
            return this->begin;
        }
        
        template<typename ValueType>
        storm::storage::sparse::state_type DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getOriginalBegin() const {
            if (this->hasPreviousBlock()) {
                return this->getPreviousBlock().getEnd();
            } else {
                return 0;
            }
        }
        
        template<typename ValueType>
        storm::storage::sparse::state_type DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getEnd() const {
            return this->end;
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::setIterator(const_iterator it) {
            this->selfIt = it;
        }
        
        template<typename ValueType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::const_iterator DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getIterator() const {
            return this->selfIt;
        }
        
        template<typename ValueType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::const_iterator DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getNextIterator() const {
            return this->getNextBlock().getIterator();
        }
        
        template<typename ValueType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::const_iterator DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getPreviousIterator() const {
            return this->getPreviousBlock().getIterator();
        }
        
        template<typename ValueType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block& DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getNextBlock() {
            return *this->next;
        }
        
        template<typename ValueType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block const& DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getNextBlock() const {
            return *this->next;
        }
        
        template<typename ValueType>
        bool DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::hasNextBlock() const {
            return this->next != nullptr;
        }
        
        template<typename ValueType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block& DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getPreviousBlock() {
            return *this->prev;
        }
        
        template<typename ValueType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block* DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getPreviousBlockPointer() {
            return this->prev;
        }

        template<typename ValueType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block* DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getNextBlockPointer() {
            return this->next;
        }

        template<typename ValueType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block const& DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getPreviousBlock() const {
            return *this->prev;
        }
        
        template<typename ValueType>
        bool DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::hasPreviousBlock() const {
            return this->prev != nullptr;
        }
        
        template<typename ValueType>
        bool DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::check() const {
            if (this->begin >= this->end) {
                assert(false);
            }
            if (this->prev != nullptr) {
                assert (this->prev->next == this);
            }
            if (this->next != nullptr) {
                assert (this->next->prev == this);
            }
            return true;
        }
        
        template<typename ValueType>
        std::size_t DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getNumberOfStates() const {
            // We need to take the original begin here, because the begin is temporarily moved.
            return (this->end - this->getOriginalBegin());
        }
        
        template<typename ValueType>
        bool DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::isMarkedAsSplitter() const {
            return this->markedAsSplitter;
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::markAsSplitter() {
            this->markedAsSplitter = true;
        }

        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::unmarkAsSplitter() {
            this->markedAsSplitter = false;
        }

        template<typename ValueType>
        std::size_t DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getId() const {
            return this->id;
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::setAbsorbing(bool absorbing) {
            this->absorbing = absorbing;
        }
        
        template<typename ValueType>
        bool DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::isAbsorbing() const {
            return this->absorbing;
        }
        
        template<typename ValueType>
        storm::storage::sparse::state_type DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getMarkedPosition() const {
            return this->markedPosition;
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::setMarkedPosition(storm::storage::sparse::state_type position) {
            this->markedPosition = position;
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::resetMarkedPosition() {
            this->markedPosition = this->begin;
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::incrementMarkedPosition() {
            ++this->markedPosition;
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::markAsPredecessorBlock() {
            this->markedAsPredecessorBlock = true;
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::unmarkAsPredecessorBlock() {
            this->markedAsPredecessorBlock = false;
        }
        
        template<typename ValueType>
        bool DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::isMarkedAsPredecessor() const {
            return markedAsPredecessorBlock;
        }
        
        template<typename ValueType>
        bool DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::hasLabel() const {
            return this->label != nullptr;
        }
        
        template<typename ValueType>
        std::string const& DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getLabel() const {
            STORM_LOG_THROW(this->label != nullptr, storm::exceptions::IllegalFunctionCallException, "Unable to retrieve label of block that has none.");
            return *this->label;
        }
        
        template<typename ValueType>
        std::shared_ptr<std::string> const& DeterministicModelStrongBisimulationDecomposition<ValueType>::Block::getLabelPtr() const {
            return this->label;
        }
        
        template<typename ValueType>
        DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::Partition(std::size_t numberOfStates) : stateToBlockMapping(numberOfStates), states(numberOfStates), positions(numberOfStates), values(numberOfStates) {
            // Create the block and give it an iterator to itself.
            typename std::list<Block>::iterator it = blocks.emplace(this->blocks.end(), 0, numberOfStates, nullptr, nullptr);
            it->setIterator(it);
            
            // Set up the different parts of the internal structure.
            for (storm::storage::sparse::state_type state = 0; state < numberOfStates; ++state) {
                states[state] = state;
                positions[state] = state;
                stateToBlockMapping[state] = &blocks.back();
            }
        }
        
        template<typename ValueType>
        DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::Partition(std::size_t numberOfStates, storm::storage::BitVector const& prob0States, storm::storage::BitVector const& prob1States, std::string const& otherLabel, std::string const& prob1Label) : stateToBlockMapping(numberOfStates), states(numberOfStates), positions(numberOfStates), values(numberOfStates) {
            typename std::list<Block>::iterator firstIt = blocks.emplace(this->blocks.end(), 0, prob0States.getNumberOfSetBits(), nullptr, nullptr);
            Block& firstBlock = *firstIt;
            firstBlock.setIterator(firstIt);

            storm::storage::sparse::state_type position = 0;
            for (auto state : prob0States) {
                states[position] = state;
                positions[state] = position;
                stateToBlockMapping[state] = &firstBlock;
                ++position;
            }
            firstBlock.setAbsorbing(true);

            typename std::list<Block>::iterator secondIt = blocks.emplace(this->blocks.end(), position, position + prob1States.getNumberOfSetBits(), &firstBlock, nullptr, std::shared_ptr<std::string>(new std::string(prob1Label)));
            Block& secondBlock = *secondIt;
            secondBlock.setIterator(secondIt);
            
            for (auto state : prob1States) {
                states[position] = state;
                positions[state] = position;
                stateToBlockMapping[state] = &secondBlock;
                ++position;
            }
            secondBlock.setAbsorbing(true);
            
            typename std::list<Block>::iterator thirdIt = blocks.emplace(this->blocks.end(), position, numberOfStates, &secondBlock, nullptr, otherLabel == "true" ? std::shared_ptr<std::string>(nullptr) : std::shared_ptr<std::string>(new std::string(otherLabel)));
            Block& thirdBlock = *thirdIt;
            thirdBlock.setIterator(thirdIt);
            
            storm::storage::BitVector otherStates = ~(prob0States | prob1States);
            for (auto state : otherStates) {
                states[position] = state;
                positions[state] = position;
                stateToBlockMapping[state] = &thirdBlock;
                ++position;
            }
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::swapStates(storm::storage::sparse::state_type state1, storm::storage::sparse::state_type state2) {
            std::swap(this->states[this->positions[state1]], this->states[this->positions[state2]]);
            std::swap(this->values[this->positions[state1]], this->values[this->positions[state2]]);
            std::swap(this->positions[state1], this->positions[state2]);
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::swapStatesAtPositions(storm::storage::sparse::state_type position1, storm::storage::sparse::state_type position2) {
            storm::storage::sparse::state_type state1 = this->states[position1];
            storm::storage::sparse::state_type state2 = this->states[position2];
            
            std::swap(this->states[position1], this->states[position2]);
            std::swap(this->values[position1], this->values[position2]);
            
            this->positions[state1] = position2;
            this->positions[state2] = position1;
        }
        
        template<typename ValueType>
        storm::storage::sparse::state_type const& DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getPosition(storm::storage::sparse::state_type state) const {
            return this->positions[state];
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::setPosition(storm::storage::sparse::state_type state, storm::storage::sparse::state_type position) {
            this->positions[state] = position;
        }
        
        template<typename ValueType>
        storm::storage::sparse::state_type const& DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getState(storm::storage::sparse::state_type position) const {
            return this->states[position];
        }
        
        template<typename ValueType>
        ValueType const& DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getValue(storm::storage::sparse::state_type state) const {
            return this->values[this->positions[state]];
        }
        
        template<typename ValueType>
        ValueType const& DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getValueAtPosition(storm::storage::sparse::state_type position) const {
            return this->values[position];
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::setValue(storm::storage::sparse::state_type state, ValueType value) {
            this->values[this->positions[state]] = value;
        }
        
        template<typename ValueType>
        std::vector<ValueType>& DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getValues() {
            return this->values;
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::increaseValue(storm::storage::sparse::state_type state, ValueType value) {
            this->values[this->positions[state]] += value;
        }

        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::updateBlockMapping(Block& block, std::vector<storm::storage::sparse::state_type>::iterator first, std::vector<storm::storage::sparse::state_type>::iterator last) {
            for (; first != last; ++first) {
                this->stateToBlockMapping[*first] = &block;
            }
        }
        
        template<typename ValueType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block& DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getFirstBlock() {
            return *this->blocks.begin();
        }
        
        template<typename ValueType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block& DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getBlock(storm::storage::sparse::state_type state) {
            return *this->stateToBlockMapping[state];
        }

        template<typename ValueType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block const& DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getBlock(storm::storage::sparse::state_type state) const {
            return *this->stateToBlockMapping[state];
        }

        template<typename ValueType>
        std::vector<storm::storage::sparse::state_type>::iterator DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getBeginOfStates(Block const& block) {
            return this->states.begin() + block.getBegin();
        }
        
        template<typename ValueType>
        std::vector<storm::storage::sparse::state_type>::iterator DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getEndOfStates(Block const& block) {
            return this->states.begin() + block.getEnd();
        }

        template<typename ValueType>
        std::vector<storm::storage::sparse::state_type>::const_iterator DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getBeginOfStates(Block const& block) const {
            return this->states.begin() + block.getBegin();
        }
        
        template<typename ValueType>
        std::vector<storm::storage::sparse::state_type>::const_iterator DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getEndOfStates(Block const& block) const {
            return this->states.begin() + block.getEnd();
        }
        
        template<typename ValueType>
        typename std::vector<ValueType>::iterator DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getBeginOfValues(Block const& block) {
            return this->values.begin() + block.getBegin();
        }
        
        template<typename ValueType>
        typename std::vector<ValueType>::iterator DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getEndOfValues(Block const& block) {
            return this->values.begin() + block.getEnd();
        }
        
        template<typename ValueType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block& DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::splitBlock(Block& block, storm::storage::sparse::state_type position) {
            // FIXME: this could be improved by splitting off the smaller of the two resulting blocks.
            
            // In case one of the resulting blocks would be empty, we simply return the current block and do not create
            // a new one.
            if (position == block.getBegin() || position == block.getEnd()) {
                return block;
            }
            
            // Actually create the new block and insert it at the correct position.
            typename std::list<Block>::iterator selfIt = this->blocks.emplace(block.getIterator(), block.getBegin(), position, block.getPreviousBlockPointer(), &block, block.getLabelPtr());
            selfIt->setIterator(selfIt);
            Block& newBlock = *selfIt;
            
            // Make the current block end at the given position.
            block.setBegin(position);
            
            // Update the mapping of the states in the newly created block.
            for (auto it = this->getBeginOfStates(newBlock), ite = this->getEndOfStates(newBlock); it != ite; ++it) {
                stateToBlockMapping[*it] = &newBlock;
            }
            
            return newBlock;
        }
        
        template<typename ValueType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block& DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::insertBlock(Block& block) {
            // Find the beginning of the new block.
            storm::storage::sparse::state_type begin;
            if (block.hasPreviousBlock()) {
                begin = block.getPreviousBlock().getEnd();
            } else {
                begin = 0;
            }
            
            // Actually insert the new block.
            typename std::list<Block>::iterator it = this->blocks.emplace(block.getIterator(), begin, block.getBegin(), block.getPreviousBlockPointer(), &block);
            Block& newBlock = *it;
            newBlock.setIterator(it);
            
            // Update the mapping of the states in the newly created block.
            for (auto it = this->getBeginOfStates(newBlock), ite = this->getEndOfStates(newBlock); it != ite; ++it) {
                stateToBlockMapping[*it] = &newBlock;
            }
            
            return *it;
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::splitLabel(storm::storage::BitVector const& statesWithLabel) {
            for (auto blockIterator = this->blocks.begin(), ite = this->blocks.end(); blockIterator != ite; ) { // The update of the loop was intentionally moved to the bottom of the loop.
                Block& block = *blockIterator;
                
                // Sort the range of the block such that all states that have the label are moved to the front.
                std::sort(this->getBeginOfStates(block), this->getEndOfStates(block), [&statesWithLabel] (storm::storage::sparse::state_type const& a, storm::storage::sparse::state_type const& b) { return statesWithLabel.get(a) && !statesWithLabel.get(b); } );
                
                // Update the positions vector.
                storm::storage::sparse::state_type position = block.getBegin();
                for (auto stateIt = this->getBeginOfStates(block), stateIte = this->getEndOfStates(block); stateIt != stateIte; ++stateIt, ++position) {
                    this->positions[*stateIt] = position;
                }
                
                // Now we can find the first position in the block that does not have the label and create new blocks.
                std::vector<storm::storage::sparse::state_type>::iterator it = std::find_if(this->getBeginOfStates(block), this->getEndOfStates(block), [&] (storm::storage::sparse::state_type const& a) { return !statesWithLabel.get(a); });
                
                // If not all the states agreed on the validity of the label, we need to split the block.
                if (it != this->getBeginOfStates(block) && it != this->getEndOfStates(block)) {
                    auto cutPoint = std::distance(this->states.begin(), it);
                    this->splitBlock(block, cutPoint);
                } else {
                    // Otherwise, we simply proceed to the next block.
                    ++blockIterator;
                }
            }
        }
        
        template<typename ValueType>
        std::list<typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block> const& DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getBlocks() const {
            return this->blocks;
        }
        
        template<typename ValueType>
        std::vector<storm::storage::sparse::state_type>& DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getStates() {
            return this->states;
        }
        
        template<typename ValueType>
        std::list<typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Block>& DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::getBlocks() {
            return this->blocks;
        }
        
        template<typename ValueType>
        bool DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::check() const {
            for (uint_fast64_t state = 0; state < this->positions.size(); ++state) {
                if (this->states[this->positions[state]] != state) {
                    assert(false);
                }
            }
            for (auto const& block : this->blocks) {
                assert(block.check());
                for (auto stateIt = this->getBeginOfStates(block), stateIte = this->getEndOfStates(block); stateIt != stateIte; ++stateIt) {
                    if (this->stateToBlockMapping[*stateIt] != &block) {
                        assert(false);
                    }
                }
            }
            return true;
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::print() const {
            for (auto const& block : this->blocks) {
                block.print(*this);
            }
            std::cout << "states in partition" << std::endl;
            for (auto const& state : states) {
                std::cout << state << " ";
            }
            std::cout << std::endl << "positions: " << std::endl;
            for (auto const& index : positions) {
                std::cout << index << " ";
            }
            std::cout << std::endl << "state to block mapping: " << std::endl;
            for (auto const& block : stateToBlockMapping) {
                std::cout << block << " ";
            }
            std::cout << std::endl;
            std::cout << "size: " << this->blocks.size() << std::endl;
            assert(this->check());
        }
        
        template<typename ValueType>
        std::size_t DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition::size() const {
            return this->blocks.size();
        }
        
        template<typename ValueType>
        DeterministicModelStrongBisimulationDecomposition<ValueType>::DeterministicModelStrongBisimulationDecomposition(storm::models::Dtmc<ValueType> const& model, bool buildQuotient) {
            STORM_LOG_THROW(!model.hasStateRewards() && !model.hasTransitionRewards(), storm::exceptions::IllegalFunctionCallException, "Bisimulation is currently only supported for models without reward structures.");
            Partition initialPartition = getLabelBasedInitialPartition(model);
            partitionRefinement(model, model.getBackwardTransitions(), initialPartition, buildQuotient);
        }

        template<typename ValueType>
        DeterministicModelStrongBisimulationDecomposition<ValueType>::DeterministicModelStrongBisimulationDecomposition(storm::models::Ctmc<ValueType> const& model, bool buildQuotient) {
            STORM_LOG_THROW(!model.hasStateRewards() && !model.hasTransitionRewards(), storm::exceptions::IllegalFunctionCallException, "Bisimulation is currently only supported for models without reward structures.");
            Partition initialPartition = getLabelBasedInitialPartition(model);
            partitionRefinement(model, model.getBackwardTransitions(), initialPartition, buildQuotient);
        }
        
        template<typename ValueType>
        DeterministicModelStrongBisimulationDecomposition<ValueType>::DeterministicModelStrongBisimulationDecomposition(storm::models::Dtmc<ValueType> const& model, std::string const& phiLabel, std::string const& psiLabel, bool bounded, bool buildQuotient) {
            STORM_LOG_THROW(!model.hasStateRewards() && !model.hasTransitionRewards(), storm::exceptions::IllegalFunctionCallException, "Bisimulation is currently only supported for models without reward structures.");
            storm::storage::SparseMatrix<ValueType> backwardTransitions = model.getBackwardTransitions();
            Partition initialPartition = getMeasureDrivenInitialPartition(model, backwardTransitions, phiLabel, psiLabel, bounded);
            partitionRefinement(model, model.getBackwardTransitions(), initialPartition, buildQuotient);
        }
        
        template<typename ValueType>
        DeterministicModelStrongBisimulationDecomposition<ValueType>::DeterministicModelStrongBisimulationDecomposition(storm::models::Ctmc<ValueType> const& model, std::string const& phiLabel, std::string const& psiLabel, bool bounded, bool buildQuotient) {
            STORM_LOG_THROW(!model.hasStateRewards() && !model.hasTransitionRewards(), storm::exceptions::IllegalFunctionCallException, "Bisimulation is currently only supported for models without reward structures.");
            storm::storage::SparseMatrix<ValueType> backwardTransitions = model.getBackwardTransitions();
            Partition initialPartition = getMeasureDrivenInitialPartition(model, backwardTransitions, phiLabel, psiLabel, bounded);
            partitionRefinement(model, model.getBackwardTransitions(), initialPartition, buildQuotient);
        }
        
        template<typename ValueType>
        std::shared_ptr<storm::models::AbstractDeterministicModel<ValueType>> DeterministicModelStrongBisimulationDecomposition<ValueType>::getQuotient() const {
            STORM_LOG_THROW(this->quotient != nullptr, storm::exceptions::IllegalFunctionCallException, "Unable to retrieve quotient model from bisimulation decomposition, because it was not built.");
            return this->quotient;
        }
        
        template<typename ValueType>
        template<typename ModelType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::buildQuotient(ModelType const& model, Partition const& partition) {
            // In order to create the quotient model, we need to construct
            // (a) the new transition matrix,
            // (b) the new labeling,
            // (c) the new reward structures.
            
            // Prepare a matrix builder for (a).
            storm::storage::SparseMatrixBuilder<ValueType> builder(this->size(), this->size());
            
            // Prepare the new state labeling for (b).
            storm::models::AtomicPropositionsLabeling newLabeling(this->size(), model.getStateLabeling().getNumberOfAtomicPropositions());
            std::set<std::string> atomicPropositionsSet = model.getStateLabeling().getAtomicPropositions();
            std::vector<std::string> atomicPropositions = std::vector<std::string>(atomicPropositionsSet.begin(), atomicPropositionsSet.end());
            for (auto const& ap : atomicPropositions) {
                newLabeling.addAtomicProposition(ap);
            }
            
            // Now build (a) and (b) by traversing all blocks.
            for (uint_fast64_t blockIndex = 0; blockIndex < this->blocks.size(); ++blockIndex) {
                auto const& block = this->blocks[blockIndex];

                // Pick one representative state. It doesn't matter which state it is, because they all behave equally.
                storm::storage::sparse::state_type representativeState = *block.begin();
                Block const& oldBlock = partition.getBlock(representativeState);
                
                // If the block is absorbing, we simply add a self-loop.
                if (oldBlock.isAbsorbing()) {
                    builder.addNextValue(blockIndex, blockIndex, storm::utility::constantOne<ValueType>());
                    
                    if (oldBlock.hasLabel()) {
                        newLabeling.addAtomicPropositionToState(oldBlock.getLabel(), blockIndex);
                    }
                } else {
                    // Compute the outgoing transitions of the block.
                    std::map<storm::storage::sparse::state_type, ValueType> blockProbability;
                    for (auto const& entry : model.getTransitionMatrix().getRow(representativeState)) {
                        storm::storage::sparse::state_type targetBlock = partition.getBlock(entry.getColumn()).getId();
                        auto probIterator = blockProbability.find(targetBlock);
                        if (probIterator != blockProbability.end()) {
                            probIterator->second += entry.getValue();
                        } else {
                            blockProbability[targetBlock] = entry.getValue();
                        }
                    }
                    
                    // Now add them to the actual matrix.
                    for (auto const& probabilityEntry : blockProbability) {
                        builder.addNextValue(blockIndex, probabilityEntry.first, probabilityEntry.second);
                    }
                    
                    // If the block has a special label, we only add that to the block.
                    if (oldBlock.hasLabel()) {
                        newLabeling.addAtomicPropositionToState(oldBlock.getLabel(), blockIndex);
                    } else {
                        // Otherwise add all atomic propositions to the equivalence class that the representative state
                        // satisfies.
                        for (auto const& ap : atomicPropositions) {
                            if (model.getStateLabeling().getStateHasAtomicProposition(ap, representativeState)) {
                                newLabeling.addAtomicPropositionToState(ap, blockIndex);
                            }
                        }
                    }
                }
            }
                        
            // Now check which of the blocks of the partition contain at least one initial state.
            for (auto initialState : model.getInitialStates()) {
                Block const& initialBlock = partition.getBlock(initialState);
                newLabeling.addAtomicPropositionToState("init", initialBlock.getId());
            }
                        
            // FIXME:
            // If reward structures are allowed, the quotient structures need to be built here.
            
            // Finally construct the quotient model.
            this->quotient = std::shared_ptr<storm::models::AbstractDeterministicModel<ValueType>>(new ModelType(builder.build(), std::move(newLabeling)));
        }
        
        template<typename ValueType>
        template<typename ModelType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::partitionRefinement(ModelType const& model, storm::storage::SparseMatrix<ValueType> const& backwardTransitions, Partition& partition, bool buildQuotient) {
            std::chrono::high_resolution_clock::time_point totalStart = std::chrono::high_resolution_clock::now();
            
            // Initially, all blocks are potential splitter, so we insert them in the splitterQueue.
            std::deque<Block*> splitterQueue;
            std::for_each(partition.getBlocks().begin(), partition.getBlocks().end(), [&] (Block& a) { splitterQueue.push_back(&a); });
            
            // Then perform the actual splitting until there are no more splitters.
            while (!splitterQueue.empty()) {
                refinePartition(backwardTransitions, *splitterQueue.front(), partition, splitterQueue);
                splitterQueue.pop_front();
            }
            
            // Now move the states from the internal partition into their final place in the decomposition. We do so in
            // a way that maintains the block IDs as indices.
            this->blocks.resize(partition.size());
            for (auto const& block : partition.getBlocks()) {
                // We need to sort the states to allow for rapid construction of the blocks.
                std::sort(partition.getBeginOfStates(block), partition.getEndOfStates(block));
                this->blocks[block.getId()] = block_type(partition.getBeginOfStates(block), partition.getEndOfStates(block), true);
            }

            // If we are required to build the quotient model, do so now.
            if (buildQuotient) {
                this->buildQuotient(model, partition);
            }
            
            std::chrono::high_resolution_clock::duration totalTime = std::chrono::high_resolution_clock::now() - totalStart;
            
            if (storm::settings::generalSettings().isShowStatisticsSet()) {
                std::cout << "Computed bisimulation quotient in " << std::chrono::duration_cast<std::chrono::milliseconds>(totalTime).count() << "ms." << std::endl;
            }
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::refineBlockProbabilities(Block& block, Partition& partition, std::deque<Block*>& splitterQueue) {
            // First, we simplify all the values. For most types this does not change anything, but for rational
            // functions, this achieves a canonicity that is used for sorting the rational functions later.
            for (auto probabilityIt = partition.getBeginOfValues(block), probabilityIte = partition.getEndOfValues(block); probabilityIt != probabilityIte; ++probabilityIt) {
                storm::utility::simplify(*probabilityIt);
            }
            
            // Sort the states in the block based on their probabilities.
            std::sort(partition.getBeginOfStates(block), partition.getEndOfStates(block), [&partition] (storm::storage::sparse::state_type const& a, storm::storage::sparse::state_type const& b) { return partition.getValue(a) < partition.getValue(b); } );
            
            // FIXME: This can probably be done more efficiently.
            std::sort(partition.getBeginOfValues(block), partition.getEndOfValues(block));
            
            // Update the positions vector.
            storm::storage::sparse::state_type position = block.getBegin();
            for (auto stateIt = partition.getBeginOfStates(block), stateIte = partition.getEndOfStates(block); stateIt != stateIte; ++stateIt, ++position) {
                partition.setPosition(*stateIt, position);
            }
            
            // Finally, we need to scan the ranges of states that agree on the probability.
            storm::storage::sparse::state_type beginIndex = block.getBegin();
            storm::storage::sparse::state_type currentIndex = beginIndex;
            storm::storage::sparse::state_type endIndex = block.getEnd();
            
            // Now we can check whether the block needs to be split, which is the case iff the probabilities for the
            // first and the last state are different.
            while (!comparator.isEqual(partition.getValueAtPosition(beginIndex), partition.getValueAtPosition(endIndex - 1))) {
                // Now we scan for the first state in the block that disagrees on the probability value.
                // Note that we do not have to check currentIndex for staying within bounds, because we know the matching
                // state is within bounds.
                ValueType const& currentValue = partition.getValueAtPosition(beginIndex);
                
                typename std::vector<ValueType>::const_iterator valueIterator = partition.getValues().begin() + beginIndex + 1;
                ++currentIndex;
                while (currentIndex < endIndex && comparator.isEqual(*valueIterator, currentValue)) {
                    ++valueIterator;
                    ++currentIndex;
                }
                
                // Now we split the block and mark it as a potential splitter.
                Block& newBlock = partition.splitBlock(block, currentIndex);
                if (!newBlock.isMarkedAsSplitter()) {
                    splitterQueue.push_back(&newBlock);
                    newBlock.markAsSplitter();
                }
                beginIndex = currentIndex;
            }
        }
        
        template<typename ValueType>
        void DeterministicModelStrongBisimulationDecomposition<ValueType>::refinePartition(storm::storage::SparseMatrix<ValueType> const& backwardTransitions, Block& splitter, Partition& partition, std::deque<Block*>& splitterQueue) {
            std::list<Block*> predecessorBlocks;
            
            // Iterate over all states of the splitter and check its predecessors.
            storm::storage::sparse::state_type currentPosition = splitter.getBegin();
            for (auto stateIterator = partition.getBeginOfStates(splitter), stateIte = partition.getEndOfStates(splitter); stateIterator != stateIte; ++stateIterator, ++currentPosition) {
                storm::storage::sparse::state_type currentState = *stateIterator;
                
                uint_fast64_t elementsToSkip = 0;
                for (auto const& predecessorEntry : backwardTransitions.getRow(currentState)) {
                    storm::storage::sparse::state_type predecessor = predecessorEntry.getColumn();
                    
                    Block& predecessorBlock = partition.getBlock(predecessor);
                    
                    // If the predecessor block has just one state, there is no point in splitting it.
                    if (predecessorBlock.getNumberOfStates() <= 1 || predecessorBlock.isAbsorbing()) {
                        continue;
                    }
                    
                    storm::storage::sparse::state_type predecessorPosition = partition.getPosition(predecessor);
                    
                    // If we have not seen this predecessor before, we move it to a part near the beginning of the block.
                    if (predecessorPosition >= predecessorBlock.getBegin()) {
                        if (&predecessorBlock == &splitter) {
                            // If the predecessor we just found was already processed (in terms of visiting its predecessors),
                            // we swap it with the state that is currently at the beginning of the block and move the start
                            // of the block one step further.
                            if (predecessorPosition <= currentPosition + elementsToSkip) {
                                partition.swapStates(predecessor, partition.getState(predecessorBlock.getBegin()));
                                predecessorBlock.incrementBegin();
                            } else {
                                // Otherwise, we need to move the predecessor, but we need to make sure that we explore its
                                // predecessors later.
                                if (predecessorBlock.getMarkedPosition() == predecessorBlock.getBegin()) {
                                    partition.swapStatesAtPositions(predecessorBlock.getMarkedPosition(), predecessorPosition);
                                    partition.swapStatesAtPositions(predecessorPosition, currentPosition + elementsToSkip + 1);
                                } else {
                                    partition.swapStatesAtPositions(predecessorBlock.getMarkedPosition(), predecessorPosition);
                                    partition.swapStatesAtPositions(predecessorPosition, predecessorBlock.getBegin());
                                    partition.swapStatesAtPositions(predecessorPosition, currentPosition + elementsToSkip + 1);
                                }
                                
                                ++elementsToSkip;
                                predecessorBlock.incrementMarkedPosition();
                                predecessorBlock.incrementBegin();
                            }
                        } else {
                            partition.swapStates(predecessor, partition.getState(predecessorBlock.getBegin()));
                            predecessorBlock.incrementBegin();
                        }
                        partition.setValue(predecessor, predecessorEntry.getValue());
                    } else {
                        // Otherwise, we just need to update the probability for this predecessor.
                        partition.increaseValue(predecessor, predecessorEntry.getValue());
                    }
                    
                    if (!predecessorBlock.isMarkedAsPredecessor()) {
                        predecessorBlocks.emplace_back(&predecessorBlock);
                        predecessorBlock.markAsPredecessorBlock();
                    }
                }
                
                // If we had to move some elements beyond the current element, we may have to skip them.
                if (elementsToSkip > 0) {
                    stateIterator += elementsToSkip;
                    currentPosition += elementsToSkip;
                }
            }
            
            // Now we can traverse the list of states of the splitter whose predecessors we have not yet explored.
            for (auto stateIterator = partition.getStates().begin() + splitter.getOriginalBegin(), stateIte = partition.getStates().begin() + splitter.getMarkedPosition(); stateIterator != stateIte; ++stateIterator) {
                storm::storage::sparse::state_type currentState = *stateIterator;
                
                for (auto const& predecessorEntry : backwardTransitions.getRow(currentState)) {
                    storm::storage::sparse::state_type predecessor = predecessorEntry.getColumn();
                    Block& predecessorBlock = partition.getBlock(predecessor);
                    storm::storage::sparse::state_type predecessorPosition = partition.getPosition(predecessor);
                    
                    if (predecessorPosition >= predecessorBlock.getBegin()) {
                        partition.swapStatesAtPositions(predecessorPosition, predecessorBlock.getBegin());
                        predecessorBlock.incrementBegin();
                        partition.setValue(predecessor, predecessorEntry.getValue());
                    } else {
                        partition.increaseValue(predecessor, predecessorEntry.getValue());
                    }
                    
                    if (!predecessorBlock.isMarkedAsPredecessor()) {
                        predecessorBlocks.emplace_back(&predecessorBlock);
                        predecessorBlock.markAsPredecessorBlock();
                    }
                }
            }
            
            // Reset the marked position of the splitter to the begin.
            splitter.setMarkedPosition(splitter.getBegin());
            
            std::list<Block*> blocksToSplit;
            
            // Now, we can iterate over the predecessor blocks and see whether we have to create a new block for
            // predecessors of the splitter.
            for (auto blockPtr : predecessorBlocks) {
                Block& block = *blockPtr;
                
                block.unmarkAsPredecessorBlock();
                block.resetMarkedPosition();
                
                // If we have moved the begin of the block to somewhere in the middle of the block, we need to split it.
                if (block.getBegin() != block.getEnd()) {
                    Block& newBlock = partition.insertBlock(block);
                    if (!newBlock.isMarkedAsSplitter()) {
                        splitterQueue.push_back(&newBlock);
                        newBlock.markAsSplitter();
                    }
                    
                    // Schedule the block of predecessors for refinement based on probabilities.
                    blocksToSplit.emplace_back(&newBlock);
                } else {
                    // In this case, we can keep the block by setting its begin to the old value.
                    block.setBegin(block.getOriginalBegin());
                    blocksToSplit.emplace_back(&block);
                }
            }
            
            // Finally, we walk through the blocks that have a transition to the splitter and split them using
            // probabilistic information.
            for (auto blockPtr : blocksToSplit) {
                if (blockPtr->getNumberOfStates() <= 1) {
                    continue;
                }
                
                refineBlockProbabilities(*blockPtr, partition, splitterQueue);
            }
        }
        
        template<typename ValueType>
        template<typename ModelType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition DeterministicModelStrongBisimulationDecomposition<ValueType>::getMeasureDrivenInitialPartition(ModelType const& model, storm::storage::SparseMatrix<ValueType> const& backwardTransitions, std::string const& phiLabel, std::string const& psiLabel, bool bounded) {
            std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01 = storm::utility::graph::performProb01(backwardTransitions, phiLabel == "true" ? storm::storage::BitVector(model.getNumberOfStates(), true) : model.getLabeledStates(phiLabel), model.getLabeledStates(psiLabel));
            Partition partition(model.getNumberOfStates(), statesWithProbability01.first, bounded ? model.getLabeledStates(psiLabel) : statesWithProbability01.second, phiLabel, psiLabel);
            return partition;
        }
        
        template<typename ValueType>
        template<typename ModelType>
        typename DeterministicModelStrongBisimulationDecomposition<ValueType>::Partition DeterministicModelStrongBisimulationDecomposition<ValueType>::getLabelBasedInitialPartition(ModelType const& model) {
            Partition partition(model.getNumberOfStates());
            for (auto const& label : model.getStateLabeling().getAtomicPropositions()) {
                if (label == "init") {
                    continue;
                }
                partition.splitLabel(model.getLabeledStates(label));
            }
            return partition;
        }
        
        template class DeterministicModelStrongBisimulationDecomposition<double>;
        template class DeterministicModelStrongBisimulationDecomposition<storm::RationalFunction>;
    }
}