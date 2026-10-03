#include "Json.h"

// Cereal's tiny-dnn archive ships its own rapidjson in namespace rapidjson.
// Keep this copy in another namespace so the two do not collide in one binary.
#define RAPIDJSON_NAMESPACE jsonadapter_rapidjson

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtemplate-body"
#endif
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

struct JsonStore
{
    jsonadapter_rapidjson::Document dom;
};

// RapidJSON's recursive parser overflows the stack on a few dozen kilobytes of
// nested brackets. Untrusted text stays iterative, and nesting past
// kMaxJsonNestingDepth is rejected before parse.
static constexpr uint32_t kMaxJsonNestingDepth = 64;
static constexpr auto kClientJsonParseFlags =
    jsonadapter_rapidjson::kParseNanAndInfFlag | jsonadapter_rapidjson::kParseIterativeFlag;

// True when object or array nesting in untrusted text exceeds kMaxJsonNestingDepth.
static bool jsonNestingTooDeep(std::string_view text)
{
    uint32_t depth = 0;
    bool inString = false;
    bool escape = false;
    for (char ch : text)
    {
        if (inString)
        {
            if (escape)
                escape = false;
            else if (ch == '\\')
                escape = true;
            else if (ch == '"')
                inString = false;
            continue;
        }
        if (ch == '"')
        {
            inString = true;
            continue;
        }
        if (ch == '{' || ch == '[')
        {
            ++depth;
            if (depth > kMaxJsonNestingDepth)
                return true;
        }
        else if ((ch == '}' || ch == ']') && depth > 0)
        {
            --depth;
        }
    }
    return false;
}

// Casts a stored node pointer to a mutable RapidJSON value.
static jsonadapter_rapidjson::Value *V(void *p)
{
    return static_cast<jsonadapter_rapidjson::Value *>(p);
}

// Casts a stored node pointer to a const RapidJSON value.
static const jsonadapter_rapidjson::Value *V(const void *p)
{
    return static_cast<const jsonadapter_rapidjson::Value *>(p);
}

// Creates a null JSON value in a new RapidJSON document.
Json::Json()
    : doc(std::make_shared<JsonStore>())
    , node(nullptr)
{
    node = &doc->dom;
}

// Binds a JSON value to an existing RapidJSON document and node.
Json::Json(std::shared_ptr<JsonStore> document, void *value, bool discardedFlag)
    : doc(std::move(document))
    , node(value)
    , discarded(discardedFlag)
{
}

// Copies a RapidJSON value into a new document the caller can own.
Json Json::fromStored(const void *rapidValue)
{
    Json copy;
    copy.doc->dom.CopyFrom(*V(rapidValue), copy.doc->dom.GetAllocator());
    copy.node = &copy.doc->dom;
    return copy;
}

// Deep-copies another JSON value into a new document.
Json::Json(const Json &other) : Json()
{
    if (V(other.node) != nullptr)
        doc->dom.CopyFrom(*V(other.node), doc->dom.GetAllocator());
    node = &doc->dom;
    discarded = other.discarded;
}

// Moves another JSON document and leaves the source empty.
Json::Json(Json &&other) noexcept
    : doc(std::move(other.doc))
    , node(other.node)
    , discarded(other.discarded)
{
    other.node = nullptr;
}

// Stores a string as a JSON string value.
Json::Json(const std::string &value) : Json()
{
    V(node)->SetString(value.data(), static_cast<jsonadapter_rapidjson::SizeType>(value.size()), doc->dom.GetAllocator());
}

// Stores a C string as a JSON string value, treating null as empty.
Json::Json(const char *value) : Json(std::string(value != nullptr ? value : "")) {}

// Stores a bool as a JSON boolean.
Json::Json(bool value) : Json()
{
    V(node)->SetBool(value);
}

// Stores a signed int as a JSON number.
Json::Json(int value) : Json()
{
    V(node)->SetInt(value);
}

// Stores an unsigned int as a JSON number.
Json::Json(unsigned value) : Json()
{
    V(node)->SetUint(value);
}

