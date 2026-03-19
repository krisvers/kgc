#pragma once

#include <iterator>
#include <unordered_map>
#include <cstdint>
#include <random>
#include <iostream>
#include <iomanip>
#include <limits>
#include <type_traits>

namespace kgc {

enum class Value : uint32_t {
    High = 1,
    Low = 2,
    Unevaluable = std::numeric_limits<uint32_t>::max() - 1,
    Undefined = std::numeric_limits<uint32_t>::max(),
};

inline Value operator&&(Value a, Value b) {
    if (static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(Value::Unevaluable) || static_cast<std::underlying_type_t<Value>>(b) == static_cast<std::underlying_type_t<Value>>(Value::Unevaluable)) {
        return Value::Unevaluable;
    } else if (static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(Value::Undefined) || static_cast<std::underlying_type_t<Value>>(b) == static_cast<std::underlying_type_t<Value>>(Value::Undefined)) {
        return Value::Undefined;
    }

    return ((static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(Value::High)) && (static_cast<std::underlying_type_t<Value>>(b) == static_cast<std::underlying_type_t<Value>>(Value::High))) ? Value::High : Value::Low;
}

inline Value operator||(Value a, Value b) {
    if (static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(Value::Unevaluable) || static_cast<std::underlying_type_t<Value>>(b) == static_cast<std::underlying_type_t<Value>>(Value::Unevaluable)) {
        return Value::Unevaluable;
    } else if (static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(Value::Undefined) || static_cast<std::underlying_type_t<Value>>(b) == static_cast<std::underlying_type_t<Value>>(Value::Undefined)) {
        return Value::Undefined;
    }

    return ((static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(Value::High)) || (static_cast<std::underlying_type_t<Value>>(b) == static_cast<std::underlying_type_t<Value>>(Value::High))) ? Value::High : Value::Low;
}

inline Value operator^(Value a, Value b) {
    if (static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(Value::Unevaluable) || static_cast<std::underlying_type_t<Value>>(b) == static_cast<std::underlying_type_t<Value>>(Value::Unevaluable)) {
        return Value::Unevaluable;
    } else if (static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(Value::Undefined) || static_cast<std::underlying_type_t<Value>>(b) == static_cast<std::underlying_type_t<Value>>(Value::Undefined)) {
        return Value::Undefined;
    }

    return (static_cast<std::underlying_type_t<Value>>(a) != static_cast<std::underlying_type_t<Value>>(b) && ((static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(Value::High)) || (static_cast<std::underlying_type_t<Value>>(b) == static_cast<std::underlying_type_t<Value>>(Value::High)))) ? Value::High : Value::Low;
}

inline Value operator==(Value a, Value b) {
    if (static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(Value::Unevaluable) || static_cast<std::underlying_type_t<Value>>(b) == static_cast<std::underlying_type_t<Value>>(Value::Unevaluable)) {
        return Value::Unevaluable;
    } else if (static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(Value::Undefined) || static_cast<std::underlying_type_t<Value>>(b) == static_cast<std::underlying_type_t<Value>>(Value::Undefined)) {
        return Value::Undefined;
    }

    return (static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(b)) ? Value::High : Value::Low;
}

inline bool operator<=(Value a, Value b) {
    if (static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(Value::Unevaluable) || static_cast<std::underlying_type_t<Value>>(b) == static_cast<std::underlying_type_t<Value>>(Value::Unevaluable)) {
        return false;
    } else if (static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(Value::Undefined) || static_cast<std::underlying_type_t<Value>>(b) == static_cast<std::underlying_type_t<Value>>(Value::Undefined)) {
        return false;
    }

    return static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(b);
}

inline bool operator>=(Value a, Value b) {
    return static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(b);
}

inline Value operator!(Value value) {
    switch (value) {
    case Value::Unevaluable:
    case Value::Undefined:
        return value;
    case Value::High:
        return Value::Low;
    case Value::Low:
        return Value::High;
    }
}

inline Value operator!=(Value const& a, Value const& b) {
    if (static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(Value::Unevaluable) || static_cast<std::underlying_type_t<Value>>(b) == static_cast<std::underlying_type_t<Value>>(Value::Unevaluable)) {
        return Value::Unevaluable;
    } else if (static_cast<std::underlying_type_t<Value>>(a) == static_cast<std::underlying_type_t<Value>>(Value::Undefined) || static_cast<std::underlying_type_t<Value>>(b) == static_cast<std::underlying_type_t<Value>>(Value::Undefined)) {
        return Value::Undefined;
    }

    return (static_cast<std::underlying_type_t<Value>>(a) != static_cast<std::underlying_type_t<Value>>(b)) ? Value::High : Value::Low;}

inline std::ostream& operator<<(std::ostream& os, Value const& v) {
    switch (v) {
    case Value::High:
        os << "Value<High>";
        break;
    case Value::Low:
        os << "Value<Low>";
        break;
    case Value::Unevaluable:
        os << "Value<Unevaluable>";
        break;
    case Value::Undefined:
        os << "Value<Undefined>";
        break;
    default:
        os << "Value<nil>";
        break;
    }

    return os;
}

template<typename T>
class Singleton {
public:
    static T& get() {
        static T instance;
        return instance;
    }

    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;

protected:
    Singleton() = default;
    ~Singleton() = default;
};

struct FamilyID {
public:
    using FamilyID_T = uint64_t;

