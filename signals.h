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

    connection& operator=(connection&& other) noexcept {
      if (this != &other) {
        if (other.is_linked()) {
          other.sig->connections.insert(other.sig->connections.as_iterator(other), *this);
          other.unlink();
          for (iteration_token* tok = other.sig->top_token; tok; tok = tok->next) {
            if (tok->current != other.sig->connections.end() && &*tok->current == &other) {
              tok->current = other.sig->connections.as_iterator(*this);
            }
          }
        }
        sig = other.sig;
        slot = std::move(other.slot);
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

    friend struct signal;
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
  struct iteration_token {
    typename connections_t::const_iterator current;
    bool destroyed;
    iteration_token* next;
  };

  void operator()(Args... args) const {
    iteration_token tok;
    tok.current = connections.begin();
    tok.next = top_token;
    tok.destroyed = false;
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
  connections_t connections;
  mutable iteration_token* top_token = nullptr;
};

} // namespace signals
