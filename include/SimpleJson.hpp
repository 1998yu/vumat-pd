#pragma once

#include <map>
#include <string>
#include <vector>

namespace vumat {
namespace json {

struct Value {
  enum class Type { Null, Bool, Number, String, Array, Object };

  Type type = Type::Null;
  bool boolean = false;
  double number = 0.0;
  std::string string;
  std::vector<Value> array;
  std::map<std::string, Value> object;
};

Value parse(const std::string& text);
Value loadFile(const std::string& path);
const Value* find(const Value& object, const std::string& name);
std::string getString(const Value& object, const std::string& name, bool required = true,
                      const std::string& fallback = "");
int getInt(const Value& object, const std::string& name, bool required = true,
           int fallback = 0);
double getDouble(const Value& object, const std::string& name, bool required = true,
                 double fallback = 0.0);
bool getBool(const Value& object, const std::string& name, bool required = true,
             bool fallback = false);
std::vector<double> getDoubleArray(const Value& object, const std::string& name);

} // namespace json
} // namespace vumat
