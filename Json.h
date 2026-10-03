#pragma once

// RapidJSON value with an nlohmann-shaped call surface: operator[], get<T>,
// contains, dump, and parse. A copy owns its own document. operator[] returns
// a Ref into the parent.

#include <cstdint>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>


// Owning document behind one JSON value, so a copied profile, message, or auth frame does not alias the parse it came from.
struct JsonStore;

class Json;

class JsonRef
{
public:
    // Empty field handle used before a handler binds it to a key in a parsed frame.
    JsonRef() = default;

    // Opens a named field inside a frame so the handler can replace a name, id, or nested object.
    JsonRef operator[](std::string_view key);
    // Reads a named field inside a frame when the handler must not change it.
    Json operator[](std::string_view key) const;
    // Opens a named field addressed by a string literal such as a type tag or "data".
    JsonRef operator[](const char *key);
    // Reads a literal-named field when the handler must not change the frame.
    Json operator[](const char *key) const;
    // Opens one element of a JSON array such as languages, messages, or profile versions.
    JsonRef operator[](size_t index);

    // Replaces this field with another JSON value, as when a nested object is moved into a frame.
    JsonRef &operator=(const Json &value);
    // Stores a display string such as a name, bio, email, or chat body in this field.
    JsonRef &operator=(const std::string &value);
    // Stores a literal type tag or short token, such as the frame's type string, in this field.
    JsonRef &operator=(const char *value);
    // Stores a flag such as read, deleted, matched, or favourite in this field.
    JsonRef &operator=(bool value);
    // Stores a small signed count such as a page size or a reason code in this field.
    JsonRef &operator=(int value);
    // Stores an unsigned count such as a list length in this field.
    JsonRef &operator=(unsigned value);
    // Stores a signed timestamp or other wide integer in this field.
    JsonRef &operator=(int64_t value);
    // Stores an account, profile, media, or conversation id in this field.
    JsonRef &operator=(uint64_t value);
    // Stores a latitude, longitude, or rating in this field.
    JsonRef &operator=(double value);
    // Replaces this field with an inline JSON array or object written at the call site.
    JsonRef &operator=(std::initializer_list<Json> init);

    // Stores an enum such as a report reason or an analytics action as its integer value.
    template <typename T, typename std::enable_if<std::is_enum_v<T>, int>::type = 0>
    JsonRef &operator=(T value)
    {
        return *this = static_cast<int64_t>(value);
    }

    // Stores a list of ids, language codes, or strings as a JSON array in this field.
    template <typename T>
    JsonRef &operator=(const std::vector<T> &values);

    // Reports whether a frame carries a key before the unpacker treats that key as required.
    bool contains(std::string_view key) const;
    // Reports a JSON null, which older clients send for an optional field they left out.
    bool is_null() const;
    // Reports a value that failed to parse, so the caller does not treat it as a real frame.
    bool is_discarded() const;
    // Reports a JSON object, which is the shape of every text frame and nested data blob.
    bool is_object() const;
    // Reports a JSON array, which is the shape of message lists, language codes, and batch payloads.
    bool is_array() const;
    // Reports a JSON string such as an email, bio, or request id.
    bool is_string() const;
    // Reports a JSON boolean such as matched, deleted, or the likes-tab filter flag.
    bool is_boolean() const;
    // Reports any JSON number, including ratings and coordinates that are not integers.
    bool is_number() const;
    // Reports an integer JSON number such as an id or a unix timestamp.
    bool is_number_integer() const;
    // Reports a non-negative integer JSON number such as an id that must not be negative.
    bool is_number_unsigned() const;
    // Counts keys or elements while a handler walks messages, languages, or a batch.
    size_t size() const;
    // Reports an empty object or array so a handler can skip a list the client did not send.
    bool empty() const;
    // Serializes this field to the UTF-8 text the gateway writes on a browser socket.
    std::string dump() const;

    // Reads this field as the C++ type the handler asked for, such as an id or a string.
    template <typename T>
    T get() const;

