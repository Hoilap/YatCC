#pragma once

#include <llvm/ADT/StringRef.h>
#include <pybind11/pybind11.h>  // 要想在 C++ 中使用 Python 语言特性，需要包含此头文件

class LLMHelper
{
public:
  enum class Role : std::uint8_t
  {
    kUser,
    kSystem,
    kAssistant,
  };

  LLMHelper(llvm::StringRef apiKey, llvm::StringRef baseURL);

  std::string create_new_session();

  void delete_session(llvm::StringRef sessionID);

  void add_content(llvm::StringRef sessionID,
                   Role role,
                   llvm::StringRef content);

  std::string chat(llvm::StringRef sessionID,
                   llvm::StringRef model,
                   const pybind11::list& handlers,
                   const pybind11::dict& params);

private:
  pybind11::object mImpl;
};