// Stores a 64-bit signed integer as a JSON number.
Json::Json(int64_t value) : Json()
{
    V(node)->SetInt64(value);
}

// Stores a 64-bit unsigned integer as a JSON number.
Json::Json(uint64_t value) : Json()
{
    V(node)->SetUint64(value);
}

// Stores a double as a JSON number.
Json::Json(double value) : Json()
{
    V(node)->SetDouble(value);
}

// Builds a JSON object by copying each named child into a new document.
Json::Json(const object_t &object) : Json()
{
    V(node)->SetObject();
    for (const auto &entry : object)
    {
        jsonadapter_rapidjson::Value name(entry.first.data(), static_cast<jsonadapter_rapidjson::SizeType>(entry.first.size()), doc->dom.GetAllocator());
        jsonadapter_rapidjson::Value child;
        child.CopyFrom(*V(entry.second.node), doc->dom.GetAllocator());
        V(node)->AddMember(name, child, doc->dom.GetAllocator());
    }
}

// Builds a JSON object from key-value pairs, or an array when the list is not pairs.
Json::Json(std::initializer_list<Json> init) : Json()
{
    bool asObject = init.size() != 0;
    for (const Json &item : init)
    {
        if (!item.is_array() || item.size() != 2 || !item[size_t{0}].is_string())
            asObject = false;
    }
    if (init.size() == 0)
        return;
    if (asObject)
    {
        ensureObject();
        for (const Json &item : init)
        {
            const std::string key = item[size_t{0}].get<std::string>();
            (*this)[key] = item[1];
        }
        return;
    }
    ensureArray();
    for (const Json &item : init)
        push_back(item);
}

// Replaces this value with an object or array built from the initializer list.
Json &Json::operator=(std::initializer_list<Json> init)
{
    *this = Json(init);
    return *this;
}

// Writes an object or array built from the initializer list into the referenced slot.
JsonRef &JsonRef::operator=(std::initializer_list<Json> init)
{
    return *this = Json(init);
}

// Builds a JSON array by copying each element into a new document.
Json::Json(const array_t &array) : Json()
{
    V(node)->SetArray();
    for (const Json &item : array)
    {
        jsonadapter_rapidjson::Value child;
        child.CopyFrom(*V(item.node), doc->dom.GetAllocator());
        V(node)->PushBack(child, doc->dom.GetAllocator());
    }
}

// Replaces this value with a deep copy of another JSON value.
Json &Json::operator=(const Json &other)
{
    Json copy(other);
    *this = std::move(copy);
    return *this;
}

// Replaces this value by moving another JSON document.
Json &Json::operator=(Json &&other) noexcept
{
    doc = std::move(other.doc);
    node = other.node;
    discarded = other.discarded;
    other.node = nullptr;
    return *this;
}

// Turns a null into an object, or throws if the value is some other type.
void Json::ensureObject()
{
    discarded = false;
    if (V(node)->IsNull())
        V(node)->SetObject();
    if (!V(node)->IsObject())
        throw type_error("expected object");
}

// Turns a null into an array, or throws if the value is some other type.
void Json::ensureArray()
{
    discarded = false;
    if (V(node)->IsNull())
        V(node)->SetArray();
    if (!V(node)->IsArray())
        throw type_error("expected array");
}

// Replaces this value with a JSON string.
Json &Json::operator=(const std::string &value)
{
    *this = Json(value);
    return *this;
}

// Replaces this value with a JSON string copied from a C string.
Json &Json::operator=(const char *value)
{
    *this = Json(value);
    return *this;
}

// Replaces this value with a JSON boolean.
Json &Json::operator=(bool value)
{
    *this = Json(value);
    return *this;
}

// Replaces this value with a JSON number from a signed int.
Json &Json::operator=(int value)
{
    *this = Json(value);
    return *this;
}

// Replaces this value with a JSON number from an unsigned int.
Json &Json::operator=(unsigned value)
{
    *this = Json(value);
    return *this;
}

