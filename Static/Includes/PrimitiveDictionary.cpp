#include <unordered_map>
#include <vector>
#include <optional>
#include <iostream>

using namespace std;

struct Primitive {
    int value;

    Primitive(int val) : value(val) {}

    operator int() const {
        return value;
    }

    bool operator==(const Primitive& primitive) const {
		return value == primitive.value;
	}
};

template <>
struct std::hash<Primitive> {
	size_t operator()(const Primitive& k) const {
		return hash<int>()(k.value);
	}
};

template <typename T>
class PrimitiveDictionary {
private:
	using Entry = pair<optional<T>, optional<T>>;

	unordered_map<optional<T>, vector<size_t>> kIndexes;	// key -> indexes
	vector<Entry> iEntries;									// index -> entry

public:
	void insert(const optional<T>& key, const optional<T>& value) {
		kIndexes[key].push_back(size());
		iEntries.push_back({
			key,
			value
		});
	}

	optional<T> get(const T& key) const {
		auto it = kIndexes.find(key);

		if(it != kIndexes.end() && !it->second.empty()) {
			return iEntries[it->second.back()].second;
		}

		return nullopt;
	}

	auto begin() const {
		return iEntries.begin();
	}

	auto end() const {
		return iEntries.end();
	}

	size_t size() const {
		return iEntries.size();
	}
};

int main() {
	PrimitiveDictionary<Primitive> dict;

	dict.insert(1, 100);
	dict.insert(2, 200);
	dict.insert(nullopt, 300);
	dict.insert(1, 400); // Повтор ключа
	dict.insert(nullopt, 500);
	dict.insert(nullopt, nullopt);

	cout << "Последнее значение для ключа 1: " << (dict.get(1) ? to_string(*dict.get(1)) : "nullopt") << endl;

	cout << "Итерация в порядке добавления:" << endl;
	for(const auto& entry : dict) {
		cout << "Key: " << (entry.first ? to_string(*entry.first) : "nullopt") << ", Value: " << (entry.second ? to_string(*entry.second) : "nullopt") << endl;
	}

	cout << "Общее количество ключей: " << dict.size() << endl;

	return 0;
}