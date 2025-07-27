#ifndef __PBNJSON_SERIALIZATION_CORE_D7380DB3C5D742B6A640DFF5474FC963
#define __PBNJSON_SERIALIZATION_CORE_D7380DB3C5D742B6A640DFF5474FC963


#ifdef WIN32
#undef max
#undef min
#else
#pragma GCC system_header
#endif

#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/tuple/to_seq.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/static_assert.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <limits>
#include <vector>
#include <list>
#include <set>
#include <memory>
#include <sstream>
#include <mutex>

namespace pbnjson {

//---------------------- Explanation -----------------------------------------------------------
//
// Lets analyze what is goign on here. The main idea - reflection in compile type.
// The code is constructed from several simple techniques.
//
// The first one - convert list of memebers from struct into nested std::pair.
// Assume there is the following struct:
//
// struct TestStruct
// {
//      int intFiled;
//      std::string strFiled;
// };
//
// In this case nested std::pair will be:
// typedef std::pair< int, std::pair< std::string, recursion_stop_type > > refl_type;
// How this pair is contructed will be explained later.
// The question: what does it mean recursion_stop_type. It is struct on which template loop will stop.
// Template loop? This is a loop that is made by compiler in compile time. Example:
//
// template<typename CurentPair> TemplateLoop()
// {
//     TemplateLoop<CurentPair::second_type>();
// }
//
// // stop loop condition - specialization for TemplateLoop with type recursion_stop_type.
// template<> TemplateLoop<recursion_stop_type> () {}
//
// Now it could be seen why nested pair is useful. The loop will continue until std::pair<>::second_type
// is not recursion_stop_type.
//
// Moving on. The first type of std::pair is just type of member. There is no much information.
// More information is needed - name of the member, accessor. For the reason the follwong struct is
// defined for every memeber in a struct(use the same TestStruct):
//
// struct meta_data_intFiled
// {
//     typdef int VarType;
//     static const char* name() {return "intFiled";}
//     typedef int metaclass::*data_accessor;
//     static data_accessor get() { return &metaclass::intFiled; }
// };
//
// struct meta_data_strFiled
// {
//     typdef std::string VarType;
//     static const char* name() {return "strFiled";}
//     typedef std::string metaclass::*data_accessor;
//     static data_accessor get() { return &metaclass::intFiled; }
// };
//
// metaclass - this is a typedef for TestStruct(typedef TestStruct metaclass).
//
// These structs will be used in nested pair instead of pure member`s type:
// typedef std::pair< meta_data_intFiled, std::pair< meta_data_strFiled, recursion_stop_type > > refl_type;
// Now there is enough information about struct's members.
//
// The second task - construct the nested pair and declare auxiliary structures from definition
// The task was done with help of preprocessor and boost preprocessor library.
// The main work is done by BOOST_PP_SEQ_FOR_EACH.
// To be able to use BOOST_PP_SEQ_FOR_EACH, members list of struct should be declared as sequence:
//
// struct TestStruct {
//    PBNJSON_SERIALIZE( TestStruct,
//         (int, intFiled)
//         (std::string, strFiled)
//    )
// };
//
// At this point definition of PBNJSON_SERIALIZE could be analyzed.
// First step it does is declaration of metaclass(is used in auxiliary struct for nested pair)
// Next step - declaration of a member - pure "type name;"
// Next step - declaration of meta_data struct for each member - PBNJSON_REFLECT_EACH
// Next step - declaration of nested pair. This step contains three substeps. The first one -
// create the part of nested pair until recursion_stop_type. The second step - add recursion_stop_type.
// The third step - construct the end of nested pair. Example:
// First step:
//   std::pair< meta_data_intFiled,
//   std::pair< meta_data_intFiled, std::pair< meta_data_strFiled,
// Second setp:
//   std::pair< meta_data_intFiled, std::pair< meta_data_strFiled, recursion_stop_type
// Third step:
//  std::pair< meta_data_intFiled, std::pair< meta_data_strFiled, recursion_stop_type >
//  std::pair< meta_data_intFiled, std::pair< meta_data_strFiled, recursion_stop_type > >
//
// That is how the neseted pair is constructed. There are lot of definitions of __PBNJSON_STRIP_TYPE,
// __PBNJSON_GENERATE_PAIR_BEGIN,... This is just to parse member defintion (std::string, strFiled),
// and extract neccessary information. There is a lot of of "magic" with preprocessor. But you could
// see the result of it work by adding -E -P options to gcc. It will produce produce a file with code
// after preprocessor. There is a sample of such file:
//
// struct TestStruct
// {
//    typedef TestStruct metaclass;
//    int intFiled;
//    struct meta_data_intFiled
//    {
//        typedef int VarType;
//        static const char * name() {return "intFiled"; }
//        typedef int metaclass::*data_accessor;
//        static data_accessor get() { return &metaclass:: intFiled;}
//    };
//    std::string strFiled;
//    struct meta_data_strFiled
//    {
//        typedef std::string VarType;
//        static const char * name() {return "strFiled"; }
//        typedef std::string metaclass::*data_accessor;
//        static data_accessor get() { return &metaclass:: strFiled; }
//    };
//    typedef std::pair< meta_data_intFiled, std::pair< meta_data_strFiled, pbnjson::recursion_stop_type > > refl_type;
//};
//
// Struct is ready. Now it can be used.
//
// There is a struct MetaDataLoopImpl. It has method "Loop". The method produce template loop through nested pair.
// The structure has partial specialization for recursion_stop_type to stop recursion.
// MetaDataLoopImpl calls operator() of struct VisitorType for every member of a target type.
//
// Serialization:
// Lets analyze SerializeVisitor. It has template operator(), that is called for every member. But the main work
// is done by SerializeHelper struct. The struct SerializeHelper has method Serialize.
// The default method implementation expects structures with meta information.
// There are set of specializations for basic types and std::vector. Attempts to serialize members of not-supported type
// will be rejected with static assert.
//
//
// Deserialization:
// There is a struct DeserializeHelper which do the main work. It has several specializations for basic C++ types, std::vector
// and strucutres with meta-info.


struct recursion_stop_type {};

//---------------------- MetaData declaration --------------------------------------------------

#define __PBNJSON_WHITESPACES " \t\r\n"

#define __PBNJSON_STRIP_TYPE(x, y) y
#define __PBNJSON_GENERATE_PAIR_BEGIN(r, data, x) data BOOST_PP_CAT(meta_data_, __PBNJSON_STRIP_TYPE x),
#define __PBNJSON_GENERATE_PAIR_END(r, data, x) data

#define __PBNJSON_ATTR_TO_SEQ_O_END
#define __PBNJSON_ATTR_TO_SEQ_1_END
#define __PBNJSON_ATTR_TO_SEQ_O(x, y) \
	((x,y)) __PBNJSON_ATTR_TO_SEQ_1
#define __PBNJSON_ATTR_TO_SEQ_1(x, y) \
	((x,y)) __PBNJSON_ATTR_TO_SEQ_O

#define __PBNJSON_REFLECT_EACH_DEFINE_1(type, Identifier) \
	type Identifier;

#define __PBNJSON_REFLECT_EACH_DEFINE(r, data, x) \
	__PBNJSON_REFLECT_EACH_DEFINE_1 x

#define __PBNJSON_REFLECT_EACH_1(type, Identifier)                                \
	struct BOOST_PP_CAT(meta_data_, Identifier)   {                             \
		typedef type VarType;                                                   \
		static const char * name() {return BOOST_PP_STRINGIZE(Identifier); }    \
		static const type& get(const metaclass& t) { return t.Identifier;  }    \
		static type& set(metaclass& t) { return t.Identifier;  }          \
	};
#define __PBNJSON_REFLECT_EACH(r, data, x) \
	__PBNJSON_REFLECT_EACH_1 x

#define __PBNJSON_SERIALIZE_INTERNAL(ClassType, ATTRIBUTES)                                       \
	typedef ClassType metaclass;                                                \
	BOOST_PP_SEQ_FOR_EACH(__PBNJSON_REFLECT_EACH, _, ATTRIBUTES);                \
	typedef BOOST_PP_SEQ_FOR_EACH(__PBNJSON_GENERATE_PAIR_BEGIN, std::pair<, ATTRIBUTES) \
			pbnjson::recursion_stop_type                                                   \
			BOOST_PP_SEQ_FOR_EACH(__PBNJSON_GENERATE_PAIR_END, >, ATTRIBUTES) refl_type; \

#define PBNJSON_SERIALIZE(ClassType, ATTRIBUTES) \
	__PBNJSON_SERIALIZE_INTERNAL(ClassType, BOOST_PP_CAT(__PBNJSON_ATTR_TO_SEQ_O ATTRIBUTES, _END))

#define __PBNJSON_SERIALIZE_INTERNAL_DEFINE(ClassType, ATTRIBUTES)  \
	BOOST_PP_SEQ_FOR_EACH(__PBNJSON_REFLECT_EACH_DEFINE, _, ATTRIBUTES) \
	__PBNJSON_SERIALIZE_INTERNAL(ClassType, ATTRIBUTES)

#define PBNJSON_SERIALIZE_DEFINE(ClassType, ATTRIBUTES) \
	__PBNJSON_SERIALIZE_INTERNAL_DEFINE(ClassType, BOOST_PP_CAT(__PBNJSON_ATTR_TO_SEQ_O ATTRIBUTES, _END))


//----------------------------- SFINAE for type metaclass ----------------------------------
// If typename "Type" has nested typename "metaclass",
// then is_metaclass<Type>::value will be true, otherwise false

struct sfinae_empty;
template <typename type> struct dummy { typedef sfinae_empty dummy_type; };

template <typename type, typename sfinae_test = sfinae_empty>
struct is_metaclass {
	//typedef boost::false_type value;
	enum {value = false};
};

template <typename type>
struct is_metaclass<type, typename dummy<typename type::metaclass>::dummy_type > {
	//typedef boost::true_type value;
	enum {value = true};
};

template<typename Type, bool x> struct UNSUPPORTED_TYPE_FOR_SERIALIZATION;
template<typename Type> struct UNSUPPORTED_TYPE_FOR_SERIALIZATION<Type, true> { enum {value = 1};};
#define ASSERT_ON_UNSUPPORTED_TYPE_FOR_SERIALIZATION(Type) \
	enum {check = sizeof(UNSUPPORTED_TYPE_FOR_SERIALIZATION<T, is_metaclass<Type>::value>)};

// If typename "Type" is a shared_ptr type and arg points to the object
// then is_shared_ptr<VarType>::is_empty(arg) returns true, otherwise false

template<typename Type> struct is_shared_ptr {
	static inline bool is_empty(const Type& arg) {
		return false;
	}
};

template<typename Type> struct is_shared_ptr<std::shared_ptr<Type>> {
	static inline bool is_empty(const std::shared_ptr<Type>& arg) {
		return arg.get() == nullptr;
	}
};

//----------------- MetaDataLoop - perfoms visitor's operator() for every member ----------------------

template<typename Type> void PrintOutType() {
	int t = 0;
}

template<typename T, typename VisitorType>
struct MetaDataLoopImpl {
	static inline bool Loop(VisitorType& visitor) {
#ifdef WIN32
		if (visitor.operator()<typename T::first_type, typename T::first_type::VarType>()) {
#else
		if (visitor.template operator()<typename T::first_type, typename T::first_type::VarType>()) {
#endif
			return MetaDataLoopImpl<typename T::second_type, VisitorType>::Loop(visitor);
		}

		return true;
	}
};

template<typename VisitorType>
struct MetaDataLoopImpl<recursion_stop_type, VisitorType> {
	static inline bool Loop(VisitorType& visitor) { return false; }
};

template<typename T, typename VisitorType>
bool MetaDataLoop(VisitorType& visitor) {
	return MetaDataLoopImpl<typename T::refl_type, VisitorType>::Loop(visitor);
}

//----------------------------- Common logic --------------------------------------

inline void InitMatchArray(bool* match_array, const char* str, size_t size, bool invert)
{
    for (size_t i = 0 ; i < 256 ; i++)
        match_array[i] = !invert;

    for (size_t i = 0 ; size!=0 ? i<size : *str ; ++i) {
        unsigned char index = *reinterpret_cast<const unsigned char*>(str++);
        match_array[index] = invert;
    }
}

template<int>
inline bool* GetMatchArray(const char* str, size_t size, bool invert) {
    static bool match_array[256];
    static std::once_flag flag;
    std::call_once(flag, InitMatchArray, match_array, str, size, invert);

    return match_array;
}
 
//---------------------------- Visitor for serialization --------------------------

class pbn_serializable
{
public:
	using PtrT = std::shared_ptr<pbn_serializable>;
	virtual std::string serialize() = 0;
};

template<typename T> struct SerializeHelper;

template<typename T>
class SerializeVisitor {
	const T& t;
	std::ostream& stream;
	bool firstTime;
public:
	SerializeVisitor(const T& _t, std::ostream& _stream)
		: t(_t)
		, stream(_stream)
		, firstTime(true)
	{
		stream << std::boolalpha << std::setprecision(std::numeric_limits<double>::digits10);
	}

