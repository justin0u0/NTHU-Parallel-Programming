#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <cmath>

typedef std::pair<std::string, int> Item;

int main(int argc, char **argv)
{
    std::string job_name = std::string(argv[1]);
    int num_reducer = std::stoi(argv[2]);
    int delay = std::stoi(argv[3]);
    std::string input_filename = std::string(argv[4]);
    int chunk_size = std::stoi(argv[5]);
    std::string locality_config_filename = std::string(argv[6]);
    std::string output_dir = std::string(argv[7]);

    std::map<std::string, int> word_count;
    // read words file
    std::ifstream input_file(input_filename);
    std::string line;
    while (getline(input_file, line))
    {
        size_t pos = 0;
        std::string word;
        std::vector<std::string> words;
        while ((pos = line.find(" ")) != std::string::npos)
        {
            word = line.substr(0, pos);
            words.push_back(word);

            if (word_count.count(word) == 0)
            {
                word_count[word] = 1;
            }
            else
            {
                word_count[word]++;
            }

            line.erase(0, pos + 1);
        }
        if (!line.empty())
            words.push_back(line);
        for (auto word : words)
        {
            std::cout << word << " ";
            if (word_count.count(word) == 0)
            {
                word_count[word] = 1;
            }
            else
            {
                word_count[word]++;
            }
        }
        std::cout << "\n";
    }

    std::vector<Item> wordcount_list;
    for (auto &item : word_count)
    {
        wordcount_list.push_back(item);
    }
    std::sort(wordcount_list.begin(), wordcount_list.end(), [](const Item &item1, const Item &item2) -> bool
              { return item1.first < item2.first; });

    size_t item_per_file = ceil((double)wordcount_list.size() / num_reducer);
    int reducer_task_id = 1;

    for (size_t i = 0; i < wordcount_list.size(); i += item_per_file)
    {
        size_t size = std::min(i + item_per_file, word_count.size());
        std::vector<Item> chunk(wordcount_list.begin() + i, wordcount_list.begin() + size);
        std::ofstream out(output_dir + job_name + "-" + std::to_string(reducer_task_id) + ".out");
        for (int j = 0; j < chunk.size(); j++)
        {
            out << chunk.at(j).first << " " << chunk.at(j).second << "\n";
        }
        reducer_task_id++;
    }

    return 0;
}