    /* enum values */
    static constexpr inline FamilyID_T Invalid = 0;

    /* implementation */
    FamilyID_T _value = 0;
    inline FamilyID(FamilyID_T value) : _value(value) {}

    inline void operator=(FamilyID_T value) {
        _value = value;
    }

    inline operator FamilyID_T() const {
        return _value;
    }
};

struct TypeID {
public:
    using TypeID_T = uint64_t;

    /* implementation */
    TypeID_T _value = 0;
    TypeID(TypeID_T value) : _value(value) {}

    void operator=(TypeID_T value) {
        _value = value;
    }

    operator TypeID_T() const {
        return _value;
    }
};

struct RegisteredFamily {
    std::string name;
};

class Registry : public Singleton<Registry> {
private:
    std::mt19937_64 _mt;
    std::unordered_map<FamilyID::FamilyID_T, RegisteredFamily> _families;

    friend class RegisteredFamilyIterator;

    using RegisteredFamilyIteratorIterator = std::unordered_map<FamilyID::FamilyID_T, RegisteredFamily>::const_iterator;

public:
    class RegisteredFamilyIterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = RegisteredFamily;
        using difference_type = std::ptrdiff_t;
        using pointer = RegisteredFamily*;
        using reference = RegisteredFamily&;

        RegisteredFamilyIteratorIterator _it;
        RegisteredFamily _family;

        RegisteredFamilyIterator(RegisteredFamilyIteratorIterator const& it) : _it(it) {
            if (it != Registry::get()._families.end()) {
                _family = _it->second;
            }
        }

        RegisteredFamily const& operator*() const {
            if (_it == Registry::get()._families.end()) {
                throw std::runtime_error("Invalid iterator for RegisteredFamily");
            }

            return _family;
        }

        RegisteredFamily const* operator->() const {
            if (_it == Registry::get()._families.end()) {
                throw std::runtime_error("Invalid iterator for RegisteredFamily");
            }

            return &_family;
        }

        RegisteredFamilyIterator& operator++() {
            throw std::runtime_error("Cannot iterate through RegisteredFamilies");
        }

        RegisteredFamilyIterator operator++(int offset) {
            throw std::runtime_error("Cannot iterate through RegisteredFamilies");
        }

        bool equal(RegisteredFamilyIterator const& other) const {
            return _it == other._it;
        }
    };

    FamilyID::FamilyID_T registerFamilyID(std::string const& name) {
        FamilyID::FamilyID_T fid;
        do {
            fid = _mt();
        } while (fid == 0 || _families.find(fid) != _families.end());

        _families[fid] = {
            .name = name
        };

        return fid;
    }

    void unregister(FamilyID::FamilyID_T fid) {
        auto it = _families.find(fid);
        if (it == _families.end()) {
            return;
        }

        _families.erase(it);
    }

    RegisteredFamilyIterator invalidFamily() const {
        return RegisteredFamilyIterator(_families.end());
    }

    RegisteredFamilyIterator getFamily(FamilyID::FamilyID_T fid) {
        auto it = _families.find(fid);
        if (it == _families.end()) {
            return invalidFamily();
        }

        return RegisteredFamilyIterator(it);
    }
};

inline bool operator==(Registry::RegisteredFamilyIterator const& a, Registry::RegisteredFamilyIterator const& b) {
    return a.equal(b);
}

inline bool operator!=(Registry::RegisteredFamilyIterator const& a, Registry::RegisteredFamilyIterator const& b) {
    return !a.equal(b);
}

struct ID {
public:
    FamilyID family;
    TypeID type;
    uint64_t instance;

