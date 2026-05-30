#include <gtest/gtest.h>

#include "DocumentCollectionLogic.h"

TEST(DocumentCollectionLogicTest, IndicesNeedingSaveListsModifiedOnly)
{
  const std::vector<DocumentState> docs = {{true, L"a.txt"}, {false, L"b.txt"}, {true, L"c.txt"}};
  const auto indices = DocumentCollectionLogic::IndicesNeedingSave(docs);
  ASSERT_EQ(indices.size(), 2u);
  EXPECT_EQ(indices[0], 0u);
  EXPECT_EQ(indices[1], 2u);
}

TEST(DocumentCollectionLogicTest, AnyModifiedDetectsChanges)
{
  EXPECT_FALSE(DocumentCollectionLogic::AnyModified({{false, L""}}));
  EXPECT_TRUE(DocumentCollectionLogic::AnyModified({{true, L""}}));
}
