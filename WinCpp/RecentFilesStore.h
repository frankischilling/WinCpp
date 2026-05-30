#pragma once

#include <string>
#include <vector>

class RecentFilesStore
{
public:
  static constexpr size_t kMaxItems = 10;

  void Load();
  void Save() const;
  void Add(const std::wstring& path);
  void Clear();

  const std::vector<std::wstring>& Items() const { return items_; }

private:
  std::wstring ConfigPath() const;

  std::vector<std::wstring> items_;
};