    ID(FamilyID f, TypeID t, uint64_t i) : family(f), type(t), instance(i) {}
    ID(ID const& id) : family(id.family), type(id.type), instance(id.instance) {}

    void operator=(ID const& other) {
        family = other.family;
        type = other.type;
        instance = other.instance;
    }

    bool operator==(ID const& other) const {
        return family == other.family && type == other.type && instance == other.instance;
    }

    bool operator<=(ID const& other) const {
        return family == other.family && type == other.type;
    }

    bool operator<(ID const& other) const {
        return family == other.family;
    }

    friend std::ostream& operator<<(std::ostream& os, ID const& id) {
        auto it = Registry::get().getFamily(id.family);
        os << "ID<";
        if (it != Registry::get().invalidFamily()) {
            os << '\'' << it._family.name << "':";
        } else {
            os << '(' << "nil" << "):";
        }

        os << std::hex << static_cast<uint64_t>(id.family) << ", " << static_cast<uint64_t>(id.type) << ", " << static_cast<uint64_t>(id.instance) << std::dec << '>';
        return os;
    }

    friend std::istream& operator>>(std::istream& is, ID& id) {
        uint64_t f, t, i;
        is >> f >> t >> i;

        id.family = f;
        id.type = t;
        id.instance = i;
        return is;
    }
};

template<typename T>
class CustomFamilyID : public Singleton<T> {
public:
    FamilyID::FamilyID_T _value;

    CustomFamilyID(std::string const& name) {
        _value = Registry::get().registerFamilyID(name);
    }

    CustomFamilyID() {
        _value = Registry::get().registerFamilyID("Unknown");
    }

    operator FamilyID::FamilyID_T() const {
        return _value;
    }

    operator FamilyID() const {
        return _value;
    }
};

namespace base {

class ITerminal;

class INode {
public:
    virtual ID getID() const = 0;

    virtual uint32_t getPossibleTerminalCount() const = 0;
    virtual uint32_t getCurrentTerminalCount() const = 0;

    virtual ITerminal* getTerminal(uint32_t terminalID) = 0;
};

class ITerminal {
public:
    virtual Value evaluate(uint64_t height) = 0;
    virtual Value shallowEvaluate() const = 0;

    virtual uint32_t getTerminalID() const = 0;
    virtual INode* getNode() const = 0;
};

class IParentNode : public virtual INode {
public:
    virtual bool isWritable() const = 0;

    virtual ITerminal* getChildWithID(ID id) const = 0;
    virtual bool replaceChildWithID(ID id, ITerminal* newTerminal) = 0;
};

class ILinearParentNode : public virtual IParentNode {
public:
    virtual bool isGrowable() const = 0;
    virtual bool isContiguous() const = 0;

    virtual uint32_t getPossibleChildCount() const = 0;
    virtual uint32_t getValidChildCount() const = 0;

    virtual ITerminal* getChildAtIndex(uint32_t index) const = 0;
    virtual bool setChildAtIndex(uint32_t index, ITerminal* terminal) = 0;

    virtual uint32_t getIndexOfChildWithFamilyID(FamilyID fid, uint32_t afterIndex = std::numeric_limits<uint32_t>::max()) const = 0;
};

template<uint32_t N>
class AbstractFixedContiguousLinearParentNode : public virtual ILinearParentNode {
protected:
    std::array<ITerminal*, N> _children;

public:
    bool isGrowable() const override {
        return false;
    }

    bool isContiguous() const override {
        return true;
    }

    uint32_t getPossibleChildCount() const override {
        return N;
    }

    uint32_t getValidChildCount() const override {
        uint32_t n = 0;
        for (uint32_t i = 0; i < N; ++i) {
            if (_children[i] != nullptr) {
                ++n;
            }
        }

        return n;
    }

    ITerminal* getChildWithID(ID id) const override {
        for (uint32_t i = 0; i < N; ++i) {
            if (_children[i] == nullptr) {
                continue;
            }

            if (_children[i]->getNode()->getID() == id) {
                return _children[i];
            }
        }

        return nullptr;
    }

    ITerminal* getChildAtIndex(uint32_t index) const override {
        if (index < N) {
            return _children[index];
        }

        return nullptr;
    }

