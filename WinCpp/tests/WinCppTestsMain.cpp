#include <gtest/gtest.h>

#include "WinCpp.h"

#include <commctrl.h>
#include <objbase.h>

class WinCppTestEnvironment : public ::testing::Environment
{
public:
  void SetUp() override
  {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX controls = {};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);
  }

  void TearDown() override
  {
    CoUninitialize();
  }
};

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::AddGlobalTestEnvironment(new WinCppTestEnvironment());
  return RUN_ALL_TESTS();
}
