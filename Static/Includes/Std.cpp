#pragma once

#include <any>
#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace std;

// ----------------------------------------------------------------

template<typename Container, typename Predicate>
bool some(const Container& container, Predicate predicate) {
	return any_of(container.begin(), container.end(), predicate);
}

template<typename Container, typename T>
bool contains(const Container& container, const T& value) {
	return find(container.begin(), container.end(), value) != container.end();
}

template<typename Container, typename UnaryPredicate>
Container filter(const Container& container, UnaryPredicate predicate) {
	Container result;
	copy_if(container.begin(), container.end(), back_inserter(result), predicate);
	return result;
}

template<typename Container, typename UnaryPredicate>
Container transform(const Container& container, UnaryPredicate predicate) {
	Container result;
	result.reserve(container.size());
	std::transform(container.begin(), container.end(), back_inserter(result), predicate);
	return result;
}

template<typename Container, typename T>
int index_of(Container container, const T& value) {
	auto it = find(container.begin(), container.end(), value);

	return it != container.end() ? it-container.begin() : -1;
}

template<typename Container, typename Predicate>
int find_index(Container container, Predicate predicate) {
	auto it = find_if(container.begin(), container.end(), predicate);

	return it != container.end() ? it-container.begin() : -1;
}

template<typename T>
shared_ptr<decay_t<T>> Ref(T&& value) {
	return std::make_shared<decay_t<T>>(forward<T>(value));
}

template <typename T, typename... Args>
shared_ptr<T> Ref(Args&&... args) {
	return std::make_shared<T>(forward<Args>(args)...);
}

static string tolower(string_view string) {
	auto result = ::string(string);

	transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });

	return result;
}

template <typename T>
optional<T> any_optcast(const any& value) {
	if(const optional<T>* v = any_cast<optional<T>>(&value)) {
		return *v;
	} else
	if(const T* v = any_cast<T>(&value)) {
		return optional<T>(*v);
	}

	return nullopt;
}

template <typename T>
shared_ptr<T> any_refcast(const any& value) {
	if(const shared_ptr<T>* v = any_cast<shared_ptr<T>>(&value)) {
		return *v;
	} else
	if(const T* v = any_cast<T>(&value)) {
		return make_shared<T>(*v);
	}

	return nullptr;
}