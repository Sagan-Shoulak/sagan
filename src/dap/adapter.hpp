#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <string>

namespace sagan::dap
{
  auto read_runtime_diagnostics(const std::filesystem::path &path,
                                std::size_t limit = 65536) -> std::string;

  // The optional debugger path is used by protocol tests. Production lookup
  // finds a bundled toolchain beside sagan-dap, not an editor-supplied tool.
  auto run_adapter(std::istream &input, std::ostream &output, std::ostream &errors,
                   std::filesystem::path debugger = {}) -> int;
}