	template<typename MetaData, typename VarType>
	inline bool operator()() {
		if (is_shared_ptr<VarType>::is_empty(MetaData::get(t))) {
			return true;
		}
		if (firstTime) {
			firstTime = false;
		} else {
			stream << ',';
		}
		stream << '\"' << MetaData::name() << '\"' << ':';
		SerializeHelper<VarType>::Serialize(stream, MetaData::get(t));
		return true;
	}
};


template<typename T> struct SerializeHelper {
	static inline void Serialize(std::ostream& stor, const T& t) {
		ASSERT_ON_UNSUPPORTED_TYPE_FOR_SERIALIZATION(T);
		stor << '{';
		SerializeVisitor<T> serializer(t, stor);
		MetaDataLoop<T>(serializer);
		stor << '}';
	}
};

template<typename T> struct SerializeHelper< std::shared_ptr<T> > {
	static inline void Serialize(std::ostream& stor, const std::shared_ptr<T>& t) {
		if (t) {
			SerializeHelper<T>::Serialize(stor, *t);
		}
	}
};

template<> struct SerializeHelper<pbn_serializable::PtrT> {
  static inline void Serialize(std::ostream& stor, const pbn_serializable::PtrT& t) {
	  if (t) {
		  stor << t->serialize();
	  }
  }
};

template<typename T> struct SerializeHelper< std::vector<T> > {
	static inline void Serialize(std::ostream& stor, const std::vector<T>& vec) {
		stor << '[';
		for(typename std::vector<T>::const_iterator i = vec.begin() ; i != vec.end() ; ++i) {
			if (i != vec.begin())
				stor << ',';
			SerializeHelper<T>::Serialize(stor, *i);
		}
		stor << ']';
	}
};

template<typename T> struct SerializeHelper< std::list<T> > {
	static inline void Serialize(std::ostream& stor, const std::list<T>& lis) {
		stor << '[';
		for (typename std::list<T>::const_iterator i = lis.begin(); i != lis.end(); ++i) {
			if (i != lis.begin())
				stor << ',';
			SerializeHelper<T>::Serialize(stor, *i);
		}
		stor << ']';
	}
};

template<typename T> struct SerializeHelper< std::set<T> > {
  static inline void Serialize(std::ostream& stor, const std::set<T>& vec) {
	  stor << '[';
	  for(typename std::set<T>::const_iterator i = vec.begin() ; i != vec.end() ; ++i) {
		  if (i != vec.begin())
			  stor << ',';
		  SerializeHelper<T>::Serialize(stor, *i);
	  }
	  stor << ']';
  }
};

template<>
inline void SerializeHelper<long int>::Serialize(std::ostream& storage, const long int& t) {
	storage << t;
}