// Replaces this value with a JSON number from a 64-bit signed integer.
Json &Json::operator=(int64_t value)
{
    *this = Json(value);
    return *this;
}

// Replaces this value with a JSON number from a 64-bit unsigned integer.
Json &Json::operator=(uint64_t value)
{
    *this = Json(value);
    return *this;
}

// Replaces this value with a JSON number from a double.
Json &Json::operator=(double value)
{
    *this = Json(value);
    return *this;
}


// Returns a new empty JSON array.
Json Json::array()
{
    Json value;
    V(value.node)->SetArray();
    return value;
}

// Returns a new empty JSON object.
Json Json::object()
{
    Json value;
    V(value.node)->SetObject();
    return value;
}

// Parses text into a RapidJSON document, or returns a discarded value when exceptions are off.
Json Json::parse(std::string_view text, const void *callback, bool allowExceptions)
{
    (void)callback;
    auto document = std::make_shared<JsonStore>();
    if (jsonNestingTooDeep(text))
    {
        if (!allowExceptions)
        {
            Json discardedValue;
            discardedValue.discarded = true;
            return discardedValue;
        }
        throw parse_error("json nesting too deep");
    }
    document->dom.Parse<kClientJsonParseFlags>(text.data(), text.size());
    if (document->dom.HasParseError())
    {
        if (!allowExceptions)
        {
            Json discardedValue;
            discardedValue.discarded = true;
            return discardedValue;
        }
        throw parse_error(jsonadapter_rapidjson::GetParseError_En(document->dom.GetParseError()));
    }
    void *raw = &document->dom;
    return Json(std::move(document), raw, false);
}

// Parses text into a JSON value and throws on malformed input.
Json Json::parse(std::string_view text)
{
    return parse(text, nullptr, true);
}


// Returns a writable object member, inserting a null when the key is missing.
JsonRef Json::operator[](std::string_view key)
{
    ensureObject();
    jsonadapter_rapidjson::Value name(key.data(), static_cast<jsonadapter_rapidjson::SizeType>(key.size()), doc->dom.GetAllocator());
    auto found = V(node)->FindMember(name);
    if (found == V(node)->MemberEnd())
    {
        V(node)->AddMember(name, jsonadapter_rapidjson::Value(jsonadapter_rapidjson::kNullType), doc->dom.GetAllocator());
        found = V(node)->FindMember(jsonadapter_rapidjson::StringRef(key.data(), key.size()));
    }
    JsonRef ref;
    ref.doc = doc.get();
    ref.slot = &found->value;
    ref.keep = doc;
    return ref;
}

// Copies an object member, throwing when the key is missing.
Json Json::operator[](std::string_view key) const
{
    if (node == nullptr || !V(node)->IsObject())
        throw type_error("expected object");
    auto found = V(node)->FindMember(jsonadapter_rapidjson::StringRef(key.data(), key.size()));
    if (found == V(node)->MemberEnd())
        throw exception("missing key");
    return fromStored(&found->value);
}

// Returns a writable object member for a C-string key.
JsonRef Json::operator[](const char *key)
{
    return (*this)[std::string_view(key != nullptr ? key : "")];
}

// Copies an object member looked up by a C-string key.
Json Json::operator[](const char *key) const
{
    return (*this)[std::string_view(key != nullptr ? key : "")];
}

// Returns a writable array element, appending a null when the index is the new end.
JsonRef Json::operator[](size_t index)
{
    ensureArray();
    if (index > V(node)->Size())
        throw type_error("index past end");
    if (index == V(node)->Size())
        V(node)->PushBack(jsonadapter_rapidjson::Value(jsonadapter_rapidjson::kNullType), doc->dom.GetAllocator());
    JsonRef ref;
    ref.doc = doc.get();
    ref.slot = &(*V(node))[static_cast<jsonadapter_rapidjson::SizeType>(index)];
    ref.keep = doc;
    return ref;
}

