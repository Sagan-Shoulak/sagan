#include "../src/lsp/json.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace
{
  namespace json = sagan::lsp::json;

  auto require(const bool condition, const char *message) -> void
  { if (!condition) throw std::runtime_error(message); }

  auto parsed_double(const std::string_view text) -> double
  {
    const auto parsed = json::parse(text);
    const auto *number = std::get_if<double>(&parsed.data);
    require(number != nullptr, "JSON number did not retain floating representation");
    return *number;
  }

  auto require_rejected(const std::string_view text) -> void
  {
    try
    {
      static_cast<void>(json::parse(text));
    }
    catch (const std::runtime_error &)
    {
      return;
    }
    throw std::runtime_error("Invalid JSON number was accepted: " + std::string(text));
  }

  auto require_serialize_rejected(const double number) -> void
  {
    try
    {
      static_cast<void>(json::serialize(json::value(number)));
    }
    catch (const std::runtime_error &)
    {
      return;
    }
    throw std::runtime_error("Non-finite JSON number was serialized");
  }
}

auto main() -> int
{
  require(parsed_double("0.5") == 0.5, "JSON decimal changed value");
  require(parsed_double("-12.75") == -12.75, "JSON negative decimal changed value");
  require(parsed_double("6.022e23") == 6.022e23, "JSON exponent changed value");
  require(parsed_double("1E-6") == 1e-6, "JSON signed exponent changed value");

  for (const double number : {0.5, -12.75, 6.022e23, 1e-6,
                              std::numeric_limits<double>::min(),
                              std::numeric_limits<double>::max()})
  {
    const auto encoded = json::serialize(json::value(number));
    require(parsed_double(encoded) == number, "JSON floating round trip changed value");
  }

  require_rejected("1.");
  require_rejected("1e");
  require_rejected("1e+");
  require_rejected("01.0");
  require_rejected("1e9999");
  require_rejected("1e-9999");
  require_serialize_rejected(std::numeric_limits<double>::infinity());
  require_serialize_rejected(-std::numeric_limits<double>::infinity());
  require_serialize_rejected(std::numeric_limits<double>::quiet_NaN());
}
