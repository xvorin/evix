#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#ifndef _REENTRANT
#error "Compile current compilation unit and link current program with -pthread!"
#endif

namespace xvorin::evix {

template <typename B>
class ClassRegistrar {
    using ObjectGenerator = std::function<std::shared_ptr<B>()>;
    using ObjectVisitor = std::function<void(const std::string& key, std::shared_ptr<B>)>;

    struct DerivedObjectInfo {
        int serial;
        ObjectGenerator get_instance;
        ObjectGenerator create_instance;
    };

public:
    template <typename D>
    struct RegisterHelper {
        RegisterHelper(const std::string& key, int serial = 100)
        {
            ClassRegistrar::register_object<D>(key, serial);
        }

        RegisterHelper(const std::string& key, ObjectGenerator generator, int serial = 100)
        {
            ClassRegistrar::register_object(key, generator, serial);
        }
    };

    template <typename D>
    static void register_object(const std::string& key, int serial = 100)
    {
        static_assert(std::is_base_of<B, D>::value, "Type to be registered is not derived from B!");
        ObjectGenerator generator = [=]() { return std::make_shared<D>(); };
        register_object(key, generator, serial);
    }

    static void register_object(const std::string& key, ObjectGenerator generator, int serial = 100)
    {
        auto create_instance = [=]() { return generator(); };
        auto once = std::make_shared<std::once_flag>();
        // The lazily-created singleton is held in a shared slot so every copy
        // of this generator (e.g. one taken out from under the lock below)
        // observes the same cached instance instead of its own private null.
        auto retval = std::make_shared<std::shared_ptr<B>>();
        auto get_instance = [=]() {
            std::call_once(*once, [&]() { *retval = create_instance(); });
            return *retval;
        };
        std::unique_lock<std::mutex> lock_holder;
        auto& storage = get_object_storage(lock_holder);
        storage[key] = { serial, get_instance, create_instance };
    }

    static std::shared_ptr<B> get_instance(const std::string& key)
    {
        ObjectGenerator gen;
        {
            std::unique_lock<std::mutex> lock_holder;
            auto& storage = get_object_storage(lock_holder);
            auto iter = storage.find(key);
            if (iter == storage.end()) {
                return nullptr;
            }
            gen = iter->second.get_instance;
        }
        // Invoke the generator (which may construct the object, run user code,
        // and even re-enter the registry) with the storage mutex released.
        return gen();
    }

    static std::shared_ptr<B> create_instance(const std::string& key)
    {
        ObjectGenerator gen;
        {
            std::unique_lock<std::mutex> lock_holder;
            auto& storage = get_object_storage(lock_holder);
            auto iter = storage.find(key);
            if (iter == storage.end()) {
                return nullptr;
            }
            gen = iter->second.create_instance;
        }
        return gen();
    }

    static void for_each(const ObjectVisitor& visitor)
    {
        std::multimap<int, std::pair<std::string, ObjectGenerator>> generators;
        {
            std::unique_lock<std::mutex> lock_holder;
            auto& storage = get_object_storage(lock_holder);
            for (auto& s : storage) {
                generators.emplace(s.second.serial, std::make_pair(s.first, s.second.get_instance));
            }
        }
        for (auto iter = generators.begin(); iter != generators.end(); iter++) {
            visitor(iter->second.first, iter->second.second());
        }
    }

    static void for_each_reverse(const ObjectVisitor& visitor)
    {
        std::multimap<int, std::pair<std::string, ObjectGenerator>> generators;
        {
            std::unique_lock<std::mutex> lock_holder;
            auto& storage = get_object_storage(lock_holder);
            for (auto& s : storage) {
                generators.emplace(s.second.serial, std::make_pair(s.first, s.second.get_instance));
            }
        }
        for (auto iter = generators.rbegin(); iter != generators.rend(); iter++) {
            visitor(iter->second.first, iter->second.second());
        }
    }

private:
    static std::map<std::string, DerivedObjectInfo>& get_object_storage(std::unique_lock<std::mutex>& holder)
    {
        static std::mutex lock;
        static std::map<std::string, DerivedObjectInfo> storage;
        std::unique_lock<std::mutex> guard(lock);
        holder.swap(guard);
        return storage;
    }
};

} // namespace xvorin::evix

#define HELPER_NAME(n) HELPER_NAME_LEVEL1(n)
#define HELPER_NAME_LEVEL1(n) HELPER_NAME_LEVEL2(n)
#define HELPER_NAME_LEVEL2(n) ClassRegister##n

#define DECLARE_CLASSTYPE(key, Base, Derive, ...)                                                                      \
    static xvorin::evix::ClassRegistrar<Base>::RegisterHelper<Derive> HELPER_NAME(__COUNTER__)(key, ##__VA_ARGS__);

#define DECLARE_GENERATOR(key, Base, Gen, ...)                                                                         \
    static xvorin::evix::ClassRegistrar<Base>::RegisterHelper<void> HELPER_NAME(__COUNTER__)(key, Gen, ##__VA_ARGS__);