// Copies an array element, throwing when the index is out of range.
Json Json::operator[](size_t index) const
{
    if (node == nullptr || !V(node)->IsArray() || index >= V(node)->Size())
        throw type_error("index past end");
    return fromStored(&(*V(node))[static_cast<jsonadapter_rapidjson::SizeType>(index)]);
}

// Reports whether this object has a member with the given key.
bool Json::contains(std::string_view key) const
{
    if (node == nullptr || !V(node)->IsObject())
        return false;
    return V(node)->FindMember(jsonadapter_rapidjson::StringRef(key.data(), key.size())) != V(node)->MemberEnd();
}

// Reports whether this value is JSON null and not a discarded parse.
bool Json::is_null() const
{
    return !discarded && node != nullptr && V(node)->IsNull();
}

// Reports whether parsing produced a discarded value instead of JSON.
bool Json::is_discarded() const
{
    return discarded;
}

// Reports whether this value is a JSON object.
bool Json::is_object() const
{
    return !discarded && node != nullptr && V(node)->IsObject();
}

// Reports whether this value is a JSON array.
bool Json::is_array() const
{
    return !discarded && node != nullptr && V(node)->IsArray();
}

// Reports whether this value is a JSON string.
bool Json::is_string() const
{
    return !discarded && node != nullptr && V(node)->IsString();
}

// Reports whether this value is a JSON boolean.
bool Json::is_boolean() const
{
    return !discarded && node != nullptr && V(node)->IsBool();
}

// Reports whether this value is any JSON number.
bool Json::is_number() const
{
    return !discarded && node != nullptr && V(node)->IsNumber();
}

// Reports whether this value is a JSON number stored as an integer.
bool Json::is_number_integer() const
{
    return !discarded && node != nullptr && V(node)->IsInt64();
}

// Reports whether this integer JSON number is zero or positive.
bool Json::is_number_unsigned() const
{
    if (discarded || node == nullptr || !V(node)->IsInt64())
        return false;
    if (V(node)->IsUint64())
        return true;
    return V(node)->GetInt64() >= 0;
}

// Returns the element, member, or string length, or zero for other types.
size_t Json::size() const
{
    if (node == nullptr)
        return 0;
    if (V(node)->IsArray())
        return V(node)->Size();
    if (V(node)->IsObject())
        return V(node)->MemberCount();
    if (V(node)->IsString())
        return V(node)->GetStringLength();
    return 0;
}

// Reports whether this value is null, discarded, or an empty array, object, or string.
bool Json::empty() const
{
    if (discarded || node == nullptr || V(node)->IsNull())
        return true;
    if (V(node)->IsArray())
        return V(node)->Empty();
    if (V(node)->IsObject())
        return V(node)->MemberCount() == 0;
    if (V(node)->IsString())
        return V(node)->GetStringLength() == 0;
    return false;
}

// Serializes this value to compact JSON text.
std::string Json::dump(int indent) const
{
    (void)indent;
    jsonadapter_rapidjson::StringBuffer buffer;
    jsonadapter_rapidjson::Writer<jsonadapter_rapidjson::StringBuffer, jsonadapter_rapidjson::UTF8<>, jsonadapter_rapidjson::UTF8<>, jsonadapter_rapidjson::CrtAllocator,
        jsonadapter_rapidjson::kWriteNanAndInfFlag>
        writer(buffer);
    if (node != nullptr)
        V(node)->Accept(writer);
    else
        writer.Null();
    return std::string(buffer.GetString(), buffer.GetSize());
}

// Appends a copy of an element to this JSON array.
void Json::push_back(const Json &item)
{
    ensureArray();
    jsonadapter_rapidjson::Value child;
    child.CopyFrom(*V(item.node), doc->dom.GetAllocator());
    V(node)->PushBack(child, doc->dom.GetAllocator());
}

