#ifndef BAKSHALIPOUR_FRAMEWORK
#define BAKSHALIPOUR_FRAMEWORK

#include <vector>
#include <string>
#include <iomanip>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <algorithm>
using namespace std;

/**
* A class for printing beautiful data tables.
* It's useful for logging the information contained in tabular structures.
*/
class Table {
public:
   Table(int width, int height) : width(width), height(height), cells(height, vector<string>(width)) {}

   void set_row(int row, const vector<string> &data, int start_col = 0) {
      // assert(data.size() + start_col == this->width);
      for (unsigned col = start_col; col < this->width; col += 1)
      this->set_cell(row, col, data[col]);
   }

   void set_col(int col, const vector<string> &data, int start_row = 0) {
      // assert(data.size() + start_row == this->height);
      for (unsigned row = start_row; row < this->height; row += 1)
      this->set_cell(row, col, data[row]);
   }

   void set_cell(int row, int col, string data) {
      // assert(0 <= row && row < (int)this->height);
      // assert(0 <= col && col < (int)this->width);
      this->cells[row][col] = data;
   }

   void set_cell(int row, int col, double data) {
      ostringstream oss;
      oss << setw(11) << fixed << setprecision(8) << data;
      this->set_cell(row, col, oss.str());
   }

   void set_cell(int row, int col, int64_t data) {
      ostringstream oss;
      oss << setw(11) << std::left << data;
      this->set_cell(row, col, oss.str());
   }

   void set_cell(int row, int col, int data) { this->set_cell(row, col, (int64_t)data); }

   void set_cell(int row, int col, uint64_t data) {
      ostringstream oss;
      oss << "0x" << setfill('0') << setw(16) << hex << data;
      this->set_cell(row, col, oss.str());
   }

   /**
   * @return The entire table as a string
   */
   string to_string() {
      vector<int> widths;
      for (unsigned i = 0; i < this->width; i += 1) {
         int max_width = 0;
         for (unsigned j = 0; j < this->height; j += 1)
         max_width = max(max_width, (int)this->cells[j][i].size());
         widths.push_back(max_width + 2);
      }
      string out;
      out += Table::top_line(widths);
      out += this->data_row(0, widths);
      for (unsigned i = 1; i < this->height; i += 1) {
         out += Table::mid_line(widths);
         out += this->data_row(i, widths);
      }
      out += Table::bot_line(widths);
      return out;
   }

   string data_row(int row, const vector<int> &widths) {
      string out;
      for (unsigned i = 0; i < this->width; i += 1) {
         string data = this->cells[row][i];
         data.resize(widths[i] - 2, ' ');
         out += " | " + data;
      }
      out += " |\n";
      return out;
   }

   static string top_line(const vector<int> &widths) { return Table::line(widths, "┌", "┬", "┐"); }

   static string mid_line(const vector<int> &widths) { return Table::line(widths, "├", "┼", "┤"); }

   static string bot_line(const vector<int> &widths) { return Table::line(widths, "└", "┴", "┘"); }

   static string line(const vector<int> &widths, string left, string mid, string right) {
      string out = " " + left;
      for (unsigned i = 0; i < widths.size(); i += 1) {
         int w = widths[i];
         for (int j = 0; j < w; j += 1)
         out += "─";
         if (i != widths.size() - 1)
         out += mid;
         else
         out += right;
      }
      return out + "\n";
   }

private:
   unsigned width;
   unsigned height;
   vector<vector<string>> cells;
};

