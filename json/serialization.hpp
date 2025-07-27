#ifndef __PBNJSON_SERIALIZATION_0F07ED48FB064FE49AC12265CF78873B
#define __PBNJSON_SERIALIZATION_0F07ED48FB064FE49AC12265CF78873B
#include "serialization_core.hpp"

namespace pbnjson {

/*--------------------------------------Usage ------------------------------------------------
*
*
*/

//---------------------------------- Serialization -------------------------------------------
template<class T>
inline void pbnjson_serialize(const T& t, std::ostream& oStream) {
	SerializeHelper<T>::Serialize(oStream, t);
}

template<class T>
inline void pbnjson_serialize(const T& t, std::string& oString) {
	std::ostringstream oStream;
	pbnjson_serialize(t, oStream);
	oString = oStream.str();
}

//----------------------------------- Deserialization -----------------------------------------
template<class T>
inline void pbnjson_deserialize(const char* data, size_t length, T& t) {
	DeserializeStorageBuffer deStor(data, length);
	DeserializeHelper<T>::Deserialize(deStor, t);
}

template<class T>
inline void pbnjson_deserialize(const std::string& iString, T& t) {
	pbnjson_deserialize(iString.c_str(), iString.length(), t);
}

template<class T>
inline void pbnjson_deserialize(std::istream& stream, T& t) {
	DeserializeStorageStream deStor(stream);
	DeserializeHelper<T>::Deserialize(deStor, t);
}


} //namespace pbnjson

#endif //__PBNJSON_SERIALIZATION_0F07ED48FB064FE49AC12265CF78873B
