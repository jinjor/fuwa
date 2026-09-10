#include "api/protocol.h"

#include <json.h>

#include <charconv>
#include <cstdlib>
#include <memory>

// JSON の読み書きは vendored の json.h（sheredom/json.h, パブリックドメイン）に任せる。
// エスケープを自分で書かないのが目的なので、書く側もこれを通す。
namespace fuwa::api {
namespace {

struct FreeValue {
  void operator()(json_value_s* value) const { std::free(value); }
};
using OwnedValue = std::unique_ptr<json_value_s, FreeValue>;

// json.h の書き出しは malloc したナル終端の utf-8 を返す。
std::string write(const json_value_s& root) {
  void* buffer = json_write_minified(&root, nullptr);
  if (buffer == nullptr) {
    return {};
  }
  std::string out(static_cast<const char*>(buffer));
  std::free(buffer);
  return out;
}

json_value_s makeValue(void* payload, json_type_e type) {
  json_value_s value{};
  value.payload = payload;
  value.type = static_cast<std::size_t>(type);
  return value;
}

OwnedValue parse(std::string_view line, std::string& error) {
  OwnedValue root(json_parse(line.data(), line.size()));
  if (!root) {
    error = "the message is not valid JSON";
  }
  return root;
}

}  // namespace

std::string encodeRequest(const std::vector<std::string>& args) {
  // 節はすべて呼び出しの間だけ生きていればよい。書き出しはこの関数の中で終わる。
  std::vector<json_string_s> strings(args.size());
  std::vector<json_value_s> values(args.size());
  std::vector<json_array_element_s> elements(args.size());

  for (std::size_t i = 0; i < args.size(); ++i) {
    strings[i] = {args[i].c_str(), args[i].size()};
    values[i] = makeValue(&strings[i], json_type_string);
    elements[i] = {&values[i], i + 1 < args.size() ? &elements[i + 1] : nullptr};
  }

  json_array_s array{args.empty() ? nullptr : elements.data(), args.size()};
  const json_value_s root = makeValue(&array, json_type_array);
  return write(root);
}

bool decodeRequest(std::string_view line, std::vector<std::string>& args, std::string& error) {
  args.clear();
  const OwnedValue root = parse(line, error);
  if (!root) {
    return false;
  }

  json_array_s* array = json_value_as_array(root.get());
  if (array == nullptr) {
    error = "a request must be an array of strings";
    return false;
  }
  for (json_array_element_s* element = array->start; element != nullptr; element = element->next) {
    const json_string_s* text = json_value_as_string(element->value);
    if (text == nullptr) {
      error = "a request must be an array of strings";
      args.clear();
      return false;
    }
    args.emplace_back(text->string, text->string_size);
  }
  return true;
}

std::string encodeResponse(const Response& response) {
  const std::string code = std::to_string(response.code);

  json_number_s number{code.c_str(), code.size()};
  json_value_s codeValue = makeValue(&number, json_type_number);

  json_string_s text{response.text.c_str(), response.text.size()};
  json_value_s textValue = makeValue(&text, json_type_string);

  json_string_s textName{"text", 4};
  json_object_element_s textElement{&textName, &textValue, nullptr};

  json_string_s codeName{"code", 4};
  json_object_element_s codeElement{&codeName, &codeValue, &textElement};

  json_object_s object{&codeElement, 2};
  const json_value_s root = makeValue(&object, json_type_object);
  return write(root);
}

bool decodeResponse(std::string_view line, Response& response, std::string& error) {
  const OwnedValue root = parse(line, error);
  if (!root) {
    return false;
  }

  json_object_s* object = json_value_as_object(root.get());
  if (object == nullptr) {
    error = "a response must be an object";
    return false;
  }

  bool sawCode = false;
  for (json_object_element_s* element = object->start; element != nullptr;
       element = element->next) {
    const std::string_view name(element->name->string, element->name->string_size);
    if (name == "code") {
      const json_number_s* number = json_value_as_number(element->value);
      if (number == nullptr) {
        error = "the response code must be a number";
        return false;
      }
      const std::string_view digits(number->number, number->number_size);
      const auto* end = digits.data() + digits.size();
      const auto parsed = std::from_chars(digits.data(), end, response.code);
      if (parsed.ec != std::errc{} || parsed.ptr != end) {
        error = "the response code must be an integer";
        return false;
      }
      sawCode = true;
    } else if (name == "text") {
      const json_string_s* text = json_value_as_string(element->value);
      if (text == nullptr) {
        error = "the response text must be a string";
        return false;
      }
      response.text.assign(text->string, text->string_size);
    }
  }

  if (!sawCode) {
    error = "the response has no code";
    return false;
  }
  return true;
}

}  // namespace fuwa::api
