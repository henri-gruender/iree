// Copyright 2021 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "iree/compiler/Dialect/HAL/IR/HALDialect.h"
#include "iree/compiler/Dialect/HAL/IR/HALOps.h"
#include "llvm/ADT/STLExtras.h"
#include "mlir/Dialect/Affine/Utils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/LogicalResult.h"

namespace mlir {
namespace iree_compiler {
namespace IREE {
namespace ABI {

// Wraps all entry points in a function that is compatible with the
// expected invocation semantics of bindings following the native IREE ABI.
class WrapEntryPointsPass
    : public PassWrapper<WrapEntryPointsPass, OperationPass<ModuleOp>> {
 public:
  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<func::FuncDialect, mlir::arith::ArithmeticDialect,
                    mlir::tensor::TensorDialect, IREE::HAL::HALDialect>();
  }

  StringRef getArgument() const override {
    return "iree-abi-wrap-entry-points";
  }

  StringRef getDescription() const override {
    return "Wraps all entry points in a function that is compatible with the "
           "expected invocation semantics of bindings following the native "
           "IREE ABI.";
  }

  void runOnOperation() override {
    auto moduleOp = getOperation();

    SmallVector<func::FuncOp, 4> entryFuncOps;
    for (auto funcOp : moduleOp.getOps<func::FuncOp>()) {
      if (funcOp.isPublic() && !funcOp->hasAttr("iree.abi.stub")) {
        entryFuncOps.push_back(funcOp);
      }
    }

    SymbolTable symbolTable(moduleOp);

    // Create a wrapper function for each entry point.
    for (auto entryFuncOp : entryFuncOps) {
      // Rename the original function so that our wrapper can use the original
      // name in its public definition.
      auto publicName = entryFuncOp.getName().str();
      auto privateName = "_" + publicName;
      auto privateNameAttr =
          mlir::StringAttr::get(entryFuncOp.getContext(), privateName);
      if (failed(symbolTable.replaceAllSymbolUses(entryFuncOp, privateNameAttr,
                                                  moduleOp))) {
        entryFuncOp.emitError() << "unknown symbol table op encountered; "
                                   "cannot fix up symbol names";
        return signalPassFailure();
      }
      entryFuncOp.setName(privateNameAttr);
      entryFuncOp.setPrivate();

      // Create the wrapper function that conforms to the IREE native ABI and
      // marshals arguments/results to the original function.
      auto wrapperFuncOp = createWrapperFunc(entryFuncOp);
      if (!wrapperFuncOp) return signalPassFailure();
      wrapperFuncOp.setPublic();
      wrapperFuncOp.setName(
          mlir::StringAttr::get(entryFuncOp.getContext(), publicName));
      moduleOp.insert(Block::iterator(entryFuncOp), wrapperFuncOp);

      wrapperFuncOp.getOperation()->setAttr("iree.abi.stub",
                                            UnitAttr::get(&getContext()));
    }
  }

 private:
  Type mapToABIType(Type type) {
    if (type.isa<TensorType>()) {
      return IREE::HAL::BufferViewType::get(type.getContext());
    }
    return type;
  }

