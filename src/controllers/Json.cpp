#include <XBase/Json.h>
#include <fstream>
#include <sstream>
#include <cctype>

namespace XBase::Json {

// --- Value accessors ---

bool Value::AsBool(bool def) const {
    return type == Bool ? std::get<bool>(data) : def;
}

double Value::AsNumber(double def) const {
    return type == Number ? std::get<double>(data) : def;
}

int Value::AsInt(int def) const {
    return type == Number ? static_cast<int>(std::get<double>(data)) : def;
}

std::string Value::AsString(const std::string& def) const {
    return type == String ? std::get<std::string>(data) : def;
}

const Value& Value::operator[](const std::string& key) const {
    static Value nullValue;
    if (type != Object) return nullValue;
    const auto& map = std::get<std::unordered_map<std::string, Value>>(data);
    auto it = map.find(key);
    return it != map.end() ? it->second : nullValue;
}

const Value& Value::operator[](size_t index) const {
    static Value nullValue;
    if (type != Array) return nullValue;
    const auto& arr = std::get<std::vector<Value>>(data);
    return index < arr.size() ? arr[index] : nullValue;
}

Value& Value::Set(const std::string& key, const Value& val) {
    if (type != Object) {
        type = Object;
        data = std::unordered_map<std::string, Value>();
    }
    auto& map = std::get<std::unordered_map<std::string, Value>>(data);
    map[key] = val;
    return map[key];
}

void Value::Push(const Value& val) {
    if (type != Array) {
        type = Array;
        data = std::vector<Value>();
    }
    std::get<std::vector<Value>>(data).push_back(val);
}

// --- Serialization ---

std::string Escape(const std::string& s) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                out += "\\u00";
                out += hex[(c >> 4) & 0x0f];
                out += hex[c & 0x0f];
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    out += '"';
    return out;
}

std::string Value::Serialize(bool pretty, int indent) const {
    std::string pad;
    if (pretty) pad = std::string(static_cast<size_t>(indent) * 2, ' ');

    switch (type) {
    case Null:
        return "null";
    case Bool:
        return std::get<bool>(data) ? "true" : "false";
    case Number:
        return std::to_string(std::get<double>(data));
    case String:
        return Escape(std::get<std::string>(data));
    case Array: {
        const auto& arr = std::get<std::vector<Value>>(data);
        if (arr.empty()) return "[]";
        std::string s = "[";
        if (pretty) s += "\n";
        for (size_t i = 0; i < arr.size(); i++) {
            if (i > 0) s += ",";
            if (pretty) s += "\n" + pad + "  ";
            s += arr[i].Serialize(pretty, indent + 1);
        }
        if (pretty) s += "\n" + pad;
        s += "]";
        return s;
    }
    case Object: {
        const auto& map = std::get<std::unordered_map<std::string, Value>>(data);
        if (map.empty()) return "{}";
        std::string s = "{";
        if (pretty) s += "\n";
        bool first = true;
        for (const auto& [k, v] : map) {
            if (!first) s += ",";
            if (pretty) s += "\n" + pad + "  ";
            s += Escape(k) + ": " + v.Serialize(pretty, indent + 1);
            first = false;
        }
        if (pretty) s += "\n" + pad;
        s += "}";
        return s;
    }
    }
    return "null";
}

// --- Parser ---

struct Parser {
    const std::string& input;
    size_t pos = 0;
    bool valid = true;

    void Fail() {
        valid = false;
    }

