#pragma once

#include <memory>

using namespace std;

template <typename T>
class TRef {
public:
    using Base = std::shared_ptr<T>;

private:
    Base ptr;

public:
    // Конструктор по умолчанию (пустой)
    TRef() noexcept = default;

    // Конструктор с передаваемым значением типа T
    TRef(const T& v) : ptr(std::make_shared<T>(v)) {}

    // Конструктор с передаваемым временным значением типа T
    TRef(T&& v) : ptr(std::make_shared<T>(std::move(v))) {}

    // Конструктор с std::shared_ptr<T>
    TRef(std::shared_ptr<T> v) noexcept : ptr(std::move(v)) {}

    // Конструктор с nullptr
    TRef(std::nullptr_t) noexcept : ptr(nullptr) {}

    // Оператор присваивания из другого TRef
    TRef& operator=(const TRef& other) {
        if (this != &other) {
            ptr = other.ptr;
        }
        return *this;
    }

    // Оператор присваивания из временного TRef
    TRef& operator=(TRef&& other) noexcept {
        if (this != &other) {
            ptr = std::move(other.ptr);
        }
        return *this;
    }

    // Оператор присваивания из std::shared_ptr<T>
    TRef& operator=(std::shared_ptr<T> other) noexcept {
        ptr = std::move(other);
        return *this;
    }

    // Оператор присваивания из значения типа T
    TRef& operator=(const T& v) {
        if (ptr) {
            *ptr = v;  // если объект существует, обновляем его
        } else {
            ptr = std::make_shared<T>(v);  // если объект не существует, создаём новый
        }
        return *this;
    }

    // Оператор присваивания из временного значения типа T
    TRef& operator=(T&& v) {
        if (ptr) {
            *ptr = std::move(v);  // если объект существует, обновляем его
        } else {
            ptr = std::make_shared<T>(std::move(v));  // если объект не существует, создаём новый
        }
        return *this;
    }

    // Оператор присваивания nullptr
    TRef& operator=(std::nullptr_t) noexcept {
        ptr = nullptr;
        return *this;
    }

    // Преобразование в bool для проверки на пустоту
    explicit operator bool() const noexcept {
        return static_cast<bool>(ptr);
    }

    // Получить указатель на объект (если не пустой)
    T* operator->() noexcept {
        return ptr.get();
    }

    const T* operator->() const noexcept {
        return ptr.get();
    }

    // Доступ к объекту (если не пустой)
    T& operator*() noexcept {
        return *ptr;
    }

    const T& operator*() const noexcept {
        return *ptr;
    }
};