  // Creates the corresponding wrapper function for the given entry point.
  //
  // We do this by creating a new function just for the bindings and calling the
  // existing entry point. This allows us to support multiple binding schemes as
  // transforms from other bindings can also perform their own equivalent
  // wrapping.
  //
  // NOTE: today we only support a single entry point; with minor tweaks we
  // could fix this up to support multiple if we wanted.
  func::FuncOp createWrapperFunc(func::FuncOp entryFuncOp) {
    // Convert argument types to those required by the binding ABI.
    //
    // NOTE: this is where we could change our signature to provide additional
    // values from the runtime bindings as may be required - like semaphores for
    // async behavior or cancellation.
    auto entryFuncType = entryFuncOp.getFunctionType();
    SmallVector<Type> inputTypes;
    for (auto oldType : entryFuncType.getInputs()) {
      inputTypes.push_back(mapToABIType(oldType));
    }
    SmallVector<Type> resultTypes;
    for (auto oldType : entryFuncType.getResults()) {
      resultTypes.push_back(mapToABIType(oldType));
    }
    auto wrapperFuncType =
        FunctionType::get(entryFuncOp.getContext(), inputTypes, resultTypes);

    auto wrapperFuncOp = func::FuncOp::create(
        entryFuncOp.getLoc(), entryFuncOp.getName(), wrapperFuncType);

    SmallVector<DictionaryAttr, 4> argAttrDict;
    entryFuncOp.getAllArgAttrs(argAttrDict);
    wrapperFuncOp.setAllArgAttrs(argAttrDict);
    SmallVector<DictionaryAttr, 4> resultAttrDict;
    entryFuncOp.getAllResultAttrs(resultAttrDict);
    wrapperFuncOp.setAllResultAttrs(resultAttrDict);

    populateReflectionAttrs(entryFuncOp, wrapperFuncOp);

    auto *entryBlock = wrapperFuncOp.addEntryBlock();
    auto entryBuilder = OpBuilder::atBlockBegin(entryBlock);

    // Build a map of result value to the argument that has its backing storage.
    SmallVector<Value> resultStorages;
    resultStorages.resize(resultTypes.size());
    for (unsigned i = 0; i < inputTypes.size(); ++i) {
      auto outputAttr =
          entryFuncOp.getArgAttrOfType<IntegerAttr>(i, "iree.abi.output");
      if (!outputAttr) continue;
      // Today all outputs need to be a !hal.buffer - we could change this
      // in the future to be something more generalized.
      auto storageArg = entryBlock->getArgument(i);
      if (!storageArg.getType().isa<IREE::HAL::BufferType>()) {
        entryFuncOp.emitError()
            << "storage argument " << i << " has an invalid type "
            << storageArg.getType() << "; must be a !hal.buffer";
        return {};
      }
      resultStorages[outputAttr.getInt()] = storageArg;
    }

    // Marshal arguments.
    SmallVector<Value> arguments;
    for (auto arg : llvm::enumerate(entryBlock->getArguments())) {
      auto oldType = entryFuncType.getInput(arg.index());
      if (auto tensorType = oldType.dyn_cast<RankedTensorType>()) {
        auto argLoc = arg.value().getLoc();
        auto importOp = entryBuilder.create<IREE::HAL::TensorImportOp>(
            argLoc, oldType, arg.value());
        arguments.push_back(importOp.target());
      } else {
        arguments.push_back(arg.value());
      }
    }

    // Make the call with the original types.
    auto callOp = entryBuilder.create<func::CallOp>(entryFuncOp.getLoc(),
                                                    entryFuncOp, arguments);

    // Marshal results.
    SmallVector<Value> results;
    for (auto result : llvm::enumerate(callOp.getResults())) {
      auto oldType = entryFuncType.getResult(result.index());
      auto newType = wrapperFuncType.getResult(result.index());
      if (oldType.isa<TensorType>()) {
        auto dynamicDims = IREE::Util::buildDynamicDimsForValue(
            entryFuncOp.getLoc(), result.value(), entryBuilder);
        results.push_back(entryBuilder.create<IREE::HAL::TensorExportOp>(
            entryFuncOp.getLoc(), newType, result.value(),
            TypeAttr::get(result.value().getType()), dynamicDims,
            resultStorages[result.index()]));
      } else {
        results.push_back(result.value());
      }
    }
    entryBuilder.create<func::ReturnOp>(entryFuncOp.getLoc(), results);

    return wrapperFuncOp;
  }

  // Populates attributes on |wrapperFuncOp| to support runtime reflection.
  void populateReflectionAttrs(func::FuncOp entryFuncOp,
                               func::FuncOp wrapperFuncOp) {
    SmallVector<NamedAttribute, 4> attrs;
    auto abiAttr = entryFuncOp->getAttr("iree.abi");
    if (abiAttr) {
      attrs.emplace_back(StringAttr::get(entryFuncOp.getContext(), "iree.abi"),
                         abiAttr);
    }
    if (!attrs.empty()) {
      auto reflectionAttr = DictionaryAttr::get(&getContext(), attrs);
      wrapperFuncOp->setAttr("iree.reflection", reflectionAttr);
    }
  }
};

std::unique_ptr<OperationPass<ModuleOp>> createWrapEntryPointsPass() {
  return std::make_unique<WrapEntryPointsPass>();
}

static PassRegistration<WrapEntryPointsPass> pass;

}  // namespace ABI
}  // namespace IREE
}  // namespace iree_compiler
}  // namespace mlir