template <class T> class SetAssociativeCache {
public:
   class Entry {
   public:
      uint64_t key;
      uint64_t index;
      uint64_t tag;
      bool valid;
      T data;
   };

   SetAssociativeCache(int size, int num_ways, int debug_level = 0)
   : size(size), num_ways(num_ways), num_sets(size / num_ways), entries(num_sets, vector<Entry>(num_ways)),
   cams(num_sets), debug_level(debug_level) {
      // assert(size % num_ways == 0);
      for (int i = 0; i < num_sets; i += 1)
      for (int j = 0; j < num_ways; j += 1)
      entries[i][j].valid = false;
      /* calculate `index_len` (number of bits required to store the index) */
      for (int max_index = num_sets - 1; max_index > 0; max_index >>= 1)
      this->index_len += 1;
   }

   /**
   * Invalidates the entry corresponding to the given key.
   * @return A pointer to the invalidated entry
   */
   Entry *erase(uint64_t key) {
      Entry *entry = this->find(key);
      uint64_t index = key % this->num_sets;
      uint64_t tag = key / this->num_sets;
      auto &cam = cams[index];
      // int num_erased = cam.erase(tag);
      cam.erase(tag);
      if (entry)
      entry->valid = false;
      // assert(entry ? num_erased == 1 : num_erased == 0);
      return entry;
   }

   /**
   * @return The old state of the entry that was updated
   */
   Entry insert(uint64_t key, const T &data) {
      Entry *entry = this->find(key);
      if (entry != nullptr) {
         Entry old_entry = *entry;
         entry->data = data;
         return old_entry;
      }
      uint64_t index = key % this->num_sets;
      uint64_t tag = key / this->num_sets;
      vector<Entry> &set = this->entries[index];
      int victim_way = -1;
      for (int i = 0; i < this->num_ways; i += 1)
      if (!set[i].valid) {
         victim_way = i;
         break;
      }
      if (victim_way == -1) {
         victim_way = this->select_victim(index);
      }
      Entry &victim = set[victim_way];
      Entry old_entry = victim;
      victim = {key, index, tag, true, data};
      auto &cam = cams[index];
      if (old_entry.valid) {
         // int num_erased = cam.erase(old_entry.tag);
         cam.erase(old_entry.tag);
         // assert(num_erased == 1);
      }
      cam[tag] = victim_way;
      return old_entry;
   }

   Entry *find(uint64_t key) {
      uint64_t index = key % this->num_sets;
      uint64_t tag = key / this->num_sets;
      auto &cam = cams[index];
      if (cam.find(tag) == cam.end())
      return nullptr;
      int way = cam[tag];
      Entry &entry = this->entries[index][way];
      // assert(entry.tag == tag && entry.valid);
      return &entry;
   }

   /**
   * Creates a table with the given headers and populates the rows by calling `write_data` on all
   * valid entries contained in the cache. This function makes it easy to visualize the contents
   * of a cache.
   * @return The constructed table as a string
   */
   string log(vector<string> headers) {
      vector<Entry> valid_entries = this->get_valid_entries();
      Table table(headers.size(), valid_entries.size() + 1);
      table.set_row(0, headers);
      for (unsigned i = 0; i < valid_entries.size(); i += 1)
      this->write_data(valid_entries[i], table, i + 1);
      return table.to_string();
   }

   int get_index_len() { return this->index_len; }

   void set_debug_level(int debug_level) { this->debug_level = debug_level; }

protected:
   /* should be overriden in children */
   virtual void write_data(Entry &entry, Table &table, int row) {}

   /**
   * @return The way of the selected victim
   */
   virtual int select_victim(uint64_t index) {
      /* random eviction policy if not overriden */
      return rand() % this->num_ways;
   }

   vector<Entry> get_valid_entries() {
      vector<Entry> valid_entries;
      for (int i = 0; i < num_sets; i += 1)
      for (int j = 0; j < num_ways; j += 1)
      if (entries[i][j].valid)
      valid_entries.push_back(entries[i][j]);
      return valid_entries;
   }

   int size;
   int num_ways;
   int num_sets;
   int index_len = 0; /* in bits */
   vector<vector<Entry>> entries;
   vector<unordered_map<uint64_t, int>> cams;
   int debug_level = 0;
};

template <class T> class LRUSetAssociativeCache : public SetAssociativeCache<T> {
   typedef SetAssociativeCache<T> Super;

public:
   LRUSetAssociativeCache(int size, int num_ways, int debug_level = 0)
   : Super(size, num_ways, debug_level), lru(this->num_sets, vector<uint64_t>(num_ways)) {}

   void set_mru(uint64_t key) { *this->get_lru(key) = this->t++; }

   void set_lru(uint64_t key) { *this->get_lru(key) = 0; }

protected:
   /* @override */
   int select_victim(uint64_t index) {
      vector<uint64_t> &lru_set = this->lru[index];
      return min_element(lru_set.begin(), lru_set.end()) - lru_set.begin();
   }

   uint64_t *get_lru(uint64_t key) {
      uint64_t index = key % this->num_sets;
      uint64_t tag = key / this->num_sets;
      // assert(this->cams[index].count(tag) == 1);
      int way = this->cams[index][tag];
      return &this->lru[index][way];
   }

   vector<vector<uint64_t>> lru;
   uint64_t t = 1;
};

uint64_t hash_index(uint64_t key, int index_len);
template <class T> inline T square(T x) { return x * x; }

#endif /* BAKSHALIPOUR_FRAMEWORK */