    uint32_t getIndexOfChildWithFamilyID(FamilyID fid, uint32_t afterIndex = std::numeric_limits<uint32_t>::max()) const override {
        for (uint32_t i = 0; i < N; ++i) {
            if (_children[i] == nullptr) {
                continue;
            }

            if (_children[i]->getNode()->getID().family == fid) {
                return i;
            }
        }

        return std::numeric_limits<uint32_t>::max();
    }
};

template<uint32_t N>
class AbstractWritableFixedContiguousLinearParentNode : public virtual AbstractFixedContiguousLinearParentNode<N> {
public:
    bool isWritable() const override {
        return true;
    }

    bool replaceChildWithID(ID id, ITerminal* newTerminal) override {
        for (uint32_t i = 0; i < N; ++i) {
            if (this->_children[i] == nullptr) {
                continue;
            }

            if (this->_children[i]->getNode()->getID() == id) {
                this->_children[i] = newTerminal;
                return true;
            }
        }

        return false;
    }

    bool setChildAtIndex(uint32_t index, ITerminal* newTerminal) override {
        if (index < N) {
            this->_children[index] = newTerminal;
            return true;
        }

        return false;
    }
};

template<uint32_t N>
class AbstractReadOnlyFixedContiguousLinearParentNode : public virtual AbstractFixedContiguousLinearParentNode<N> {
public:
    bool isWritable() const override {
        return false;
    }

    bool replaceChildWithID(ID id, INode* newNode) const override {
        return false;
    }

    bool setChildAtIndex(uint32_t index, INode* newNode) const override {
        return false;
    }
};

}

namespace builtin {

class Family : public CustomFamilyID<Family> {
public:
    Family() : CustomFamilyID<Family>("BuiltIn") {}

    struct Misc {
        static inline constexpr TypeID::TypeID_T Uniform = 1;
        static inline constexpr TypeID::TypeID_T Buffer = 2;
    };

    struct Gate {
        static inline constexpr TypeID::TypeID_T Nand = 3;
        static inline constexpr TypeID::TypeID_T Nor = 4;
        static inline constexpr TypeID::TypeID_T And = 5;
        static inline constexpr TypeID::TypeID_T Or = 6;
        static inline constexpr TypeID::TypeID_T Not = 7;
        static inline constexpr TypeID::TypeID_T Xor = 8;
    };

    static inline constexpr TypeID::TypeID_T Circuit = 9;
};

namespace misc {

class UniformNode : public base::INode {
private:
    static inline uint64_t nextInstance = 0;

    class Terminal : public base::ITerminal {
    public:
        UniformNode& _node;

        Terminal(UniformNode& node) : _node(node) {}

        Value evaluate(uint64_t height) override {
            return shallowEvaluate();
        }

        Value shallowEvaluate() const override {
            return _node._value;
        }

        uint32_t getTerminalID() const override {
            return 0;
        }

        UniformNode* getNode() const override {
            return &_node;
        }
    };

    ID _id;
    Value _value;
    Terminal _terminal;

public:
    UniformNode() : _id(Family::get(), Family::Misc::Uniform, nextInstance++), _value(Value::Undefined), _terminal(*this) {}

    UniformNode(Value value) : _id(Family::get(), Family::Misc::Uniform, nextInstance++), _value(value), _terminal(*this) {}

    ID getID() const override {
        return _id;
    }

    uint32_t getPossibleTerminalCount() const override {
        return 1;
    }

    uint32_t getCurrentTerminalCount() const override {
        return 1;
    }

    Terminal* getTerminal(uint32_t tid) override {
        if (tid != 0) {
            return nullptr;
        }

        return &_terminal;
    }

    void setValue(Value value) {
        _value = value;
    }

    Value getValue() {
        return _value;
    }
};

class BufferNode : public base::AbstractWritableFixedContiguousLinearParentNode<1> {
private:
    static inline uint64_t nextInstance = 0;

    class Terminal : public base::ITerminal {
    public:
        BufferNode& _node;

        Terminal(BufferNode& node) : _node(node) {}

        Value evaluate(uint64_t height) override {
            base::ITerminal* depends = _node.getChildAtIndex(0);
            if (depends == nullptr) {
                return Value::Undefined;
            }

            if (height == 0) {
                return depends->shallowEvaluate();
            }

            return depends->evaluate(height - 1);
        }

        Value shallowEvaluate() const override {
            return Value::Unevaluable;
        }

        uint32_t getTerminalID() const override {
            return 0;
        }

        BufferNode* getNode() const override {
    return &_node;
        }
    };

    ID _id;
    Terminal _terminal;

public:
    BufferNode() : _id(Family::get(), Family::Misc::Buffer, nextInstance++), _terminal(*this) {

    }

    ID getID() const override {
        return _id;
    }

