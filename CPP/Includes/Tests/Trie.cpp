#pragma once

#include <iostream>
#include <memory>
#include <unordered_map>
#include <string>
#include <string_view>
#include <optional>
#include <initializer_list>

using namespace std;

class Trie {
private:
	struct Node {
		unordered_map<char, unique_ptr<Node>> children;
		bool leaf = false;

		Node() {}

		Node(const Node& node) {
			leaf = node.leaf;

			for(const auto& [key, child] : node.children) {
				children[key] = make_unique<Node>(*child);
			}
		}

		Node(Node&&) noexcept = default;
		Node& operator=(Node&&) noexcept = default;
	};

	unique_ptr<Node> root;

	unique_ptr<Node> cloneNode(const Node* node) const {
		if(!node) {
			return nullptr;
		}

		auto node_ = make_unique<Node>(*node);

		return node_;
	}

public:
	Trie() : root(make_unique<Node>()) {}
	Trie(const Trie& trie) : root(cloneNode(trie.root.get())) {}

	Trie(initializer_list<string> words) : root(make_unique<Node>()) {
		for (const auto& word : words) {
			add(word);
		}
	}

	Trie& operator=(const Trie& trie) {
		if(this != &trie) {
			root = cloneNode(trie.root.get());
		}

		return *this;
	}

	Trie(Trie&&) noexcept = default;
	Trie& operator=(Trie&&) noexcept = default;

	void add(const string& word) {
		Node* node = root.get();

		for(char c : word) {
			if(node->children.find(c) == node->children.end()) {
				node->children[c] = make_unique<Node>();
			}

			node = node->children[c].get();
		}

		node->leaf = true;
	}

	optional<string> search(string_view text, int position, bool shortest = true) const {
		const Node* node = root.get();
		optional<string> longestMatch;
		string match;

		for(int i = position; i < text.length(); i++) {
			char c = text[i];

			if(node->children.find(c) == node->children.end()) {
				break;
			}

			match += c;
			node = node->children.at(c).get();

			if(node->leaf) {
				if(shortest) {
					return match;
				}

				longestMatch = match;
			}
		}

		return longestMatch;
	}
};

/*
int main() {
	Trie trie = {"abc", "ab", "abcd", "bcd"};
	string text = "abcd";
	auto result = trie.search(text, 0);

	if(result) {
		cout << "Found: " << *result << endl;
	} else {
		cout << "Not found!" << endl;
	}

	return 0;
}
*/