#include "../src/lsp/server.hpp"

#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
  using J = sagan::lsp::json::value;

  auto require(const bool condition, const char *message) -> void
  { if (!condition) throw std::runtime_error(message); }

  auto frame(const J &message) -> std::string
  {
    const auto encoded = sagan::lsp::json::serialize(message);
    return "Content-Length: " + std::to_string(encoded.size()) + "\r\n\r\n" + encoded;
  }
}

auto main() -> int
{
  using namespace sagan::lsp;
  for (int iteration = 0; iteration < 16; ++iteration)
  {
    server idle;
    std::istringstream input;
    std::ostringstream output;
    require(run(input, output, idle) == 0 && output.str().empty(),
            "idle LSP worker did not join cleanly at end of input");
  }

  server initialized;
  std::istringstream initialized_input(frame(
      J::object{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}}));
  std::ostringstream initialized_output;
  require(run(initialized_input, initialized_output, initialized) == 0 &&
              initialized_output.str().starts_with("Content-Length: ") &&
              initialized_output.str().find("\"id\":1") != std::string::npos,
          "LSP worker did not drain a queued request before joining");

  server truncated;
  std::istringstream truncated_input("Content-Length: 20\r\n\r\n{}");
  std::ostringstream truncated_output;
  require(run(truncated_input, truncated_output, truncated) == 1 && truncated_output.str().empty(),
          "LSP worker did not join after a truncated frame");
}
