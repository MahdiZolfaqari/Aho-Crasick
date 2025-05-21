#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <omp.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cctype>
#include <mutex>

using namespace std;

const int ALPHABET_SIZE = 26;

struct TrieNode
{
    TrieNode *children[ALPHABET_SIZE] = {nullptr};
    TrieNode *failureLink = nullptr;
    vector<int> output;

    TrieNode() : failureLink(nullptr) {}
};

class AhoCorasick
{
private:
    TrieNode *root;
    vector<string> patterns;
    mutex queueMutex;

public:
    AhoCorasick(const vector<string> &patterns) : patterns(patterns)
    {
        root = new TrieNode();
        buildTrie();
    }

    void buildTrie()
    {
        for (int i = 0; i < patterns.size(); i++)
        {
            TrieNode *node = root;
            for (char ch : patterns[i])
            {
                int index = ch - 'a';
                if (!node->children[index])
                    node->children[index] = new TrieNode();
                node = node->children[index];
            }
            node->output.push_back(i);
        }
    }

    void buildFailureLinksParallel(int numThreads)
    {
        queue<TrieNode *> q;
        root->failureLink = root;

        for (int i = 0; i < ALPHABET_SIZE; i++)
        {
            if (root->children[i])
            {
                root->children[i]->failureLink = root;
                q.push(root->children[i]);
            }
            else
            {
                root->children[i] = root;
            }
        }

        while (!q.empty())
        {
            int size = q.size();
            vector<TrieNode *> currentLevel(size);

            for (int i = 0; i < size; i++)
            {
                currentLevel[i] = q.front();
                q.pop();
            }

            vector<TrieNode *> nextLevel;

#pragma omp parallel num_threads(numThreads)
            {
                vector<TrieNode *> localNext;

#pragma omp for schedule(static)
                for (int i = 0; i < size; i++)
                {
                    TrieNode *node = currentLevel[i];

                    for (int c = 0; c < ALPHABET_SIZE; c++)
                    {
                        TrieNode *child = node->children[c];
                        if (child)
                        {
                            TrieNode *fail = node->failureLink;
                            while (!fail->children[c])
                                fail = fail->failureLink;

                            child->failureLink = fail->children[c];

                            child->output.insert(
                                child->output.end(),
                                child->failureLink->output.begin(),
                                child->failureLink->output.end());

                            localNext.push_back(child);
                        }
                    }
                }

#pragma omp critical
                nextLevel.insert(nextLevel.end(), localNext.begin(), localNext.end());
            }

            for (TrieNode *node : nextLevel)
            {
                q.push(node);
            }
        }
    }

    struct alignas(64) ThreadResult
    {
        vector<string> matches;
    };

    void searchParallel(const string &text, int numThreads)
    {
        int textLength = text.size();
        int maxPatternLen = 0;

        for (const string &pattern : patterns)
            maxPatternLen = max(maxPatternLen, (int)pattern.length());

        int chunkSize = (textLength + numThreads - 1) / numThreads;

        vector<ThreadResult> threadResults(numThreads);

#pragma omp parallel num_threads(numThreads)
        {
            int t = omp_get_thread_num();
            int start = t * chunkSize;
            int end = min(start + chunkSize + maxPatternLen - 1, textLength);

            TrieNode *node = root;
            vector<string> localMatches;

            for (int i = start; i < end; ++i)
            {
                int index = text[i] - 'a';
                while (!node->children[index])
                    node = node->failureLink;
                node = node->children[index];

                for (int patternIndex : node->output)
                {
                    int matchPos = i - patterns[patternIndex].size() + 1;
                    if (matchPos >= start && matchPos < start + chunkSize)
                    {
                        int patternLen = patterns[patternIndex].length();
                        int endPos = matchPos + patternLen - 1;

                        stringstream ss;
                        ss << "Pattern \"" << patterns[patternIndex]
                           << "\" found from index " << matchPos
                           << " to " << endPos;
                        localMatches.push_back(ss.str());
                    }
                }
            }

            threadResults[t].matches = move(localMatches);
        }

        for (const auto &result : threadResults)
        {
            for (const auto &line : result.matches)
            {
                cout << line << endl;
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
    int numThreads = 4; // 1, 2, 3, 4
    omp_set_num_threads(numThreads);

    vector<string> patterns = {
        "secret", "alimohammed", "black", "anarchist", "hallucination", "melancholy", "condition", "arab", "particular", "copyright", "head", "bomb", "lost",
        "substantial", "information", "possibility", "race", "hold", "found", "aladdin", "antagonist", "compliance", "agreement", "distribute", "prayer"};

    string text = loadCleanText("sample4.txt"); // sample1.txt, sample2.txt, sample3.txt, sample4.txt

    AhoCorasick ac(patterns);

    auto start = chrono::high_resolution_clock::now();

    ac.buildFailureLinksParallel(numThreads);
    ac.searchParallel(text, numThreads);

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> duration = end - start;

    cout << "\nSearch completed in " << duration.count() << " seconds.\n";

    return 0;
}

//...+ ساخت فیلیر لینک بصورت موازی و جستجو هم بصورت موازی + جلوگیری از فالس شیرینگ