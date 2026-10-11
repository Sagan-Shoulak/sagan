#include "../src/dap/breakpoints.hpp"
#include "../src/modules/resolver.hpp"
#include "../src/semantic/analyzer.hpp"
#include "../src/semantic/type_checker.hpp"
#include "../src/source/provider.hpp"
#include "../src/syntax/syntax.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace
{
  auto require(const bool value, const char *message) -> void
  {
    if (!value) throw std::runtime_error(message);
  }
}

auto main() -> int
{
  const std::string text = "let 🚀 = 42\nprint(🚀)\n";
  const sagan::source::document_snapshot document(
      {{sagan::source::document_id{1}, sagan::source::document_uri{"untitled:dap"}, {}}, 5, text});
  const auto parsed = sagan::syntax::analyze(document, {.recover = false});
  require(parsed.value && parsed.value->strict_ast, "DAP source did not parse");
  const auto &tree = *parsed.value->strict_ast;
  const auto model = semantic::analyze(tree);
  const auto types = semantic::check_types(tree);
  const auto generated = codegen::generate_cpp_mapped(tree, types, "demo.sagan");
  const auto metadata = sagan::language_service::derive_debug_metadata(document, model, types, generated);
  const auto mapped = sagan::dap::map_breakpoint(metadata, generated, document, {1, 8});
  require(mapped.resolved && mapped.resolved->position.line == 1 &&
              mapped.resolved->generated_line > 0 && mapped.resolved->generated_column > 0,
          "UTF-16 breakpoint did not map to generated source");
  require(!sagan::dap::map_breakpoint(metadata, generated, document, {2, 0}).resolved &&
              !sagan::dap::map_breakpoint(metadata, generated, document, {20, 0}).resolved &&
              !sagan::dap::map_breakpoint(metadata, generated, document, {0, 5}).resolved,
          "Nonexecutable line unexpectedly accepted a breakpoint");

  const std::string exception_text =
      "hope scream \"caught\"\n"
      "unless \"caught\" print(\"handled\")\n"
      "scream \"uncaught\"\n";
  const sagan::source::document_snapshot exception_document(
      {{sagan::source::document_id{2}, sagan::source::document_uri{"untitled:dap-exceptions"}, {}},
       1, exception_text});
  const auto exception_parsed = sagan::syntax::analyze(exception_document, {.recover = false});
  require(exception_parsed.value && exception_parsed.value->strict_ast,
          "DAP exception source did not parse");
  const auto &exception_tree = *exception_parsed.value->strict_ast;
  const auto exception_model = semantic::analyze(exception_tree);
  const auto exception_types = semantic::check_types(exception_tree);
  const auto exception_cpp = codegen::generate_cpp_mapped(exception_tree, exception_types,
                                                           "exceptions.sagan");
  const auto exception_metadata = sagan::language_service::derive_debug_metadata(
      exception_document, exception_model, exception_types, exception_cpp);
  require(exception_metadata.exceptions.size() == 2,
          "Sagan scream statements did not produce two exception sites");
  const auto first_exception = exception_document.to_utf16(
      exception_metadata.exceptions.front().source.bytes.begin);
  const auto second_exception = exception_document.to_utf16(
      exception_metadata.exceptions.back().source.bytes.begin);
  require(first_exception && first_exception->line == 0 && second_exception &&
              second_exception->line == 2 &&
              exception_metadata.exceptions.front().generated_begin < exception_cpp.text.size() &&
              exception_metadata.exceptions.back().generated_begin < exception_cpp.text.size(),
          "Exception sites did not preserve exact source and generated positions");

  const sagan::source::disk_source_provider source;
  const auto main = source.read_path("tests/fixtures/modules/module_demo/main.sagan");
  const auto imported = source.read_path("tests/fixtures/modules/module_demo/guidance.sagan");
  require(main && imported, "Multi-module fixture missing");
  const auto linked = modules::link("tests/fixtures/modules/module_demo/main.sagan");
  const auto linked_model = semantic::analyze(linked);
  const auto linked_types = semantic::check_types(linked);
  const auto linked_cpp = codegen::generate_cpp_mapped(linked, linked_types);
  const auto linked_metadata = sagan::language_service::derive_debug_metadata(
      *main.value, linked_model, linked_types, linked_cpp, &source);
  const auto exception_entry = source.read_path("tests/fixtures/modules/dap_exceptions/main.sagan");
  const auto exception_support = source.read_path("tests/fixtures/modules/dap_exceptions/support.sagan");
  require(exception_entry && exception_support, "Imported exception fixture missing");
  const auto exception_linked = modules::link("tests/fixtures/modules/dap_exceptions/main.sagan");
  const auto exception_linked_model = semantic::analyze(exception_linked);
  const auto exception_linked_types = semantic::check_types(exception_linked);
  const auto exception_linked_cpp = codegen::generate_cpp_mapped(
      exception_linked, exception_linked_types);
  const auto exception_linked_metadata = sagan::language_service::derive_debug_metadata(
      *exception_entry.value, exception_linked_model, exception_linked_types,
      exception_linked_cpp, &source);
  const auto imported_exception = std::find_if(
      exception_linked_metadata.exceptions.begin(), exception_linked_metadata.exceptions.end(),
      [&](const auto &candidate)
      {
        return candidate.source_path &&
            candidate.source_path->filename() == std::filesystem::path("support.sagan");
      });
  require(imported_exception != exception_linked_metadata.exceptions.end() &&
              imported_exception->source_path &&
              imported_exception->generated_begin < exception_linked_cpp.text.size(),
          "Imported Sagan scream did not preserve exception metadata");
  bool found_imported = false;
  for (const auto &candidate : linked_metadata.breakpoints)
  {
    if (candidate.source.document != imported.value->identity().id) continue;
    const auto position = imported.value->to_utf16(candidate.source.bytes.begin);
    if (!position) continue;
    const auto resolution = sagan::dap::map_breakpoint(linked_metadata, linked_cpp,
                                                       *imported.value, *position);
    require(resolution.resolved && resolution.resolved->source.document ==
                imported.value->identity().id, "Imported breakpoint mapped to another module");
    found_imported = true;
    break;
  }
  require(found_imported, "No imported source breakpoint was available");
  return 0;
}