    // Reads a nested key from this field, or returns the fallback when an older client omitted it.
    template <typename T>
    T value(std::string_view key, T fallback) const;

    // Copies this field into an owning JSON value so it can outlive the parent frame.
    operator Json() const;

    // One conversion so `name = j["name"]` is not also a char assignment,
    // and `uint8_t age = j["age"]` is not a tie among the integer operators.
    template <typename T, typename std::enable_if<
        std::is_same_v<T, std::string> || std::is_same_v<T, bool> || std::is_same_v<T, double> || std::is_same_v<T, float> ||
        std::is_same_v<T, std::vector<uint8_t>> || std::is_same_v<T, std::vector<std::string>> || std::is_enum_v<T> ||
        (std::is_integral_v<T> && !std::is_same_v<T, char> && !std::is_same_v<T, bool>), int>::type = 0>
    operator T() const
    {
        if constexpr (std::is_enum_v<T>)
            return static_cast<T>(get<int64_t>());
        else if constexpr (std::is_same_v<T, std::string>)
            return get<std::string>();
        else if constexpr (std::is_same_v<T, bool>)
            return get<bool>();
        else if constexpr (std::is_same_v<T, float>)
            return static_cast<float>(get<double>());
        else if constexpr (std::is_same_v<T, double>)
            return get<double>();
        else if constexpr (std::is_same_v<T, std::vector<uint8_t>>)
            return get<std::vector<uint8_t>>();
        else if constexpr (std::is_same_v<T, std::vector<std::string>>)
            return get<std::vector<std::string>>();
        else if constexpr (std::is_unsigned_v<T>)
            return static_cast<T>(get<uint64_t>());
        else
            return static_cast<T>(get<int64_t>());
    }

    // Compares this field to a type tag such as the string that names a like or a message.
    bool operator==(const char *text) const;
    // Compares this field to a dynamic string such as an email or a request id.
    bool operator==(const std::string &text) const;
    // Compares this field to an account, profile, or conversation id.
    bool operator==(uint64_t number) const;
    // Compares this field to a small integer such as a reason code.
    bool operator==(int number) const;
    // Compares this field to a flag such as read or deleted.
    bool operator==(bool flag) const;

    // Cursor over one field's object keys or array elements while a serializer walks a nested frame.
    struct iterator
    {
        const JsonRef *parent = nullptr;
        size_t index = 0;
        bool object = false;

        // Advances to the next key or element inside this field.
        iterator &operator++()
        {
            ++index;
            return *this;
        }
        // Reports whether two cursors still refer to different positions in the same field.
        bool operator!=(const iterator &other) const
        {
            return index != other.index;
        }
        // Yields the JSON value at this cursor so the caller can copy a nested message or language.
        Json operator*() const;
        // Returns the object key at this cursor, such as the name of a nested frame field.
        std::string key() const;
        // Returns a mutable nested field at this cursor so a walker can edit one value.
        JsonRef value() const;
    };

    // Starts a walk over this field's object keys or array elements.
    iterator begin() const;
    // Ends a walk over this field's object keys or array elements.
    iterator end() const;

    JsonStore *doc = nullptr;
    void *slot = nullptr;
    std::shared_ptr<JsonStore> keep;

private:
    // Turns this field into a JSON object before a handler inserts children into it.
    void ensureObject();
    // Turns this field into a JSON array before a handler appends messages or ids.
    void ensureArray();
};

