//===- ConvertConst.cpp - Quantizes constant ops --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/Quant/Passes.h"
#include "mlir/Dialect/Quant/QuantOps.h"
#include "mlir/Dialect/Quant/QuantizeUtils.h"
#include "mlir/Dialect/Quant/UniformSupport.h"
#include "mlir/Dialect/StandardOps/IR/Ops.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Matchers.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/StandardTypes.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;
using namespace mlir::quant;

namespace {
struct ConvertConstPass : public FunctionPass<ConvertConstPass> {
/// Include the generated pass utilities.
#define GEN_PASS_QuantConvertConst
#include "mlir/Dialect/Quant/Passes.h.inc"

  void runOnFunction() override;
};

struct QuantizedConstRewrite : public OpRewritePattern<QuantizeCastOp> {
  using OpRewritePattern<QuantizeCastOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(QuantizeCastOp qbarrier,
                                PatternRewriter &rewriter) const override;
};

} // end anonymous namespace

/// Matches a [constant] -> [qbarrier] where the qbarrier results type is
/// quantized and the operand type is quantizable.

LogicalResult
QuantizedConstRewrite::matchAndRewrite(QuantizeCastOp qbarrier,
                                       PatternRewriter &rewriter) const {
  Attribute value;

  // Is the operand a constant?
  if (!matchPattern(qbarrier.arg(), m_Constant(&value))) {
    return failure();
  }

  // Does the qbarrier convert to a quantized type. This will not be true
  // if a quantized type has not yet been chosen or if the cast to an equivalent
  // storage type is not supported.
  Type qbarrierResultType = qbarrier.getResult().getType();
  QuantizedType quantizedElementType =
      QuantizedType::getQuantizedElementType(qbarrierResultType);
  if (!quantizedElementType) {
    return failure();
  }
  if (!QuantizedType::castToStorageType(qbarrierResultType)) {
    return failure();
  }

  // Is the operand type compatible with the expressed type of the quantized
  // type? This will not be true if the qbarrier is superfluous (converts
  // from and to a quantized type).
  if (!quantizedElementType.isCompatibleExpressedType(
          qbarrier.arg().getType())) {
    return failure();
  }

  // Is the constant value a type expressed in a way that we support?
  if (!value.isa<FloatAttr>() && !value.isa<DenseElementsAttr>() &&
      !value.isa<SparseElementsAttr>()) {
    return failure();
  }

  Type newConstValueType;
  auto newConstValue =
      quantizeAttr(value, quantizedElementType, newConstValueType);
  if (!newConstValue) {
    return failure();
  }

  // When creating the new const op, use a fused location that combines the
  // original const and the qbarrier that led to the quantization.
  auto fusedLoc = FusedLoc::get(
      {qbarrier.arg().getDefiningOp()->getLoc(), qbarrier.getLoc()},
      rewriter.getContext());
  auto newConstOp =
      rewriter.create<ConstantOp>(fusedLoc, newConstValueType, newConstValue);
  rewriter.replaceOpWithNewOp<StorageCastOp>(qbarrier, qbarrier.getType(),
                                             newConstOp);
  return success();
}

void ConvertConstPass::runOnFunction() {
  OwningRewritePatternList patterns;
  auto func = getFunction();
  auto *context = &getContext();
  patterns.insert<QuantizedConstRewrite>(context);
  applyPatternsGreedily(func, patterns);
}

std::unique_ptr<OpPassBase<FuncOp>> mlir::quant::createConvertConstPass() {
  return std::make_unique<ConvertConstPass>();
}
