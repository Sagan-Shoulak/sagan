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

  const std::string class_text =
      "class Nested {\n"
      "  let value: Int = 1\n"
      "}\n"
      "class Probe {\n"
      "  let count: Int\n"
      "  let ready: Bool\n"
      "  let label: String\n"
      "  let nested: Nested\n"
      "}\n"
      "fun arrays(): Int {\n"
      "  const READINGS = [1, 2, 3]\n"
      "  let values = [4, 5, 6]\n"
      "  let other = [7, 8]\n"
      "  let first, second = [14], [15, 16]\n"
      "  let nested = [[17]]\n"
      "  values[1] = 9\n"
      "  (values)[0] += 1\n"
      "  values = [10, 11]\n"
      "  let assigned = values := [12, 13]\n"
      "  values, other = other, values\n"
      "  values[0]++\n"
      "  return READINGS[0] + assigned[0]\n"
      "}\n";
  const sagan::source::document_snapshot class_document(
      {{sagan::source::document_id{3}, sagan::source::document_uri{"untitled:dap-class-fields"}, {}},
       1, class_text});
  const auto class_parsed = sagan::syntax::analyze(class_document, {.recover = false});
  require(class_parsed.value && class_parsed.value->strict_ast, "DAP class fields did not parse");
  const auto &class_tree = *class_parsed.value->strict_ast;
  const auto class_model = semantic::analyze(class_tree);
  const auto class_types = semantic::check_types(class_tree);
  const auto class_cpp = codegen::generate_cpp_mapped(class_tree, class_types, "fields.sagan");
  const auto class_metadata = sagan::language_service::derive_debug_metadata(
      class_document, class_model, class_types, class_cpp);
  require(class_metadata.class_fields.size() == 5,
          "Direct source class fields did not produce debugger metadata");
  const auto scalar_fields = std::count_if(class_metadata.class_fields.begin(),
      class_metadata.class_fields.end(), [](const auto &field)
      {
        return field.owner_type == "Probe" &&
            field.representation == sagan::language_service::debug_value_representation::direct_value;
      });
  require(scalar_fields == 3 &&
              std::any_of(class_metadata.class_fields.begin(), class_metadata.class_fields.end(),
                  [](const auto &field)
                  {
                    return field.name == "nested" && field.owner_type == "Probe" &&
                        field.representation ==
                            sagan::language_service::debug_value_representation::unavailable;
                  }),
          "Class field metadata did not distinguish direct scalar and nested values");
  require(std::any_of(class_metadata.class_fields.begin(), class_metadata.class_fields.end(),
              [](const auto &field)
              {
                return field.owner_type == "Nested" && field.name == "value" &&
                    field.representation ==
                        sagan::language_service::debug_value_representation::direct_value;
              }),
          "Nested class metadata did not retain its own direct scalar field");
  require(class_metadata.array_views.size() == 6 &&
              std::all_of(class_metadata.array_views.begin(), class_metadata.array_views.end(),
                  [](const auto &view) { return view.element_type == "Int8"; }) &&
              std::any_of(class_metadata.array_views.begin(), class_metadata.array_views.end(),
                  [](const auto &view)
                  { return view.generated_name == codegen::generated_debug_array_identifier("values"); }) &&
              std::any_of(class_metadata.array_views.begin(), class_metadata.array_views.end(),
                  [](const auto &view)
                  { return view.generated_name == codegen::generated_debug_array_identifier("first"); }),
          "Local scalar Arrays did not produce stable debugger views");
  require(class_cpp.text.find("#ifdef SAGAN_DEBUG_ARRAY_VIEW") != std::string::npos &&
              class_cpp.text.find(codegen::generated_debug_array_identifier("READINGS")) !=
                  std::string::npos &&
              class_cpp.text.find(codegen::generated_debug_array_identifier("values")) !=
                  std::string::npos &&
              class_cpp.text.find("SAGAN_DEBUG_ARRAY_REFRESH") != std::string::npos &&
              class_cpp.text.find("sagan_debug_assignment_") != std::string::npos &&
              class_cpp.text.find("sagan_debug_mutation_") != std::string::npos,
          "Generated C++ omitted mutable debug-only Array synchronization");

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