	template<>
inline void SerializeHelper<int>::Serialize(std::ostream& storage, const int& t) {
	storage << t;
}

template<>
inline void SerializeHelper<long long>::Serialize(std::ostream& storage, const long long& t) {
	storage << t;
}

template<>
inline void SerializeHelper<unsigned int>::Serialize(std::ostream& storage, const unsigned int& t) {
	storage << t;
}

template<>
inline void SerializeHelper<unsigned long long>::Serialize(std::ostream& storage, const unsigned long long& t) {
	storage << t;
}

#ifdef WIN32
template<>
inline void SerializeHelper<__int64>::Serialize(std::ostream& storage, const __int64& t) {
	storage << t;
}
template<>
inline void SerializeHelper<unsigned __int64>::Serialize(std::ostream& storage, const unsigned __int64& t) {
	storage << t;
}
#endif

template<>
inline void SerializeHelper<double>::Serialize(std::ostream& storage, const double& t) {
	storage << t;
}

template<>
inline void SerializeHelper<bool>::Serialize(std::ostream& storage, const bool& t) {
	storage << t;
}

template<>
inline void SerializeHelper<std::string>::Serialize(std::ostream& storage, const std::string& t) {

	bool* array = GetMatchArray<__LINE__>("\"\\/\b\f\n\r\t\0", 9, true);

	storage << '\"';
	for(std::string::const_iterator i = t.begin() ; i!= t.end() ; ++i) {
		const char curChar = *i;
		if (array[static_cast<const unsigned char>(curChar)]) {
			storage << '\\';
			switch(curChar) {
			case '\"':
				storage << '\"';
				break;
			case '\\':
				storage << '\\';
				break;
			case '/':
				storage << '/';
				break;
			case '\b':
				storage << 'b';
				break;
			case '\f':
				storage << 'f';
				break;
			case '\n':
				storage << 'n';
				break;
			case '\r':
				storage << 'r';
				break;
			case '\t':
				storage << 't';
				break;
			case 0:
				storage << "u0000";
				break;
			}
		} else {
			storage << curChar;
		}
	}
	storage << '\"';
}

//----------------------------------- Deserialization --------------------------------------------------------------

struct pbnjson_error : public std::runtime_error {
	explicit pbnjson_error(const std::string & error)
		: std::runtime_error(error)
	{}
};

struct not_enough_data : public pbnjson_error {
	not_enough_data()
		: pbnjson_error("more data expected")
	{}
};

struct syntax_error : public pbnjson_error {
	syntax_error()
		: pbnjson_error("there is a syntax error")
	{}
};

struct numeric_overflow : public pbnjson_error {
	numeric_overflow()
		: pbnjson_error("parsed value does not fit to expected type")
	{}
};

struct unicode_is_not_supported : public pbnjson_error {
	unicode_is_not_supported()
		: pbnjson_error("Unicode characters are not supported")
	{}
};

class DeserializeStorage
{
public:
	virtual ~DeserializeStorage() throw() {}

