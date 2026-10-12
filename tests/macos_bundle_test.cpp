#include "../src/driver/native_runner.hpp"
#include "../src/modules/resolver.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace
{
  auto require(const bool condition, const char *message) -> void
  { if (!condition) throw std::runtime_error(message); }

  auto bytes(const std::filesystem::path &path) -> std::string
  {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not read bundle test file");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  }
}

auto main() -> int
{
  const auto root = std::filesystem::absolute("build/macos-bundle-test");
  std::error_code ignored;
  std::filesystem::remove_all(root, ignored);
  const auto executable = driver::prepare_macos_application_bundle(root);
  const auto contents = root / "Sagan.app" / "Contents";
  require(executable == contents / "MacOS" / "program",
          "macOS bundle returned the wrong executable path");
  require(std::filesystem::is_directory(executable.parent_path()),
          "macOS bundle omitted Contents/MacOS");
  require(bytes(contents / "Resources" / "sagan.icns") ==
              bytes(driver::application_icon_resource("macos")),
          "macOS bundle changed the canonical Sagan icon");
  const auto plist = bytes(contents / "Info.plist");
  for (const auto *required : {"<key>CFBundleExecutable</key><string>program</string>",
                               "<key>CFBundleIconFile</key><string>sagan.icns</string>",
                               "<key>CFBundleIdentifier</key><string>org.sagan.generated</string>",
                               "<key>CFBundlePackageType</key><string>APPL</string>"})
    require(plist.find(required) != std::string::npos, "macOS bundle Info.plist is incomplete");

  modules::module_graph graph;
  graph.package.emplace();
  graph.package->mode = modules::application_mode::windowed;
  require(driver::compilation_inputs_for(graph).windowed,
          "windowed package mode did not reach native compilation inputs");
  graph.package->mode = modules::application_mode::console;
  require(!driver::compilation_inputs_for(graph).windowed,
          "console package mode unexpectedly requested an application bundle");
  std::filesystem::remove_all(root, ignored);
}
