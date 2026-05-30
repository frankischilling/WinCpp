#pragma once

#include <string>
#include <vector>

struct DocumentState
{
  bool modified = false;
  std::wstring path;
};

namespace DocumentCollectionLogic
{
std::vector<size_t> IndicesNeedingSave(const std::vector<DocumentState>& documents);
bool AnyModified(const std::vector<DocumentState>& documents);
}
