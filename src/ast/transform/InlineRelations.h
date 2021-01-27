/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file InlineRelations.h
 *
 * Transformation pass to inline marked relations
 *
 ***********************************************************************/

#pragma once

#include "ast/TranslationUnit.h"
#include "ast/transform/Transformer.h"
#include <string>

namespace souffle::ast::transform {

/**
 * Transformation pass to inline marked relations
 */
class InlineRelationsTransformer : public Transformer {
public:
    std::string getName() const override {
        return "InlineRelationsTransformer";
    }

private:
    InlineRelationsTransformer* cloneImpl() const override {
        return new InlineRelationsTransformer();
    }

    bool transform(TranslationUnit& translationUnit) override;
};

}  // namespace souffle::ast::transform