template <typename T>
// Converts this JSON value to T and throws when the RapidJSON type does not match.
T Json::get() const
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        if (!is_string())
            throw type_error("expected string");
        return std::string(V(node)->GetString(), V(node)->GetStringLength());
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        if (!is_boolean())
            throw type_error("expected bool");
        return V(node)->GetBool();
    }
    else if constexpr (std::is_same_v<T, double>)
    {
        if (!is_number())
            throw type_error("expected number");
        return V(node)->GetDouble();
    }
    else if constexpr (std::is_same_v<T, std::vector<uint8_t>>)
    {
        if (!is_array())
            throw type_error("expected array");
        std::vector<uint8_t> bytes;
        bytes.reserve(V(node)->Size());
        for (const auto &entry : V(node)->GetArray())
        {
            if (!entry.IsUint64() || entry.GetUint64() > 255)
                throw type_error("expected byte");
            bytes.push_back(static_cast<uint8_t>(entry.GetUint64()));
        }
        return bytes;
    }
    else if constexpr (std::is_same_v<T, std::vector<std::string>>)
    {
        if (!is_array())
            throw type_error("expected array");
        std::vector<std::string> values;
        values.reserve(V(node)->Size());
        for (const auto &entry : V(node)->GetArray())
        {
            if (!entry.IsString())
                throw type_error("expected string");
            values.emplace_back(entry.GetString(), entry.GetStringLength());
        }
        return values;
    }
    else if constexpr (std::is_integral_v<T>)
    {
        if (!is_number())
            throw type_error("expected number");
        if (V(node)->IsUint64())
            return static_cast<T>(V(node)->GetUint64());
        if (V(node)->IsInt64())
            return static_cast<T>(V(node)->GetInt64());
        return static_cast<T>(V(node)->GetDouble());
    }
    else
    {
        static_assert(sizeof(T) == 0, "unsupported json get");
    }
}


// Copies this object's members into a map of owned JSON values.
Json::object_t Json::itemsObject() const
{
    object_t object;
    if (!is_object())
        return object;
    for (auto it = V(node)->MemberBegin(); it != V(node)->MemberEnd(); ++it)
    {
        object.emplace(std::string(it->name.GetString(), it->name.GetStringLength()), fromStored(&it->value));
    }
    return object;
}

// Copies this array's elements into a vector of owned JSON values.
Json::array_t Json::itemsArray() const
{
    array_t array;
    if (!is_array())
        return array;
    array.reserve(V(node)->Size());
    for (const auto &entry : V(node)->GetArray())
        array.push_back(fromStored(&entry));
    return array;
}

// Reports whether this JSON string equals a C string.
bool Json::operator==(const char *text) const
{
    return is_string() && std::string(V(node)->GetString(), V(node)->GetStringLength()) == text;
}

// Reports whether this JSON string equals a string.
bool Json::operator==(const std::string &text) const
{
    return is_string() && std::string(V(node)->GetString(), V(node)->GetStringLength()) == text;
}

// Reports whether this JSON integer equals an unsigned 64-bit number.
bool Json::operator==(uint64_t number) const
{
    return is_number_integer() && get<uint64_t>() == number;
}

// Reports whether this JSON integer equals a signed int.
bool Json::operator==(int number) const
{
    return is_number_integer() && get<int64_t>() == number;
}

// Reports whether this JSON boolean equals the given flag.
bool Json::operator==(bool flag) const
{
    return is_boolean() && V(node)->GetBool() == flag;
}

// Returns an owned copy of the object member or array element at this iterator.
Json Json::iterator::operator*() const
{
    return value();
}

// Returns the object key at this iterator, or empty when iterating an array.
std::string Json::iterator::key() const
{
    if (!object || parent == nullptr || V(parent->node) == nullptr)
        return {};
    const auto &member = V(parent->node)->MemberBegin()[index];
    return std::string(member.name.GetString(), member.name.GetStringLength());
}

// Copies the object member or array element at this iterator.
Json Json::iterator::value() const
{
    if (parent == nullptr || V(parent->node) == nullptr)
        return Json();
    if (object)
        return Json::fromStored(&V(parent->node)->MemberBegin()[index].value);
    return Json::fromStored(&(*V(parent->node))[static_cast<jsonadapter_rapidjson::SizeType>(index)]);
}

