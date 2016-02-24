#include "DFTState.h"
#include "DFTElements.h"
#include "DFT.h"

namespace storm {
    namespace storage {

        template<typename ValueType>
        DFTState<ValueType>::DFTState(DFT<ValueType> const& dft, DFTStateGenerationInfo const& stateGenerationInfo, size_t id) : mStatus(dft.stateVectorSize()), mId(id), mDft(dft), mStateGenerationInfo(stateGenerationInfo)  {
            
            // Initialize uses
            for(size_t id  : mDft.getSpareIndices()) {
                std::shared_ptr<DFTGate<ValueType> const> elem = mDft.getGate(id);
                assert(elem->isSpareGate());
                assert(elem->nrChildren() > 0);
                this->setUses(id, elem->children()[0]->id());
            }
            
            // Initialize activation
            propagateActivation(mDft.getTopLevelIndex());

            std::vector<size_t> alwaysActiveBEs = dft.nonColdBEs();
            mIsCurrentlyFailableBE.insert(mIsCurrentlyFailableBE.end(), alwaysActiveBEs.begin(), alwaysActiveBEs.end());
        }

        template<typename ValueType>
        DFTElementState DFTState<ValueType>::getElementState(size_t id) const {
            return static_cast<DFTElementState>(getElementStateInt(id));
        }
        
        template<typename ValueType>
        DFTDependencyState DFTState<ValueType>::getDependencyState(size_t id) const {
            return static_cast<DFTDependencyState>(getElementStateInt(id));
        }

        template<typename ValueType>
        int DFTState<ValueType>::getElementStateInt(size_t id) const {
            return mStatus.getAsInt(mStateGenerationInfo.getStateIndex(id), 2);
        }

        template<typename ValueType>
        size_t DFTState<ValueType>::getId() const {
            return mId;
        }

        template<typename ValueType>
        void DFTState<ValueType>::setId(size_t id) {
            mId = id;
        }

        template<typename ValueType>
        bool DFTState<ValueType>::isOperational(size_t id) const {
            return getElementState(id) == DFTElementState::Operational;
        }

        template<typename ValueType>
        bool DFTState<ValueType>::hasFailed(size_t id) const {
            return mStatus[mStateGenerationInfo.getStateIndex(id)];
        }
        
        template<typename ValueType>
        bool DFTState<ValueType>::hasFailed(storm::storage::BitVector const& state, size_t indexId) {
            return state[indexId];
        }

        template<typename ValueType>
        bool DFTState<ValueType>::isFailsafe(size_t id) const {
            return mStatus[mStateGenerationInfo.getStateIndex(id)+1];
        }
        
        template<typename ValueType>
        bool DFTState<ValueType>::isFailsafe(storm::storage::BitVector const& state, size_t indexId) {
            return state[indexId+1];
        }

        template<typename ValueType>
        bool DFTState<ValueType>::dontCare(size_t id) const {
            return getElementState(id) == DFTElementState::DontCare;
        }
        
        template<typename ValueType>
        bool DFTState<ValueType>::dependencyTriggered(size_t id) const {
            return getElementStateInt(id) > 0;
        }
        
        template<typename ValueType>
        bool DFTState<ValueType>::dependencySuccessful(size_t id) const {
            return mStatus[mStateGenerationInfo.getStateIndex(id)];
        }
        template<typename ValueType>
        bool DFTState<ValueType>::dependencyUnsuccessful(size_t id) const {
            return mStatus[mStateGenerationInfo.getStateIndex(id)+1];
        }

        template<typename ValueType>
        void DFTState<ValueType>::setFailed(size_t id) {
            mStatus.set(mStateGenerationInfo.getStateIndex(id));
        }

        template<typename ValueType>
        void DFTState<ValueType>::setFailsafe(size_t id) {
            mStatus.set(mStateGenerationInfo.getStateIndex(id)+1);
        }

        template<typename ValueType>
        void DFTState<ValueType>::setDontCare(size_t id) {
            mStatus.setFromInt(mStateGenerationInfo.getStateIndex(id), 2, static_cast<uint_fast64_t>(DFTElementState::DontCare) );
        }
        
        template<typename ValueType>
        void DFTState<ValueType>::setDependencySuccessful(size_t id) {
            // No distinction between successful dependency and no dependency at all
            // => we do not set bit
            //mStatus.set(mStateGenerationInfo.mIdToStateIndex(id));
        }

        template<typename ValueType>
        void DFTState<ValueType>::setDependencyUnsuccessful(size_t id) {
            mStatus.set(mStateGenerationInfo.getStateIndex(id)+1);
        }

        template<typename ValueType>
        void DFTState<ValueType>::beNoLongerFailable(size_t id) {
            auto it = std::find(mIsCurrentlyFailableBE.begin(), mIsCurrentlyFailableBE.end(), id);
            if(it != mIsCurrentlyFailableBE.end()) {
                mIsCurrentlyFailableBE.erase(it);
            }
        }

        template<typename ValueType>
        bool DFTState<ValueType>::updateFailableDependencies(size_t id) {
            assert(hasFailed(id));
            for (size_t i = 0; i < mDft.getDependencies().size(); ++i) {
                std::shared_ptr<DFTDependency<ValueType> const> dependency = mDft.getDependency(mDft.getDependencies()[i]);
                if (dependency->triggerEvent()->id() == id) {
                    if (getElementState(dependency->dependentEvent()->id()) == DFTElementState::Operational) {
                        assert(!isFailsafe(dependency->dependentEvent()->id()));
                        mFailableDependencies.push_back(dependency->id());
                        STORM_LOG_TRACE("New dependency failure: " << dependency->toString());
                    }
                }
            }
            return nrFailableDependencies() > 0;
        }

