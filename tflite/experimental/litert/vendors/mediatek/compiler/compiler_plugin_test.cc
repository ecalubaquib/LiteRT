// Copyright 2024 Google LLC.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstddef>
#include <string>

#include <gtest/gtest.h>
#include "absl/strings/string_view.h"
#include "tflite/experimental/litert/c/litert_common.h"
#include "tflite/experimental/litert/c/litert_logging.h"
#include "tflite/experimental/litert/c/litert_model.h"
#include "tflite/experimental/litert/c/litert_op_code.h"
#include "tflite/experimental/litert/cc/litert_expected.h"
#include "tflite/experimental/litert/cc/litert_macros.h"
#include "tflite/experimental/litert/core/model/model.h"
#include "tflite/experimental/litert/test/common.h"
#include "tflite/experimental/litert/test/matchers.h"
#include "tflite/experimental/litert/test/test_models.h"
#include "tflite/experimental/litert/vendors/c/litert_compiler_plugin.h"
#include "tflite/experimental/litert/vendors/cc/litert_compiler_plugin.h"

namespace litert {
namespace {

using ::testing::Values;

// clang-format off
const auto kSupportedOps = Values(
    "add_cst.tflite",
    "add_simple.tflite",
    "simple_add_op.tflite");
// clang-format on

TEST(TestQnnPlugin, GetConfigInfo) {
  EXPECT_STREQ(LiteRtGetCompilerPluginSocManufacturer(), "MediaTek");

  auto plugin = CreatePlugin();

  LiteRtParamIndex num_supported_soc_models;
  LITERT_ASSERT_OK(LiteRtGetNumCompilerPluginSupportedSocModels(
      plugin.get(), &num_supported_soc_models));
  ASSERT_EQ(num_supported_soc_models, 12);

  const char* config_id;
  LITERT_ASSERT_OK(
      LiteRtGetCompilerPluginSupportedSocModel(plugin.get(), 0, &config_id));
  EXPECT_STREQ(config_id, "mt6853");
}

TEST(TestQnnPlugin, PartitionAdd) {
  auto plugin = CreatePlugin();
  auto model = testing::LoadTestFileModel("add_simple.tflite");

  LiteRtOpListT selected_op_list;
  LITERT_ASSERT_OK(LiteRtCompilerPluginPartition(
      plugin.get(), model.Subgraph(0)->Get(), &selected_op_list));
  const auto selected_ops = selected_op_list.Vec();

  ASSERT_EQ(selected_ops.size(), 1);
  EXPECT_EQ(selected_ops[0]->OpCode(), kLiteRtOpCodeTflAdd);
}

// /////////////////////////////////////////////////////////////////////////////

class MtkPluginOpCompatibilityTest
    : public ::testing::TestWithParam<std::string> {};

TEST_P(MtkPluginOpCompatibilityTest, SupportedOpsTest) {
  LITERT_LOG(LITERT_INFO, "Testing TFLite model: %s", GetParam().c_str());
  auto plugin = CreatePlugin();
  auto model = testing::LoadTestFileModel(GetParam());

  const auto subgraph = model.MainSubgraph();
  LiteRtSubgraph litert_subgraph = subgraph->Get();

  LiteRtCompiledResult compiled;
  LITERT_ASSERT_OK(LiteRtCompilerPluginCompile(
      plugin.get(), /*soc_model=*/nullptr, &litert_subgraph, 1, &compiled));

  LiteRtParamIndex num_byte_code;
  LITERT_ASSERT_OK(
      LiteRtCompiledResultNumByteCodeModules(compiled, &num_byte_code));
  ASSERT_EQ(num_byte_code, 1);

  const void* byte_code;
  size_t byte_code_size;

  LITERT_ASSERT_OK(LiteRtGetCompiledResultByteCode(
      compiled, /*byte_code_idx=*/0, &byte_code, &byte_code_size));

  absl::string_view byte_code_string(reinterpret_cast<const char*>(byte_code),
                                     byte_code_size);
  ASSERT_FALSE(byte_code_string.empty());

  const void* op_data;
  size_t op_data_size;
  LiteRtParamIndex byte_code_idx;

  LITERT_ASSERT_OK(LiteRtGetCompiledResultCallInfo(
      compiled, /*call_idx=*/0, &op_data, &op_data_size, &byte_code_idx));

  EXPECT_EQ(byte_code_idx, 0);

  absl::string_view op_data_string(reinterpret_cast<const char*>(op_data),
                                   op_data_size);
  EXPECT_EQ(op_data_string, "Partition_0");

  LiteRtDestroyCompiledResult(compiled);
}

INSTANTIATE_TEST_SUITE_P(SupportedOpsTest, MtkPluginOpCompatibilityTest,
                         kSupportedOps);

}  // namespace
}  // namespace litert