// Returns an iterator at the first object member or array element.
Json::iterator Json::begin() const
{
    iterator it;
    it.parent = this;
    it.object = is_object();
    return it;
}

// Returns an iterator one past the last object member or array element.
Json::iterator Json::end() const
{
    iterator it;
    it.parent = this;
    it.object = is_object();
    it.index = size();
    return it;
}

// Turns the referenced null into an object, or throws if the slot is some other type.
void JsonRef::ensureObject()
{
    if (V(slot)->IsNull())
        V(slot)->SetObject();
    if (!V(slot)->IsObject())
        throw Json::type_error("expected object");
}

// Turns the referenced null into an array, or throws if the slot is some other type.
void JsonRef::ensureArray()
{
    if (V(slot)->IsNull())
        V(slot)->SetArray();
    if (!V(slot)->IsArray())
        throw Json::type_error("expected array");
}

// Returns a writable child of the referenced object, inserting a null when the key is missing.
JsonRef JsonRef::operator[](std::string_view key)
{
    ensureObject();
    jsonadapter_rapidjson::Value name(key.data(), static_cast<jsonadapter_rapidjson::SizeType>(key.size()), doc->dom.GetAllocator());
    auto found = V(slot)->FindMember(name);
    if (found == V(slot)->MemberEnd())
    {
        V(slot)->AddMember(name, jsonadapter_rapidjson::Value(jsonadapter_rapidjson::kNullType), doc->dom.GetAllocator());
        found = V(slot)->FindMember(jsonadapter_rapidjson::StringRef(key.data(), key.size()));
    }
    JsonRef ref;
    ref.doc = doc;
    ref.slot = &found->value;
    ref.keep = keep;
    return ref;
}

// Copies a child of the referenced object, throwing when the key is missing.
Json JsonRef::operator[](std::string_view key) const
{
    if (slot == nullptr || !V(slot)->IsObject())
        throw Json::type_error("expected object");
    auto found = V(slot)->FindMember(jsonadapter_rapidjson::StringRef(key.data(), key.size()));
    if (found == V(slot)->MemberEnd())
        throw Json::exception("missing key");
    return Json::fromStored(&found->value);
}

// Returns a writable child of the referenced object for a C-string key.
JsonRef JsonRef::operator[](const char *key)
{
    return (*this)[std::string_view(key != nullptr ? key : "")];
}

// Copies a child of the referenced object looked up by a C-string key.
Json JsonRef::operator[](const char *key) const
{
    return static_cast<const JsonRef &>(*this)[std::string_view(key != nullptr ? key : "")];
}

// Returns a writable element of the referenced array, appending a null at the new end.
JsonRef JsonRef::operator[](size_t index)
{
    ensureArray();
    if (index > V(slot)->Size())
        throw Json::type_error("index past end");
    if (index == V(slot)->Size())
        V(slot)->PushBack(jsonadapter_rapidjson::Value(jsonadapter_rapidjson::kNullType), doc->dom.GetAllocator());
    JsonRef ref;
    ref.doc = doc;
    ref.slot = &(*V(slot))[static_cast<jsonadapter_rapidjson::SizeType>(index)];
    ref.keep = keep;
    return ref;
}

// Overwrites the referenced slot with a copy of a JSON value.
JsonRef &JsonRef::operator=(const Json &incoming)
{
    V(slot)->CopyFrom(*V(incoming.node), doc->dom.GetAllocator());
    return *this;
}

// Overwrites the referenced slot with a JSON string.
JsonRef &JsonRef::operator=(const std::string &incoming)
{
    V(slot)->SetString(incoming.data(), static_cast<jsonadapter_rapidjson::SizeType>(incoming.size()), doc->dom.GetAllocator());
    return *this;
}

// Overwrites the referenced slot with a JSON string from a C string.
JsonRef &JsonRef::operator=(const char *incoming)
{
    return *this = std::string(incoming != nullptr ? incoming : "");
}