        template<typename ValueType>
        std::pair<std::shared_ptr<DFTBE<ValueType> const>, bool> DFTState<ValueType>::letNextBEFail(size_t index)
        {
            STORM_LOG_TRACE("currently failable: " << getCurrentlyFailableString());
            if (nrFailableDependencies() > 0) {
                // Consider failure due to dependency
                assert(index < nrFailableDependencies());
                std::shared_ptr<DFTDependency<ValueType> const> dependency = mDft.getDependency(getDependencyId(index));
                std::pair<std::shared_ptr<DFTBE<ValueType> const>,bool> res(mDft.getBasicElement(dependency->dependentEvent()->id()), true);
                mFailableDependencies.erase(mFailableDependencies.begin() + index);
                setFailed(res.first->id());
                setDependencySuccessful(dependency->id());
                return res;
            } else {
                // Consider "normal" failure
                assert(index < nrFailableBEs());
                std::pair<std::shared_ptr<DFTBE<ValueType> const>,bool> res(mDft.getBasicElement(mIsCurrentlyFailableBE[index]), false);
                mIsCurrentlyFailableBE.erase(mIsCurrentlyFailableBE.begin() + index);
                setFailed(res.first->id());
                return res;
            }
        }
 
        template<typename ValueType>
        void DFTState<ValueType>::letDependencyBeUnsuccessful(size_t index) {
            assert(nrFailableDependencies() > 0 && index < nrFailableDependencies());
            std::shared_ptr<DFTDependency<ValueType> const> dependency = mDft.getDependency(getDependencyId(index));
            mFailableDependencies.erase(mFailableDependencies.begin() + index);
            setDependencyUnsuccessful(dependency->id());
        }

        template<typename ValueType>
        void DFTState<ValueType>::activate(size_t repr) {
            size_t activationIndex = mStateGenerationInfo.getSpareActivationIndex(repr);
            assert(!mStatus[activationIndex]);
            mStatus.set(activationIndex);
            propagateActivation(repr);
        }

        template<typename ValueType>
        bool DFTState<ValueType>::isActive(size_t id) const {
            assert(mDft.isRepresentative(id));
            return mStatus[mStateGenerationInfo.getSpareActivationIndex(id)];
        }
            
        template<typename ValueType>
        void DFTState<ValueType>::propagateActivation(size_t representativeId) {
            for(size_t elem : mDft.module(representativeId)) {
                if(mDft.getElement(elem)->isColdBasicElement() && isOperational(elem)) {
                    mIsCurrentlyFailableBE.push_back(elem);
                } else if (mDft.getElement(elem)->isSpareGate() && !isActive(uses(elem))) {
                    activate(uses(elem));
                }
            }
        }

        template<typename ValueType>
        uint_fast64_t DFTState<ValueType>::uses(size_t id) const {
            return extractUses(mStateGenerationInfo.getSpareUsageIndex(id));
        }

        template<typename ValueType>
        uint_fast64_t DFTState<ValueType>::extractUses(size_t from) const {
            assert(mStateGenerationInfo.usageInfoBits() < 64);
            return mStatus.getAsInt(from, mStateGenerationInfo.usageInfoBits());
        }

        template<typename ValueType>
        bool DFTState<ValueType>::isUsed(size_t child) const {
            return (std::find(mUsedRepresentants.begin(), mUsedRepresentants.end(), child) != mUsedRepresentants.end());
        }

        template<typename ValueType>
        void DFTState<ValueType>::setUses(size_t spareId, size_t child) {
            mStatus.setFromInt(mStateGenerationInfo.getSpareUsageIndex(spareId), mStateGenerationInfo.usageInfoBits(), child);
            mUsedRepresentants.push_back(child);
        }

        template<typename ValueType>
        bool DFTState<ValueType>::claimNew(size_t spareId, size_t currentlyUses, std::vector<std::shared_ptr<DFTElement<ValueType>>> const& children) {
            auto it = children.begin();
            while ((*it)->id() != currentlyUses) {
                assert(it != children.end());
                ++it;
            }
            ++it;
            while(it != children.end()) {
                size_t childId = (*it)->id();
                if(!hasFailed(childId) && !isUsed(childId)) {
                    setUses(spareId, childId);
                    if(isActive(currentlyUses)) {
                        activate(childId);
                    }
                    return true;
                }
                ++it;
            }
            return false;
        }
        
        template<typename ValueType>
        bool DFTState<ValueType>::orderBySymmetry() {
            bool changed = false;
            for (size_t pos = 0; pos < mStateGenerationInfo.getSymmetrySize(); ++pos) {
                // Check each symmetry
                size_t length = mStateGenerationInfo.getSymmetryLength(pos);
                std::vector<size_t> symmetryIndices = mStateGenerationInfo.getSymmetryIndices(pos);
                // Sort symmetry group in decreasing order by bubble sort
                // TODO use better algorithm?
                size_t tmp, elem1, elem2;
                size_t n = symmetryIndices.size();
                do {
                    tmp = 0;
                    for (size_t i = 1; i < n; ++i) {
                        elem1 = mStatus.getAsInt(symmetryIndices[i-1], length);
                        elem2 = mStatus.getAsInt(symmetryIndices[i], length);
                        if (elem1 < elem2) {
                            // Swap elements
                            mStatus.setFromInt(symmetryIndices[i-1], length, elem2);
                            mStatus.setFromInt(symmetryIndices[i], length, elem1);
                            tmp = i;
                            changed = true;
                        }
                    }
                    n = tmp;
                } while (n > 0);
            }
            return changed;
        }


        // Explicitly instantiate the class.
        template class DFTState<double>;

#ifdef STORM_HAVE_CARL
        template class DFTState<RationalFunction>;
#endif

    }
}