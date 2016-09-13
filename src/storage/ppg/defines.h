#pragma once
#include <cassert>
#include <vector>
namespace storm {
    namespace ppg {
        class ProgramGraph;
        class ProgramEdgeGroup;
        class ProgramLocation;
        using ProgramLocationIdentifier = uint64_t;
        using ProgramActionIdentifier = uint64_t;
        using ProgramEdgeGroupIdentifier =  uint64_t;
        using ProgramEdgeIdentifier = uint64_t;
        
    }
}