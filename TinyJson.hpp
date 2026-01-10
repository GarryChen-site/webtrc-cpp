#ifndef TINYJSON_HPP
#define TINYJSON_HPP

#include <concepts>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace tinyjson {

struct Value;

using Object = std::map<std::string, Value>;
using Array = std::vector<Value>;

enum class Type { Null, String, Number, Object, Array, Boolean };

struct Value {
  using Variant =
      std::variant<std::monostate, std::string, double, Object, Array, bool>;

  Variant data;

  Value() : data(std::monostate{}) {}
  Value(std::string s) : data(std::move(s)) {}
  Value(const char *s) : data(std::string(s)) {}

  template <std::floating_point T> Value(T d) : data(static_cast<double>(d)) {}

  template <std::integral T> Value(T d) : data(static_cast<double>(d)) {}

  Value(bool b) : data(b) {}

  static Value object() {
    Value v;
    v.data = Object{};
    return v;
  }

  static Value array() {
    Value v;
    v.data = Array{};
    return v;
  }

  [[nodiscard]] Type type() const { return static_cast<Type>(data.index()); }

  void set(std::string key, Value val) {
    if (auto *obj = std::get_if<Object>(&data)) {
      (*obj)[std::move(key)] = std::move(val);
    }
  }

  void push(Value val) {
    if (auto *arr = std::get_if<Array>(&data)) {
      arr->push_back(std::move(val));
    }
  }

  [[nodiscard]] std::string serialize() const {
    return std::visit(
        [](auto &&arg) -> std::string {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, std::monostate>) {
            return "null";
          } else if constexpr (std::is_same_v<T, std::string>) {
            return std::format("\"{}\"", arg);
          } else if constexpr (std::is_same_v<T, double>) {
            return std::format("{}", arg);
          } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
          } else if constexpr (std::is_same_v<T, Object>) {
            std::string s = "{";
            for (auto it = arg.begin(); it != arg.end(); ++it) {
              if (it != arg.begin())
                s += ",";
              s += std::format("\"{}\":{}", it->first, it->second.serialize());
            }
            return s + "}";
          } else if constexpr (std::is_same_v<T, Array>) {
            std::string s = "[";
            for (size_t i = 0; i < arg.size(); ++i) {
              if (i > 0)
                s += ",";
              s += arg[i].serialize();
            }
            return s + "]";
          }
        },
        data);
  }
};

class Parser {
  std::string_view src;
  size_t pos = 0;

  void skip_ws() {
    while (pos < src.size() && std::isspace(src[pos]))
      ++pos;
  }

  char peek() {
    skip_ws();
    return pos < src.size() ? src[pos] : 0;
  }

  char next() {
    skip_ws();
    return pos < src.size() ? src[pos++] : 0;
  }

  std::string parse_string() {
    if (next() != '\"')
      return "";
    size_t start = pos;
    while (pos < src.size() && src[pos] != '\"')
      pos++;
    std::string_view s = src.substr(start, pos - start);
    if (pos < src.size())
      pos++;
    return std::string(s);
  }

  double parse_number() {
    size_t start = pos;
    while (pos < src.size() &&
           (std::isdigit(src[pos]) || src[pos] == '.' || src[pos] == '-'))
      pos++;
    return std::stod(std::string(src.substr(start, pos - start)));
  }

public:
  Value parse(std::string_view input) {
    src = input;
    pos = 0;
    return parse_value();
  }

  Value parse_value() {
    char c = peek();
    switch (c) {
    case '\"':
      return Value(parse_string());
    case '{':
      return parse_object();
    case '[':
      return parse_array();
    case 't':
      pos += 4;
      return Value(true);
    case 'f':
      pos += 5;
      return Value(false);
    case 'n':
      pos += 4;
      return {};
    default:
      if (std::isdigit(c) || c == '-')
        return Value(parse_number());
      return {};
    }
  }

  Value parse_object() {
    next();

    Value v = Value::object();
    while (peek() != '}' && peek() != 0) {
      std::string key = parse_string();
      if (next() != ':')
        break;
      v.set(std::move(key), parse_value());
      if (peek() == ',')
        next();
    }
    next();
    return v;
  }

  Value parse_array() {
    next();

    Value v = Value::array();
    while (peek() != ']' && peek() != 0) {
      v.push(parse_value());
      if (peek() == ',')
        next();
    }
    next();
    return v;
  }
};

} // namespace tinyjson

#endif