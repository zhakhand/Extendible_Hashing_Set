#ifndef EH_SET_H
#define EH_SET_H

#include <functional>
#include <algorithm>
#include <iostream>
#include <stdexcept>

template <typename Key, size_t N = 16>
class EH_set {
public:
    class Iterator;
    using value_type = Key;
    using key_type = Key;
    using reference = value_type &;
    using const_reference = const value_type &;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using const_iterator = Iterator;
    using iterator = const_iterator;
    using key_compare = std::less<key_type>;
    using key_equal = std::equal_to<key_type>;
    using hasher = std::hash<key_type>;

private:
    struct Bucket;
    Bucket** directory;
    size_t curr_size;
    size_t global_depth;
    void add(const key_type &key);
public:
    EH_set() : curr_size{0},global_depth{1}{
        directory = new Bucket*[1ULL << global_depth];
        for (size_t i{0}; i < (1ULL << global_depth); ++i) {
            directory[i] = new Bucket;
        }
    }
    EH_set(std::initializer_list<key_type> ilist) : EH_set{}{
        insert(ilist);
    }
    template<typename InputIt> EH_set(InputIt first, InputIt last) : EH_set{}{
        insert(first, last);
    } 
    EH_set(const EH_set &other) : EH_set{other.begin(), other.end()}{}

    ~EH_set(){
        for(size_t i{0}; i < (1ULL << global_depth); ++i){
            if(directory[i] != nullptr && (i & ((1ULL << directory[i]->local_depth) - 1)) == i){
                Bucket* bucket = directory[i];
                delete bucket;
            }
        }
        delete[] directory;
    }

    EH_set &operator=(const EH_set &other){
        if(this == &other) return *this;
        EH_set temp{other};
        swap(temp);
        return *this;
    }
    EH_set &operator=(std::initializer_list<key_type> ilist){
        EH_set temp{ilist};
        swap(temp);
        return *this;
    }

    size_type size() const{return curr_size;}
    bool empty() const{return size() == 0;} 

    void insert(std::initializer_list<key_type> ilist){insert(ilist.begin(), ilist.end());}
    std::pair<iterator,bool> insert(const key_type &key){
        if(!count(key)){
            add(key);
            curr_size++;
            return {iterator(*this, key), true};
        }else{
            return {iterator(*this, key), false};
        }
    }
    template<typename InputIt> void insert(InputIt first, InputIt last){
        for(auto it{first}; it != last; ++it){
            insert(*it);
        }
    }

    void double_dir(){
        Bucket** new_dir{new Bucket*[1ULL << (global_depth + 1)]{nullptr}};
        for(size_type i{0}; i < (1ULL << global_depth); ++i){
            new_dir[i] = new_dir[i + (1ULL << global_depth)] = directory[i];
        }
        delete[] directory;
        directory = new_dir;
        global_depth++;
    }

    void clear(){
        EH_set temp;
        swap(temp);
    }

    size_type erase(const key_type &key){
        auto er{directory[hasher{}(key) & ((1ULL << global_depth) - 1)]->erase(key)};
        curr_size -= er;
        return er;
    }

    size_type count(const key_type &key) const{
        return find(key) != end() ? 1 : 0;
    }

    iterator find(const key_type &key) const{
        size_type index{hasher{}(key) & ((1ULL << global_depth)-1)};
        Bucket* bucket = directory[index];
        if(bucket->find(key) != nullptr) return iterator(*this, key);
        return end();
    }

    void swap(EH_set &other){
        using std::swap;
        swap(directory, other.directory);
        swap(curr_size, other.curr_size);
        swap(global_depth, other.global_depth);
    }

    const_iterator begin() const{
        if(curr_size == 0) return end();
        return const_iterator(*this);
    }

    const_iterator end() const{
        return const_iterator();
    }

    void dump(std::ostream &o = std::cerr) const{
        o << "N = " << N << " size = " << curr_size << ", d = " << global_depth << '\n';
        for(size_type i{0}; i < (1ULL << global_depth); ++i){
            o << i  << ": [";
            if(!directory[i]) {
                o << "]\n";
                continue;
            }
            for(size_type j{0}; j < directory[i]->count; ++j){
                o << " " << directory[i]->keys[j] << " ";
            }
            o << "] t = " << directory[i]->local_depth << '\n';
        }
    }

    friend bool operator==(const EH_set &lhs, const EH_set &rhs){
        if(lhs.curr_size != rhs.curr_size) return false;
        for(auto &k : rhs) if(!lhs.count(k)) return false;
        return true;
    }

    friend bool operator!=(const EH_set &lhs, const EH_set &rhs){
        return !(lhs == rhs);
    }
};


