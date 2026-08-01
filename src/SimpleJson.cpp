#include "SimpleJson.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace vumat {
namespace json {
namespace {

class Parser {
public:
  explicit Parser(std::string text) : text_(std::move(text)) {}

  Value parse() {
    skipWhitespace();
    Value value = parseValue();
    skipWhitespace();
    if (pos_ != text_.size())
      throw std::runtime_error("Unexpected trailing content in JSON input.");
    return value;
  }

private:
  Value parseValue() {
    skipWhitespace();
    if (pos_ >= text_.size())
      throw std::runtime_error("Unexpected end of JSON input.");

    const char ch = text_[pos_];
    if (ch == '"')
      return parseStringValue();
    if (ch == '[')
      return parseArray();
    if (ch == '{')
      return parseObject();
    if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == '+')
      return parseNumber();
    if (consumeLiteral("true"))
      return makeBool(true);
    if (consumeLiteral("false"))
      return makeBool(false);
    if (consumeLiteral("null"))
      return {};
    throw std::runtime_error("Unsupported JSON token.");
  }

  Value parseStringValue() {
    Value value;
    value.type = Value::Type::String;
    value.string = parseString();
    return value;
  }

  Value makeBool(bool value) {
    Value result;
    result.type = Value::Type::Bool;
    result.boolean = value;
    return result;
  }

  std::string parseString() {
    expect('"');
    std::string result;
    while (pos_ < text_.size()) {
      const char ch = text_[pos_++];
      if (ch == '"')
        return result;
      if (ch != '\\') {
        result.push_back(ch);
        continue;
      }
      if (pos_ >= text_.size())
        throw std::runtime_error("Unterminated JSON escape sequence.");
      const char escaped = text_[pos_++];
      switch (escaped) {
      case '"':
      case '\\':
      case '/':
        result.push_back(escaped);
        break;
      case 'b':
        result.push_back('\b');
        break;
      case 'f':
        result.push_back('\f');
        break;
      case 'n':
        result.push_back('\n');
        break;
      case 'r':
        result.push_back('\r');
        break;
      case 't':
        result.push_back('\t');
        break;
      default:
        throw std::runtime_error("Unsupported JSON string escape.");
      }
    }
    throw std::runtime_error("Unterminated JSON string.");
  }

  Value parseNumber() {
    const size_t begin = pos_;
    while (pos_ < text_.size()) {
      const char ch = text_[pos_];
      if (!(std::isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == '+' ||
            ch == '.' || ch == 'e' || ch == 'E'))
        break;
      ++pos_;
    }
    Value value;
    value.type = Value::Type::Number;
    value.number = std::strtod(text_.substr(begin, pos_ - begin).c_str(), nullptr);
    return value;
  }

  Value parseArray() {
    expect('[');
    Value value;
    value.type = Value::Type::Array;
    skipWhitespace();
    if (peek(']')) {
      ++pos_;
      return value;
    }
    while (true) {
      value.array.push_back(parseValue());
      skipWhitespace();
      if (peek(']')) {
        ++pos_;
        return value;
      }
      expect(',');
    }
  }

  Value parseObject() {
    expect('{');
    Value value;
    value.type = Value::Type::Object;
    skipWhitespace();
    if (peek('}')) {
      ++pos_;
      return value;
    }
    while (true) {
      skipWhitespace();
      const std::string key = parseString();
      skipWhitespace();
      expect(':');
      value.object[key] = parseValue();
      skipWhitespace();
      if (peek('}')) {
        ++pos_;
        return value;
      }
      expect(',');
    }
  }

  bool consumeLiteral(const std::string& literal) {
    if (text_.compare(pos_, literal.size(), literal) != 0)
      return false;
    pos_ += literal.size();
    return true;
  }

  void expect(char expected) {
    skipWhitespace();
    if (pos_ >= text_.size() || text_[pos_] != expected)
      throw std::runtime_error(std::string("Expected JSON token: ") + expected);
    ++pos_;
  }

  bool peek(char expected) const {
    return pos_ < text_.size() && text_[pos_] == expected;
  }

  void skipWhitespace() {
    while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])))
      ++pos_;
  }

  std::string text_;
  size_t pos_ = 0;
};

std::string readFile(const std::string& path) {
  std::ifstream input(path);
  if (!input)
    throw std::runtime_error("Failed to open JSON file: " + path);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

const Value* findField(const Value& object, const std::string& name) {
  if (object.type != Value::Type::Object)
    throw std::runtime_error("JSON root must be an object.");
  const auto it = object.object.find(name);
  return it == object.object.end() ? nullptr : &it->second;
}

template <typename T>
T missingField(const std::string& name, const T& fallback) {
  return fallback;
}

} // namespace

Value parse(const std::string& text) {
  return Parser(text).parse();
}

Value loadFile(const std::string& path) {
  return parse(readFile(path));
}

const Value* find(const Value& object, const std::string& name) {
  return findField(object, name);
}

std::string getString(const Value& object, const std::string& name, bool required,
                      const std::string& fallback) {
  const Value* value = findField(object, name);
  if (!value) {
    if (required)
      throw std::runtime_error("Missing string field: " + name);
    return fallback;
  }
  if (value->type != Value::Type::String)
    throw std::runtime_error("Expected string field: " + name);
  return value->string;
}

int getInt(const Value& object, const std::string& name, bool required, int fallback) {
  const Value* value = findField(object, name);
  if (!value) {
    if (required)
      throw std::runtime_error("Missing integer field: " + name);
    return fallback;
  }
  if (value->type != Value::Type::Number)
    throw std::runtime_error("Expected numeric field: " + name);
  return static_cast<int>(value->number);
}

double getDouble(const Value& object, const std::string& name, bool required, double fallback) {
  const Value* value = findField(object, name);
  if (!value) {
    if (required)
      throw std::runtime_error("Missing numeric field: " + name);
    return fallback;
  }
  if (value->type != Value::Type::Number)
    throw std::runtime_error("Expected numeric field: " + name);
  return value->number;
}

bool getBool(const Value& object, const std::string& name, bool required, bool fallback) {
  const Value* value = findField(object, name);
  if (!value) {
    if (required)
      throw std::runtime_error("Missing boolean field: " + name);
    return fallback;
  }
  if (value->type == Value::Type::Bool)
    return value->boolean;
  if (value->type == Value::Type::Number)
    return value->number != 0.0;
  throw std::runtime_error("Expected boolean field: " + name);
}

std::vector<double> getDoubleArray(const Value& object, const std::string& name) {
  const Value* value = findField(object, name);
  if (!value || value->type != Value::Type::Array)
    throw std::runtime_error("Expected numeric array field: " + name);
  std::vector<double> result;
  result.reserve(value->array.size());
  for (const Value& entry : value->array) {
    if (entry.type != Value::Type::Number)
      throw std::runtime_error("Expected numeric entry in array field: " + name);
    result.push_back(entry.number);
  }
  return result;
}

} // namespace json
} // namespace vumat
