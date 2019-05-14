#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <boost/shared_ptr.hpp>

template<typename T>
class LockFreeMap {
public:
    LockFreeMap(float load_factor) :
        _current(new T),
        _previous(new T),
        _map_size(0),
        _load_factor(load_factor),
        _bucket_count(_current->bucket_count()) {
    }

    template<typename K, typename V>
    void push(const K& key, const V& value) {
        ++_map_size;
        for (;;) {
            if (!_full()) {
                (*_current)[key] = value;
                return;
            } else {
                std::lock_guard<std::mutex> lock(_extend_mutex);
                if (_full()) {
                    *_previous = *_current;
                    (*_previous)[key] = value;
                    auto tmp = boost::atomic_load(&_current);
                    boost::atomic_store(&_current, _previous);
                    boost::atomic_store(&_previous, tmp);
                    _bucket_count.store(_current->bucket_count());
                }
            }
        }
    }

    boost::shared_ptr<T> share() {
        return boost::atomic_load(&_current);
    }


private:
    bool _full() {
        return _map_size /  _bucket_count >= _load_factor;
    }

private:
    boost::shared_ptr<T> _current;
    boost::shared_ptr<T> _previous;
    std::atomic<int> _map_size;
    float _load_factor;
    std::atomic<size_t> _bucket_count;

    std::mutex _extend_mutex;
};

void test_lock_free_map() {
    typedef std::unordered_map<long, std::pair<long, std::string>> InfoMap;
    LockFreeMap<InfoMap> lock_free_map(0.75);
    lock_free_map.push(123, std::make_pair(456, "789"));
    lock_free_map.push(456, std::make_pair(456, "789"));
    auto user_map = lock_free_map.share();
    for (auto it = user_map->begin(); it != user_map->end(); ++it) {
        std::cout << "user_map, key=" << it->first << ", value: first=" << it->second.first << ", second=" << it->second.second << std::endl;
    }
}

int main(int argc, char* argv[]) {
    test_lock_free_map();
    return 0;
}