    uint32_t getPossibleTerminalCount() const override {
        return 1;
    }

    uint32_t getCurrentTerminalCount() const override {
        return 1;
    }

    Terminal* getTerminal(uint32_t tid) override {
        if (tid != 0) {
            return nullptr;
        }

        return &_terminal;
    }
};

}

namespace gate {

template<uint32_t N, typename T, typename Evaluator>
class AbstractGate : public base::AbstractWritableFixedContiguousLinearParentNode<N> {
private:
    static inline uint64_t nextInstance = 0;

    class Terminal : public base::ITerminal {
    public:
        AbstractGate& _node;

        Terminal(AbstractGate& node) : _node(node) {}

        Value evaluate(uint64_t height) override {
            std::array<base::ITerminal*, N> depends;
            for (uint32_t i = 0; i < N; ++i) {
                depends[i] = _node.getChildAtIndex(i);
            }

            return Evaluator::evaluate(reinterpret_cast<T&>(_node), depends, height);
        }

        Value shallowEvaluate() const override {
            return Evaluator::shallowEvaluate(reinterpret_cast<T&>(_node));
        }

        uint32_t getTerminalID() const override {
            return 1;
        }

        AbstractGate* getNode() const override {
            return &_node;
        }
    };

    ID _id;
    Terminal _terminal;

public:
    AbstractGate(FamilyID::FamilyID_T family, TypeID::TypeID_T type) : _id(family, type, nextInstance++), _terminal(*this) {}

    AbstractGate(FamilyID::FamilyID_T family, TypeID::TypeID_T type, std::array<base::ITerminal*, N> children) : _id(family, type, nextInstance++), _terminal(*this) {
        for (uint32_t i = 0; i < N; ++i) {
            base::AbstractWritableFixedContiguousLinearParentNode<N>::setChildAtIndex(i, children[i]);
        }
    }

    ID getID() const override {
        return _id;
    }

    uint32_t getPossibleTerminalCount() const override {
        return 1;
    }

    uint32_t getCurrentTerminalCount() const override {
        return 1;
    }

    Terminal* getTerminal(uint32_t tid) override {
        if (tid != 0) {
            return nullptr;
        }

        return &_terminal;
    }
};

#define KGC_DEFINE_GATE_BINARY(family_, type_, name_, eval_, shEval_) \
class name_##Evaluator; \
class name_ : public kgc::builtin::gate::AbstractGate<2, name_, name_##Evaluator> { \
public: \
    name_() : kgc::builtin::gate::AbstractGate<2, name_, name_##Evaluator>(family_, type_) {} \
    name_(std::array<kgc::base::ITerminal*, 2> children) : kgc::builtin::gate::AbstractGate<2, name_, name_##Evaluator>(family_, type_, children) {} \
}; \
class name_##Evaluator { \
public: \
    static kgc::Value evaluate(name_& self, std::array<kgc::base::ITerminal*, 2> depends, uint64_t height) { \
        if (depends[0] == nullptr || depends[1] == nullptr) { return kgc::Value::Undefined; } \
        kgc::Value l, r; \
        if (height == 0) { \
            l = depends[0]->shallowEvaluate(); \
            r = depends[1]->shallowEvaluate(); \
        } else { \
            l = depends[0]->evaluate(height - 1); \
            r = depends[1]->evaluate(height - 1); \
        } \
        eval_; \
    } \
    static kgc::Value shallowEvaluate(name_& self) { \
        shEval_; \
    } \
};

KGC_DEFINE_GATE_BINARY(
    Family::get(),
    Family::Gate::Nand,
    Nand,
    /* evaluate */ {
        return !(l && r);
    },
    /* shallowEvaluate */ {
        return Value::Unevaluable;
    }
)

KGC_DEFINE_GATE_BINARY(
    Family::get(),
    Family::Gate::Nor,
    Nor,
    /* evaluate */ {
        return !(l || r);
    },
    /* shallowEvaluate */ {
        return Value::Unevaluable;
    }
)

KGC_DEFINE_GATE_BINARY(
    Family::get(),
    Family::Gate::And,
    And,
    /* evaluate */ {
        return l && r;
    },
    /* shallowEvaluate */ {
        return Value::Unevaluable;
    }
)

KGC_DEFINE_GATE_BINARY(
    Family::get(),
    Family::Gate::Or,
    Or,
    /* evaluate */ {
        return l && r;
    },
    /* shallowEvaluate */ {
        return Value::Unevaluable;
    }
)

}

}

}