	virtual bool Eof() = 0;
	virtual char Get() = 0;
	virtual char Peek() = 0;

	template<int id>
	std::string SeekToChar(const char* str, bool invert, bool skip_objects = false) {

		static const char scope_chars_open[]	= "{[\"";
		static const char scope_chars_close[]	= "}]\"";

		std::string result;
		bool* array = GetMatchArray<id>(str, 0, invert);
		char prev_char = 0;
		std::vector<char> scope_chars;

		while (true) {
			char current_char = Peek();

			bool scope_char_popped = false;
			if (skip_objects && prev_char != '\\')
			{
				if (!scope_chars.empty())
				{
					const char * close_char = strchr(scope_chars_close, current_char);
					if (close_char != NULL && scope_chars.back() == scope_chars_open[close_char - scope_chars_close])
					{
						scope_chars.pop_back();
						scope_char_popped = true;
					}
				}

				if (!scope_char_popped && (scope_chars.empty() || scope_chars.back() != '"') && strchr(scope_chars_open, current_char) != NULL)
					scope_chars.push_back(current_char);
			}

			if (!scope_char_popped && scope_chars.empty() && !array[static_cast<unsigned char>(current_char)])
				break;

			result += current_char;
			Get();

			if (current_char == '\\' && prev_char == '\\')
				prev_char = 0;
			else
				prev_char = current_char;
		}

		if (invert && Eof())
			throw not_enough_data();

		return result;
	}
};

class DeserializeStorageStream : public DeserializeStorage {
	std::istream & stream;
	int data;
	bool eof;

public:
	explicit DeserializeStorageStream(std::istream & _stream) : stream(_stream), data(0), eof(false)
	{
	}