    void SkipWhitespace() {
        while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) pos++;
    }

    char Peek() {
        SkipWhitespace();
        return pos < input.size() ? input[pos] : '\0';
    }

    char Consume() {
        SkipWhitespace();
        return pos < input.size() ? input[pos++] : '\0';
    }

    bool Match(char c) {
        SkipWhitespace();
        if (pos < input.size() && input[pos] == c) { pos++; return true; }
        return false;
    }

    Value ParseValue() {
        SkipWhitespace();
        if (pos >= input.size()) {
            Fail();
            return {};
        }
        char c = input[pos];
        if (c == '{') return ParseObject();
        if (c == '[') return ParseArray();
        if (c == '"') return ParseString();
        if (c == 't' || c == 'f') return ParseBool();
        if (c == 'n') {
            if (!ParseLiteral("null")) Fail();
            return {};
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return ParseNumber();
        Fail();
        return {};
    }

    Value ParseObject() {
        Value val;
        val.type = Value::Object;
        val.data = std::unordered_map<std::string, Value>();
        auto& map = std::get<std::unordered_map<std::string, Value>>(val.data);
        Consume(); // '{'
        if (Match('}')) return val;
        while (valid) {
            if (Peek() != '"') {
                Fail();
                return {};
            }
            std::string key = ParseString().AsString();
            if (!Match(':')) {
                Fail();
                return {};
            }
            map[key] = ParseValue();
            if (!valid) return {};
            if (Match('}')) return val;
            if (!Match(',')) {
                Fail();
                return {};
            }
        }
        return {};
    }

    Value ParseArray() {
        Value val;
        val.type = Value::Array;
        val.data = std::vector<Value>();
        auto& arr = std::get<std::vector<Value>>(val.data);
        Consume(); // '['
        if (Match(']')) return val;
        while (valid) {
            arr.push_back(ParseValue());
            if (!valid) return {};
            if (Match(']')) return val;
            if (!Match(',')) {
                Fail();
                return {};
            }
        }
        return {};
    }

    Value ParseString() {
        if (Consume() != '"') {
            Fail();
            return {};
        }
        std::string s;
        bool closed = false;
        while (pos < input.size()) {
            char c = input[pos++];
            if (c == '"') {
                closed = true;
                break;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                Fail();
                return {};
            }
            if (c == '\\' && pos < input.size()) {
                char esc = input[pos++];
                switch (esc) {
                case '"': s += '"'; break;
                case '\\': s += '\\'; break;
                case '/': s += '/'; break;
                case 'b': s += '\b'; break;
                case 'f': s += '\f'; break;
                case 'n': s += '\n'; break;
                case 'r': s += '\r'; break;
                case 't': s += '\t'; break;
                case 'u': {
                    if (pos + 4 > input.size()) {
                        Fail();
                        return {};
                    }
                    unsigned int cp = 0;
                    for (int i = 0; i < 4; i++) {
                        char h = input[pos++];
                        cp *= 16;
                        if (h >= '0' && h <= '9') cp += h - '0';
                        else if (h >= 'a' && h <= 'f') cp += h - 'a' + 10;
                        else if (h >= 'A' && h <= 'F') cp += h - 'A' + 10;
                        else {
                            Fail();
                            return {};
                        }
                    }
                    if (cp < 0x80) {
                        s += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        s += static_cast<char>(0xC0 | (cp >> 6));
                        s += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        s += static_cast<char>(0xE0 | (cp >> 12));
                        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        s += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default:
                    Fail();
                    return {};
                }
            } else if (c == '\\') {
                Fail();
                return {};
            } else {
                s += c;
            }
        }
        if (!closed) {
            Fail();
            return {};
        }
        return Value(s);
    }

    Value ParseNumber() {
        const size_t start = pos;
        if (Peek() == '-') pos++;
        if (pos >= input.size()) {
            Fail();
            return {};
        }
        if (input[pos] == '0') {
            pos++;
        } else if (std::isdigit(static_cast<unsigned char>(input[pos]))) {
            while (pos < input.size() && std::isdigit(static_cast<unsigned char>(input[pos]))) pos++;
        } else {
            Fail();
            return {};
        }
        if (pos < input.size() && input[pos] == '.') {
            pos++;
            const size_t fractionStart = pos;
            while (pos < input.size() && std::isdigit(static_cast<unsigned char>(input[pos]))) pos++;
            if (pos == fractionStart) {
                Fail();
                return {};
            }
        }
        if (pos < input.size() && (input[pos] == 'e' || input[pos] == 'E')) {
            pos++;
            if (pos < input.size() && (input[pos] == '+' || input[pos] == '-')) pos++;
            const size_t exponentStart = pos;
            while (pos < input.size() && std::isdigit(static_cast<unsigned char>(input[pos]))) pos++;
            if (pos == exponentStart) {
                Fail();
                return {};
            }
        }
        try {
            return Value(std::stod(input.substr(start, pos - start)));
        } catch (...) {
            Fail();
            return {};
        }
    }

    Value ParseBool() {
        if (ParseLiteral("true")) return Value(true);
        if (ParseLiteral("false")) return Value(false);
        Fail();
        return {};
    }

    bool ParseLiteral(const char* lit) {
        size_t len = std::strlen(lit);
        if (pos + len > input.size() || input.compare(pos, len, lit) != 0) return false;
        pos += len;
        return true;
    }
};

Value Value::Parse(const std::string& text) {
    Parser parser{text};
    Value value = parser.ParseValue();
    parser.SkipWhitespace();
    if (!parser.valid || parser.pos != text.size()) return {};
    return value;
}

Value Value::Load(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return {};
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return Parse(text);
}

bool Value::Save(const Value& val, const std::string& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) return false;
    file << val.Serialize(true);
    return true;
}

} // namespace XBase::Json