class Json
{
public:
    // Error raised when a frame's JSON shape does not match what the caller asked to read.
    struct exception : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };
    // Error raised when inbound text is not JSON, before a frame is handed to a handler.
    struct parse_error : exception
    {
        using exception::exception;
    };
    // Error raised when a field is present but has the wrong JSON type for the handler.
    struct type_error : exception
    {
        using exception::exception;
    };

    using object_t = std::map<std::string, Json>;
    using array_t = std::vector<Json>;

    // Empty JSON value, used as the starting point for an outbound frame.
    Json();
    // Copies a frame so the copy keeps its own document after the original is discarded.
    Json(const Json &other);
    // Takes ownership of a frame's document without copying the parsed tree.
    Json(Json &&other) noexcept;
    // Builds a JSON string value for a name, bio, email, or chat body.
    Json(const std::string &value);
    // Builds a JSON string value from a literal type tag or other fixed token.
    Json(const char *value);
    // Builds a JSON boolean for a flag such as matched, deleted, or favourite.
    Json(bool value);
    // Builds a JSON number for a small signed count such as a reason code.
    Json(int value);
    // Builds a JSON number for an unsigned count such as a list length.
    Json(unsigned value);
    // Builds a JSON number for a signed timestamp or other wide integer.
    Json(int64_t value);
    // Builds a JSON number for an account, profile, media, or conversation id.
    Json(uint64_t value);
    // Builds a JSON number for a latitude, longitude, or rating.
    Json(double value);
    // Builds a JSON object from a map of fields, which is the shape jwt-cpp uses for claims.
    Json(const object_t &object);
    // Builds a JSON array from a vector of values, such as a list of messages or languages.
    Json(const array_t &array);
    // Builds a JSON value from an inline list of fields or elements written at the call site.
    Json(std::initializer_list<Json> init);

    // Copies another JSON value into this one, keeping a separate document for the frame.
    Json &operator=(const Json &other);
    // Moves another JSON value into this one so the frame is not parsed twice.
    Json &operator=(Json &&other) noexcept;
    // Replaces this value with a display string such as a name, bio, email, or chat body.
    Json &operator=(const std::string &value);
    // Replaces this value with a literal type tag or other fixed token.
    Json &operator=(const char *value);
    // Replaces this value with a flag such as read, deleted, matched, or favourite.
    Json &operator=(bool value);
    // Replaces this value with a small signed count such as a page size or reason code.
    Json &operator=(int value);
    // Replaces this value with an unsigned count such as a list length.
    Json &operator=(unsigned value);
    // Replaces this value with a signed timestamp or other wide integer.
    Json &operator=(int64_t value);
    // Replaces this value with an account, profile, media, or conversation id.
    Json &operator=(uint64_t value);
    // Replaces this value with a latitude, longitude, or rating.
    Json &operator=(double value);
    // Replaces this value with an inline JSON array or object written at the call site.
    Json &operator=(std::initializer_list<Json> init);

    // Replaces this value with an enum such as a report reason, stored as its integer.
    template <typename T, typename std::enable_if<std::is_enum_v<T>, int>::type = 0>
    Json &operator=(T value)
    {
        return *this = static_cast<int64_t>(value);
    }

    // Replaces this value with a JSON array built from a list of ids, names, or language codes.
    template <typename T>
    Json &operator=(const std::vector<T> &values);

    // Creates an empty JSON array for languages, messages, or a batch of small frames.
    static Json array();
    // Creates an empty JSON object for a new outbound text frame.
    static Json object();
    // Creates a JSON object from an inline list of fields for an outbound frame.
    static Json object(std::initializer_list<Json> init);
    // Parses one text frame into a JSON value the unpacker can then validate.
    static Json parse(std::string_view text);
    // Parses a text frame with the extra arguments jwt-cpp and older call sites still pass.
    static Json parse(std::string_view text, const void *callback, bool allowExceptions);
    // Auth frames are a vector with a trailing NUL so the bytes can be treated as text.
    static Json parse(const std::vector<char> &bytes)
    {
        std::size_t n = bytes.size();
        if (n > 0 && bytes.back() == '\0')
            --n;
        return parse(std::string_view(bytes.data(), n));
    }
    // Parses a text frame from an iterator pair, including auth bytes that are not already a string.
    template <typename It>
    static Json parse(It first, It last);

    // Opens a named field on this frame so the handler can replace it.
    JsonRef operator[](std::string_view key);
    // Reads a named field on this frame when the handler must not change it.
    Json operator[](std::string_view key) const;
    // Opens a literal-named field such as the frame type or the nested data object.
    JsonRef operator[](const char *key);
    // Reads a literal-named field on this owning frame when the caller is only inspecting it.
    Json operator[](const char *key) const;
    // Opens one element of this frame when the value is an array of messages or ids.
    JsonRef operator[](size_t index);
    // Reads one element of this frame when the value is an array the handler must not change.
    Json operator[](size_t index) const;

    // Reports whether this frame carries a key before the unpacker requires it.
    bool contains(std::string_view key) const;
    // Reports a JSON null so a handler can tell an explicit empty from a missing frame.
    bool is_null() const;
    // Reports a discarded value so a failed parse is not treated as a profile or message.
    bool is_discarded() const;
    // Reports that this value is a JSON object, the shape of a text frame.
    bool is_object() const;
    // Reports that this value is a JSON array of messages, languages, or batched frames.
    bool is_array() const;
    // Reports that this value is a string such as an email, bio, or request id.
    bool is_string() const;
    // Reports that this value is a boolean flag from a client frame.
    bool is_boolean() const;
    // Reports that this value is a JSON number, including ratings and coordinates.
    bool is_number() const;
    // Reports that this value is an integer id or timestamp.
    bool is_number_integer() const;
    // Reports that this value is a non-negative integer id.
    bool is_number_unsigned() const;
    // Counts the keys or elements in this frame.
    size_t size() const;
    // Reports that this frame has no keys or elements, so the handler can skip it.
    bool empty() const;
    // Serializes this frame to the UTF-8 text sent to a browser, with optional pretty indentation in debug.
    std::string dump(int indent = -1) const;
    // Appends one element to a JSON array of messages, languages, or batched frames.
    void push_back(const Json &item);

    // Reads this frame as the C++ type the handler asked for.
    template <typename T>
    T get() const;

    // Reads a key from this frame, or returns the fallback when an older client omitted it.
    template <typename T>
    T value(std::string_view key, T fallback) const;

    // Copies this object into a map of fields for jwt-cpp claim checks.
    object_t itemsObject() const;
    // Copies this array into a vector of JSON values for a handler that walks messages or languages.
    array_t itemsArray() const;

    // Converts this frame to the C++ type a handler assigns, so a name or an age does not bind the wrong overload.
    template <typename T, typename std::enable_if<
        std::is_same_v<T, std::string> || std::is_same_v<T, bool> || std::is_same_v<T, double> || std::is_same_v<T, float> ||
        std::is_same_v<T, std::vector<uint8_t>> || std::is_same_v<T, std::vector<std::string>> || std::is_enum_v<T> ||
        (std::is_integral_v<T> && !std::is_same_v<T, char> && !std::is_same_v<T, bool>), int>::type = 0>
    operator T() const
    {
        if constexpr (std::is_enum_v<T>)
            return static_cast<T>(get<int64_t>());
        else if constexpr (std::is_same_v<T, std::string>)
            return get<std::string>();
        else if constexpr (std::is_same_v<T, bool>)
            return get<bool>();
        else if constexpr (std::is_same_v<T, float>)
            return static_cast<float>(get<double>());
        else if constexpr (std::is_same_v<T, double>)
            return get<double>();
        else if constexpr (std::is_same_v<T, std::vector<uint8_t>>)
            return get<std::vector<uint8_t>>();
        else if constexpr (std::is_same_v<T, std::vector<std::string>>)
            return get<std::vector<std::string>>();
        else if constexpr (std::is_unsigned_v<T>)
            return static_cast<T>(get<uint64_t>());
        else
            return static_cast<T>(get<int64_t>());
    }

    // Compares this frame to a type tag such as the string that names a like or a message.
    bool operator==(const char *text) const;
    // Compares this frame to a dynamic string such as an email or a request id.
    bool operator==(const std::string &text) const;
    // Compares this frame to an account, profile, or conversation id.
    bool operator==(uint64_t number) const;
    // Compares this frame to a small integer such as a reason code.
    bool operator==(int number) const;
    // Compares this frame to a flag such as read or deleted.
    bool operator==(bool flag) const;

    // Cursor over this frame's object keys or array elements.
    struct iterator
    {
        const Json *parent = nullptr;
        size_t index = 0;
        bool object = false;

        // Advances to the next key or element in this frame.
        iterator &operator++()
        {
            ++index;
            return *this;
        }
        // Reports whether two cursors still refer to different positions in this frame.
        bool operator!=(const iterator &other) const
        {
            return index != other.index;
        }
        // Yields the JSON value at this cursor so a walker can copy a nested message or language.
        Json operator*() const;
        // Returns the object key at this cursor inside the frame.
        std::string key() const;
        // Returns a mutable field at this cursor so a walker can edit one nested value.
        Json value() const;
    };

    // Starts a walk over this frame's keys or elements.
    iterator begin() const;
    // Ends a walk over this frame's keys or elements.
    iterator end() const;

    friend class JsonRef;
    // Writes this frame to a stream for logs and debug dumps.
    friend std::ostream &operator<<(std::ostream &os, const Json &value);
    // Reads JSON text from a stream into a frame value.
    friend std::istream &operator>>(std::istream &is, Json &value);