	bool Eof()
	{
		return eof;
	}

	char Get()
	{
		if (eof)
			throw not_enough_data();

		data = stream.get();
		if (data < 0)
		{
			eof = true;
			data = 0;
		}

		return data;
	}

	char Peek()
	{
		return data;
	}
};

class DeserializeStorageBuffer : public DeserializeStorage {
	const char* data;
	size_t length;
	const char* curPos;
	const char* endPtr;
public:

	DeserializeStorageBuffer(const char* _data, size_t _length)
		: data(_data)
		, length(_length)
		, curPos(_data)
		, endPtr(_data + _length)
	{
	}

	bool Eof()
	{
		return curPos == endPtr;
	}

	char Get() {
		if (Eof())
			throw not_enough_data();

		++curPos;
		if (curPos < endPtr)
			return *curPos;

		return 0;
	}

	char Peek() {
		if (Eof())
			return 0;

		return *curPos;
	}
};


template<typename T> struct DeserializeHelper;

template<typename T>
class DeserializeVisitor {
	T& t;
	DeserializeStorage& storage;
	std::string& targetName;
public:
	DeserializeVisitor(T& _t, DeserializeStorage& _stor, std::string& _targetName)
		: t(_t)
		, storage(_stor)
		, targetName(_targetName)
	{}

	template<typename MetaData, typename VarType>
	inline bool operator()() {
		if (MetaData::name() == targetName) {
			DeserializeHelper<VarType>::Deserialize(storage, MetaData::set(t));
			return false;
		}
		return true;
	}
};

template<typename T> struct DeserializeHelper {
    static inline void Deserialize(DeserializeStorage& stor, T& t) {
        ASSERT_ON_UNSUPPORTED_TYPE_FOR_SERIALIZATION(T);

        // wait for begin of an object
        std::string null = stor.SeekToChar<__LINE__>("{l", false);
        null += stor.Get();
        null += stor.Peek();
        if (null == "null")
        {
            stor.Get();
            return;
        }

        // iterate members
        while(true) {
            stor.SeekToChar<__LINE__>(__PBNJSON_WHITESPACES ",", true);
            if (stor.Peek() == '}') // end of object
                break;
            else if (stor.Peek() != '\"')
                throw syntax_error();

            stor.Get();
            std::string name = stor.SeekToChar<__LINE__>("\"", false);
            stor.SeekToChar<__LINE__>(":", false);
            stor.Get();

            // skip whitesapces;
            stor.SeekToChar<__LINE__>(__PBNJSON_WHITESPACES, true);

            DeserializeVisitor<T> visitor(t, stor, name);
            if (!MetaDataLoop<T>(visitor)) // skip unknown field
                stor.SeekToChar<__LINE__>(",}", false, true);
        }

        stor.SeekToChar<__LINE__>("}", false);
        if (!stor.Eof())
            stor.Get();
    }
};

template<typename T> struct DeserializeHelper< std::shared_ptr<T> > {
	static inline void Deserialize(DeserializeStorage& stor, std::shared_ptr<T>& ptr) {

		T* p = new (std::nothrow) T();
		if (p != nullptr)
		{
			ptr.reset(p);
			DeserializeHelper<T>::Deserialize(stor, *ptr);
		}
	}
};

template<typename T> struct DeserializeHelper< std::vector<T> > {
	static inline void Deserialize(DeserializeStorage& stor, std::vector<T>& outVec) {
		std::vector<T> vec;
        std::string null = stor.SeekToChar<__LINE__>("[,}]", false);
        if (stor.Peek() != '[')
        {
            if (null != "null" && null != "{}")
            {
                throw syntax_error();
            }
            return;
        }
        stor.Get();
        stor.SeekToChar<__LINE__>(__PBNJSON_WHITESPACES, true);
        while (stor.Peek() != ']') {
            T t;
            DeserializeHelper<T>::Deserialize(stor, t);
            vec.push_back(t);
            stor.SeekToChar<__LINE__>(",]", false);
            if (stor.Peek() == ',') {
                stor.Get();
                stor.SeekToChar<__LINE__>(__PBNJSON_WHITESPACES, true);
            }
        }
		stor.Get();
		if (!vec.empty())
		{
			outVec.swap(vec);
		}
	}
};

template<typename T> struct DeserializeHelper< std::list<T> > {
	static inline void Deserialize(DeserializeStorage& stor, std::list<T>& lis) {
		std::string null = stor.SeekToChar<__LINE__>("[,}]", false);
		if (stor.Peek() != '[')
		{
            if (null != "null" && null != "{}")
            {
                throw syntax_error();
            }
			return;
		}
		stor.Get();
		stor.SeekToChar<__LINE__>(__PBNJSON_WHITESPACES, true);
		while (stor.Peek() != ']') {
			T t;
			DeserializeHelper<T>::Deserialize(stor, t);
			lis.push_back(t);
			stor.SeekToChar<__LINE__>(",]", false);
			if (stor.Peek() == ',') {
				stor.Get();
				stor.SeekToChar<__LINE__>(__PBNJSON_WHITESPACES, true);
			}
		}
		stor.Get();
	}
};

template<typename T> struct DeserializeHelper< std::set<T> > {
    static inline void Deserialize(DeserializeStorage& stor, std::set<T>& se) {
        std::string null = stor.SeekToChar<__LINE__>("[,}]", false);
        if (stor.Peek() != '[')
        {
            if (null != "null" && null != "{}")
            {
                throw syntax_error();
            }
            return;
        }
        stor.Get();
        stor.SeekToChar<__LINE__>(__PBNJSON_WHITESPACES, true);
        while (stor.Peek() != ']') {
            T t;
            DeserializeHelper<T>::Deserialize(stor, t);
            se.insert(t);
            stor.SeekToChar<__LINE__>(",]", false);
            if (stor.Peek() == ',') {
                stor.Get();
                stor.SeekToChar<__LINE__>(__PBNJSON_WHITESPACES, true);
            }
        }
        stor.Get();
    }
};


class SkipQuotes
{
public:
	explicit SkipQuotes(DeserializeStorage & storage) : m_do_close(false), m_storage(storage)
	{
		if (m_storage.Peek() == '"')
		{
			m_storage.Get();
			m_do_close = true;
		}
	}

