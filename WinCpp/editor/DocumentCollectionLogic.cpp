#include "DocumentCollectionLogic.h"

namespace DocumentCollectionLogic
{
std::vector<size_t> IndicesNeedingSave(const std::vector<DocumentState>& documents)
{
  std::vector<size_t> indices;
  for (size_t i = 0; i < documents.size(); ++i)
  {
    if (documents[i].modified)
    {
      indices.push_back(i);
    }
  }
  return indices;
}

bool AnyModified(const std::vector<DocumentState>& documents)
{
  for (const DocumentState& doc : documents)
  {
    if (doc.modified)
    {
      return true;
    }
  }
  return false;
}
}