private:
    std::shared_ptr<JsonStore> doc;
    void *node = nullptr;
    bool discarded = false;

    // Binds a JSON value to one node inside an existing document without copying the whole frame.
    explicit Json(std::shared_ptr<JsonStore> store, void *node, bool discardedFlag);
    // Wraps a stored RapidJSON node as a Json value after a document has been parsed.
    static Json fromStored(const void *rapidValue);
    // Turns this value into an object before a handler inserts frame fields.
    void ensureObject();
    // Turns this value into an array before a handler appends messages or ids.
    void ensureArray();
};


// Builds a JSON array from a C++ list of ids, names, or language codes and stores it in this value.
template <typename T>
inline Json &Json::operator=(const std::vector<T> &values)
{
    Json array = Json::array();
    for (const T &item : values)
    {
        if constexpr (std::is_same_v<T, Json>)
            array.push_back(item);
        else if constexpr (std::is_same_v<T, std::string>)
            array.push_back(Json(item));
        else if constexpr (std::is_same_v<T, bool>)
            array.push_back(Json(item));
        else if constexpr (std::is_floating_point_v<T>)
            array.push_back(Json(static_cast<double>(item)));
        else if constexpr (std::is_unsigned_v<T>)
            array.push_back(Json(static_cast<uint64_t>(item)));
        else
            array.push_back(Json(static_cast<int64_t>(item)));
    }
    *this = std::move(array);
    return *this;
}