	~SkipQuotes() throw()
	{
		try
		{
			close();
		}
		catch(...)
		{
		}
	}

	void close()
	{
		if (m_do_close)
		{
			m_do_close = false;
			if (m_storage.Peek() != '"')
				throw not_enough_data();
			m_storage.Get();
		}
	}


private:
	bool					m_do_close;
	DeserializeStorage &	m_storage;
};

template <class T>
class NumericTypeLen
{
    struct GetDigCount
    {
        int operator()(T number)
        {
            std::stringstream ss;
            ss << number;
            if (number < 0)
            {
                return ss.str().length() - 1;
            }
            return ss.str().length();
        }
    };

public:
    static int max()
    {
        GetDigCount op;
        return op(std::numeric_limits<T>::max());
    }

    static int min()
    {
        GetDigCount op;
        return op(std::numeric_limits<T>::min());
    }
};


template<>
inline void DeserializeHelper<int>::Deserialize(DeserializeStorage& storage, int& t) {
	SkipQuotes qutes(storage);

	bool minus = false;
	if (storage.Peek() == '-')
	{
		storage.Get();
		minus = true;
	}

	std::string str = storage.SeekToChar<__LINE__>("0123456789", true);
	if (str.empty())
	{
		std::string null = storage.SeekToChar<__LINE__>(",}", false);
        if (null != "null" && null != "{}")
        {
            throw syntax_error();
        }
		t = 0;
		return;
	}

	long val = strtol(str.c_str(), NULL, 10);
	if (minus)
		val = -val;

	static int maxLenInt = NumericTypeLen<int>::max();
    static int minLenInt = NumericTypeLen<int>::min();

	if (   (val > std::numeric_limits<int>::max())
		|| (val < std::numeric_limits<int>::min())
		|| (str.length() > minLenInt && val < 0)
		|| (str.length() > maxLenInt && val > 0 ))
	{
		throw numeric_overflow();
	}

	t = static_cast<int>(val);

	qutes.close();
}

template<>
inline void DeserializeHelper<unsigned int>::Deserialize(DeserializeStorage& storage, unsigned int& t) {
    SkipQuotes qutes(storage);

    std::string str = storage.SeekToChar<__LINE__>("0123456789", true);
    if (str.empty())
    {
        std::string null = storage.SeekToChar<__LINE__>(",}", false);
        if (null != "null" && null != "{}")
        {
            throw syntax_error();
        }
        t = 0;
        return;
    }

    unsigned long val = strtoul(str.c_str(), NULL, 10);

    static int maxLenUInt = NumericTypeLen<unsigned int>::max();

    if ((val > std::numeric_limits<unsigned int>::max()) || (str.length() > maxLenUInt))
    {
        throw numeric_overflow();
    }

    t = static_cast<unsigned int>(val);

    qutes.close();
}

template<>
inline void DeserializeHelper<long int>::Deserialize(DeserializeStorage& storage, long int& t) {
	SkipQuotes qutes(storage);

	bool minus = false;
	if (storage.Peek() == '-')
	{
		storage.Get();
		minus = true;
	}

	std::string str = storage.SeekToChar<__LINE__>("0123456789", true);
	if (str.empty())
	{
		std::string null = storage.SeekToChar<__LINE__>(",}", false);
        if (null != "null" && null != "{}")
        {
            throw syntax_error();
        }
		t = 0;
		return;
	}

	long long val = strtoll(str.c_str(), NULL, 10);
	if (minus)
		val = -val;

    static int maxLenLongInt = NumericTypeLen<long int>::max();
    static int minLenLongInt = NumericTypeLen<long int>::min();

	if (   (val > std::numeric_limits<long int>::max())
		   || (val < std::numeric_limits<long int>::min())
		   || (str.length() > minLenLongInt && val < 0)
		   || (str.length() > maxLenLongInt && val > 0 ))
	{
		throw numeric_overflow();
	}

	t = static_cast<long int>(val);

	qutes.close();
}

template<>
inline void DeserializeHelper<unsigned long int>::Deserialize(DeserializeStorage& storage, unsigned long int& t) {
    SkipQuotes qutes(storage);

    std::string str = storage.SeekToChar<__LINE__>("0123456789", true);
    if (str.empty())
    {
        std::string null = storage.SeekToChar<__LINE__>(",}", false);
        if (null != "null" && null != "{}")
        {
            throw syntax_error();
        }
        t = 0;
        return;
    }

    unsigned long long val = strtoull(str.c_str(), NULL, 10);

    static int maxLenULongInt = NumericTypeLen<unsigned long int>::max();

    if ((val > std::numeric_limits<unsigned long int>::max()) || (str.length() > maxLenULongInt))
    {
        throw numeric_overflow();
    }

    t = static_cast<unsigned long int>(val);

    qutes.close();
}

template<>
inline void DeserializeHelper<long long>::Deserialize(DeserializeStorage& storage, long long& t) {
	SkipQuotes qutes(storage);

	bool minus = false;
	if (storage.Peek() == '-')
	{
		storage.Get();
		minus = true;
	}

	std::string str = storage.SeekToChar<__LINE__>("0123456789", true);
	if (str.empty())
	{
		std::string null = storage.SeekToChar<__LINE__>(",}", false);
        if (null != "null" && null != "{}")
        {
            throw syntax_error();
        }
		t = 0;
		return;
	}

    static int maxLenLongLong = NumericTypeLen<long long>::max();
    static int minLenLongLon = NumericTypeLen<long long>::min();

	if ((!minus && str.length() > maxLenLongLong)
			|| (minus && str.length() > minLenLongLon))
	{
		throw numeric_overflow();
	}

	long long val = strtoll(str.c_str(), NULL, 10);
	if (minus)
		val = -val;

	t = val;

	qutes.close();
}

template<>
inline void DeserializeHelper<unsigned long long>::Deserialize(DeserializeStorage& storage, unsigned long long& t) {
	SkipQuotes qutes(storage);

	std::string str = storage.SeekToChar<__LINE__>("0123456789", true);
	if (str.empty())
	{
		std::string null = storage.SeekToChar<__LINE__>(",}", false);
        if (null != "null" && null != "{}")
        {
            throw syntax_error();
        }
		t = 0;
		return;
	}

    static int maxLenULongLong = NumericTypeLen<unsigned long long>::max();

	if (str.length() > maxLenULongLong)
	{
		throw numeric_overflow();
	}

	unsigned long long val = strtoull(str.c_str(), NULL, 10);

	t = val;

	qutes.close();
}

#ifdef WIN32
template<>
inline void DeserializeHelper<__int64>::Deserialize(DeserializeStorage& storage, __int64& t) {
	SkipQuotes qutes(storage);

	bool minus = false;
	if (storage.Peek() == '-')
	{
		storage.Get();
		minus = true;
	}

	std::string str = storage.SeekToChar<__LINE__>("0123456789", true);
	if (str.empty())
	{
		std::string null = storage.SeekToChar<__LINE__>(",}", false);
		if (null != "null")
			throw syntax_error();
		t = 0L;
		return;
	}

	__int64 val = _strtoi64(str.c_str(), NULL, 10);
	if (minus)
		val = -val;

	if (   (val > std::numeric_limits<__int64>::max())
		|| (val < std::numeric_limits<__int64>::min())
		|| (str.length() > 21 && val < 0)
		|| (str.length() > 20 && val > 0 ))
	{
		throw numeric_overflow();
	}

	t = static_cast<__int64>(val);

	qutes.close();
}

template<>
inline void DeserializeHelper<unsigned __int64>::Deserialize(DeserializeStorage& storage, unsigned __int64& t) {
	SkipQuotes qutes(storage);

	std::string str = storage.SeekToChar<__LINE__>("0123456789", true);
	if (str.empty())
	{
		std::string null = storage.SeekToChar<__LINE__>(",}", false);
		if (null != "null")
			throw syntax_error();
		t = 0L;
		return;
	}

	unsigned __int64 val = _strtoui64(str.c_str(), NULL, 10);

	if (   (val > std::numeric_limits<unsigned __int64>::max())
		|| (str.length() > 20))
	{
		throw numeric_overflow();
	}

	t = static_cast<unsigned __int64>(val);

	qutes.close();
}
#endif

template<>
inline void DeserializeHelper<double>::Deserialize(DeserializeStorage& storage, double& t) {
	SkipQuotes qutes(storage);

	bool minus = false;
	if (storage.Peek() == '-')
	{
		storage.Get();
		minus = true;
	}

	// find floating point if presents
	std::string str = storage.SeekToChar<__LINE__>("0123456789", true);
	if (str.empty())
	{
		std::string null = storage.SeekToChar<__LINE__>(",}", false);
        if (null != "null" && null != "{}")
        {
            throw syntax_error();
        }
		t = 0.0;
		return;
	}

	if ('.' == storage.Peek()) {
		str += storage.Peek();
		storage.Get();
		str += storage.SeekToChar<__LINE__>("0123456789", true);
	}

	//// check if number of digits fits the limit of double
	//if (str.length() > std::numeric_limits<double>::digits10) {
	//    throw numeric_overflow();
	//}

	if (('e' == storage.Peek()) || ('E' == storage.Peek())) {
		str += storage.Get();
		if (('-' == storage.Peek()) || ('+' == storage.Peek())) {
			str += storage.Get();
		}

		// check zero length exponent
		std::string exp_digits = storage.SeekToChar<__LINE__>("0123456789", true);
		if (exp_digits.empty())
			throw syntax_error();

		//check if exponent id in valid range
		long exponent = strtol(exp_digits.c_str(), NULL, 10);
		//exponent += long((float_pos ? (float_pos - before_float) : (before_exp - before_float)) - 1);
		if ( (exponent < std::numeric_limits<double>::min_exponent10)
			 || (exponent > std::numeric_limits<double>::max_exponent10) )
		{
			throw numeric_overflow();
		}
		str += exp_digits;
	}

	t = strtod(str.c_str(), NULL);
	if (minus)
		t = -t;

	qutes.close();
}

template<>
inline void DeserializeHelper<bool>::Deserialize(DeserializeStorage& storage, bool& t) {
	SkipQuotes qutes(storage);

	std::string str = boost::trim_copy(storage.SeekToChar<__LINE__>("{},\"", false));
	if (str == "true") {
		t = true;
	}
	else if (str == "false" || str == "null") {
		t = false;
	}
	else {
		throw syntax_error();
	}

	qutes.close();
}

template<>
inline void DeserializeHelper<std::string>::Deserialize(DeserializeStorage& storage, std::string& t) {

	if (storage.Peek() != '\"')
	{
		std::string null = storage.SeekToChar<__LINE__>(",}", false);
        if (null != "null" && null != "{}")
        {
            throw syntax_error();
        }

		t.clear();
		return;
	}

	std::string out;
	do
	{
		storage.Get();
		out += storage.SeekToChar<__LINE__>("\"\\", false);
		if (storage.Peek() == '\"')
			break;

		// check escape characters
		switch(storage.Get()) {
		case 'u': {
			char number[4] = {0,0,0,0};
			for (int i = 0; i < 4; ++i) {
				number[i] = storage.Get();
			}
			unsigned long val = strtoul(number, NULL, 4);
			if (val > 255)
				throw unicode_is_not_supported();
			out += static_cast<char>(val);
			break;
		}
		case '\"':
			out += '\"';
			break;
		case '\\':
			out += '\\';
			break;
		case '/':
			out += '/';
			break;
		case 'b':
			out += '\b';
			break;
		case 'f':
			out += '\f';
			break;
		case 'n':
			out += '\n';
			break;
		case 'r':
			out += '\r';
			break;
		case 't':
			out += '\t';
			break;
		default:
			throw syntax_error();
		}
	}
	while(true);

	storage.Get();
	t.swap(out);
}

}

//#ifdef WIN32
//#define max(a,b) ((a) < (b) ? (b) : (a))
//#define min(a,b) ((a) > (b) ? (b) : (a))
//#endif

#endif // __PBNJSON_SERIALIZATION_CORE_D7380DB3C5D742B6A640DFF5474FC963