template<typename Key, size_t N>
void EH_set<Key, N>::add(const key_type &key) {
    size_t ind{hasher{}(key) & ((1ULL << global_depth) - 1)};
    if(directory[ind]->count < N){
        directory[ind]->insert(key);
        return;
    }
    //splitting logic

    Bucket* bucket = directory[ind];
    if(bucket->local_depth >= global_depth){
        //indexexpansion
        double_dir();
    }
    bucket = directory[ind];
    Bucket* new_bucket{new Bucket{}};
    new_bucket->local_depth = ++bucket->local_depth;

    bucket->count = 0; //clear old bucket

    for(size_t i{0}; i < N; ++i){ //shift bit to the right and isolate the lsb. if bit is 0 return the value to old bucket.
        (hasher{}(bucket->keys[i])) >> (bucket->local_depth - 1) & 1 ? new_bucket->insert(bucket->keys[i]) : bucket->insert(bucket->keys[i]);
    }

    size_type dist{(1ULL << (bucket->local_depth - 1))}; //distance between old buckets
    size_type start{(ind & (dist - 1)) + dist};  //first occurence of the new bucket
    dist += dist; //distance between pointers to new bucket
    for(size_type i{start}; i < (1ULL << global_depth); i+=dist){
        directory[i] = new_bucket;
    }
    add(key); //recursively recall the method
}

template <typename Key, size_t N>
struct EH_set<Key, N>::Bucket{
    Key keys[N];
    size_type local_depth{1};
    size_type count{0};

    const key_type* find(const key_type &key) const {
        if(count == 0) return nullptr;
        for(size_type i{0}; i < count; ++i){
            if(key_equal{}(key, keys[i])) return &keys[i];
        }
        return nullptr;
    }

    const key_type *insert(const key_type &key){
        return count == N ? nullptr : &(keys[count++] = key);
    }

    size_t erase(const key_type &k){
        if(count == 0) return 0;
        for(size_t i{0}; i < count; ++i){
            if(key_equal{}(k, keys[i])){
                if (i != count - 1) std::swap(keys[i], keys[count - 1]);
                count--;
                return 1;
            }
        }
        return 0;
    }
};



template <typename Key, size_t N>
class EH_set<Key,N>::Iterator {
public:
  using value_type = Key;
  using difference_type = std::ptrdiff_t;
  using reference = const value_type &;
  using pointer = const value_type *;
  using iterator_category = std::forward_iterator_tag;

private:
    Bucket **dir;
    size_t dir_size;
    size_t buck_ind;
    pointer ptr;

    void skip(){
        while(buck_ind < dir_size){
            if(dir[buck_ind]){
                if(dir[buck_ind]->count > 0 && (buck_ind & ((1ULL << dir[buck_ind]->local_depth) - 1)) == buck_ind){
                    if(!ptr){
                        ptr = dir[buck_ind]->keys;
                        return;
                    }
                }
            }
            ++buck_ind;
        }
        ptr = nullptr;
    }

public:
  explicit Iterator (const EH_set &d) : dir{d.directory}, dir_size{(1ULL << d.global_depth)}, buck_ind{0}, ptr{nullptr}{
    skip();
  }

  Iterator(const EH_set &d, const key_type &key) : dir{d.directory}, dir_size{1ULL << d.global_depth}{
    buck_ind = hasher{}(key) & ((1ULL << d.global_depth) - 1);
    buck_ind = buck_ind & ((1ULL << dir[buck_ind]->local_depth) - 1);
    ptr = dir[buck_ind] ? dir[buck_ind]->find(key) : nullptr;
  }

  Iterator() : dir{nullptr}, dir_size{0}, buck_ind{0}, ptr{nullptr}{}
  reference operator*() const{
    return *ptr;
  }

  pointer operator->() const{
    return ptr;
  }

  Iterator &operator++(){
    if(ptr){
        ++ptr;
        if(ptr >= dir[buck_ind]->keys + dir[buck_ind]->count){
            ++buck_ind;
            ptr = nullptr;
            skip();
        }
    }
    return *this;
  }

  Iterator operator++(int){
    auto temp{*this};
    ++*this;
    return temp;
  }

  friend bool operator==(const Iterator &lhs, const Iterator &rhs){
    return lhs.ptr == rhs.ptr;
  }

  friend bool operator!=(const Iterator &lhs, const Iterator &rhs){
    return !(lhs == rhs);
  }
};

template <typename Key, size_t N>
void swap(EH_set<Key,N> &lhs, EH_set<Key,N> &rhs) { lhs.swap(rhs); }

#endif