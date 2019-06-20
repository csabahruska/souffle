/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file CompiledRecord.h
 *
 * The central interface for handling records in the compiled execution.
 *
 ***********************************************************************/

#pragma once

#include "CompiledTuple.h"
#include "ParallelUtils.h"
#include "RecordTable.h"
#include "Util.h"

#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace souffle {

// ----------------------------------------------------------------------------
//                              Declarations
// ----------------------------------------------------------------------------

/**
 * A function packing a tuple of the given arity into a reference.
 */
template <typename Tuple>
RamDomain pack(const Tuple& tuple);

/**
 * A function obtaining a pointer to the tuple addressed by the given reference.
 */
template <typename Tuple>
const Tuple& unpack(RamDomain ref);

/**
 * Obtains the null-reference constant.
 */
template <typename TupleType>
RamDomain getNull();

/**
 * Determines whether the given reference is the null reference encoding
 * the absence of any nested record.
 */
template <typename TupleType>
bool isNull(RamDomain ref);

// ----------------------------------------------------------------------------
//                              Definitions
// ----------------------------------------------------------------------------

template <typename TupleType>
RamDomain getNull() {
    return 0;
}

template <typename TupleType>
bool isNull(RamDomain ref) {
    return ref == 0;
}

/**
 * A superclass for all generated record map classes.
 */
class GeneralRecordMap {
public:
    GeneralRecordMap() {
        // store an internal reference to the created record map
        getMaps().insert(this);
    }

    /**
     * Get the reference-to-tuple mapping for the current record map.
     */
    virtual std::map<RamDomain, std::vector<RamDomain>> getRecordReferences() const = 0;

    /**
     * Get the reference-to-tuple mapping generated so far across all stored maps.
     */
    static const RecordTable& getRecordTable() {
        static RecordTable recordTable;
        bool created = false;

        // only construct record table once
        if (!created) {
            created = true;
            // add in all records from all created maps
            // TODO: more efficient? unnecessary copies here
            for (const auto* recordMap : getMaps()) {
                for (const auto& pair : recordMap->getRecordReferences()) {
                    RamDomain ref = pair.first;
                    const auto& tuple = pair.second;
                    recordTable.addRecord(ref, tuple);
                }
            }
        }

        return recordTable;
    }

    /**
     * Get the references to all created record maps.
     */
    static std::set<GeneralRecordMap*>& getMaps() {
        static std::set<GeneralRecordMap*> createdMaps;
        return createdMaps;
    }
};

namespace detail {

/**
 * A bidirectional mapping between tuples and reference indices.
 */
template <typename Tuple>
class RecordMap : public GeneralRecordMap {
    // create blocks of a million entries
    static const std::size_t BLOCK_SIZE = 1 << 20;

    /** The definition of the tuple type handled by this instance */
    using tuple_type = Tuple;

    /** The definition of the type of a tuple block */
    using block_type = std::array<tuple_type, BLOCK_SIZE>;

    /** The type utilized for the block index */
    using block_index_type = std::vector<std::unique_ptr<block_type>>;

    /** The mapping from tuples to references/indices */
    std::unordered_map<tuple_type, RamDomain> r2i;

    /** The mapping from indices to tuples */
    block_index_type i2r;

    /** a lock for the pack operation */
    Lock pack_lock;

public:
    RecordMap() : GeneralRecordMap() {}

    /**
     * Packs the given tuple -- and may create a new reference if necessary.
     */
    RamDomain pack(const tuple_type& tuple) {
        RamDomain index;

        {
            // lock pack operation
            auto leas = pack_lock.acquire();  // lock hold till end of scope
            (void)leas;                       // avoid warning

            // try lookup
            auto pos = r2i.find(tuple);
            if (pos != r2i.end()) {
                // take the previously assigned value
                index = pos->second;
            } else {
                // add tuple to index
                index = r2i.size() + 1;  // since 0 is skipped for the Nil element
                r2i[tuple] = index;

                // assert that new index is smaller than the range
                assert(index != std::numeric_limits<RamDomain>::max());

                if (index / BLOCK_SIZE == i2r.size()) {
                    i2r.push_back(std::make_unique<block_type>());
                }

                // create entry for unpacking
                auto& list = i2r[index / BLOCK_SIZE];

                // insert tuple
                (*list)[index % BLOCK_SIZE] = tuple;
            }
        }

        // done
        return index;
    }

    /**
     * Obtains a pointer to the tuple addressed by the given index.
     */
    const tuple_type& unpack(RamDomain index) {
        // just look up the right spot
        return (*(i2r[index / BLOCK_SIZE]))[index % BLOCK_SIZE];
    }

    std::map<int, std::vector<RamDomain>> getRecordReferences() const override {
        // TODO: more efficient here maybe?
        std::map<int, std::vector<RamDomain>> recordValues;
        for (const auto& pair : r2i) {
            recordValues[pair.second] = pair.first.toVector();
        }
        return recordValues;
    }
};

/**
 * Specialisation for empty records
 */
template <>
class RecordMap<ram::Tuple<RamDomain, 0>> : public GeneralRecordMap {
public:
    RamDomain pack(const ram::Tuple<RamDomain, 0>& tuple) {
        return 1;
    }

    const ram::Tuple<RamDomain, 0>& unpack(RamDomain index) {
        static ram::Tuple<RamDomain, 0> empty;
        return empty;
    }

    std::map<int, std::vector<RamDomain>> getRecordReferences() const override {
        return std::map<int, std::vector<RamDomain>>({{1, {}}});
    }
};

/**
 * The static access function for record of a certain type.
 */
template <typename Tuple>
RecordMap<Tuple>& getRecordMap() {
    static RecordMap<Tuple> map;
    return map;
}
}  // namespace detail

template <typename Tuple>
RamDomain pack(const Tuple& tuple) {
    return detail::getRecordMap<Tuple>().pack(tuple);
}

template <typename Tuple>
const Tuple& unpack(RamDomain ref) {
    return detail::getRecordMap<Tuple>().unpack(ref);
}

}  // end of namespace souffle