// Overwrites the referenced slot with a JSON boolean.
JsonRef &JsonRef::operator=(bool incoming)
{
    V(slot)->SetBool(incoming);
    return *this;
}

// Overwrites the referenced slot with a JSON number from a signed int.
JsonRef &JsonRef::operator=(int incoming)
{
    V(slot)->SetInt(incoming);
    return *this;
}

// Overwrites the referenced slot with a JSON number from an unsigned int.
JsonRef &JsonRef::operator=(unsigned incoming)
{
    V(slot)->SetUint(incoming);
    return *this;
}

// Overwrites the referenced slot with a JSON number from a 64-bit signed integer.
JsonRef &JsonRef::operator=(int64_t incoming)
{
    V(slot)->SetInt64(incoming);
    return *this;
}

// Overwrites the referenced slot with a JSON number from a 64-bit unsigned integer.
JsonRef &JsonRef::operator=(uint64_t incoming)
{
    V(slot)->SetUint64(incoming);
    return *this;
}

// Overwrites the referenced slot with a JSON number from a double.
JsonRef &JsonRef::operator=(double incoming)
{
    V(slot)->SetDouble(incoming);
    return *this;
}


// Reports whether the referenced object has a member with the given key.
bool JsonRef::contains(std::string_view key) const
{
    if (slot == nullptr || !V(slot)->IsObject())
        return false;
    return V(slot)->FindMember(jsonadapter_rapidjson::StringRef(key.data(), key.size())) != V(slot)->MemberEnd();
}

// Reports whether the referenced RapidJSON slot is null.
bool JsonRef::is_null() const { return slot != nullptr && V(slot)->IsNull(); }
// Reports false, because a referenced slot is a stored JSON value.
bool JsonRef::is_discarded() const { return false; }
// Reports whether the referenced slot is a JSON object.
bool JsonRef::is_object() const { return slot != nullptr && V(slot)->IsObject(); }
// Reports whether the referenced slot is a JSON array.
bool JsonRef::is_array() const { return slot != nullptr && V(slot)->IsArray(); }
// Reports whether the referenced slot is a JSON string.
bool JsonRef::is_string() const { return slot != nullptr && V(slot)->IsString(); }
// Reports whether the referenced slot is a JSON boolean.
bool JsonRef::is_boolean() const { return slot != nullptr && V(slot)->IsBool(); }
// Reports whether the referenced slot is any JSON number.
bool JsonRef::is_number() const { return slot != nullptr && V(slot)->IsNumber(); }

// Reports whether the referenced slot is a JSON number stored as an integer.
bool JsonRef::is_number_integer() const
{
    return slot != nullptr && V(slot)->IsInt64();
}

// Reports whether the referenced integer is zero or positive.
bool JsonRef::is_number_unsigned() const
{
    if (slot == nullptr || !V(slot)->IsInt64())
        return false;
    if (V(slot)->IsUint64())
        return true;
    return V(slot)->GetInt64() >= 0;
}

// Returns the referenced element, member, or string length, or zero for other types.
size_t JsonRef::size() const
{
    if (slot == nullptr)
        return 0;
    if (V(slot)->IsArray())
        return V(slot)->Size();
    if (V(slot)->IsObject())
        return V(slot)->MemberCount();
    if (V(slot)->IsString())
        return V(slot)->GetStringLength();
    return 0;
}

// Reports whether the referenced slot is null or an empty array, object, or string.
bool JsonRef::empty() const
{
    if (slot == nullptr || V(slot)->IsNull())
        return true;
    if (V(slot)->IsArray())
        return V(slot)->Empty();
    if (V(slot)->IsObject())
        return V(slot)->MemberCount() == 0;
    if (V(slot)->IsString())
        return V(slot)->GetStringLength() == 0;
    return false;
}

// Serializes the referenced slot to compact JSON text.
std::string JsonRef::dump() const
{
    return static_cast<Json>(*this).dump();
}