// Parses iterator-delimited frame text by handing the copied bytes to the string parser.
template <typename It>
inline Json Json::parse(It first, It last)
{
    std::string text(first, last);
    return parse(std::string_view(text), nullptr, true);
}

// Returns a nested field from this frame, or the fallback when the key was left out.
template <typename T>
inline T Json::value(std::string_view key, T fallback) const
{
    if (!contains(key))
        return fallback;
    return (*this)[key].template get<T>();
}

// Reads a field by first copying it into an owning JSON value.
template <typename T>
inline T JsonRef::get() const
{
    return static_cast<Json>(*this).template get<T>();
}

// Returns a nested field from a field reference, or the fallback when the key was left out.
template <typename T>
inline T JsonRef::value(std::string_view key, T fallback) const
{
    if (!contains(key))
        return fallback;
    return (*this)[key].template get<T>();
}

// Assigns a JSON array built from a C++ list into the field a handler is filling.
template <typename T>
inline JsonRef &JsonRef::operator=(const std::vector<T> &values)
{
    Json array = Json::array();
    array = values;
    return *this = array;
}

// Prints a field by first copying it into an owning JSON value.
inline std::ostream &operator<<(std::ostream &os, const JsonRef &value)
{
    return os << static_cast<Json>(value);
}

// Parses text into a JSON value and records the error string, for config and import paths that must not throw.
bool parseJson(std::string_view text, Json &out, std::string &error);

// Builds an object from an inline field list by forwarding to the JSON value constructor.
inline Json Json::object(std::initializer_list<Json> init)
{
    return Json(init);
}
