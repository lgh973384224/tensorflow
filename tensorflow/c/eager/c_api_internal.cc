/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/c/eager/c_api_internal.h"

#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/host_info.h"

TFE_Op* NewOrResetOp(TFE_Context* ctx, const char* op_or_function_name,
                     const char* raw_device_name, TF_Status* status,
                     TFE_Op* op_to_reset) {
  const char* name = op_or_function_name;  // Shorthand
  const tensorflow::AttrTypeMap* types;
  bool is_function = false;
  status->status = tensorflow::AttrTypeMapForOp(name, &types, &is_function);
  if (!status->status.ok()) {
    return nullptr;
  }

  if (op_to_reset && op_to_reset->ctx != ctx) {
    status->status = tensorflow::errors::Internal(
        "Cannot reset a TFE_Op from another TFE_Context");
    return nullptr;
  }

  std::unique_ptr<TFE_OpInferenceContext> inference_ctx;
  if (!is_function) {
    const tensorflow::OpDef* op_def;
    status->status = tensorflow::OpDefForOp(op_or_function_name, &op_def);
    if (!status->status.ok()) {
      return nullptr;
    }
    inference_ctx.reset(new TFE_OpInferenceContext(op_def));
  } else if (!ctx->context->FindFunctionByName(name)) {
    status->status = tensorflow::errors::NotFound(
        "'", name,
        "' is neither a type of a primitive operation nor a name "
        "of a function registered in binary running on ",
        tensorflow::port::Hostname(),
        ". Make sure the operation or function is "
        "registered in the binary running in this process.");
    return nullptr;
  }

  if (op_to_reset) {
    status->status = op_to_reset->Reset(
        name, is_function, types, raw_device_name, std::move(inference_ctx));
    return op_to_reset;
  }

  TFE_Op* new_op =
      new TFE_Op(ctx, name, is_function, types, std::move(inference_ctx));
  status->status = new_op->operation.SetDeviceName(raw_device_name);
  return new_op;
}