// Copies the referenced slot into an owned JSON value.
JsonRef::operator Json() const
{
    if (slot == nullptr)
        return Json();
    return Json::fromStored(slot);
}

// Reports whether the referenced JSON string equals a C string.
bool JsonRef::operator==(const char *text) const
{
    return static_cast<Json>(*this) == text;
}

// Reports whether the referenced JSON string equals a string.
bool JsonRef::operator==(const std::string &text) const
{
    return static_cast<Json>(*this) == text;
}

// Reports whether the referenced JSON integer equals an unsigned 64-bit number.
bool JsonRef::operator==(uint64_t number) const
{
    return static_cast<Json>(*this) == number;
}

// Reports whether the referenced JSON integer equals a signed int.
bool JsonRef::operator==(int number) const
{
    return static_cast<Json>(*this) == number;
}

// Reports whether the referenced JSON boolean equals the given flag.
bool JsonRef::operator==(bool flag) const
{
    return static_cast<Json>(*this) == flag;
}

// Returns an owned copy of the referenced member or element at this iterator.
Json JsonRef::iterator::operator*() const
{
    return value();
}

// Returns the object key at this reference iterator, or empty when iterating an array.
std::string JsonRef::iterator::key() const
{
    if (!object || parent == nullptr || V(parent->slot) == nullptr)
        return {};
    const auto &member = V(parent->slot)->MemberBegin()[index];
    return std::string(member.name.GetString(), member.name.GetStringLength());
}

// Returns a reference to the object member or array element at this iterator.
JsonRef JsonRef::iterator::value() const
{
    JsonRef ref;
    if (parent == nullptr || V(parent->slot) == nullptr)
        return ref;
    ref.doc = parent->doc;
    ref.keep = parent->keep;
    if (object)
        ref.slot = &V(parent->slot)->MemberBegin()[index].value;
    else
        ref.slot = &(*V(parent->slot))[static_cast<jsonadapter_rapidjson::SizeType>(index)];
    return ref;
}

// Returns an iterator at the first member or element of the referenced value.
JsonRef::iterator JsonRef::begin() const
{
    iterator it;
    it.parent = this;
    it.object = is_object();
    return it;
}

// Returns an iterator one past the last member or element of the referenced value.
JsonRef::iterator JsonRef::end() const
{
    iterator it;
    it.parent = this;
    it.object = is_object();
    it.index = (is_array() || is_object()) ? size() : 0;
    return it;
}

// Writes this JSON value to a stream as compact text.
std::ostream &operator<<(std::ostream &os, const Json &value)
{
    os << value.dump();
    return os;
}

// Parses the rest of a stream into a JSON value.
std::istream &operator>>(std::istream &is, Json &value)
{
    std::ostringstream buffer;
    buffer << is.rdbuf();
    value = Json::parse(buffer.str());
    return is;
}

// Parses text into a JSON value and stores the RapidJSON error text on failure.
bool parseJson(std::string_view text, Json &out, std::string &error)
{
    if (jsonNestingTooDeep(text))
    {
        error = "json nesting too deep";
        return false;
    }
    auto document = std::make_shared<JsonStore>();
    document->dom.Parse<kClientJsonParseFlags>(text.data(), text.size());
    if (document->dom.HasParseError())
    {
        error = jsonadapter_rapidjson::GetParseError_En(document->dom.GetParseError());
        return false;
    }
    out = Json::parse(text, nullptr, true);
    return true;
}

template std::string Json::get<std::string>() const;
template bool Json::get<bool>() const;
template double Json::get<double>() const;
template uint64_t Json::get<uint64_t>() const;
template uint32_t Json::get<uint32_t>() const;
template uint8_t Json::get<uint8_t>() const;
template int64_t Json::get<int64_t>() const;
template int Json::get<int>() const;
template std::vector<uint8_t> Json::get<std::vector<uint8_t>>() const;
template std::vector<std::string> Json::get<std::vector<std::string>>() const;
