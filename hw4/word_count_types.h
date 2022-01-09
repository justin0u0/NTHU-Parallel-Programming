#ifndef _WORD_COUNT_TYPES_H_
#define _WORD_COUNT_TYPES_H_

#include <list>
#include <string>
#include <utility>
#include <vector>

class WordCountKV {
private:
	unsigned int hashCode;
public:
	std::string key;
	int value;

	WordCountKV() {}

	WordCountKV(const std::string& key, int value) : key(key), value(value) {
		hashCode = std::hash<std::string>()(key);
	}

	bool operator < (const WordCountKV& rhs) const {
		return key.compare(rhs.key) < 0;
	}

	unsigned int hash(unsigned int modulus) {
		return hashCode % modulus;
	}
};

typedef std::list<WordCountKV> ListWordCountKV;
typedef std::vector<WordCountKV> VectorWordCountKV;
typedef std::pair<WordCountKV, int> PairWordCountKVInt;
typedef std::vector<PairWordCountKVInt> VectorPairWordCountKVInt;

class WordCountKVs {
public:
	std::string key;
	std::vector<int> values;

	WordCountKVs() {}

	WordCountKVs(const std::string& key) : key(key) {}

	int compare(const WordCountKV& b) {
		return key.compare(b.key);
	}
};

typedef std::vector<WordCountKVs> VectorWordCountKVs;

#endif // _WORD_COUNT_TYPES_H_
