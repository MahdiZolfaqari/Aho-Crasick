#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cctype>

using namespace std;

const int ALPHABET_SIZE = 26; // Assume we only have letters a-z

// Data structure for Trie nodes
struct TrieNode
{
    TrieNode *children[ALPHABET_SIZE] = {nullptr};
    TrieNode *failureLink = nullptr;
    vector<int> output; // Index of patterns that end in this node

    TrieNode() : failureLink(nullptr) {}
};

// Class for Aho-Corasick
class AhoCorasick
{
private:
    TrieNode *root;          // The root of the Trie
    vector<string> patterns; // List of patterns

public:
    AhoCorasick(const vector<string> &patterns) : patterns(patterns)
    {
        root = new TrieNode();
        buildTrie();
        buildFailureLinks();
    }

    // Step 1: Construct the Trie
    void buildTrie()
    {
        for (int i = 0; i < patterns.size(); i++)
        {
            TrieNode *node = root;
            for (char ch : patterns[i])
            {
                int index = ch - 'a';
                if (!node->children[index])
                {
                    node->children[index] = new TrieNode();
                }
                node = node->children[index];
            }
            node->output.push_back(i); // Register the pattern in the last node
        }
    }

    // Step 2: Calculation of Failure Links
    void buildFailureLinks()
    {
        queue<TrieNode *> q;
        root->failureLink = root;

        // Initialization for first level children
        for (int i = 0; i < ALPHABET_SIZE; i++)
        {
            if (root->children[i])
            {
                root->children[i]->failureLink = root;
                q.push(root->children[i]);
            }
            else
            {
                root->children[i] = root; // Link to root for missing characters
            }
        }

        // Process the rest of the nodes
        while (!q.empty())
        {
            TrieNode *node = q.front();
            q.pop();

            for (int i = 0; i < ALPHABET_SIZE; i++)
            {
                if (node->children[i])
                {
                    TrieNode *fail = node->failureLink;
                    while (!fail->children[i])
                    {
                        fail = fail->failureLink;
                    }
                    node->children[i]->failureLink = fail->children[i];

                    // Combining the outputs of the failure node
                    node->children[i]->output.insert(
                        node->children[i]->output.end(),
                        fail->children[i]->output.begin(),
                        fail->children[i]->output.end());

                    q.push(node->children[i]);
                }
            }
        }
    }

    // Step 3: Search in the text
    void search(const string &text)
    {
        TrieNode *node = root;
        for (int i = 0; i < text.size(); i++)
        {
            int index = text[i] - 'a';
            while (!node->children[index])
            {
                node = node->failureLink; // Move backward in the Trie
            }
            node = node->children[index];

            // If a pattern is found in this position, print it
            for (int patternIndex : node->output)
            {
                cout << "Pattern \"" << patterns[patternIndex]
                     << "\" found at position " << (i - patterns[patternIndex].size() + 1) << endl;
            }
        }
    }
};

string loadCleanText(const string &filename)
{
    ifstream file(filename);
    stringstream ss;
    string line;

    while (getline(file, line))
    {
        for (char c : line)
        {
            if (isalpha(c))
                ss << (char)tolower(c);
        }
    }

    return ss.str();
}

int main()
{
    vector<string> patterns = {
        "he", "she", "his", "hers", "kk", "hello", "folder", "books", "high", "world", "wild",
        "black", "trump", "zoo", "play", "wolf", "jack", "matt", "her", "his", "held", "found"};

    // 🗂️ Load cleaned text from file
    string text = loadCleanText("sample.txt");

    AhoCorasick ac(patterns);

    auto start = chrono::high_resolution_clock::now();

    ac.search(text);

    auto end = chrono::high_resolution_clock::now();

    chrono::duration<double> duration = end - start;
    cout << "\nSearch completed in " << duration.count() << " seconds.\n";

    return 0;
}
