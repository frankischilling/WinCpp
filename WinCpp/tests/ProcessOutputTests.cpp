#include <gtest/gtest.h>

#include "ProcessOutput.h"

TEST(ProcessOutputTest, CapturesEchoCommandOutput)
{
  std::string output;
  int exitCode = -1;
  ASSERT_TRUE(ProcessOutput::RunCaptureStdout(L"cmd /c echo WinCppTest", &output, &exitCode));
  EXPECT_NE(output.find("WinCppTest"), std::string::npos);
  EXPECT_EQ(exitCode, 0);
}
