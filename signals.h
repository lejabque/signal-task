#pragma once
#include <functional>
#include "intrusive_list.h"

namespace signals {

template<typename T>
struct signal;

template<typename... Args>
struct signal<void(Args...)> {
  using slot_t = std::function<void(Args...)>;

  struct connection : intrusive::list_element<struct connection_tag> {
    connection() noexcept = default;

    connection(signal* sig, slot_t slot) noexcept
        : sig(sig),
          slot(std::move(slot)) {
      sig->connections.push_front(*this);
    }
    connection(connection const&) = delete;
    connection(connection const&&) = delete;

    connection(connection&& other) noexcept
        : slot(std::move(other.slot)),
          sig(other.sig) {
      replace_sig(other);
    }

    connection& operator=(connection&& other) noexcept {
      if (this != &other) {
        unlink();
        sig = other.sig;
        slot = std::move(other.slot);
        replace_sig(other);
      }
      return *this;
    }

    void disconnect() noexcept {
      if (is_linked()) {
        unlink();
        slot = {};
        for (iteration_token* tok = sig->top_token; tok; tok = tok->next) {
          if (tok->current != sig->connections.end() && &*tok->current == this) {
            ++tok->current;
          }
        }
        sig = nullptr;
      }
    }

    ~connection() {
      disconnect();
    }

    friend struct signal;

   private:
    void replace_sig(connection& other) noexcept {
      if (other.is_linked()) {
        sig->connections.insert(sig->connections.as_iterator(other), *this);
        other.unlink();
        for (iteration_token* tok = sig->top_token; tok; tok = tok->next) {
          if (tok->current != sig->connections.end() && &*tok->current == &other) {
            tok->current = sig->connections.as_iterator(*this);
          }
        }
      }
    }

   private:
    signal* sig;
    slot_t slot;
  };

  using connections_t = intrusive::list<connection, struct connection_tag>;

  signal() = default;

  signal(signal const&) = delete;
  signal& operator=(signal const&) = delete;

  ~signal() {
    for (iteration_token* tok = top_token; tok; tok = tok->next) {
      tok->destroyed = true;
    }
  }

  connection connect(std::function<void(Args...)> slot) noexcept {
    return connection(this, std::move(slot));
  }

  void operator()(Args... args) const {
    iteration_token tok(connections.begin(), top_token);
    top_token = &tok;
    try {
      while (tok.current != connections.end()) {
        auto copy = tok.current;
        ++tok.current;
        copy->slot(args...);
        if (tok.destroyed) {
          return;
        }
      }
    } catch (...) {
      top_token = tok.next;
      throw;
    }
    top_token = tok.next;
  }

 private:
  struct iteration_token {
    using iterator_t = typename connections_t::const_iterator;
    iteration_token(iterator_t current, iteration_token* next)
        : current(std::move(current)),
          next(next),
          destroyed(false) {}

    iterator_t current;
    iteration_token* next;
    bool destroyed;
  };

  connections_t connections;
  mutable iteration_token* top_token = nullptr;
};

} // namespace signals
